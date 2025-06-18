#include <linux/delay.h>
#include "bm_common.h"
#include "bm_pcie.h"
#include "bm_card.h"
#include "bm_fw.h"
#include "bm1688_reg.h"
#include "bm1688_irq.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#endif

#ifndef SOC_MODE
#include "i2c.h"
#include <linux/pci.h>
#include "bm1688_card.h"

static struct pci_dev *bm_pci_upstream_bridge(struct pci_dev *dev)
{
	if (pci_is_root_bus(dev->bus))
		return NULL;

	return dev->bus->self;
}

int bm1688_card_get_chip_index(struct bm_device_info *bmdi)
{
	int mode = bmdi->cinfo.mode;

	if (mode & 0x4)
		bmdi->cinfo.chip_index = 0x1;
	else
		bmdi->cinfo.chip_index = 0x0;

	return 0;
}

int bm1688_card_get_chip_num(struct bm_device_info *bmdi)
{
	int mode = 0x0;
	int num = 0x1;

	if (bmdi->bmcd != NULL)
		return bmdi->bmcd->chip_num;

	mode = bmdi->cinfo.mode;

	if (mode & 0x8)
		num = 0x2;

	pr_info("chip num = %d\n", num);
	return num;
}

static void bm1688_init_i2c_for_mcu(struct bm_device_info *bmdi)
{
	u32 i2c_index = 0x1;
	u32 tx_level = 0;
	u32 rx_level = 0;
	u32 target_addr = 0x17;

	bmdrv_i2c_init(bmdi, i2c_index, rx_level, tx_level, target_addr);
}

int bm1688_get_board_version_from_mcu(struct bm_device_info *bmdi)
{
	int board_version = 0;
	int flags = 0x0;
	u8 mcu_sw_version, board_type, hw_version = 0;

	if (bmdi->cinfo.platform == PALLADIUM) {
		bmdi->cinfo.board_version = 0;
		return 0;
	}

	flags = mcu_info_reg_read(bmdi, 0);

	if (flags == 0xdeadbeef) {
		board_version = mcu_info_reg_read(bmdi, 4);
		board_type = (board_version >> 8) & 0xff;
		mcu_sw_version = (board_version >> 16) & 0xff;
		hw_version = (board_version >> 24) & 0xff;

		board_version = board_type << 8 | (int)hw_version;
		board_version = board_version | (int)mcu_sw_version << 16;
		bmdi->cinfo.board_version = board_version;
		return 0;
	}

	bm1688_init_i2c_for_mcu(bmdi);

	if (bm_mcu_read_reg(bmdi, 0, &board_type) != 0)
		return -1;
	if (bm_mcu_read_reg(bmdi, 2, &hw_version) != 0)
		return -1;
	if (bm_mcu_read_reg(bmdi, 1, &mcu_sw_version) != 0)
		return -1;

	if (board_type == 0x0 && hw_version == 0x4) {
		if (bm_get_board_type(bmdi, &board_type) != 0) {
			return -1;
		}
	}

	board_version = (int)board_type;
	board_version = board_version << 8 | (int)hw_version;
	board_version = board_version | (int)mcu_sw_version << 16;
	bmdi->cinfo.board_version = board_version;
	return 0;
}

int bm1688_get_board_type_by_id(struct bm_device_info *bmdi, char *s_board_type, int id)
{
	int ret = 0;

	switch (id) {
	case BOARD_TYPE_EVB:
		strncpy(s_board_type, "EVB", 10);
		break;

	default:
		strncpy(s_board_type, "Error", 10);
		pr_info("Ivalid board type %d\n", id);
		ret = -1;
		break;
	}

	return ret;
}

int bm1688_get_board_version_by_id(struct bm_device_info *bmdi, char *s_board_version,
		int board_id, int pcb_version, int bom_version)
{
	int ret = 0;
	int board_version = (bom_version & 0xf) | ((pcb_version & 0xf) << 4);

	switch (board_id) {
	case BOARD_TYPE_EVB:
	case BOARD_TYPE_SC5:
		if (board_version == 0x4)
			strncpy(s_board_version, "V1_2", 10);
		else if (pcb_version == 0x0)
			strncpy(s_board_version, "V1_1", 10);
		else
			strncpy(s_board_version, "invalid", 10);
		break;
	case BOARD_TYPE_SA5:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SE5:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SM5_P:
	case BOARD_TYPE_SM5_S:
		if (pcb_version == 0x1)
			if (bom_version == 0x0)
				strncpy(s_board_version, "V1_0", 10);
			else if (bom_version == 0x1)
				strncpy(s_board_version, "V1_1", 10);
			else if (bom_version == 0x2)
				strncpy(s_board_version, "V1_2", 10);
			else
				strncpy(s_board_version, "invalid", 10);
		else
			strncpy(s_board_version, "invalid", 10);
		break;
	case BOARD_TYPE_SA6:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SC5_H:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SM5M_P:
		if (board_version == 0x30) {
			strncpy(s_board_version, "V1_3", 10);
		} else {
			if (bom_version == 0x2)
				strncpy(s_board_version, "V1_2", 10);
			else
				strncpy(s_board_version, "V1_0", 10);
		}
		break;
	case BOARD_TYPE_SC5_PLUS:
		if (board_version == 0x0)
			strncpy(s_board_version, "V1_0", 10);
		else if (board_version == 0x1)
			strncpy(s_board_version, "V1_1", 10);
		else
			snprintf(s_board_version, 10, "V1_%d", board_version);
		break;
	case BOARD_TYPE_SC5_PRO:
		if (board_version == 0x0)
			strncpy(s_board_version, "V1_0", 10);
		else if (board_version == 0x1)
			strncpy(s_board_version, "V1_1", 10);
		else
			snprintf(s_board_version, 10, "V1_%d", board_version);
		break;
	case BOARD_TYPE_SC7_PRO:
		if (board_version == 0x11)
			strncpy(s_board_version, "V1_0", 10);
		else
			snprintf(s_board_version, 10, "V1_%d", board_version);
		break;
	case BOARD_TYPE_SC7_PLUS:
		if (board_version == 0x11)
			strncpy(s_board_version, "V1_0", 10);
		else
			snprintf(s_board_version, 10, "V1_%d", board_version);
		break;
	default:
		strncpy(s_board_version, "invalid", 10);
		pr_info("Ivalid board type %d\n", board_id);
		ret = -1;
		break;
	}

	return ret;
}

void bm1688_get_clk_temperature(struct bm_device_info *bmdi)
{
	switch (BM1688_BOARD_TYPE(bmdi)) {
	case BOARD_TYPE_SM5_P:
                {
                        if ((int)(BM1688_HW_VERSION(bmdi)) == 18) {
                                bmdi->c_attr.thermal_info.half_clk_tmp = 100;
				bmdi->c_attr.thermal_info.min_clk_tmp = 110;
                        } else {
                                bmdi->c_attr.thermal_info.half_clk_tmp = 85;
				bmdi->c_attr.thermal_info.min_clk_tmp = 90;
			}
                        break;
                }
	default:
		bmdi->c_attr.thermal_info.half_clk_tmp = 85;
		bmdi->c_attr.thermal_info.min_clk_tmp = 90;
		break;

	}
}

void bm1688_get_fusing_temperature(struct bm_device_info *bmdi, int *max_tmp, int *support_tmp)
{
	switch (BM1688_BOARD_TYPE(bmdi)) {
        case BOARD_TYPE_SM5_P:
                {
                        if ((int)(BM1688_HW_VERSION(bmdi)) == 18) {
                                *max_tmp = 120;
                                *support_tmp = 95;
                        } else {
                                *max_tmp = 95;
                                *support_tmp = 90;
			}
                        break;
                }
        default:
                *max_tmp = 95;
                *support_tmp = 90;
                break;

        }
}
#endif

#ifndef SOC_MODE
void bm1688_stop_smbus(struct bm_device_info *bmdi)
{
/* mask i2c2*/
	int value = 0x0;

	value = intc_reg_read(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET);
	value = value | (0x1 << 27);
	intc_reg_write(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
}
#endif

void bm1688_stop_c906(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	u32 ctrl_word;
#endif
	/*send irq 183 to C906_0/1, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_SET_OFFSET, 0x1 << 6);
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_SET_OFFSET, 0x1 << 10);
	mdelay(10);

#ifdef SOC_MODE
	reset_control_assert(bmdi->cinfo.tc906b0);
	reset_control_assert(bmdi->cinfo.tc906b1);
#else
	/* reset C906 */
	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	ctrl_word &= ~(0x3 << 27);
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
#endif
	pr_info("C906 firmware stop\n");
}

void bm1688_start_c906(struct bm_device_info *bmdi)
{
	u64 c906_park_0 = C906_0_PARK;
	u64 c906_park_1 = C906_1_PARK;
	u32 c906_reset_base_l_0 = c906_park_0 & 0xffffffff;
	u32 c906_reset_base_h_0 = c906_park_0 >> 32;
	u32 c906_reset_base_l_1 = c906_park_1 & 0xffffffff;
	u32 c906_reset_base_h_1 = c906_park_1 >> 32;
	u32 c906_clk;
	u32 ctrl_word;

	// clear irq 183 in C906_0/1
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 6);
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 10);

	bdc_reg_write(bmdi, 0x218, c906_reset_base_l_0);
	bdc_reg_write(bmdi, 0x21c, c906_reset_base_h_0);
	bdc_reg_write(bmdi, 0x220, c906_reset_base_l_1);
	bdc_reg_write(bmdi, 0x224, c906_reset_base_h_1);

	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	ctrl_word &= ~(0x3 << 27);
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
	udelay(100);

	c906_clk = bdc_reg_read(bmdi, 0);
	// tc906 clk default 0, need to open
	if ((c906_clk & (0x3 << 5)) != (0x3 << 5))
		bdc_reg_write(bmdi, 0, c906_clk | (0x3 << 5));

	ctrl_word |= 0x3 << 27;
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
	udelay(100);

	pr_info("C906 firmware start\n");
}

void bm1688_resume_tpu(struct bm_device_info *bmdi, u32 c906_park_0_l,
						u32 c906_park_0_h, u32 c906_park_1_l, u32 c906_park_1_h)
{
	u32 c906_clk;
	u32 ctrl_word;

	// clear irq 183 in C906_0/1
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 6);
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 10);

	bdc_reg_write(bmdi, 0x218, c906_park_0_l);
	bdc_reg_write(bmdi, 0x21c, c906_park_0_h);
	bdc_reg_write(bmdi, 0x220, c906_park_1_l);
	bdc_reg_write(bmdi, 0x224, c906_park_1_h);

	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	ctrl_word &= ~(0x3 << 27);
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
	udelay(100);

	c906_clk = bdc_reg_read(bmdi, 0);
	// tc906 clk default 0, need to open
	if ((c906_clk & (0x3 << 5)) != (0x3 << 5))
		bdc_reg_write(bmdi, 0, c906_clk | (0x3 << 5));

	ctrl_word |= 0x3 << 27;
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
	udelay(100);

	pr_info("C906 firmware resume\n");

}

int bm1688_reset_tpu(struct bm_device_info *bmdi)
{
	u32 timeout_ms = bmdi->cinfo.delay_ms;
	u32 timeout;

	/*send irq 183 to C906_0/1, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_SET_OFFSET, 0x1 << 6);
	udelay(100);
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_SET_OFFSET, 0x1 << 10);
	udelay(100);

	// set hau_flush_enable to 1 then check hau_flush_done
	timeout = timeout_ms * 1000;
	hau_reg_write(bmdi, 0x100, hau_reg_read(bmdi, 0x100) | (0x1 << 3));
	while ((hau_reg_read(bmdi, 0x100) & (0x1 << 4)) == 0) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("hau_stopper_done timeout\n");
			return -EBUSY;
		}
        }

	// set tpu0/1_stopper_enable to 1 then check cfg_stopper_done
	timeout = timeout_ms * 1000;
	tpu_reg_write_idx(bmdi, 0x15c, tpu_reg_read_idx(bmdi, 0x15c, 0) | (0x1 << 9), 0);
	while ((tpu_reg_read_idx(bmdi, 0x15c, 0) & (0x1 << 8)) == 0) {
                udelay(1);
		if (--timeout == 0) {
			pr_err("tpu0_stopper_done timeout\n");
			return -EBUSY;
		}
        }
	timeout = timeout_ms * 1000;
	tpu_reg_write_idx(bmdi, 0x15c, tpu_reg_read_idx(bmdi, 0x15c, 1) | (0x1 << 9), 1);
	while ((tpu_reg_read_idx(bmdi, 0x15c, 1) & (0x1 << 8)) == 0) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("tpu1_stopper_done timeout\n");
			return -EBUSY;
		}
        }

	// set gdma0/1_sys_pulse to 1 then check pulse done
	timeout = timeout_ms * 1000;
	gdma_reg_write_idx(bmdi, 0x160, gdma_reg_read_idx(bmdi, 0x160, 0) | 0x1, 0);
	while ((gdma_reg_read_idx(bmdi, 0x160, 0) & (0xf << 12)) != (0xf << 12)) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("gdma0_stopper_done timeout\n");
			return -EBUSY;
		}
        }
	timeout = timeout_ms * 1000;
	gdma_reg_write_idx(bmdi, 0x160, gdma_reg_read_idx(bmdi, 0x160, 1) | 0x1, 1);
	while ((gdma_reg_read_idx(bmdi, 0x160, 1) & (0xf << 12)) != (0xf << 12)) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("gdma1_stopper_done timeout\n");
			return -EBUSY;
		}
        }

	// clear irq 183 in C906_0/1
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 6);
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 10);

	// reset tpu0/1 gdma0/1 hau tc9060/1
	top_reg_write(bmdi, TOP_SW_RESET0, top_reg_read(bmdi, TOP_SW_RESET0) & ~0x3F800000);
	udelay(100);
	top_reg_write(bmdi, TOP_SW_RESET0, top_reg_read(bmdi, TOP_SW_RESET0) | 0x3F800000);

        mdelay(1);
	pr_info("tpu sys reset\n");
	return 0;
}

int bm1688_l2_sram_init(struct bm_device_info *bmdi)
{
	//bmdev_memcpy_s2d_internal(bmdi, L2_SRAM_START_ADDR + L2_SRAM_TPU_TABLE_OFFSET,
	//		l2_sram_table, sizeof(l2_sram_table), false);
	return 0;
}

#ifdef SOC_MODE
void bm1688_tpu_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 tpu0 reset\n");
	reset_control_assert(bmdi->cinfo.tpu0);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.tpu0);
	PR_TRACE("bm1688 tpu1 reset\n");
	reset_control_assert(bmdi->cinfo.tpu1);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.tpu1);
}

void bm1688_gdma_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 gdma0 reset\n");
	reset_control_assert(bmdi->cinfo.gdma0);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.gdma0);
	PR_TRACE("bm1688 gdma1 reset\n");
	reset_control_assert(bmdi->cinfo.gdma1);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.gdma1);
}

void bm1688_hau_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 hau reset\n");
	reset_control_assert(bmdi->cinfo.hau);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.hau);
}

void bm1688_tpu_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 tpu clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.tpu_clk);
}

void bm1688_tpu_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 tpu clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.tpu_clk);
}

void bm1688_top_fab0_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 top_fab0 clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.top_fab0_clk);
}

void bm1688_top_fab0_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 top_fab0 clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.top_fab0_clk);
}

void bm1688_tc906b_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 tc906b clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.tc906b_clk);
}

void bm1688_tc906b_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 tc906b clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.tc906b_clk);
}

void bm1688_timer_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 timer clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.timer_clk);
}

void bm1688_timer_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 timer clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.timer_clk);
}

void bm1688_gdma_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 gdma clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.gdma_clk);
}

void bm1688_gdma_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1688 gdma clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.gdma_clk);
}
#else
void bm1688_tpu_clk_enable(struct bm_device_info *bmdi){ }
void bm1688_tpu_clk_disable(struct bm_device_info *bmdi){ }
void bm1688_gdma_clk_enable(struct bm_device_info *bmdi){ }
void bm1688_gdma_clk_disable(struct bm_device_info *bmdi){ }
#endif
