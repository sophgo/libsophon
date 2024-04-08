#ifndef _BM_CTL_H_
#define _BM_CTL_H_

#define ATTR_FAULT_VALUE		(int)(0xFFFFFC00)
#define ATTR_NOTSUPPORTED_VALUE		(int)(0xFFFFFC01)

typedef struct bm_dev_list {
	LIST_ENTRY   list;
	struct bm_device_info *bmdi;
} BM_DEV_LIST, *BM_DEV_PLIST;

typedef struct bm_ctrl_info {
    WDFDRIVER  ctl_driver;
	//LIST_ENTRY bm_dev_list;
	u32 dev_count;
	char dev_ctl_name[20];
} BMCI, *PBMCI;

struct bm_ctrl_info *bmci;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BMCI, Bm_Get_Bmci)

#define NTDEVICE_NAME_STRING L"\\Device\\bmdev_ctl"
#define SYMBOLIC_NAME_STRING L"\\DosDevices\\bmdev_ctl"
#define BMDEV_CTL_POOL_TAG 'BCTL'

typedef struct _CONTROL_DEVICE_EXTENSION {

    HANDLE FileHandle;  // Store your control data here

} CONTROL_DEVICE_EXTENSION, *PCONTROL_DEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTROL_DEVICE_EXTENSION, ControlGetData)

struct chip_info;
int bmctl_ioctl_get_attr(struct bm_smi_attr *smi_attr);
int bmctl_ioctl_get_proc_gmem(struct bm_smi_proc_gmem *smi_gmem);
int bmdrv_init_bmci(WDFDRIVER Driver);
int bmdrv_remove_bmci(void);
int bmdrv_ctrl_add_dev();
int bmdrv_ctrl_del_dev();
int bmctl_ioctl_set_led(unsigned long arg);
int bmctl_ioctl_set_ecc(unsigned long arg);
struct bm_device_info *bmctl_get_bmdi(int dev_id);
struct bm_device_info *bmctl_get_card_bmdi(struct bm_device_info *bmdi);
int bmctl_ioctl_recovery(unsigned long arg);

NTSTATUS bmdrv_add_ctl_device(IN WDFDRIVER       Driver,
                             IN PWDFDEVICE_INIT DeviceInit);

#endif
