#include "bm_common.h"
#include "bm_msgfifo.tmh"

void bmdrv_msg_irq_handler(struct bm_device_info *bmdi)
{
    struct api_fifo_entry api_entry   = {0};
    LIST_ENTRY *handle_list = NULL;
	struct bm_api_info *api_info;
	u32 pending_msgirq_cnt;
	int count;
	u32 channel;
#ifdef PCIE_MODE_ENABLE_CPU
	LIST_ENTRY list_tmp;
    PLIST_ENTRY list;
	u32 next_rp;
	u32 tmp;
	u64 global_seq;
	u64 api_data;
	u32 find_device_sync_flag;
	int timeout_ms = bmdi->cinfo.delay_ms * 2;
#endif
	u64 time_us;
	InitializeListHead(&list_tmp);
	bmdi->cinfo.bmdrv_clear_msgirq(bmdi);
	pending_msgirq_cnt = bmdi->cinfo.bmdrv_pending_msgirq_cnt(bmdi);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"The pending msg irq is %d\n", pending_msgirq_cnt);
    u64         tick_count;
    LARGE_INTEGER CurTime, Freq;
    CurTime    = KeQueryPerformanceCounter(&Freq);
    tick_count = (CurTime.QuadPart * 1000000) / Freq.QuadPart;
    time_us = tick_count;

#ifdef PCIE_MODE_ENABLE_CPU
	if (bmdi->cinfo.irq_id == MSG_IRQ_ID_CHANNEL_XPU)
		channel = BM_MSGFIFO_CHANNEL_XPU;
	else
		channel = BM_MSGFIFO_CHANNEL_CPU;
	if (BM_MSGFIFO_CHANNEL_CPU == channel) {
		if (bmdi->api_info[channel].sw_rp == gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU))
			return;
	}
#else
	channel = BM_MSGFIFO_CHANNEL_XPU;
#endif
	api_info = &bmdi->api_info[channel];
    WdfSpinLockAcquire(api_info->api_fifo_spinlock);

	while (pending_msgirq_cnt) {
		if (BM_MSGFIFO_CHANNEL_XPU == channel) {
			count = kfifo_out(&api_info->api_fifo, (unsigned char *)&api_entry, API_ENTRY_SIZE);
			if (count < API_ENTRY_SIZE) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "The dequeue entry size %d is not correct!\n", count);
				break;
			}
		}
#ifdef PCIE_MODE_ENABLE_CPU
		else {
			next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, sizeof(bm_kapi_header_t) / sizeof(u32));
			tmp = shmem_reg_read_enh(bmdi, next_rp, channel);
			next_rp = bmdev_msgfifo_add_pointer(bmdi, next_rp, 1);
			global_seq = ((u64)tmp << 32) + shmem_reg_read_enh(bmdi, next_rp, channel);
			next_rp = bmdev_msgfifo_add_pointer(bmdi, next_rp, 1);
			tmp = shmem_reg_read_enh(bmdi, next_rp, channel);
			next_rp = bmdev_msgfifo_add_pointer(bmdi, next_rp, 1);
			api_data = ((u64)tmp << 32) + shmem_reg_read_enh(bmdi, next_rp, channel);

			find_device_sync_flag = 1;
			list_for_each(list, &api_info->api_list) {
				if ((1 == find_device_sync_flag)
					&& ((u64)(((struct api_list_entry *)list)->api_entry.thd_info) == DEVICE_SYNC_MARKER)) {
					// device sync marker; NOT a real api entry
					api_info->device_sync_cpl = (u64)(((struct api_list_entry *)list)->api_entry.dev_api_seq);
					KeSetEvent(&(api_info->dev_sync_doneEvent), 0, FALSE);
					list_tmp.Blink = list->Blink;
					list_tmp.Flink = list->Flink;
					RemoveEntryList(list);
                    MmFreeNonCachedMemory((struct api_list_entry *)list,
                                         sizeof(struct api_list_entry));
					list = &list_tmp;
					continue;
				} else {
					find_device_sync_flag = 0;
				}
				if ((((struct api_list_entry *)list)->api_entry.api_done_flag & API_FLAG_FINISH)
					|| ((((struct api_list_entry *)list)->api_entry.api_done_flag == API_FLAG_DONE)
						&& (time_us - ((struct api_list_entry *)list)->api_entry.sent_time_us > ((u64)timeout_ms * 1000)))) {
					list_tmp.Blink = list->Blink;
					list_tmp.Flink = list->Flink;
					RemoveEntryList(list);
                    MmFreeNonCachedMemory((struct api_list_entry *)list,
                                         sizeof(struct api_list_entry));
					list = &list_tmp;
					continue;
				}
				if (((struct api_list_entry *)list)->api_entry.global_api_seq == global_seq) {
					((struct api_list_entry *)list)->api_entry.api_data = api_data;
					((struct api_list_entry *)list)->api_entry.api_done_flag = API_FLAG_DONE;
					KeSetEvent(&(((struct api_list_entry *)list)->api_entry.api_doneEvent), 0, FALSE);
					memcpy(&api_entry, &((struct api_list_entry *)list)->api_entry, sizeof(struct api_fifo_entry));
					break; /* list_for_each */
				}
			}
			if (list == (&api_info->api_list)) {
				break; /* while (pending_msgirq_cnt) */
			}
		}
#endif
		if ((u64)api_entry.thd_info == DEVICE_SYNC_MARKER) {
			// device sync marker; NOT a real api entry
			api_info->device_sync_cpl = api_entry.dev_api_seq;
            KeSetEvent(&api_info->dev_sync_doneEvent, 0, FALSE);
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"device_sync:%d syc api send and syc api complete %d\n",api_info->device_sync_last,
                                api_info->device_sync_cpl);
			continue;
		} else {
			// msg api done
            WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);

			list_for_each(handle_list, &bmdi->handle_list) {
				if (CONTAINING_RECORD(handle_list, struct bm_handle_info, list) ==  api_entry.h_info)
					break;
			}
			if (handle_list == &bmdi->handle_list) {
				api_entry.thd_info = NULL;
				api_entry.h_info = NULL;
			}
			if (api_entry.thd_info && api_entry.h_info) {
				api_entry.thd_info->cpl_api_seq = api_entry.thd_api_seq;
				api_entry.h_info->h_cpl_api_seq = api_entry.thd_info->cpl_api_seq;
			}
			bmdrv_post_api_process(bmdi, api_entry, channel);
			if (api_entry.h_info )
                KeSetEvent(&api_entry.h_info->h_msg_doneEvent, 0, FALSE);
			if (api_entry.thd_info)	{
                KeSetEvent(&api_entry.thd_info->msg_doneEvent, 0, FALSE);
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"thd_info:pid %lld complete api %d\n", api_entry.thd_info->user_pid,
						 api_entry.thd_api_seq);
			}
            WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

			// only for debug
			if (BM_MSGFIFO_CHANNEL_XPU == channel) {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "the channel is [BM_MSGFIFO_CHANNEL_XPU]\n");
			} else {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "the channel is [BM_MSGFIFO_CHANNEL_CPU]\n");
			}

			if (BM_MSGFIFO_CHANNEL_XPU == channel) {
				pending_msgirq_cnt--;
			} else {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "The sw_rp is %d, the read data is %d\n", bmdi->api_info[channel].sw_rp, gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU));
				if (bmdi->api_info[channel].sw_rp == gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU))
					pending_msgirq_cnt--;
			}
		}
	}
    KeSetEvent(&api_info->msg_doneEvent, 0, FALSE);

	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		// possible more device sync markers after processing msg done
		do {
			count = kfifo_out_peek(&api_info->api_fifo, (u8 *)&api_entry, API_ENTRY_SIZE);
			if (count == API_ENTRY_SIZE) {
				if ((u64)api_entry.thd_info == DEVICE_SYNC_MARKER) {
					// device sync marker
					count = kfifo_out(&api_info->api_fifo, (u8 *)&api_entry, API_ENTRY_SIZE);
					api_info->device_sync_cpl = api_entry.dev_api_seq;
                    KeSetEvent(&api_info->dev_sync_doneEvent, 0, FALSE);
					TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"device_sync_peek:%d syc api send and syc api complete %d\n",api_info->device_sync_last,
                                api_info->device_sync_cpl);
				} else {
					// msg api
					break;
				}
			} else {
				// msg api
				break;
			}
		} while (count != 0);
	}
    WdfSpinLockRelease(api_info->api_fifo_spinlock);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "Get out of [bmdrv_msg_irq_handler]\n");
}

#ifdef SOC_MODE
 bmdrv_irq_handler_msg(int irq, void *data)
{
	struct bm_device_info *bmdi = data;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"bmdrv: msg irq received.\n");
	bmdrv_msg_irq_handler(bmdi);
	return IRQ_HANDLED;
}
#endif

int bmdev_msgfifo_add_pointer(struct bm_device_info *bmdi, u32 cur_pointer, int val)
{
	int max_len = bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM;
	int new_pointer = 0;

	new_pointer = cur_pointer + val;
	if (new_pointer >= max_len)
		new_pointer -= max_len;
	return new_pointer;
}

int bmdev_copy_to_msgfifo(struct bm_device_info *bmdi, bm_kapi_header_t *api_header_p, bm_api_t *bm_api_p, bm_kapi_opt_header_t *api_opt_header_p, u32 channel)
{
	u32 cur_wp;
	u32 next_wp;
	u32 idx, msg_buf = 0;
	u32 word_size;
	u32 header_size;

	if (BM_MSGFIFO_CHANNEL_XPU == channel)
		cur_wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU);
	else if (BM_MSGFIFO_CHANNEL_CPU == channel)
		cur_wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU);
	else {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_API, "invalid channel: %d\n", channel);
		return 0;
	}

	shmem_reg_write_enh(bmdi, cur_wp, api_header_p->api_id, channel);
	next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, offsetof(bm_kapi_header_t, api_size) / sizeof(u32));
	shmem_reg_write_enh(bmdi, next_wp, api_header_p->api_size, channel);
	next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, offsetof(bm_kapi_header_t, api_handle) / sizeof(u32));
	shmem_reg_write_enh(bmdi, next_wp, (u32)(api_header_p->api_handle), channel);
	next_wp = bmdev_msgfifo_add_pointer(bmdi, next_wp, 1);
	shmem_reg_write_enh(bmdi, next_wp, (u32)(api_header_p->api_handle >> 32), channel);
	next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, offsetof(bm_kapi_header_t, api_seq) / sizeof(u32));
	shmem_reg_write_enh(bmdi, next_wp, api_header_p->api_seq, channel);
#if 0
	// no need to write, but should left the space
	next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, offsetof(bm_kapi_header_t, duration)/sizeof(u32));
	shmem_reg_write_enh(bmdi, next_wp & SHAREMEM_MASK, api_header_p->duration);
	next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, offsetof(bm_kapi_header_t, result)/sizeof(u32));
	shmem_reg_write_enh(bmdi, next_wp & SHAREMEM_MASK, api_header_p->result);
#endif

	if (api_opt_header_p != NULL) {
		next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, sizeof(bm_kapi_header_t) / sizeof(u32));
		shmem_reg_write_enh(bmdi, next_wp, (u32)(api_opt_header_p->global_api_seq >> 32), channel);
		next_wp = bmdev_msgfifo_add_pointer(bmdi, next_wp, 1);
		shmem_reg_write_enh(bmdi, next_wp, (u32)(api_opt_header_p->global_api_seq), channel);
		next_wp = bmdev_msgfifo_add_pointer(bmdi, next_wp, 1);
		shmem_reg_write_enh(bmdi, next_wp, (u32)(api_opt_header_p->api_data >> 32), channel);
		next_wp = bmdev_msgfifo_add_pointer(bmdi, next_wp, 1);
		shmem_reg_write_enh(bmdi, next_wp, (u32)(api_opt_header_p->api_data), channel);
		header_size = sizeof(bm_kapi_header_t) + sizeof(bm_kapi_opt_header_t);
	} else {
		header_size = sizeof(bm_kapi_header_t);  // sizeof(bm_api_id_t) + sizeof(u32) * 4 + sizeof(u64);
	}
	word_size = api_header_p->api_size + header_size / sizeof(u32);

	if (api_header_p->api_id == BM_API_QUIT)
		return 0;

	for (idx = 0; idx < api_header_p->api_size; idx++) {
		next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, header_size / sizeof(u32) + idx);
        RtlCopyMemory(&msg_buf,(u32 *)(bm_api_p->api_addr) + idx, sizeof(u32));
		shmem_reg_write_enh(bmdi, next_wp, msg_buf, channel);
	}
	next_wp = bmdev_msgfifo_add_pointer(bmdi, cur_wp, word_size);
	if (BM_MSGFIFO_CHANNEL_XPU == channel)
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU, next_wp);
	else
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU, next_wp);
	return 0;
}

int bmdev_invalidate_pending_apis(struct bm_device_info *bmdi, struct bm_handle_info *h_info)
{
	u32 channel, cur_rp, next_rp, wp;
	u32 word_size, api_size;
	u64 api_handle;

	channel = BM_MSGFIFO_CHANNEL_XPU;

	ExAcquireFastMutex(& bmdi->api_info[channel].api_mutex);

	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		cur_rp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_XPU);
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU);
	} else if (BM_MSGFIFO_CHANNEL_CPU == channel) {
		cur_rp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU);
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU);
	} else {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_API,"invalid channel: %d\n", channel);
		return -1;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"cur_rp %d, wp %d\n", cur_rp, wp);
	while (cur_rp != wp) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"start invalidating\n");
		next_rp = bmdev_msgfifo_add_pointer(bmdi, cur_rp, offsetof(bm_kapi_header_t, api_size) / sizeof(u32));
		api_size = shmem_reg_read_enh(bmdi, next_rp, channel);
		word_size = api_size + (sizeof(bm_kapi_header_t)) / sizeof(u32);
		next_rp = bmdev_msgfifo_add_pointer(bmdi, cur_rp, offsetof(bm_kapi_header_t, api_handle) / sizeof(u32));
		api_handle = shmem_reg_read_enh(bmdi, next_rp, channel);
		next_rp = bmdev_msgfifo_add_pointer(bmdi, cur_rp, offsetof(bm_kapi_header_t, api_handle) / sizeof(u32) + 1);
		api_handle |= (u64)shmem_reg_read_enh(bmdi, next_rp, channel) << 32;
		if (api_handle == (u64)h_info->fileObject) {
			next_rp = bmdev_msgfifo_add_pointer(bmdi, cur_rp, offsetof(bm_kapi_header_t, api_handle) / sizeof(u32));
			shmem_reg_write_enh(bmdi, next_rp, 0, channel);
			next_rp = bmdev_msgfifo_add_pointer(bmdi, cur_rp, offsetof(bm_kapi_header_t, api_handle) / sizeof(u32) + 1);
			shmem_reg_write_enh(bmdi, next_rp, 0, channel);
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"api handle %llx in msgfifo is invalidated\n", api_handle);
		}
		cur_rp = bmdev_msgfifo_add_pointer(bmdi, cur_rp, word_size);
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"invalidate complete\n");
    ExReleaseFastMutex(&bmdi->api_info[channel].api_mutex);
	return 0;
}

int bmdev_msgfifo_get_api_data(struct bm_device_info *bmdi, u32 channel, u64 api_handle, u64 *data, s32 timeout)
{
#ifdef PCIE_MODE_ENABLE_CPU
	struct bm_api_info *api_info;
	LIST_ENTRY *list;
	struct api_fifo_entry *api_entry = NULL;
	int ret = 1;
	int match = 0;
    LARGE_INTEGER li;

	if (channel == BM_MSGFIFO_CHANNEL_XPU)
		return -1;

	if (timeout < 0)
		li.QuadPart  = WDF_REL_TIMEOUT_IN_MS(bmdi->cinfo.delay_ms);
	else
		li.QuadPart  = WDF_REL_TIMEOUT_IN_MS(timeout);

	api_info = &bmdi->api_info[channel];
    WdfSpinLockAcquire(api_info->api_fifo_spinlock);
	list_for_each(list, &api_info->api_list) {
		api_entry = &((struct api_list_entry *)list)->api_entry;
		if (api_entry->global_api_seq == api_handle) {
			api_entry->api_done_flag = API_FLAG_WAIT;
			match = 1;
			break;
		}
	}
	WdfSpinLockRelease(api_info->api_fifo_spinlock);
	if (1 == match) {
        if (0 == (api_entry->api_done_flag & API_FLAG_DONE))
			ret = KeWaitForSingleObject(&api_entry->api_doneEvent, Executive, KernelMode, 0, &li);
		if (ret == STATUS_SUCCESS) {
			*data = api_entry->api_data;
			api_entry->api_done_flag = API_FLAG_FINISH;
			return 0;
		} else
           TraceEvents(
               TRACE_LEVEL_ERROR, TRACE_API, "get api data time out , ret is %d, global:%lld\n", ret, api_entry->global_api_seq);
	}
#endif
	return -1;
}

bool bmdev_msgfifo_empty(struct bm_device_info *bmdi, u32 channel)
{
	u32 wp, rp;

	if (channel == BM_MSGFIFO_CHANNEL_XPU) {
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU);
		rp = bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].sw_rp;

		return (wp == rp);
	}
#ifdef PCIE_MODE_ENABLE_CPU
	else {
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU);
		rp = bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].sw_rp;

		return (wp == rp);
	}
#else
	return TRUE;
#endif
}

static u32 bmdev_msgfifo_get_free_slots(struct bm_device_info *bmdi, u32 channel)
{
	u32 wp, rp;

	if (channel == BM_MSGFIFO_CHANNEL_XPU) {
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU);
#if SYNC_API_INT_MODE == 1
		rp = bmdi->api_info[channel].sw_rp;
#else
		rp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_XPU);
#endif
		if (wp >= rp)
			return (bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM - (wp - rp) - 1);
		else
			return (rp - wp - 1);
	}
#ifdef PCIE_MODE_ENABLE_CPU
	else {
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU);
#if SYNC_API_INT_MODE == 1
		rp = bmdi->api_info[channel].sw_rp;
#else
		rp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU);
#endif
		if (wp >= rp)
			return (bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM - (wp - rp) - 1);
		else
			return (rp - wp - 1);
	}
#endif
}

static const char *api_desc(int api_id) {
	switch (api_id) {
	case BM_API_ID_RESERVED                         : return "BM_API_ID_RESERVED ";
	case BM_API_ID_MEM_SET                          : return "BM_API_ID_MEM_SET  ";
	case BM_API_ID_MEM_CPY                          : return "BM_API_ID_MEM_CPY  ";
	/*
	 * Basic Computation APIs
	 */
	case BM_API_ID_MD_SCALAR                        : return "BM_API_ID_MD_SCALAR";
	case BM_API_ID_MD_SUM                           : return "BM_API_ID_MD_SUM   ";
	case BM_API_ID_MD_LINEAR                        : return "BM_API_ID_MD_LINEAR";
	case BM_API_ID_MD_CMP                           : return "BM_API_ID_MD_CMP   ";
	case BM_API_ID_MD_SFU                           : return "BM_API_ID_MD_SFU   ";
	case BM_API_ID_IMG_SUM                          : return "BM_API_ID_IMG_SUM  ";
	case BM_API_ID_FLOAT2INT8                        : return "BM_API_ID_FLOAT2INT8";
	/*
	 * Layer APIs
	 */
	case BM_API_ID_DROPOUT_FWD                      : return "BM_API_ID_DROPOUT_FWD                   ";
	case BM_API_ID_DROPOUT_BWD                      : return "BM_API_ID_DROPOUT_BWD                   ";
	case BM_API_ID_ACCURACY_LAYER                   : return "BM_API_ID_ACCURACY_LAYER                ";
	case BM_API_ID_LOG_FWD                          : return "BM_API_ID_LOG_FWD                       ";
	case BM_API_ID_LOG_BWD                          : return "BM_API_ID_LOG_BWD                       ";
	case BM_API_ID_SIGMOID_CROSS_ENTROPY_LOSS_FWD   : return "BM_API_ID_SIGMOID_CROSS_ENTROPY_LOSS_FWD";
	case BM_API_ID_SIGMOID_CROSS_ENTROPY_LOSS_BWD   : return "BM_API_ID_SIGMOID_CROSS_ENTROPY_LOSS_BWD";
	case BM_API_ID_CONTRASTIVE_LOSS_FWD             : return "BM_API_ID_CONTRASTIVE_LOSS_FWD          ";
	case BM_API_ID_CONTRASTIVE_LOSS_BWD             : return "BM_API_ID_CONTRASTIVE_LOSS_BWD          ";
	case BM_API_ID_FILTER_FWD                       : return "BM_API_ID_FILTER_FWD                    ";
	case BM_API_ID_FILTER_BWD                       : return "BM_API_ID_FILTER_BWD                    ";
	case BM_API_ID_SPLIT_BWD                        : return "BM_API_ID_SPLIT_BWD                     ";
	case BM_API_ID_PRELU_FWD                        : return "BM_API_ID_PRELU_FWD                     ";
	case BM_API_ID_PRELU_BWD                        : return "BM_API_ID_PRELU_BWD                     ";
	case BM_API_ID_SCALE_FWD                        : return "BM_API_ID_SCALE_FWD                     ";
	case BM_API_ID_SCALE_BWD                        : return "BM_API_ID_SCALE_BWD                     ";
	case BM_API_ID_THRESHOLD_FWD                    : return "BM_API_ID_THRESHOLD_FWD                 ";
	case BM_API_ID_EXP_FWD                          : return "BM_API_ID_EXP_FWD                       ";
	case BM_API_ID_EXP_BWD                          : return "BM_API_ID_EXP_BWD                       ";
	case BM_API_ID_POWER_FWD                        : return "BM_API_ID_POWER_FWD                     ";
	case BM_API_ID_POWER_BWD                        : return "BM_API_ID_POWER_BWD                     ";
	case BM_API_ID_EUCLIDEAN_LOSS_FWD               : return "BM_API_ID_EUCLIDEAN_LOSS_FWD            ";
	case BM_API_ID_EUCLIDEAN_LOSS_BWD               : return "BM_API_ID_EUCLIDEAN_LOSS_BWD            ";
	case BM_API_ID_SILENCE_BWD                      : return "BM_API_ID_SILENCE_BWD                   ";
	case BM_API_ID_LSTM_UNIT_FWD                    : return "BM_API_ID_LSTM_UNIT_FWD                 ";
	case BM_API_ID_LSTM_UNIT_BWD                    : return "BM_API_ID_LSTM_UNIT_BWD                 ";
	case BM_API_ID_ELTWISE_FWD                      : return "BM_API_ID_ELTWISE_FWD                   ";
	case BM_API_ID_ELTWISE_BWD                      : return "BM_API_ID_ELTWISE_BWD                   ";
	case BM_API_ID_BIAS_FWD                         : return "BM_API_ID_BIAS_FWD                      ";
	case BM_API_ID_BIAS_BWD                         : return "BM_API_ID_BIAS_BWD                      ";
	case BM_API_ID_ELU_FWD                          : return "BM_API_ID_ELU_FWD                       ";
	case BM_API_ID_ELU_BWD                          : return "BM_API_ID_ELU_BWD                       ";
	case BM_API_ID_ABSVAL_FWD                       : return "BM_API_ID_ABSVAL_FWD                    ";
	case BM_API_ID_ABSVAL_BWD                       : return "BM_API_ID_ABSVAL_BWD                    ";
	case BM_API_ID_BNLL_FWD                         : return "BM_API_ID_BNLL_FWD                      ";
	case BM_API_ID_BNLL_BWD                         : return "BM_API_ID_BNLL_BWD                      ";
	case BM_API_ID_PERMUTE                          : return "BM_API_ID_PERMUTE                       ";
	case BM_API_ID_ROI_POOLING_FWD                  : return "BM_API_ID_ROI_POOLING_FWD               ";
	case BM_API_ID_PSROIPOOLING_FWD                 : return "BM_API_ID_PSROIPOOLING_FWD              ";
	case BM_API_ID_NORMALIZE_FWD                    : return "BM_API_ID_NORMALIZE_FWD                 ";
	case BM_API_ID_ACTIVE_FWD                       : return "BM_API_ID_ACTIVE_FWD                    ";
	case BM_API_ID_ACTIVE_FWD_FIX8B                 : return "BM_API_ID_ACTIVE_FWD_FIX8B              ";
	/*
	 * Layer APIs Parallel version
	 */
	case BM_API_ID_CONV_FWD_PARALLEL                : return "BM_API_ID_CONV_FWD_PARALLEL          ";
	case BM_API_ID_DECONV_FWD_PARALLEL              : return "BM_API_ID_DECONV_FWD_PARALLEL        ";
	case BM_API_ID_CONV_BWD_BIAS_PARALLEL           : return "BM_API_ID_CONV_BWD_BIAS_PARALLEL     ";
	case BM_API_ID_POOLING_FWD_PARALLEL             : return "BM_API_ID_POOLING_FWD_PARALLEL       ";
	case BM_API_ID_POOLING_BWD_PARALLEL             : return "BM_API_ID_POOLING_BWD_PARALLEL       ";
	case BM_API_ID_POOLING_FWD_TRAIN_PARALLEL       : return "BM_API_ID_POOLING_FWD_TRAIN_PARALLEL ";
	case BM_API_ID_POOLING_FWD_TRAIN_INDEX_PARALLEL : return "BM_API_ID_POOLING_FWD_TRAIN_INDEX_PARALLEL ";
	case BM_API_ID_FC_FWD_PARALLEL                  : return "BM_API_ID_FC_FWD_PARALLEL            ";
	case BM_API_ID_FC_BWD_PARALLEL                  : return "BM_API_ID_FC_BWD_PARALLEL            ";
	case BM_API_ID_LRN_FWD_PARALLEL                 : return "BM_API_ID_LRN_FWD_PARALLEL           ";
	case BM_API_ID_LRN_BWD_PARALLEL                 : return "BM_API_ID_LRN_BWD_PARALLEL           ";
	case BM_API_ID_BN_FWD_INF_PARALLEL              : return "BM_API_ID_BN_FWD_INF_PARALLEL        ";
	case BM_API_ID_BN_FWD_TRAIN_PARALLEL            : return "BM_API_ID_BN_FWD_TRAIN_PARALLEL      ";
	case BM_API_ID_BN_BWD_PARALLEL                  : return "BM_API_ID_BN_BWD_PARALLEL            ";
	case BM_API_ID_SIGMOID_FWD_PARALLEL             : return "BM_API_ID_SIGMOID_FWD_PARALLEL       ";
	case BM_API_ID_SIGMOID_FWD_PARALLEL_FIX8B       : return "BM_API_ID_SIGMOID_FWD_PARALLEL_FIX8B ";
	case BM_API_ID_SIGMOID_BWD_PARALLEL             : return "BM_API_ID_SIGMOID_BWD_PARALLEL       ";
	case BM_API_ID_TANH_FWD_PARALLEL                : return "BM_API_ID_TANH_FWD_PARALLEL          ";
	case BM_API_ID_TANH_FWD_PARALLEL_FIX8B          : return "BM_API_ID_TANH_FWD_PARALLEL_FIX8B    ";
	case BM_API_ID_TANH_BWD_PARALLEL                : return "BM_API_ID_TANH_BWD_PARALLEL          ";
	case BM_API_ID_RELU_FWD_PARALLEL                : return "BM_API_ID_RELU_FWD_PARALLEL          ";
	case BM_API_ID_RELU_BWD_PARALLEL                : return "BM_API_ID_RELU_BWD_PARALLEL          ";
	case BM_API_ID_SOFTMAX_FWD_PARALLEL             : return "BM_API_ID_SOFTMAX_FWD_PARALLEL       ";
	case BM_API_ID_SOFTMAX_BWD_PARALLEL             : return "BM_API_ID_SOFTMAX_BWD_PARALLEL       ";
	case BM_API_ID_SOFTMAX_LOSS_FWD_PARALLEL        : return "BM_API_ID_SOFTMAX_LOSS_FWD_PARALLEL  ";
	case BM_API_ID_SOFTMAX_LOSS_BWD_PARALLEL        : return "BM_API_ID_SOFTMAX_LOSS_BWD_PARALLEL  ";
	case BM_API_ID_SOFTMAX_LOSS_BIDIR_PARALLEL      : return "BM_API_ID_SOFTMAX_LOSS_BIDIR_PARALLEL";
	case BM_API_ID_COEFF_UPDATE_SGD_PARALLEL        : return "BM_API_ID_COEFF_UPDATE_SGD_PARALLEL  ";
	case BM_API_ID_MULTI_FULLNET                    : return "BM_API_ID_MULTI_FULLNET              ";
	case BM_API_ID_DYNAMIC_FULLNET                  : return "BM_API_ID_DYNAMIC_FULLNET            ";
	case BM_API_ID_DYNAMIC_FULLNET_EX               : return "BM_API_ID_DYNAMIC_FULLNET_EX          ";
	/*
	 layer APIS int8
	 */
	case BM_API_ID_CONV_FWD_FIX8B_PARALLEL          : return "BM_API_ID_CONV_FWD_FIX8B_PARALLEL    ";
	case BM_API_ID_FC_FWD_FIX8B_PARALLEL            : return "BM_API_ID_FC_FWD_FIX8B_PARALLEL      ";
	case BM_API_ID_FC_WEIGHT_DECOMPRESS             : return "BM_API_ID_FC_WEIGHT_DECOMPRESS      ";
	case BM_API_ID_ELTWISE_FWD_FIX8B_PARALLEL       : return "BM_API_ID_ELTWISE_FWD_FIX8B_PARALLEL ";
	case BM_API_ID_DECONV_FIX8B_FWD_PARALLEL        : return "BM_API_ID_DECONV_FIX8B_FWD_PARALLEL  ";
	// bm_blas api
	case BM_API_ID_SCALE                            : return "BM_API_ID_SCALE";
	case BM_API_ID_AXPY                             : return "BM_API_ID_AXPY ";
	case BM_API_ID_DOT                              : return "BM_API_ID_DOT  ";
	case BM_API_ID_ASUM                             : return "BM_API_ID_ASUM ";
	case BM_API_ID_NRM2                             : return "BM_API_ID_NRM2 ";
	case BM_API_ID_ROT                              : return "BM_API_ID_ROT  ";
	case BM_API_ID_ROTM                             : return "BM_API_ID_ROTM ";
	case BM_API_ID_GEMM                             : return "BM_API_ID_GEMM ";
	case BM_API_ID_SYMM                             : return "BM_API_ID_SYMM ";
	case BM_API_ID_TRMM                             : return "BM_API_ID_TRMM ";
	case BM_API_ID_TPMV                             : return "BM_API_ID_TPMV ";
	case BM_API_ID_TBMV                             : return "BM_API_ID_TBMV ";
	// image process api
	case BM_API_ID_DEPTHWISE_FWD                    : return "BM_API_ID_DEPTHWISE_FWD";
	case BM_API_ID_DEPTHWISE_BWD_INPUT              : return "BM_API_ID_DEPTHWISE_BWD_INPUT";
	case BM_API_ID_DEPTHWISE_BWD_FILTER             : return "BM_API_ID_DEPTHWISE_BWD_FILTER";
	case BM_API_ID_LSTM_FWD                         : return "BM_API_ID_LSTM_FWD";
	// bmcv APIs
	case BM_API_ID_CV_WARP                          : return "BM_API_ID_CV_WARP";
	case BM_API_ID_CV_RESIZE                        : return "BM_API_ID_CV_RESIZE";

	case BM_API_ID_BNSCALE_FWD_FIX8B_PARALLEL       : return "BM_API_ID_BNSCALE_FWD_FIX8B_PARALLEL";

	/*
	 * End
	 */
	default:
	    return "";
	}
}

void bmdev_dump_msgfifo(struct bm_device_info *bmdi, u32 channel)
{
	u32 wp, rp, new_rp;
	u32 header_size;
	u32 data;

	//spin_lock(&msg_lock);

	if (GP_REG_MESSAGE_WP_CHANNEL_XPU == channel) {
		header_size = (sizeof(bm_kapi_header_t)) / sizeof(u32);

		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU);
#if SYNC_API_INT_MODE == 1
		rp = bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].sw_rp;
#else
		rp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_XPU);
#endif
		new_rp = rp;

		if (wp == rp) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"the fifo is empty.\n");
		} else {
				data = shmem_reg_read_enh(bmdi, new_rp, BM_MSGFIFO_CHANNEL_XPU);
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d tpu api [%d:%s] timeout\n", bmdi->dev_index, data, api_desc(data));
		}
	}
#ifdef PCIE_MODE_ENABLE_CPU
	else {
		wp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU);
#if SYNC_API_INT_MODE == 1
		rp = bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].sw_rp;
#else
		rp = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU);
#endif
		new_rp = rp;

		if (wp == rp)
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"the fifo is empty.\n");
		else {
			while (new_rp != wp) {

				TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d message in cpu fifo : %d\n", bmdi->dev_index,
                shmem_reg_read_enh(bmdi, new_rp, BM_MSGFIFO_CHANNEL_CPU));
				new_rp = bmdev_msgfifo_add_pointer(bmdi, new_rp, 1);
			}
		}
	}
#endif
	//spin_unlock(&msg_lock);
}

int bmdev_wait_msgfifo(struct bm_device_info *bmdi, u32 slot_number, u32 ms, u32 channel)
{
	int ret_wait = 1;
    LARGE_INTEGER li;
    NTSTATUS status=0;

    li.QuadPart  = WDF_REL_TIMEOUT_IN_MS(ms);

	while (!(bmdev_msgfifo_get_free_slots(bmdi, channel) >= slot_number) && (ret_wait != 0)) {
        if (channel == BM_MSGFIFO_CHANNEL_XPU) {
            status = KeWaitForSingleObject(&bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].msg_doneEvent, Executive, KernelMode, FALSE, &li);
            if (status == STATUS_SUCCESS)
                ret_wait = 1;
            else
                ret_wait = 0;
		}else if (channel == BM_MSGFIFO_CHANNEL_CPU) {
			status = KeWaitForSingleObject(&bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].msg_doneEvent, Executive, KernelMode, 0, &li);
			if (status == STATUS_SUCCESS){
				ret_wait = 1;
			}else{
				ret_wait = 0;
			}
        }
	}
	if (ret_wait)
		return 0;
	bmdev_dump_msgfifo(bmdi, channel);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"bmdev : wait msg fifo empty slot %d timeout!\n", slot_number);
	return -1;
}

#ifndef SOC_MODE
void bm_msg_request_irq(struct bm_device_info *bmdi)
{
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bmdev :start to request irq for msg to xpu!\n");
	bmdrv_submodule_request_irq(bmdi, MSG_IRQ_ID_CHANNEL_XPU, bmdrv_msg_irq_handler);
#ifdef PCIE_MODE_ENABLE_CPU
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bmdev :start to request irq for msg to cpu!\n");
	bmdrv_submodule_request_irq(bmdi, MSG_IRQ_ID_CHANNEL_CPU, bmdrv_msg_irq_handler);
#endif
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bmdev :out of [bm_msg_request_irq]!\n");
}

void bm_msg_free_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_free_irq(bmdi, MSG_IRQ_ID_CHANNEL_XPU);
#ifdef PCIE_MODE_ENABLE_CPU
	bmdrv_submodule_free_irq(bmdi, MSG_IRQ_ID_CHANNEL_CPU);
#endif
}
#endif
