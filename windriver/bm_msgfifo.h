#ifndef _BMDEV_MSGFIFO_H_
#define _BMDEV_MSGFIFO_H_

#define SYNC_API_INT_MODE 1
#define PCIE_MODE_ENABLE_CPU

#ifndef PCIE_MODE_ENABLE_CPU
#define BM_MSGFIFO_CHANNEL_NUM   1       /* one msgfifo, channel 0 for ARM9                     */
#else
#define BM_MSGFIFO_CHANNEL_NUM   2       /* two msgfifo, channel 0 for ARM9, channel 1 for CPU  */
#endif
#define BM_MSGFIFO_CHANNEL_XPU   0
#define BM_MSGFIFO_CHANNEL_CPU   1

struct bm_device_info;
int bmdev_msgfifo_add_pointer(struct bm_device_info *bmdi, u32 cur_pointer, int val);
int bmdev_copy_to_msgfifo(struct bm_device_info *bmdi, bm_kapi_header_t *api_header_p, bm_api_t *p_bm_api, bm_kapi_opt_header_t *api_opt_header_p, u32 channel);
int bmdev_invalidate_pending_apis(struct bm_device_info *bmdi, struct bm_handle_info *h_info);
bool bmdev_msgfifo_empty(struct bm_device_info *bmdi, u32 channel);
void bmdev_dump_msgfifo(struct bm_device_info *bmdi, u32 channel);
int bmdev_wait_msgfifo(struct bm_device_info *bmdi, u32 slot_number, u32 ms, u32 channel);
int bmdev_msgfifo_get_api_data(struct bm_device_info *bmdi, u32 channel, u64 api_handle, u64 *data, s32 timeout);

void bmdrv_msg_irq_handler(struct bm_device_info *bmdi);

void bm_msg_request_irq(struct bm_device_info *bmdi);
void bm_msg_free_irq(struct bm_device_info *bmdi);

#endif
