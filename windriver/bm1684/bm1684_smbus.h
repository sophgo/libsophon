#ifndef _BM1684_SMBUS_H_
#define _BM1684_SMBUS_H_


struct smbus_devinfo {
    u8 chip_temp_reg;
    u8 board_temp_reg;
    u8 board_power_reg;
    u8 fan_speed_reg;
    u8 vendor_id_reg;
    u8 hw_version_reg;
    u8 fw_version_reg;
    u8 board_name_reg;
    u8 sub_vendor_id_reg;
    u8 sn_reg;
    u8 mcu_version_reg;
};

void bmdrv_smbus_set_default_value(struct bm_device_info *bmdi);

#endif
