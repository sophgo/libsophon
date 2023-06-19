#ifndef _BM_I2C_H_
#define _BM_I2C_H_

#define I2C_68127_ADDR	       	0x5c
#define I2C_68127_ADDR_SM5M	0x50
#define I2C_IS6608A_ADDR_SM5M	0x30
#define I2C_1331_ADDR	       	0x48
#define I2C_68224_ADDR          0x50

struct bm_device_info;
int bmdrv_i2c_init(struct bm_device_info *bmdi, u32 i2c_index, u32 tx_level, u32 rx_level, u32 target_addr);
void bmdrv_power_and_temp_i2c_init(struct bm_device_info  *bmdi);
int bm_i2c_set_target_addr(struct bm_device_info *bmdi, u32, u32 target_addr);
int bm_i2c_write_byte(struct bm_device_info *, u32, u8, u8);
int bm_i2c_read_byte(struct bm_device_info *, u32, u8, u8 *);
int bm_i2c_read_hword(struct bm_device_info *bmdi, u32, u8 cmd, u16 *data);
int bm_i2c_write_hword(struct bm_device_info *, u32, u8, u16);
int bm_get_eeprom_reg(struct bm_device_info *, u16, u8 *);
int bm_set_eeprom_reg(struct bm_device_info *, u16, u8);
int bm_mcu_program(struct bm_device_info *, void *, size_t, size_t);
int bm_mcu_checksum(struct bm_device_info *, u32, u32, void *);
int bm_mcu_read(struct bm_device_info *, u8 *, size_t, size_t);
int bm_mcu_read_reg(struct bm_device_info *, u8, u8*);
int bm_i2c_read_slave(struct bm_device_info *bmdi, unsigned long arg);
int bm_i2c_write_slave(struct bm_device_info *bmdi, unsigned long arg);
int bm_i2c_smbus_access(struct bm_device_info *bmdi, unsigned long arg);
#endif
