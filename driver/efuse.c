#include "efuse.h"
#include <linux/delay.h>
#include <linux/device.h>
#include "bm_common.h"



static void efuse_mode_config(struct bm_device_info *bmdi, u32 val)
{
	efuse_reg_write(bmdi, EFUSE_MODE, val);
}

static void efuse_mode_wait_ready(struct bm_device_info *bmdi)
{
	int loop = 100;
	while (efuse_reg_read(bmdi, EFUSE_MODE) != 0x80) {
		if (loop-- > 0) {
			mdelay(1);
		} else {
			break;
		}
	}
}

static void efuse_set_bit(struct bm_device_info *bmdi, u32 address, u32 bit_i)
{

	const u32 address_mask = (1 << EFUSE_NUM_ADDRESS_BITS) - 1;
        u32 adr_val;

        if (address > ((1 << EFUSE_NUM_ADDRESS_BITS) - 1)) {
		pr_info("sophon %d efuse_set_bit address = 0x%x is too large\n", bmdi->dev_index, address);
                return;
	}

        efuse_reg_write(bmdi, EFUSE_MODE, 0x0);
	efuse_mode_wait_ready(bmdi);
        // embedded write
        adr_val = (address & address_mask) | ((bit_i & 0x1f) << EFUSE_NUM_ADDRESS_BITS);
        efuse_reg_write(bmdi, EFUSE_ADDR, adr_val);
        efuse_reg_write(bmdi, EFUSE_MODE, 0x3);
	efuse_mode_wait_ready(bmdi);

}

static void efuse_raw_write(struct bm_device_info *bmdi, u32 address, u32 val)
{
	int i = 0;

        for (i = 0; i < 32; i++) {
                if ((val >> i) & 1)
                        efuse_set_bit(bmdi, address, i);
        }

}

static u32 efuse_raw_read(struct bm_device_info *bmdi, u32 address)
{
	const u32 address_mask = (1 << EFUSE_NUM_ADDRESS_BITS) - 1;
        u32 adr_val;

        efuse_reg_write(bmdi, EFUSE_MODE, 0x0);
	efuse_mode_wait_ready(bmdi);

        adr_val = (address & address_mask);
        efuse_reg_write(bmdi, EFUSE_ADDR, adr_val);
	efuse_mode_config(bmdi, 0x2);

	efuse_mode_wait_ready(bmdi);
	return efuse_reg_read(bmdi, EFUSE_RD_DATA);
}

void bmdrv_efuse_write(struct bm_device_info *bmdi, u32 address, u32 val)
{
	efuse_raw_write(bmdi, address, val);
}

u32 bmdrv_efuse_read(struct bm_device_info *bmdi, u32 address)
{
	return efuse_raw_read(bmdi, address);
}

