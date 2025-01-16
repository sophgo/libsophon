
#ifndef _SGTPUV8_DDR_H_
#define _SGTPUV8_DDR_H_

struct bm_device_info;
int SGTPUV8_ddr_top_init(struct bm_device_info *bmdi);
void SGTPUV8_ddr_phy_reg_write16(struct bm_device_info *bmdi, u32 addr, u32 value);
u16 SGTPUV8_ddr_phy_reg_read16(struct bm_device_info *bmdi, u32 addr);
void SGTPUV8_disable_ddr_interleave(struct bm_device_info *bmdi);
void SGTPUV8_enable_ddr_interleave(struct bm_device_info *bmdi);
void SGTPUV8x_enable_ddr_interleave(struct bm_device_info *bmdi);
void SGTPUV8_enable_ddr_refresh_sync_d0a_d0b_d1_d2(struct bm_device_info *bmdi);
void SGTPUV8_enable_ddr_refresh_sync_d0a_d0b(struct bm_device_info *bmdi);
void SGTPUV8_ddr0a_ecc_irq_handler(struct bm_device_info *bmdi);
void SGTPUV8_ddr0b_ecc_irq_handler(struct bm_device_info *bmdi);
void SGTPUV8_ddr_ecc_request_irq(struct bm_device_info *bmdi);
void SGTPUV8_ddr_ecc_free_irq(struct bm_device_info *bmdi);
int SGTPUV8_pld_ddr_top_init(struct bm_device_info *bmdi);
int SGTPUV8_lpddr4_init(struct bm_device_info *bmdi);
int SGTPUV8_ddr_format_by_cdma(struct bm_device_info *bmdi);
#endif
/*******************************************************/

