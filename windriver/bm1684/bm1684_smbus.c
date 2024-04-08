#include "../bm_common.h"
#include "bm1684_smbus.tmh"

void bmdrv_smbus_set_default_value(struct bm_device_info *bmdi)
{
    UNREFERENCED_PARAMETER(bmdi);
    /*	u32 value = 0;
	int i = 0;

	for (i = 0; i < DTCM_MEM_DEV_INFO_SIZE; i++)
		iowrite8(0, bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + i);

	pci_read_config_dword(pdev, 0, &value);
	dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.vendor_id_reg, value, sizeof(u32));
	pci_read_config_word(pdev, 0x2c, (u16 *)&value);
	dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.sub_vendor_id_reg, value, sizeof(u16));
	dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.fw_version_reg, BM_DRIVER_VERSION, sizeof(u32));
	dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.hw_version_reg, BM1684_HW_VERSION(bmdi), sizeof(u8));
	dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.board_name_reg, BM1684_BOARD_TYPE(bmdi), sizeof(u8));

	if (!bmdi->boot_info.fan_exist)
		dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.fan_speed_reg, 0xff, sizeof(u8));*/
}
