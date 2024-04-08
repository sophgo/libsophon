#ifndef _BM_DRV_H_
#define _BM_DRV_H_

struct bm_device_info;
int bmdrv_software_init(struct bm_device_info *bmdi);
void bmdrv_software_deinit(struct bm_device_info *bmdi);
//int bmdrv_class_create(void);
//int bmdrv_class_destroy(void);
//struct class *bmdrv_class_get(void);
//int bmdev_register_device(struct bm_device_info *bmdi);
//int bmdev_unregister_device(struct bm_device_info *bmdi);
//int bmdev_ctl_register_device(struct bm_ctrl_info *bmci);
//int bmdev_ctl_unregister_device(struct bm_ctrl_info *bmci);
void bmdrv_post_api_process(struct bm_device_info *bmdi,
		struct api_fifo_entry api_entry, u32 channel);

int bm_ioctl(struct bm_device_info *bmdi,
             _In_ WDFREQUEST        Request,
             _In_ size_t            OutputBufferLength,
             _In_ size_t            InputBufferLength,
             _In_ u32             IoControlCode);
#endif
