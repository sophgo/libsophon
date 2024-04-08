#include "../bm_common.h"
#include "bm1684_reg.h"
#include "bm1684_cdma.tmh"

/* CDMA bit definition */
#define BM1684_CDMA_RESYNC_ID_BIT                  2
#define BM1684_CDMA_ENABLE_BIT                     0
#define BM1684_CDMA_INT_ENABLE_BIT                 3

#define BM1684_CDMA_EOD_BIT                        2
#define BM1684_CDMA_PLAIN_DATA_BIT                 6
#define BM1684_CDMA_OP_CODE_BIT                    7

void bm1684_clear_cdmairq(struct bm_device_info *bmdi)
{
	int reg_val;

	reg_val = cdma_reg_read(bmdi, CDMA_INT_STATUS);
	reg_val |= (1 << 0x09);
	cdma_reg_write(bmdi, CDMA_INT_STATUS, reg_val);
}

u32 bm1684_cdma_transfer(struct bm_device_info *bmdi, pbm_cdma_arg parg, bool lock_cdma, WDFFILEOBJECT fileObject)
{
	struct bm_thread_info *ti = NULL;
	struct bm_handle_info *h_info = NULL;
	struct bm_trace_item *ptitem = NULL;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
    u32                    timeout_ms = bmdi->cinfo.delay_ms;
    LARGE_INTEGER          li;
    u32 src_addr_hi = 0;
	u32 src_addr_lo = 0;
	u32 dst_addr_hi = 0;
	u32 dst_addr_lo = 0;
	u32 cdma_max_payload = 0;
	u32 reg_val = 0;
	u64 src = parg->src;
	u64 dst = parg->dst;
	u32 count = 3000;
	u64 nv_cdma_start_us = 0;
	u64 nv_cdma_end_us = 0;
	u64 nv_cdma_send_us = 0;
	u32 lock_timeout = timeout_ms * 1000;
	u32 int_mask_val;
    NTSTATUS  status;

	nv_cdma_send_us = bmdev_timer_get_time_us(bmdi);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	if (fileObject) {
        if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DRIVER,
                        "bm-sophon%d cdma: file list is not found!\n",
                        bmdi->dev_index);
            return BM_EINVAL;
        }
    }

	if (lock_cdma) {
        ExAcquireFastMutex(&memcpy_info->cdma_Mutex);
    }
	//Check the cdma is used by others(ARM9) or not.
	lock_timeout = timeout_ms * 1000;
    li.QuadPart  = WDF_REL_TIMEOUT_IN_MS(timeout_ms);

	while (top_reg_read(bmdi, TOP_CDMA_LOCK)) {
		bm_udelay(1);
		if (--lock_timeout == 0) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_CDMA, "cdma resource wait timeout\n");
            if (lock_cdma) {
                ExReleaseFastMutex(&memcpy_info->cdma_Mutex);
            }
            return BM_EINVAL;
		}
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CDMA,"CDMA lock wait %dus\n", timeout_ms * 1000 - lock_timeout);

    HANDLE api_pid = PsGetCurrentThreadId();
    ti = bmdrv_find_thread_info(h_info, (u64)api_pid);

	if (ti && (parg->dir == HOST2CHIP)) {
		ti->profile.cdma_out_counter++;
		bmdi->profile.cdma_out_counter++;
	} else {
		if (ti) {
			ti->profile.cdma_in_counter++;
			bmdi->profile.cdma_in_counter++;
		}
	}

	if (ti && ti->trace_enable) {
		int size = sizeof(struct bm_trace_item);
        ptitem   = (struct bm_trace_item *)WdfMemoryGetBuffer(bmdi->trace_info.trace_memHandle, (size_t*)&size);
		ptitem->payload.trace_type = 0;
		ptitem->payload.cdma_dir = parg->dir;
		ptitem->payload.sent_time = nv_cdma_send_us;
		ptitem->payload.start_time = 0;
	}
	/*Disable CDMA*/
	reg_val = cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
	reg_val &= ~(1 << BM1684_CDMA_ENABLE_BIT);
	cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);
	/*Set PIO mode*/
	reg_val = cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
	reg_val &= ~(1 << 1);
	cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);

	int_mask_val = cdma_reg_read(bmdi, CDMA_INT_MASK);
	int_mask_val &= ~(1 << 9);
	cdma_reg_write(bmdi, CDMA_INT_MASK, int_mask_val);

#ifdef SOC_MODE
	if (!parg->use_iommu) {
		src |= (u64)0x3f<<36;
		dst |= (u64)0x3f<<36;
	}
#else
	if (parg->dir != CHIP2CHIP) {
		if (!parg->use_iommu) {
			if (parg->dir == CHIP2HOST)
				src |= (u64)0x3f<<36;
			else
				dst |= (u64)0x3f<<36;
		}
	}
#endif
	src_addr_hi = src >> 32;
	src_addr_lo = src & 0xffffffff;
	dst_addr_hi = dst >> 32;
	dst_addr_lo = dst & 0xffffffff;
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_CDMA,
                "src:0x%llx dst:0x%llx size:%lld\n",
                src,
                dst,
                parg->size);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_CDMA,
                "src_addr_hi 0x%x src_addr_low 0x%x\n",
                src_addr_hi,
                src_addr_lo);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_CDMA,
                "dst_addr_hi 0x%x dst_addr_low 0x%x\n",
                dst_addr_hi,
                dst_addr_lo);

	cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP7, (u32)parg->size);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP2, 0);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP1, 1);

	/*set max payload which equal to PCIE bandwidth.*/
#ifdef SOC_MODE
	cdma_max_payload = 0x1;
#else
	cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
	cdma_max_payload &= 0x7;

	cdma_reg_write(bmdi, CDMA_CMD_9F8, cdma_max_payload);

	/* Using interrupt(10s timeout) or polling for detect cmda done */
	if (parg->intr) {

		cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
		reg_val |= ((1 << BM1684_CDMA_ENABLE_BIT) | (1 << BM1684_CDMA_INT_ENABLE_BIT));
		cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);
		nv_cdma_start_us = bmdev_timer_get_time_us(bmdi);
		cdma_reg_write(bmdi, CDMA_CMD_ACCP0, (1 << BM1684_CDMA_ENABLE_BIT)
				| (1 << BM1684_CDMA_INT_ENABLE_BIT));
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CDMA, "wait cdma\n");

        status = KeWaitForSingleObject(&memcpy_info->cdma_completionEvent,
                              Executive,
                              KernelMode,
                              FALSE,
                              &li);
		nv_cdma_end_us = bmdev_timer_get_time_us(bmdi);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_CDMA,
                    "time = %lld, function_num = %d, start = %lld, end = %lld, "
                    "size = %llx, max_payload = %d\n",
			nv_cdma_end_us - nv_cdma_start_us, bmdi->cinfo.chip_index&0x3,
			nv_cdma_start_us, nv_cdma_end_us, parg->size, cdma_max_payload);

		if (status == STATUS_SUCCESS) {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_CDMA,
                        "End : wait cdma done\n");
		} else {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_CDMA,
                        "End : wait cdma timeout! CDMA_INT_STATUS= 0x%x\n",
                        cdma_reg_read(bmdi, CDMA_INT_STATUS));
			top_reg_write(bmdi, TOP_CDMA_LOCK, 0);
            if (lock_cdma) {
                ExReleaseFastMutex(&memcpy_info->cdma_Mutex);
            }
            return BM_EINVAL;
		}
	} else {
		cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
		reg_val |= (1 << BM1684_CDMA_ENABLE_BIT);
		reg_val &= ~(1 << BM1684_CDMA_INT_ENABLE_BIT);
		cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);
		nv_cdma_start_us = bmdev_timer_get_time_us(bmdi);
		cdma_reg_write(bmdi, CDMA_CMD_ACCP0, (1 << BM1684_CDMA_ENABLE_BIT)
				| (1 << BM1684_CDMA_INT_ENABLE_BIT));
		while (((cdma_reg_read(bmdi, CDMA_INT_STATUS) >> 0x9) & 0x1) != 0x1) {
			bm_udelay(1);
			if (--count == 0) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_CDMA,
                            "cdma polling wait timeout\n");
				top_reg_write(bmdi, TOP_CDMA_LOCK, 0);
                if (lock_cdma) {
                    ExReleaseFastMutex(&memcpy_info->cdma_Mutex);
                }
                return BM_EINVAL;
			}
		}
		nv_cdma_end_us = bmdev_timer_get_time_us(bmdi);
		reg_val = cdma_reg_read(bmdi, CDMA_INT_STATUS);
		reg_val |= (1 << 0x09);
		cdma_reg_write(bmdi, CDMA_INT_STATUS, reg_val);

		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CDMA,"cdma transfer using polling mode end\n");
	}

	if (ti && (parg->dir == HOST2CHIP)) {
		ti->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
		bmdi->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_CDMA,
                    "profile.cdma_out_time = %lld, pid = %llx \n",
                    ti->profile.cdma_out_time,ti->user_pid);
	} else {
		if (ti) {
			ti->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
			bmdi->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_CDMA,
                        "profile.cdma_in_time = %lld,pid = %llx\n",
                        ti->profile.cdma_in_time,ti->user_pid);
		}
	}

	if (ti && ti->trace_enable) {
		ptitem->payload.end_time = nv_cdma_end_us;
		ptitem->payload.start_time = nv_cdma_start_us;

		InitializeListHead(&ptitem->node);
        WdfSpinLockAcquire(ti->trace_spinlock);
        InsertTailList(&ptitem->node, &ti->trace_list);
        ti->trace_item_num++;
        WdfSpinLockRelease(ti->trace_spinlock);
	}

	top_reg_write(bmdi, TOP_CDMA_LOCK, 0);

	if (lock_cdma) {
        ExReleaseFastMutex(&memcpy_info->cdma_Mutex);
    }
	return 0;
}