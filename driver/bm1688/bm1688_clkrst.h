#ifndef _BM1688_CLKRST_H_
#define _BM1688_CLKRST_H_

typedef enum {
	BM1688_MODULE_TPU = 0,
	BM1688_MODULE_GDMA = 1,
	BM1688_MODULE_TC906B = 2,
	BM1688_MODULE_HAU = 3
} BM1688_MODULE_ID;

#define BM1688_TPLL_REFDIV_MIN 1
#define BM1688_TPLL_REFDIV_MAX 63

#define BM1688_TPLL_FBDIV_MIN 16
#define BM1688_TPLL_FBDIV_MAX 320

#define BM1688_TPLL_POSTDIV1_MIN 1
#define BM1688_TPLL_POSTDIV1_MAX 7
#define BM1688_TPLL_POSTDIV2_MIN 1
#define BM1688_TPLL_POSTDIV2_MAX 7

#define BM1688_TPLL_FREF 25

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

void bm1688_bmdrv_clk_set_tpu_divider(struct bm_device_info *bmdi, int devider_factor);
void bm1688_bmdrv_clk_set_tpu_divider_fpll(struct bm_device_info *bmdi, int devider_factor);
void bm1688_bmdrv_clk_set_close(struct bm_device_info *bmdi);
int bm1688_bmdrv_clk_get_tpu_divider(struct bm_device_info *bmdi);
int bm1688_bmdev_clk_ioctl_set_tpu_divider(struct bm_device_info* bmdi, unsigned long arg);
int bm1688_bmdev_clk_ioctl_set_tpu_freq(struct bm_device_info* bmdi, unsigned long arg);
int bm1688_bmdrv_clk_get_tpu_freq(struct bm_device_info *bmdi);
int bm1688_bmdrv_clk_set_tpu_target_freq(struct bm_device_info *bmdi, int target);
int bm1688_bmdev_clk_ioctl_get_tpu_freq(struct bm_device_info* bmdi, unsigned long arg);
int bm1688_bmdev_clk_ioctl_set_module_reset(struct bm_device_info* bmdi, unsigned long arg);

void bm1688_bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi);
void bm1688_bmdrv_clk_disable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi);
void bm1688_bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void bm1688_bmdrv_clk_disable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void bm1688_bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void bm1688_bmdrv_clk_disable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);

int bm1688_bmdev_clk_hwlock_lock(struct bm_device_info* bmdi);
void bm1688_bmdev_clk_hwlock_unlock(struct bm_device_info* bmdi);

void bm1688_bmdrv_sw_reset_tpu(struct bm_device_info *bmdi);
void bm1688_bmdrv_sw_reset_gdma(struct bm_device_info *bmdi);
void bm1688_bmdrv_sw_reset_tc906b(struct bm_device_info *bmdi);
void bm1688_bmdrv_sw_reset_hau(struct bm_device_info *bmdi);
#ifdef SOC_MODE
void bm1688_modules_reset(struct bm_device_info* bmdi);
int bm1688_modules_reset_init(struct bm_device_info* bmdi);
void bm1688_modules_tpu_system_reset(struct bm_device_info *bmdi);
int bm1688_modules_clk_init(struct bm_device_info* bmdi);
void bm1688_modules_clk_deinit(struct bm_device_info* bmdi);
void bm1688_modules_clk_enable(struct bm_device_info* bmdi);
void bm1688_modules_clk_disable(struct bm_device_info* bmdi);
#endif

#endif
