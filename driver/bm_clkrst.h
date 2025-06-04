#ifndef _SGTPUV8_CLKRST_H_
#define _SGTPUV8_CLKRST_H_

void SGTPUV8_modules_reset(struct bm_device_info *bmdi);
int SGTPUV8_modules_reset_init(struct bm_device_info *bmdi);
int SGTPUV8_modules_clk_init(struct bm_device_info *bmdi);
void SGTPUV8_modules_clk_deinit(struct bm_device_info *bmdi);
void SGTPUV8_modules_clk_enable(struct bm_device_info *bmdi);
void SGTPUV8_modules_clk_disable(struct bm_device_info *bmdi);


#endif
