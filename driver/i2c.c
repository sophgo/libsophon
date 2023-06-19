#include <linux/delay.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_attr.h"
#include "i2c.h"
#ifndef SOC_MODE
#include "bm1684_card.h"
#include "bm_pcie.h"
#endif

/* Designware I2C bit definition */
#define DW_I2C_CON_MASTER_MODE              (0x01 << 0)
#define DW_I2C_CON_STANDARD_SPEED           (0x01 << 1)
#define DW_I2C_CON_FULL_SPEED               (0x02 << 1)
#define DW_I2C_CON_HIGH_SPEED               (0x03 << 1)
#define DW_I2C_CON_10B_ADDR_SLAVE           (0x01 << 3)
#define DW_I2C_CON_10B_ADDR_MASTER          (0x01 << 4)
#define DW_I2C_CON_RESTART_EN               (0x01 << 5)
#define DW_I2C_CON_SLAVE_DIS                (0x01 << 6)

#define DW_I2C_INT_RX_UNDER                 (0x01 << 0)
#define DW_I2C_INT_RX_OVER                  (0x01 << 1)
#define DW_I2C_INT_RX_FULL                  (0x01 << 2)
#define DW_I2C_INT_TX_OVER                  (0x01 << 3)
#define DW_I2C_INT_TX_EMPTY                 (0x01 << 4)
#define DW_I2C_INT_RD_REQ                   (0x01 << 5)
#define DW_I2C_INT_TX_ABRT                  (0x01 << 6)
#define DW_I2C_INT_RX_DONE                  (0x01 << 7)
#define DW_I2C_INT_ACTIVITY                 (0x01 << 8)
#define DW_I2C_INT_STOP_DET                 (0x01 << 9)
#define DW_I2C_INT_START_DET                (0x01 << 10)
#define DW_I2C_INT_GEN_ALL                  (0x01 << 11)

#define DW_I2C_INT_RX_UNDER_MASK            (0x01 << 0)
#define DW_I2C_INT_RX_OVER_MASK             (0x01 << 1)
#define DW_I2C_INT_RX_FULL_MASK             (0x01 << 2)
#define DW_I2C_INT_TX_OVER_MASK             (0x01 << 3)
#define DW_I2C_INT_TX_EMPTY_MASK            (0x01 << 4)
#define DW_I2C_INT_RD_REQ_MASK              (0x01 << 5)
#define DW_I2C_INT_TX_ABRT_MASK             (0x01 << 6)
#define DW_I2C_INT_RX_DONE_MASK             (0x01 << 7)
#define DW_I2C_INT_ACTIVITY_MASK            (0x01 << 8)
#define DW_I2C_INT_STOP_DET_MASK            (0x01 << 9)
#define DW_I2C_INT_START_DET_MASK           (0x01 << 10)
#define DW_I2C_INT_GEN_ALL_MASK             (0x01 << 11)

#define DW_I2C_INT_RX_UNDER_RAW             (0x01 << 0)
#define DW_I2C_INT_RX_OVER_RAW              (0x01 << 1)
#define DW_I2C_INT_RX_FULL_RAW              (0x01 << 2)
#define DW_I2C_INT_TX_OVER_RAW              (0x01 << 3)
#define DW_I2C_INT_TX_EMPTY_RAW             (0x01 << 4)
#define DW_I2C_INT_RD_REQ_RAW               (0x01 << 5)
#define DW_I2C_INT_TX_ABRT_RAW              (0x01 << 6)
#define DW_I2C_INT_RX_DONE_RAW              (0x01 << 7)
#define DW_I2C_INT_ACTIVITY_RAW             (0x01 << 8)
#define DW_I2C_INT_STOP_DET_RAW             (0x01 << 9)
#define DW_I2C_INT_START_DET_RAW            (0x01 << 10)
#define DW_I2C_INT_GEN_ALL_RAW              (0x01 << 11)

#define DW_I2C_CMD_DATA_READ_BIT           (0x01 << 8)
#define DW_I2C_CMD_DATA_STOP_BIT           (0x01 << 9)
#define DW_I2C_CMD_DATA_RESTART_BIT        (0x01 << 10)

#define DW_I2C_STAUS_MST_ACTIVITY          (0x01 << 5)
#define DW_I2C_STAUS_ACTIVITY              (0x01 << 0)

#define I2C_TIME_OUT           50000
/*i2c register*/

#define I2C_CON                0x000
#define I2C_TAR                0x004
#define I2C_SAR                0x008
#define I2C_HS_MADDR           0x00c
#define I2C_DATA_CMD           0x010
#define I2C_SS_SCL_HCNT        0x014
#define I2C_SS_SCL_LCNT        0x018
#define I2C_FS_SCL_HCNT        0x01c
#define I2C_FS_SCL_LCNT        0x020
#define I2C_INT_STAT           0x02c
#define I2C_INT_MASK           0x030
#define I2C_RAW_INT_STAT       0x034
#define I2C_RX_TL              0x038
#define I2C_TX_TL              0x03c
#define I2C_CLR_INTR           0x040
#define I2C_ENABLE             0x06c
#define I2C_STATUS             0x070
#define I2C_RXFLR              0x078
#define I2C_SDA_HOLD           0x07c
#define I2C_TX_ABRT_SOURCE     0x080
#define I2C_FS_SPKLEN          0x0a0
int bmdrv_i2c_init(struct bm_device_info *bmdi, u32 i2c_index, u32 rx_level, u32 tx_level, u32 target_addr)
{
	u32 reg_val = 0;

	/*disable i2c*/
	i2c_reg_write(bmdi, i2c_index, I2C_ENABLE, 0);

	/*set maximum speed supported*/
	reg_val = DW_I2C_CON_MASTER_MODE | DW_I2C_CON_SLAVE_DIS | DW_I2C_CON_STANDARD_SPEED | DW_I2C_CON_RESTART_EN;
	i2c_reg_write(bmdi, i2c_index, I2C_CON, reg_val);

	/*set rx fifo threshold level*/
	i2c_reg_write(bmdi, i2c_index, I2C_RX_TL, rx_level);

	/*set tx fifo threshold level*/
	i2c_reg_write(bmdi, i2c_index, I2C_TX_TL, tx_level);

	if (bmdi->cinfo.chip_id == 0x1682) {
		i2c_reg_write(bmdi, i2c_index, I2C_FS_SCL_HCNT, 0x14);
		i2c_reg_write(bmdi, i2c_index, I2C_SS_SCL_HCNT, 0x69);
		i2c_reg_write(bmdi, i2c_index, I2C_FS_SCL_LCNT, 0x27);
		i2c_reg_write(bmdi, i2c_index, I2C_SS_SCL_LCNT, 0x7c);
	} else if (bmdi->cinfo.chip_id == 0x1684) {
		i2c_reg_write(bmdi, i2c_index, I2C_SS_SCL_HCNT, 0x1b0);
		i2c_reg_write(bmdi, i2c_index, I2C_SS_SCL_LCNT, 0x1f8);
	}

	i2c_reg_write(bmdi, i2c_index, I2C_FS_SPKLEN, 0x01);
	i2c_reg_write(bmdi, i2c_index, I2C_SDA_HOLD, 0x04);

	/*enable all interrupts*/
	i2c_reg_write(bmdi, i2c_index, I2C_INT_MASK, 0x0);

	i2c_reg_write(bmdi, i2c_index, I2C_TAR, target_addr);

	/* Enable i2c */
	i2c_reg_write(bmdi, i2c_index, I2C_ENABLE, 1);

	i2c_reg_read(bmdi, i2c_index, I2C_CLR_INTR);

	return 0;
}
#ifndef SOC_MODE
void bmdrv_power_and_temp_i2c_init(struct bm_device_info  *bmdi)
{
	u32 i2c_addr = 0;
	u32 i2c_index = 0;
	u32 rx_level = 1;
	u32 tx_level = 0;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		i2c_addr = I2C_1331_ADDR;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) {
		if (BM1684_HW_VERSION(bmdi) != 0x30 && BM1684_HW_VERSION(bmdi) != 0x31) {
			i2c_addr = I2C_68127_ADDR_SM5M;
		} else {
			i2c_addr = I2C_IS6608A_ADDR_SM5M;
			rx_level = 0;
		}
	}

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		i2c_addr = 0x4c;
		rx_level = 0;
	}

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		i2c_addr = I2C_68224_ADDR;
	}

	bmdrv_i2c_init(bmdi, i2c_index, rx_level, tx_level, i2c_addr);

	i2c_addr = 0x17;
	i2c_index = 1;
	rx_level = 0;
	tx_level = 0;
	bmdrv_i2c_init(bmdi, i2c_index, rx_level, tx_level, i2c_addr);
}
#endif
int bm_i2c_set_target_addr(struct bm_device_info *bmdi, u32 i2c_index, u32 target_addr)
{
	i2c_reg_write(bmdi, i2c_index, I2C_ENABLE, 0);
	i2c_reg_write(bmdi, i2c_index, I2C_TAR, target_addr);
	i2c_reg_write(bmdi, i2c_index, I2C_ENABLE, 1);

	return 0;
}

void bm_i2c_recovery(struct bm_device_info *bmdi, u32 i2c_index)
{
	u32 i2c_addr = 0;
	u32 tx_level = 0;
	u32 rx_level = 0;

	i2c_addr = i2c_reg_read(bmdi, i2c_index, I2C_TAR);
	rx_level = i2c_reg_read(bmdi, i2c_index, I2C_RX_TL);
	tx_level = i2c_reg_read(bmdi, i2c_index, I2C_TX_TL);

	bmdrv_i2c_init(bmdi, i2c_index, rx_level, tx_level, i2c_addr);
	pr_info("i2c recovery done\n");
}

static inline int bm_check_i2c_reg_bits(struct bm_device_info *bmdi, u32 i2c_index, u64 offset, u32 mask, u32 wait_loop)
{
	u32 try_loop = 0;
	u32 reg_val;

	while (1) {
		reg_val = i2c_reg_read(bmdi, i2c_index, offset);
		if ((reg_val & mask) != 0)
			return reg_val & mask;

		if (wait_loop != 0) {
			try_loop++;		//wait_loop == 0 means wait for ever
			udelay(1);
			if (try_loop >= wait_loop)
				break;			//timeout break
		}
	}
	return 0;
}

static inline int bm_check_i2c_reg_bits_zero(struct bm_device_info *bmdi, u32 i2c_index, u64 offset, u32 mask, u32 wait_loop)
{
	u32 try_loop = 0;
	u32 reg_val;

	while (1) {
		reg_val = i2c_reg_read(bmdi, i2c_index, offset);
		if ((reg_val & mask) == 0)
			return 1;

		if (wait_loop != 0) {
			try_loop++;		//wait_loop == 0 means wait for ever
			udelay(1);
			if (try_loop >= wait_loop)
				break;			//timeout break
		}
	}
	return 0;
}

int bm_i2c_read_hword(struct bm_device_info *bmdi, u32 i2c_index, u8 cmd, u16 *data)
{
	u32 cmd_data;
	u32 int_stat;
	int i, rxflr;

	*data = 0;

	cmd_data = cmd;
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	cmd_data = DW_I2C_CMD_DATA_READ_BIT;
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);
	cmd_data = DW_I2C_CMD_DATA_READ_BIT;
	if (bmdi->cinfo.chip_id == 0x1686) {
		cmd_data |= DW_I2C_CMD_DATA_STOP_BIT;
	}
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_RX_FULL, I2C_TIME_OUT);
	if (int_stat != 0) {
		rxflr = i2c_reg_read(bmdi, i2c_index, I2C_RXFLR);
		//pr_info("valid rx flr is %d\n", rxflr);
		for (i = 0; i < rxflr; i++) {
			cmd_data = i2c_reg_read(bmdi, i2c_index, I2C_DATA_CMD);
			cmd_data = cmd_data & 0xFF;
			*data |= cmd_data << (8 * i);
			//pr_info("data read %d :%d\n", i, cmd_data);
		}
	} else {
		dev_err(bmdi->cinfo.device, "---i2c0 Rx Timeout, int stat: 0x%08x\n", int_stat);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c0 Tx Timeout, int stat: 0x%08x ---\n", int_stat);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 1 << 0, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c0 Check idle Timeout , int stat: 0x%08x ---\n", int_stat);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
	if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {
		dev_err(bmdi->cinfo.device, "---i2c0 Tx Aborted, intr stat: 0x%08x, abt reason: 0x%08x ===\n",
			int_stat, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	return 0;
}

int bm_i2c_write_hword(struct bm_device_info *bmdi, u32 i2c_index, u8 cmd, u16 data)
{
        u32 cmd_data;
        u32 int_stat;

        cmd_data = (u32)cmd;
        i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

        int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
        if (int_stat == 0) {
                dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
                        int_stat, bmdi->dev_index ,i2c_index);
                bm_i2c_recovery(bmdi, i2c_index);
                return -1;
        }

        cmd_data = (u32)(data & 0xFF);
        i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

        int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
        if (int_stat == 0) {
                dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
                        int_stat, bmdi->dev_index ,i2c_index);
                bm_i2c_recovery(bmdi, i2c_index);
                return -1;
        }

        cmd_data = (u32)((data >> 8) & 0xFF);
	if (bmdi->cinfo.chip_id == 0x1686) {
		cmd_data |= DW_I2C_CMD_DATA_STOP_BIT;
	}
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

        int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
        if (int_stat == 0) {
                dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
                        int_stat, bmdi->dev_index ,i2c_index);
                bm_i2c_recovery(bmdi, i2c_index);
                return -1;
        }

        int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 1 << 0, I2C_TIME_OUT);
        if (int_stat == 0) {
                dev_err(bmdi->cinfo.device, "---i2c Check idle fail, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n",
                        int_stat, bmdi->dev_index ,i2c_index);
                bm_i2c_recovery(bmdi, i2c_index);
                return -1;
        }

        int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
        if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {
                dev_err(bmdi->cinfo.device, "---i2c Tx Aborted, intr stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x, abt reason: 0x%08x ===\n",
                        int_stat, bmdi->dev_index ,i2c_index, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
                bm_i2c_recovery(bmdi, i2c_index);
                return -1;
        }

        return 0;
}

int bm_get_eeprom_reg(struct bm_device_info *bmdi, u16 index, u8 *data)
{
	u32 cmd_data;
	int i;
	u32 int_stat;
	u32 i2c_index = 0x1;

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x6a);

	for (i = 0; i < 2; i++) {
		cmd_data = (index >> (8 * (1 - i))) & 0xff;
		i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);
	}

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "--- Tx Timeout, int stat: 0x%08x ---\n", int_stat);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	cmd_data = DW_I2C_CMD_DATA_READ_BIT;
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_RX_FULL, I2C_TIME_OUT);
	if (int_stat != 0) {
		cmd_data = i2c_reg_read(bmdi, i2c_index, I2C_DATA_CMD);
		*data = (u8)cmd_data;
	} else {
		dev_err(bmdi->cinfo.device, "--Rx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n", 
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	if (bmdi->cinfo.chip_id == 0x1686) {
		cmd_data = DW_I2C_CMD_DATA_STOP_BIT;
		i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);
	}

	int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 1 << 0, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c Check idle fail, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n", 
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
	if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {
		dev_err(bmdi->cinfo.device, "---i2c Tx Aborted, intr stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x, abt reason: 0x%08x ===\n",
			int_stat, bmdi->dev_index ,i2c_index, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	return 0;
}

int bm_set_eeprom_reg(struct bm_device_info *bmdi, u16 index, u8 data)
{
	u32 cmd_data;
	int i;
	u32 int_stat;
	u32 i2c_index = 0x1;

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x6a);

	for (i = 0; i < 3; i++) {
		if (i >= 0 && i <= 1)
			cmd_data = (index >> (8 * (1 - i))) & 0xff;
		else
			cmd_data = data | DW_I2C_CMD_DATA_STOP_BIT;
		i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);
	}

	if (bmdi->cinfo.chip_id == 0x1686) {
		cmd_data = DW_I2C_CMD_DATA_STOP_BIT;
		i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);
	}

	int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
	if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {
		dev_err(bmdi->cinfo.device, "---i2c Tx Aborted, intr stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x, abt reason: 0x%08x ===\n",
			int_stat, bmdi->dev_index ,i2c_index, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	} else {
		int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
		if (int_stat == 0) {
			dev_err(bmdi->cinfo.device, "--Rx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n", 
			int_stat, bmdi->dev_index ,i2c_index);
			bm_i2c_recovery(bmdi, i2c_index);
			return -1;
		}
		int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 0x1, I2C_TIME_OUT);
		if (int_stat == 0) {
			dev_err(bmdi->cinfo.device, "--- Check status Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n",
				int_stat, bmdi->dev_index ,i2c_index);
			bm_i2c_recovery(bmdi, i2c_index);
			return -1;
		}
	}
	return 0;
}
#define REG_STAGE		(0x3c)
#define REG_LOG			(0x62)
#define REG_CMD			(0x03)
#define REG_CALC_CKSUM		(0x63)
#define REG_CKSUM_OFF		(0x64)
#define REG_CKSUM_LEN		(0x68)
#define REG_CKSUM		(0x6c)
#define REG_OFFSET		(0x7c)
#define REG_DATA		(0x80)
#define REG_FLUSH		(0xff)

#define CMD_MCU_UPDATE		(0x08)

#define STAGE_APP		0
#define STAGE_LOADER		1
#define STAGE_UPGRADER		2

int bm_i2c_write_byte(struct bm_device_info *bmdi, u32 i2c_index, u8 cmd, u8 data)
{
	u32 cmd_data;
	u32 int_stat;

	cmd_data = (u32)cmd;
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	cmd_data = (u32)data;
	if (bmdi->cinfo.chip_id == 0x1686) {
		cmd_data |= DW_I2C_CMD_DATA_STOP_BIT;
	}
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 1 << 0, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c Check idle fail, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n",
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
	if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {
		dev_err(bmdi->cinfo.device, "---i2c Tx Aborted, intr stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x, abt reason: 0x%08x ===\n",
			int_stat, bmdi->dev_index ,i2c_index, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	return 0;
}

int bm_i2c_read_byte(struct bm_device_info *bmdi, u32 i2c_index, u8 cmd, u8 *data)
{
	u32 cmd_data;
	u32 int_stat;

	cmd_data = (u32)cmd;
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	cmd_data = DW_I2C_CMD_DATA_READ_BIT;
	if (bmdi->cinfo.chip_id == 0x1686) {
		cmd_data |= DW_I2C_CMD_DATA_STOP_BIT;
	}
	i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_RX_FULL, I2C_TIME_OUT);
	if (int_stat != 0) {
		cmd_data = i2c_reg_read(bmdi, i2c_index, I2C_DATA_CMD);
		*data = (u8)cmd_data;
	} else {
		dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 0x1, I2C_TIME_OUT);
	if (int_stat == 0) {
		dev_err(bmdi->cinfo.device, "---i2c Check idle fail, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n",
			int_stat, bmdi->dev_index ,i2c_index);
		bm_i2c_recovery(bmdi, i2c_index);
		return -1;
	}

	return 0;
}


int bm_mcu_read_reg(struct bm_device_info *bmdi, u8 cmd, u8 *data)
{
	int ret;
	u32 i2c_index = 0x1;

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x17);
	ret = bm_i2c_read_byte(bmdi, i2c_index, cmd, data);
	if (ret)
		pr_err("bm-sophon%d, mcu read byte failed, ret = 0x%x\n", bmdi->dev_index, ret);
	return ret;
}

int bm_mcu_send_u32(struct bm_device_info *bmdi, u8 cmd, u32 data)
{
	u8 buf[4];
	int ret = 0;
	int i = 0;
	int i2c_index = 0x1;

	buf[0] = (data >> 24) & 0xff;
	buf[1] = (data >> 16) & 0xff;
	buf[2] = (data >> 8) & 0xff;
	buf[3] = data & 0xff;

	for (i = 0; i < 4; i++) {
		ret = bm_i2c_write_byte(bmdi, i2c_index, cmd + i, buf[i]);
		if (ret)
			return ret;
		udelay(300);
	}
	return ret;
}


static int bm_mcu_check_stage(struct bm_device_info *bmdi, int stage)
{
	u8 data = 0;
	int ret = 0;
	int cnt = 0;
	u32 i2c_index = 0x1;

	if (stage == STAGE_UPGRADER) {
		cnt = 10;
		do {
			ret = bm_i2c_read_byte(bmdi, i2c_index, REG_STAGE, &data);
			if (ret) {
				pr_err("i2c bus cannot access\n");
				return ret;
			}
			if (data != STAGE_UPGRADER) {
				pr_info("not in upgrader stage try to enter, stage %d\n", data);
				bm_i2c_write_byte(bmdi, i2c_index, REG_CMD, CMD_MCU_UPDATE);
				msleep(1000);
				cnt--;
			} else {
				break;
			}
		} while (cnt);

		if (cnt == 0) {
			pr_err("cannot enter to stage %d\n", stage);
			return -1;
		} else {
			return 0;
		}
	} else {
		pr_err("unsupported stage.\n");
		return -1;
	}
}

static int bm_mcu_send_page(struct bm_device_info *bmdi, u8 *buf, u32 offset)
{
	int ret = 0;
	int i = 0;
	u32 i2c_index = 0x1;

	mutex_lock(&bmdi->c_attr.attr_mutex);

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x17);

	if (bm_mcu_check_stage(bmdi, STAGE_UPGRADER)) {
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}

	if (bm_mcu_send_u32(bmdi, REG_OFFSET, offset)) {
		pr_err("mcu send u32 reg_offset %d failed\n", offset);
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}

	for (i = 0; i < 128; ++i) {
		ret = bm_i2c_write_byte(bmdi, i2c_index, REG_DATA + i, *(buf + i));
		if (ret) {
			pr_err("%dth data failed\n", i);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}
		udelay(200);
	}
	mutex_unlock(&bmdi->c_attr.attr_mutex);

	mdelay(15); // wait for flash write page done

	return 0;
}

int bm_mcu_program(struct bm_device_info *bmdi, void *buf, size_t size, size_t offset)
{
	unsigned long nbytes, burn_size, percentage;
	u8 tmp[128];

	if (offset & (128 - 1)) {
		pr_err("program offset must 128 byte aligned\n");
		return -1;
	}

	pr_info("program flash offset 0x%08lx length 0x%08lx\n", offset, size);

	for (nbytes = 0; nbytes < size; nbytes += 128) {
		memset(tmp, 0x0, 128);
		burn_size = size - nbytes >= 128 ? 128 : size - nbytes;
		memcpy(tmp, ((u8 *)buf) + nbytes, burn_size);
		if (bm_mcu_send_page(bmdi, tmp, offset + nbytes))
			return -1;
		percentage = ((nbytes + 128) * 100 / size);
		percentage = percentage > 100 ? 100 : percentage;
		if (percentage % 10 == 0)
			pr_info("program page, offset 0x%08lx, %lu%%\n",
				offset + nbytes, percentage);
	}
	return 0;
}

int bm_mcu_checksum(struct bm_device_info *bmdi, u32 offset, u32 len, void *cksum)
{
	unsigned char calc_cksum[16];
	int ret = 0;
	int i = 0;
	u32 i2c_index = 0x1;

	mutex_lock(&bmdi->c_attr.attr_mutex);

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x17);

	if (bm_mcu_check_stage(bmdi, STAGE_UPGRADER)) {
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}

	// start offset
	if (bm_mcu_send_u32(bmdi, REG_CKSUM_OFF, offset)) {
		pr_err("mcu send u32 reg_cksum_offset %d failed\n", offset);
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}
	// calculated length
	if (bm_mcu_send_u32(bmdi, REG_CKSUM_LEN, len)) {
		pr_err("mcu send u32 reg_cksum_len %d failed\n", len);
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}
	// start calculation
	ret = bm_i2c_write_byte(bmdi, i2c_index, REG_CALC_CKSUM, 1);
	if (ret) {
		pr_err("cannot require checksum calculation\n");
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}

	mdelay(1000); // wait for checksum calculation

	memset(calc_cksum, 0x0, sizeof(calc_cksum));
	for (i = 0; i < 16; i++) {
		if (bm_i2c_read_byte(bmdi, i2c_index, REG_CKSUM + i, &calc_cksum[i])) {
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}
	}
	mutex_unlock(&bmdi->c_attr.attr_mutex);
	memcpy(cksum, calc_cksum, 16);
	return 0;
}

int bm_mcu_read(struct bm_device_info *bmdi, u8 *buf, size_t len, size_t offset)
{
	u32 i, j;
	u32 i2c_index = 0x1;

	pr_info("reading mcu ...\n");
	mutex_lock(&bmdi->c_attr.attr_mutex);

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x17);

	if (bm_mcu_check_stage(bmdi, STAGE_UPGRADER)) {
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -1;
	}
	for (i = offset, j = 0; j < len; j++, i++) {
		if (i % 128 == 0) {
			bm_mcu_send_u32(bmdi, REG_OFFSET, i);
		}

		if (bm_i2c_read_byte(bmdi, i2c_index, REG_DATA + (i & 127), &buf[j])) {
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			pr_err("mcu i2c read byte failed!\n");
			return -1;
		}
	}
	mutex_unlock(&bmdi->c_attr.attr_mutex);
	return 0;
}

#ifndef SOC_MODE
int bm_i2c_read_slave(struct bm_device_info *bmdi, unsigned long arg)
{
	struct bm_i2c_param i2c_buf;
	u8 temps = 0;
	int ret = 0;
	u32 i2c_index = 0;

	if (copy_from_user(&i2c_buf, (struct bm_i2c_param __user *)arg, sizeof(struct bm_i2c_param)))
		return -EFAULT;

	mutex_lock(&bmdi->c_attr.attr_mutex);
	if (i2c_buf.i2c_index == 1)
		i2c_index = 0x1;

	bm_i2c_set_target_addr(bmdi, i2c_index, i2c_buf.i2c_slave_addr);
	ret = bm_i2c_read_byte(bmdi, i2c_index, i2c_buf.i2c_cmd, &temps);
	if (ret) {
		printk("i2c read value failed!\n");
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -EFAULT;
	}

	i2c_buf.value = temps;
	mutex_unlock(&bmdi->c_attr.attr_mutex);

	if (copy_to_user((struct bm_i2c_param __user *)arg, &i2c_buf, sizeof(struct bm_i2c_param)))
		return -EFAULT;

	return 0;
}

int bm_i2c_write_slave(struct bm_device_info *bmdi, unsigned long arg)
{
	struct bm_i2c_param i2c_buf;
	int ret = 0;
	u32 i2c_index = 0;

	if (copy_from_user(&i2c_buf, (struct bm_i2c_param __user *)arg, sizeof(struct bm_i2c_param)))
		return -EFAULT;

	mutex_lock(&bmdi->c_attr.attr_mutex);
	if (i2c_buf.i2c_index == 1) {
		i2c_index = 1;

		ret = bm_set_eeprom_reg(bmdi, i2c_buf.i2c_cmd, i2c_buf.value);
		if (ret) {
			printk("i2c1 write value failed!\n");
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -EFAULT;
		}
	} else  {
		printk("currently only supports i2c1 writing\n");
		mutex_unlock(&bmdi->c_attr.attr_mutex);
		return -EFAULT;
	}
	mutex_unlock(&bmdi->c_attr.attr_mutex);

	if (copy_to_user((struct bm_i2c_param __user *)arg, &i2c_buf, sizeof(struct bm_i2c_param)))
		return -EFAULT;

	return 0;
}

int bm_i2c_smbus_access(struct bm_device_info *bmdi, unsigned long arg)
{
	struct bm_i2c_smbus_ioctl_info i2c_smbus_buf;
	u16 cmd_data;
	u32 int_stat;
	u32 bitdate;
	int i = 0;
	u32 rx_level;
	u32 tx_level;
	u8 i2c_index = 0;

	if (copy_from_user(&i2c_smbus_buf, (struct bm_i2c_smbus_ioctl_info __user *)arg, sizeof(struct bm_i2c_smbus_ioctl_info)))
		return -EFAULT;

	mutex_lock(&bmdi->c_attr.attr_mutex);

	if(i2c_smbus_buf.i2c_index == 1)
		i2c_index = 0x1;
	else
		i2c_index = 0x0;

	rx_level = 0;
	tx_level = 0;
	bmdrv_i2c_init(bmdi, i2c_index, rx_level, tx_level, i2c_smbus_buf.i2c_slave_addr);

	if (i2c_smbus_buf.op == 1){
		cmd_data = i2c_smbus_buf.i2c_cmd;
		i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);
		bitdate = DW_I2C_CMD_DATA_READ_BIT;
		for (i = 0; i < i2c_smbus_buf.length; i++)
			i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, bitdate);

		// if (bmdi->cinfo.chip_id == 0x1686) {
		// 	bitdate |= DW_I2C_CMD_DATA_STOP_BIT;
		// }
		// i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, bitdate);

		for (i = 0; i < i2c_smbus_buf.length; i++){
			int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_RX_FULL, I2C_TIME_OUT);
			if (int_stat != 0) {
				cmd_data = i2c_reg_read(bmdi, i2c_index, I2C_DATA_CMD);
				i2c_smbus_buf.data[i] = (u8)cmd_data;
			}else {
				dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
					int_stat, bmdi->dev_index , i2c_index);
				bm_i2c_recovery(bmdi, i2c_index);
				mutex_unlock(&bmdi->c_attr.attr_mutex);
				return -1;
			}
		}

		int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
		if (int_stat == 0) {
			dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
			int_stat, bmdi->dev_index ,i2c_smbus_buf.i2c_index);
			bm_i2c_recovery(bmdi, i2c_index);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}

		int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 0x1, I2C_TIME_OUT);
		if (int_stat == 0) {
			dev_err(bmdi->cinfo.device, "---i2c Check idle fail, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n",
				int_stat, bmdi->dev_index, i2c_index);
			bm_i2c_recovery(bmdi, i2c_index);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}

	} else{
		cmd_data = i2c_smbus_buf.i2c_cmd;
		i2c_reg_write(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

		int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
		if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {

			dev_err(bmdi->cinfo.device, "cmd not slave acknowledge\n");
			dev_err(bmdi->cinfo.device, "---i2c Tx Aborted, intr stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x, abt reason: 0x%08x ===\n",
				int_stat, bmdi->dev_index ,i2c_index, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
			bm_i2c_recovery(bmdi, i2c_index);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}

		int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
		if (int_stat == 0) {
				dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
					int_stat, bmdi->dev_index ,i2c_index);
				bm_i2c_recovery(bmdi, i2c_index);
				mutex_unlock(&bmdi->c_attr.attr_mutex);
				return -1;
        }

		for(i = 0; i < i2c_smbus_buf.length; i++){
			cmd_data = i2c_smbus_buf.data[i];
			i2c_reg_write_byte(bmdi, i2c_index, I2C_DATA_CMD, cmd_data);

			int_stat = bm_check_i2c_reg_bits(bmdi, i2c_index, I2C_RAW_INT_STAT, DW_I2C_INT_TX_EMPTY, I2C_TIME_OUT);
        	if (int_stat == 0) {
				dev_err(bmdi->cinfo.device, "---i2c Tx Timeout, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x\n",
						int_stat, bmdi->dev_index ,i2c_index);
				bm_i2c_recovery(bmdi, i2c_index);
				mutex_unlock(&bmdi->c_attr.attr_mutex);
				return -1;
			}

		}

		int_stat = bm_check_i2c_reg_bits_zero(bmdi, i2c_index, I2C_STATUS, 1 << 0, I2C_TIME_OUT);
		if (int_stat == 0) {
			dev_err(bmdi->cinfo.device, "---i2c Check idle fail, int stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x ---\n",
					int_stat, bmdi->dev_index ,i2c_index);
			bm_i2c_recovery(bmdi, i2c_index);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}

		int_stat = i2c_reg_read(bmdi, i2c_index, I2C_RAW_INT_STAT);
		if (int_stat & DW_I2C_INT_TX_ABRT_RAW) {
			dev_err(bmdi->cinfo.device, "---i2c Tx Aborted, intr stat: 0x%08x, bmdi->dev_index = 0x%08x, i2c_index = 0x%08x, abt reason: 0x%08x ===\n",
					int_stat, bmdi->dev_index ,i2c_index, i2c_reg_read(bmdi, i2c_index, I2C_TX_ABRT_SOURCE));
			bm_i2c_recovery(bmdi, i2c_index);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			return -1;
		}

	}

	mutex_unlock(&bmdi->c_attr.attr_mutex);

	if (copy_to_user((struct bm_i2c_smbus_ioctl_info __user *)arg, &i2c_smbus_buf, sizeof(struct bm_i2c_smbus_ioctl_info)))
		return -EFAULT;

	return 0;
}

#endif
