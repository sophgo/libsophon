#include <linux/delay.h>
#include "bm_common.h"
#include "bm_pcie.h"
#include "bm_card.h"
#include "l2_sram_table.h"
#include "bm1684_reg.h"
#include "bm1684_irq.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#endif

#ifndef SOC_MODE
#include "i2c.h"
#include <linux/pci.h>
#include "bm1684_card.h"

static struct pci_dev *bm_pci_upstream_bridge(struct pci_dev *dev)
{
	if (pci_is_root_bus(dev->bus))
		return NULL;

	return dev->bus->self;
}

int bm1684_card_get_chip_index(struct bm_device_info *bmdi)
{
	struct pci_dev *parent = NULL;
	int mode = 0x0;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO)) {
		parent = bm_pci_upstream_bridge(bmdi->cinfo.pcidev);
		if (parent != NULL)
			bmdi->cinfo.chip_index = PCI_SLOT(parent->devfn);
		else {
			bmdi->cinfo.chip_index = 0x0;
			pr_info("parent is null\n");
		}
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
		mode = bmdrv_pcie_get_mode(bmdi) & 0x7;
		if (mode == 0x6)
			bmdi->cinfo.chip_index = 0x0;
		else if (mode == 0x1)
			bmdi->cinfo.chip_index = 0x1;
		else if (mode == 0x0)
			bmdi->cinfo.chip_index = 0x2;
		else
			bmdi->cinfo.chip_index = 0x0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) {
		mode = bmdrv_pcie_get_mode(bmdi) & 0x7;
		if ((mode == 0x7) || (mode == 0x6))
			bmdi->cinfo.chip_index = 0x0;
		else if (mode == 0x2)
			bmdi->cinfo.chip_index = 0x1;
		else if (mode == 0x1)
			if ((int)BM1684_HW_VERSION(bmdi) == 17) {
				bmdi->cinfo.chip_index = 0x1; //0x2;
			} else {
				bmdi->cinfo.chip_index = 0x2;
			}
		else if (mode == 0x0)
			if ((int)BM1684_HW_VERSION(bmdi) == 17) {
				bmdi->cinfo.chip_index = 0x2; //0x3;
			} else {
				bmdi->cinfo.chip_index = 0x3;
			}
		else
			bmdi->cinfo.chip_index = 0x0;
	} else
		bmdi->cinfo.chip_index = 0x0;

	return 0;
}

int bm1684_card_get_chip_num(struct bm_device_info *bmdi)
{
	int mode = 0x0;
	int num = 0x0;

	if (bmdi->bmcd != NULL)
		return bmdi->bmcd->chip_num;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return 0x8;

	mode = bmdrv_pcie_get_mode(bmdi) & 0x7;

	switch (mode) {
	case 0x7:
	case 0x2:
		num = 0x4;
		break;
	case 0x6:
		num = 0x3;
		break;
	case 0x0:
	case 0x1:
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) {
			if ((int)BM1684_HW_VERSION(bmdi) != 17) {
				num = 0x4;
				break;
			}
		}
		num = 0x3;
		break;
	case 0x4:
	case 0x3:
		num = 0x1;
		break;
	default:
		num = 0x1;
		break;
	}
	pr_info("chip num = %d\n", num);
	return num;
}

static void bm1684_init_i2c_for_mcu(struct bm_device_info *bmdi)
{
	u32 i2c_index = 0x1;
	u32 tx_level = 0;
	u32 rx_level = 0;
	u32 target_addr = 0x17;

	bmdrv_i2c_init(bmdi, i2c_index, rx_level, tx_level, target_addr);
}

int bm1684_correct_hw_version(struct bm_device_info *bmdi, u8 hw_version)
{
	static u8 chip0_hw_version = 0;

	bm1684_card_get_chip_index(bmdi);

	if (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC5_PLUS)
		return 0;

	if ((bmdi->cinfo.chip_index == 0) && (BM1684_HW_VERSION(bmdi) != 0x0)) {
		chip0_hw_version = hw_version;
	}

	if ((bmdi->cinfo.chip_index != 0) && (chip0_hw_version != 0)) {
		bmdi->cinfo.board_version &= 0xFFFFFF00;
		bmdi->cinfo.board_version |= chip0_hw_version;
	}

	return 0;
}

int bm1684_get_board_version_from_mcu(struct bm_device_info *bmdi)
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

	bm1684_init_i2c_for_mcu(bmdi);

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
	bm1684_correct_hw_version(bmdi, hw_version);
	return 0;
}

int bm1684_get_board_type_by_id(struct bm_device_info *bmdi, char *s_board_type, int id)
{
	int ret = 0;

	switch (id) {
	case BOARD_TYPE_EVB:
		strncpy(s_board_type, "EVB", 10);
		break;
	case BOARD_TYPE_SC5:
		strncpy(s_board_type, "SC5", 10);
		break;
	case BOARD_TYPE_SA5:
		strncpy(s_board_type, "SA5", 10);
		break;
	case BOARD_TYPE_SE5:
		strncpy(s_board_type, "SE5", 10);
		break;
	case BOARD_TYPE_SM5_P:
		{
			if ((int)(BM1684_HW_VERSION(bmdi)) == 18)
				strncpy(s_board_type, "SM5_W", 10);
			else
				strncpy(s_board_type, "SM5_P", 10);
			break;
		}
	case BOARD_TYPE_SM5_S:
		strncpy(s_board_type, "SM5_S", 10);
		break;
	case BOARD_TYPE_SA6:
		strncpy(s_board_type, "SA6", 10);
		break;
	case BOARD_TYPE_SC5_PLUS:
		if ((int)BM1684_HW_VERSION(bmdi) == 3)
			strncpy(s_board_type, "SC5R", 10);
		else
			strncpy(s_board_type, "SC5+", 10);
		break;
	case BOARD_TYPE_SC5_H:
		strncpy(s_board_type, "SC5H", 10);
		break;
	case BOARD_TYPE_SM5M_P:
		strncpy(s_board_type, "SM5M", 10);
		break;
	case BOARD_TYPE_SC5_PRO:
		strncpy(s_board_type, "SC5P", 10);
		break;
	case BOARD_TYPE_BM1684X_EVB:
		strncpy(s_board_type, "EVB", 10);
		break;
	case BOARD_TYPE_SC7_PRO:
		strncpy(s_board_type, "SC7P", 10);
		break;
	case BOARD_TYPE_SC7_PLUS:
		strncpy(s_board_type, "SC7+", 10);
		break;
	default:
		strncpy(s_board_type, "Error", 10);
		pr_info("Ivalid board type %d\n", id);
		ret = -1;
		break;
	}

	return ret;
}

int bm1684_get_board_version_by_id(struct bm_device_info *bmdi, char *s_board_version,
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
	case BOARD_TYPE_BM1684X_EVB:
		strncpy(s_board_version, "V1_0", 10);
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

void bm1684_get_clk_temperature(struct bm_device_info *bmdi)
{
	switch (BM1684_BOARD_TYPE(bmdi)) {
	case BOARD_TYPE_SM5_P:
                {
                        if ((int)(BM1684_HW_VERSION(bmdi)) == 18) {
                                bmdi->c_attr.thermal_info.half_clk_tmp = 100;
				bmdi->c_attr.thermal_info.min_clk_tmp = 110;
                        } else {
                                bmdi->c_attr.thermal_info.half_clk_tmp = 85;
				bmdi->c_attr.thermal_info.min_clk_tmp = 90;
			}
                        break;
                }
	case BOARD_TYPE_SC7_PRO:
		{
			bmdi->c_attr.thermal_info.half_clk_tmp = 65;
			bmdi->c_attr.thermal_info.min_clk_tmp = 95;
			bmdi->c_attr.thermal_info.max_clk_tmp = 60;
			bmdi->c_attr.thermal_info.extreme_tmp = 105;
			break;
		}
	default:
		bmdi->c_attr.thermal_info.half_clk_tmp = 85;
		bmdi->c_attr.thermal_info.min_clk_tmp = 90;
		break;

	}
}

void bm1684_get_fusing_temperature(struct bm_device_info *bmdi, int *max_tmp, int *support_tmp)
{
	switch (BM1684_BOARD_TYPE(bmdi)) {
        case BOARD_TYPE_SM5_P:
                {
                        if ((int)(BM1684_HW_VERSION(bmdi)) == 18) {
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
void bm1684_stop_smbus(struct bm_device_info *bmdi)
{
/* mask i2c2*/
	int value = 0x0;

	value = intc_reg_read(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_MASK_L_OFFSET);
	value = value | (0x1 << 27);
	intc_reg_write(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_MASK_L_OFFSET, value);
}
#endif

void bm1684_stop_arm9(struct bm_device_info *bmdi)
{
	u32 ctrl_word;
	/*send  fiq 6 to arm9, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_ARM9_IRQ_SET_OFFSET, 0x1 << 14);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		msleep(500);
#endif
	udelay(50);
#ifdef SOC_MODE
	reset_control_assert(bmdi->cinfo.arm9);
#else
	/*stop smbus*/
	bm1684_stop_smbus(bmdi);

	/* reset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	ctrl_word &= ~(1 << 1);
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
#endif
	/* switch the itcm to pcie */
	ctrl_word = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	ctrl_word |= (1 << 1);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, ctrl_word);
}

void bm1684_start_arm9(struct bm_device_info *bmdi)
{
	u32 ctrl_word;
	/* switch itcm to arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	ctrl_word &= ~(1 << 1);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, ctrl_word);
#ifdef SOC_MODE
	reset_control_deassert(bmdi->cinfo.arm9);
#else
	/* dereset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	do {
		ctrl_word |= (1 << 1);
		top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
	} while (((ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0)) & (1 << 1)) != (1 << 1));
#endif
}

void bm1684x_stop_a53lite(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	u32 ctrl_word;
#endif
	/*send  fiq 6 to arm9, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_ARM9_IRQ_SET_OFFSET, 0x1 << 3);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		msleep(500);
#endif
	udelay(50);
#ifdef SOC_MODE
	reset_control_assert(bmdi->cinfo.arm9);
#else
	/*stop smbus*/
	bm1684_stop_smbus(bmdi);

	/* reset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	ctrl_word &= ~(1 << 1);
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
#endif
}

void bm1684x_start_a53lite(struct bm_device_info *bmdi)
{
        u64 a53lite_park = 0x100000000;
        u32 a53lite_reset_base = (a53lite_park >> 4) & 0xffffffff;
        u32 tmp;

        top_reg_write(bmdi, 0x194, (0x1 << 3));
        top_reg_write(bmdi, 0x03c, a53lite_reset_base);

        tmp = top_reg_read(bmdi, 0xc00);
        tmp &= ~(1 << 1);
        top_reg_write(bmdi, 0xc00, tmp);
        mdelay(100);
        tmp |= 1 << 1;
        top_reg_write(bmdi, 0xc00, tmp);
        mdelay(100);
        pr_info("top1 value = %x\n", top_reg_read(bmdi,0));
}

int bm1684_l2_sram_init(struct bm_device_info *bmdi)
{
	bmdev_memcpy_s2d_internal(bmdi, L2_SRAM_START_ADDR + L2_SRAM_TPU_TABLE_OFFSET,
			l2_sram_table, sizeof(l2_sram_table));
	return 0;
}

#ifdef SOC_MODE
void bm1684_tpu_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu reset\n");
	reset_control_assert(bmdi->cinfo.tpu);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.tpu);
}

void bm1684_gdma_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 gdma reset\n");
	reset_control_assert(bmdi->cinfo.gdma);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.gdma);
}

void bm1684_tpu_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.tpu_clk);
}

void bm1684_tpu_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.tpu_clk);
}

void bm1684_fixed_tpu_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu fixed tpu clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.fixed_tpu_clk);
}

void bm1684_fixed_tpu_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu fixed tpu clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.fixed_tpu_clk);
}

void bm1684_arm9_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 arm9 clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.arm9_clk);
}

void bm1684_arm9_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 arm9 clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.arm9_clk);
}

void bm1684_sram_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 sram clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.sram_clk);
}

void bm1684_sram_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 sram clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.sram_clk);
}

void bm1684_gdma_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 gdma clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.gdma_clk);
}

void bm1684_gdma_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 gdma clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.gdma_clk);
}

void bm1684_intc_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 intc clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.intc_clk);
}

void bm1684_intc_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 intc clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.intc_clk);
}

void bm1684_hau_ngs_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 hau ngs clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.hau_ngs_clk);
}

void bm1684_hau_ngs_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 hau ngs clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.hau_ngs_clk);
}

void bm1684_tpu_only_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu only clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.tpu_only_clk);
}

void bm1684_tpu_only_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu only clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.tpu_only_clk);
}
#endif
