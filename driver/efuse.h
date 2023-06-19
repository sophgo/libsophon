#include "bm_common.h"

#define EFUSE_MODE                0x0
#define EFUSE_ADDR                0x4
#define EFUSE_RD_DATA             0xc

#define EFUSE_MODE_MASK           0x3

#define EFUSE_MODE_RDY            (0x1 << 7)
#define EFUSE_NUM_ADDRESS_BITS    7

void bmdrv_efuse_write(struct bm_device_info *bmdi, u32 address, u32 val);
u32 bmdrv_efuse_read(struct bm_device_info *bmdi, u32 address);
