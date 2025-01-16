#ifndef _SGTPUV8_CLKRST_H_
#define _SGTPUV8_CLKRST_H_

typedef enum {
	SGTPUV8_MODULE_TPU = 0,
	SGTPUV8_MODULE_GDMA = 1,
	SGTPUV8_MODULE_TC906B = 2,
	SGTPUV8_MODULE_HAU = 3
} SGTPUV8_MODULE_ID;

#define SGTPUV8_TPLL_REFDIV_MIN 1
#define SGTPUV8_TPLL_REFDIV_MAX 63

#define SGTPUV8_TPLL_FBDIV_MIN 16
#define SGTPUV8_TPLL_FBDIV_MAX 320

#define SGTPUV8_TPLL_POSTDIV1_MIN 1
#define SGTPUV8_TPLL_POSTDIV1_MAX 7
#define SGTPUV8_TPLL_POSTDIV2_MIN 1
#define SGTPUV8_TPLL_POSTDIV2_MAX 7

#define SGTPUV8_TPLL_FREF 25

#define BIT_CLK_TPU_ENABLE	13
#define BIT_CLK_TPU_ASSERT	0

#define BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM	2
#define BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC		4
#define BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC		5

#define BIT_SW_RESET_TPU0	23
#define BIT_SW_RESET_TPU1	24
#define BIT_SW_RESET_GDMA0	25
#define BIT_SW_RESET_GDMA1	26
#define BIT_SW_RESET_TC906B0	27
#define BIT_SW_RESET_TC906B1	28
#define BIT_SW_RESET_HAU	29

void SGTPUV8_bmdrv_clk_set_tpu_divider(struct bm_device_info *bmdi, int devider_factor);
void SGTPUV8_bmdrv_clk_set_tpu_divider_fpll(struct bm_device_info *bmdi, int devider_factor);
void SGTPUV8_bmdrv_clk_set_close(struct bm_device_info *bmdi);
int SGTPUV8_bmdrv_clk_get_tpu_divider(struct bm_device_info *bmdi);
int SGTPUV8_bmdev_clk_ioctl_set_tpu_divider(struct bm_device_info *bmdi, unsigned long arg);
int SGTPUV8_bmdev_clk_ioctl_set_tpu_freq(struct bm_device_info *bmdi, unsigned long arg);
int SGTPUV8_bmdrv_clk_get_tpu_freq(struct bm_device_info *bmdi);
int SGTPUV8_bmdrv_clk_set_tpu_target_freq(struct bm_device_info *bmdi, int target);
int SGTPUV8_bmdev_clk_ioctl_get_tpu_freq(struct bm_device_info *bmdi, unsigned long arg);
int SGTPUV8_bmdev_clk_ioctl_set_module_reset(struct bm_device_info *bmdi, unsigned long arg);

void SGTPUV8_bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_clk_disable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_clk_disable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_clk_disable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);

int SGTPUV8_bmdev_clk_hwlock_lock(struct bm_device_info *bmdi);
void SGTPUV8_bmdev_clk_hwlock_unlock(struct bm_device_info *bmdi);

void SGTPUV8_bmdrv_sw_reset_tpu(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_sw_reset_gdma(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_sw_reset_tc906b(struct bm_device_info *bmdi);
void SGTPUV8_bmdrv_sw_reset_hau(struct bm_device_info *bmdi);

void SGTPUV8_modules_reset(struct bm_device_info *bmdi);
int SGTPUV8_modules_reset_init(struct bm_device_info *bmdi);
int SGTPUV8_modules_clk_init(struct bm_device_info *bmdi);
void SGTPUV8_modules_clk_deinit(struct bm_device_info *bmdi);
void SGTPUV8_modules_clk_enable(struct bm_device_info *bmdi);
void SGTPUV8_modules_clk_disable(struct bm_device_info *bmdi);


#endif
