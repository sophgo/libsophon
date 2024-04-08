#include "bm_common.h"

#include "bm_io.tmh"

void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
		u8 **bar_vaddr, u32 *offset)
{
	/* Choose bar the address belongs to, and compute the offset on bar */
	if (address >= pbar_info->bar4_dev_start &&
			address < pbar_info->bar4_dev_start + pbar_info->bar4_len) {
		*bar_vaddr = pbar_info->bar4_vaddr;
		*offset = address - (u32)pbar_info->bar4_dev_start;
	} else if (address >= pbar_info->bar2_dev_start &&
			address < pbar_info->bar2_dev_start + pbar_info->bar2_len) {
		*bar_vaddr = pbar_info->bar2_vaddr;
        *offset    = address - (u32)pbar_info->bar2_dev_start;
	} else if (address >= pbar_info->bar0_dev_start &&
			address < pbar_info->bar0_dev_start + pbar_info->bar0_len) {
		*bar_vaddr = pbar_info->bar0_vaddr;
        *offset    = address - (u32)pbar_info->bar0_dev_start;
	} else if (address >= pbar_info->bar1_dev_start &&
			address < pbar_info->bar1_dev_start + pbar_info->bar1_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_dev_start;
    } else if (address >= pbar_info->bar1_part9_dev_start &&
			address < pbar_info->bar1_part9_dev_start + pbar_info->bar1_part9_len){
    	*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part9_dev_start +
                  (u32)pbar_info->bar1_part9_offset;
    } else if (address >= pbar_info->bar1_part8_dev_start &&
			address < pbar_info->bar1_part8_dev_start + pbar_info->bar1_part8_len){
    	*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part8_dev_start +
                  (u32)pbar_info->bar1_part8_offset;
    }else if (address >= pbar_info->bar1_part7_dev_start &&
			address < pbar_info->bar1_part7_dev_start + pbar_info->bar1_part7_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part7_dev_start +
                  (u32)pbar_info->bar1_part7_offset;
	} else if (address >= pbar_info->bar1_part6_dev_start &&
			address < pbar_info->bar1_part6_dev_start + pbar_info->bar1_part6_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part6_dev_start +
                  (u32)pbar_info->bar1_part6_offset;
	} else if (address >= pbar_info->bar1_part5_dev_start &&
			address < pbar_info->bar1_part5_dev_start + pbar_info->bar1_part5_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part5_dev_start +
                  (u32)pbar_info->bar1_part5_offset;
	} else if (address >= pbar_info->bar1_part4_dev_start &&
			address < pbar_info->bar1_part4_dev_start + pbar_info->bar1_part4_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part4_dev_start +
                  (u32)pbar_info->bar1_part4_offset;
	} else if (address >= pbar_info->bar1_part3_dev_start &&
			address < pbar_info->bar1_part3_dev_start + pbar_info->bar1_part3_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part3_dev_start +
                  (u32)pbar_info->bar1_part3_offset;
	} else if (address >= pbar_info->bar1_part0_dev_start &&
			address < pbar_info->bar1_part0_dev_start + pbar_info->bar1_part0_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part0_dev_start +
                  (u32)pbar_info->bar1_part0_offset;
	} else if (address >= pbar_info->bar1_part1_dev_start &&
			address < pbar_info->bar1_part1_dev_start + pbar_info->bar1_part1_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part1_dev_start +
                  (u32)pbar_info->bar1_part1_offset;
	} else if (address >= pbar_info->bar1_part2_dev_start &&
			address < pbar_info->bar1_part2_dev_start + pbar_info->bar1_part2_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
        *offset    = address - (u32)pbar_info->bar1_part2_dev_start +
                  (u32)pbar_info->bar1_part2_offset;
	} else {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%s invalid address address = 0x%x\n", __func__, address);
	}
		//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bar addr = 0x%p, offset = 0x%x, address 0x%x\n", *bar_vaddr, *offset, address);
}

void bm_get_bar_base(struct bm_bar_info *pbar_info, u32 address, ULONG64 *base)
{
	/* Choose bar the address belongs to, and compute the offset on bar */
	if (address >= pbar_info->bar2_dev_start &&
			address < pbar_info->bar2_dev_start + pbar_info->bar2_len) {
		*base = pbar_info->bar2_start;
	} else if (address >= pbar_info->bar0_dev_start &&
			address < pbar_info->bar0_dev_start + pbar_info->bar0_len) {
		*base = pbar_info->bar0_start;
	} else if (address >= pbar_info->bar1_dev_start &&
			address < pbar_info->bar1_dev_start + pbar_info->bar1_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part6_dev_start &&
			address < pbar_info->bar1_part6_dev_start + pbar_info->bar1_part6_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part5_dev_start &&
			address < pbar_info->bar1_part5_dev_start + pbar_info->bar1_part5_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part4_dev_start &&
			address < pbar_info->bar1_part4_dev_start + pbar_info->bar1_part4_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part3_dev_start &&
			address < pbar_info->bar1_part3_dev_start + pbar_info->bar1_part3_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part0_dev_start &&
			address < pbar_info->bar1_part0_dev_start + pbar_info->bar1_part0_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part1_dev_start &&
			address < pbar_info->bar1_part1_dev_start + pbar_info->bar1_part1_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part2_dev_start &&
			address < pbar_info->bar1_part2_dev_start + pbar_info->bar1_part2_len) {
		*base = pbar_info->bar1_start;
	} else {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "pcie mode invalid address\n");
	}
}

static void *bm_get_devmem_vaddr(struct bm_device_info *bmdi, u32 address)
{
	u32 offset = 0;
	u32 *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, (u8 **)&bar_vaddr, &offset);
	return bar_vaddr + offset;
}

VOID local_iowrite64(u64 val, void *addr) {
    WRITE_REGISTER_ULONG64((u64 *)addr, val);
}

VOID local_iowrite32(u32 val, void *addr) {
    WRITE_REGISTER_ULONG((volatile ULONG*)addr, val);
}

VOID local_iowrite16(USHORT val, void *addr) {
    WRITE_REGISTER_USHORT((USHORT*)addr, val);
}

VOID local_iowrite8(UCHAR val, void *addr) {
    WRITE_REGISTER_UCHAR((UCHAR*)addr, val);
}

u32 bm_read32(struct bm_device_info *bmdi, u32 address)
{
	u32 offset = 0;
	u32 *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, (u8 **)&bar_vaddr, &offset);
	return READ_REGISTER_ULONG((volatile ULONG*)((u64)bar_vaddr + offset));
}

u32 bm_write32(struct bm_device_info *bmdi, u32 address, u32 data)
{
	u32 offset = 0;
	u32 *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, (u8 **)&bar_vaddr, &offset);
	local_iowrite32(data, (void*)((u64)bar_vaddr + offset));
	return 0;
}

UCHAR bm_read8(struct bm_device_info *bmdi, u32 address)
{
    return READ_REGISTER_UCHAR(bm_get_devmem_vaddr(bmdi, address));
}

u32 bm_write8(struct bm_device_info *bmdi, u32 address, UCHAR data)
{
	local_iowrite8(data, bm_get_devmem_vaddr(bmdi, address));
	return 0;
}

USHORT bm_read16(struct bm_device_info *bmdi, u32 address)
{
	return READ_REGISTER_USHORT(bm_get_devmem_vaddr(bmdi, address));
}

u32 bm_write16(struct bm_device_info *bmdi, u32 address, USHORT data)
{
	local_iowrite16(data, bm_get_devmem_vaddr(bmdi, address));
	return 0;
}

ULONG64 bm_read64(struct bm_device_info *bmdi, u32 address)
{
	ULONG64 temp = 0;

	temp = bm_read32(bmdi, address + 4);
	temp = (ULONG64)bm_read32(bmdi, address) | (temp << 32);
	return temp;
}

ULONG64 bm_write64(struct bm_device_info *bmdi, u32 address, ULONG64 data)
{
	bm_write32(bmdi, address, data & 0xFFFFFFFF);
	bm_write32(bmdi, address + 4, data >> 32);
	return 0;
}

void bm_reg_init_vaddr(struct bm_device_info *bmdi, u32 address, u8 **reg_base_vaddr)
{
	u32 offset = 0;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	if (address == 0) {
		*reg_base_vaddr = NULL;
		return;
	}
	bm_get_bar_offset(pbar_info, address, reg_base_vaddr, &offset);
    (U8 *) *reg_base_vaddr += offset;
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"%!FUNC! device address = 0x%x, vaddr = 0x%llx\n", address, (u64)*reg_base_vaddr);
}

/* Define register operation as a generic marco. it provide
 * xxx_reg_read/write wrapper, currently support read/write u32,
 * using bm_read/write APIs directly for access USHORT/UCHAR.
 */
void mcu_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.mcu_info_bar_vaddr + reg_offset);
}

u32 mcu_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return READ_REGISTER_ULONG((volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.mcu_info_bar_vaddr + reg_offset));
}

void dev_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int len)
{
	int i, j;
	char *p = (CHAR *)&val;
    char *addr = NULL;

	for (i = len - 1, j = 0; i >= 0; i--) {
        addr = (char *)((u64)(bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr) + reg_offset + j++);
        local_iowrite8(*(p + i), addr);
    }
}

UCHAR dev_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_UCHAR(
        (bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset));
}

void shmem_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, u32 channel)
{
    u64 addr = (u64)channel * (bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM) * 4;
    local_iowrite32(val,bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr + addr + reg_offset);
}

u32 shmem_reg_read(struct bm_device_info *bmdi, u32 reg_offset, u32 channel)
{
    u64 addr = (u64)channel *(bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM) * 4;
    return READ_REGISTER_ULONG(
        (volatile ULONG*)(bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr + addr +
        reg_offset));
}

void top_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr + reg_offset);
}

u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr +
        reg_offset));
}

void gp_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr + reg_offset);
}

u32 gp_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr +
        reg_offset));
}

//void i2c_reg_write(struct bm_device_info *bmdi, u32 i2c_index, u32 reg_offset, u32 val)
void i2c_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr + reg_offset);
	//local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr + reg_offset + i2c_index*0x2000);
}

//u32 i2c_reg_read(struct bm_device_info *bmdi, u32 i2c_index, u32 reg_offset)
u32 i2c_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr + reg_offset));
	// return READ_REGISTER_ULONG(
	//         (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr +
	//         reg_offset + i2c_index*0x2000));
}

/* smbus is i2c0 master in bm1684 */
void smbus_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr - 0x2000 + reg_offset);
}

u32 smbus_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr -
        0x2000 + reg_offset));
}

void pwm_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr + reg_offset);
}

u32 pwm_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr +
        reg_offset));
}

void cdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.cdma_bar_vaddr + reg_offset);
}

u32 cdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.cdma_bar_vaddr +
        reg_offset));
}

void ddr_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr + reg_offset);
}

u32 ddr_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr +
        reg_offset));
}

void bdc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr + reg_offset);
}

u32 bdc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr +
        reg_offset));
}

void smmu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr + reg_offset);
}

u32 smmu_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr +
        reg_offset));
}

void intc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.intc_bar_vaddr + reg_offset);
}

u32 intc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.intc_bar_vaddr +
        reg_offset));
}

void cfg_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.cfg_bar_vaddr + reg_offset);
}

u32 cfg_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.cfg_bar_vaddr +
        reg_offset));
}
void nv_timer_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr + reg_offset);
}

u32 nv_timer_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr +
        reg_offset));
}

void spi_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.spi_bar_vaddr + reg_offset);
}

u32 spi_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.spi_bar_vaddr +
        reg_offset));
}

void gpio_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gpio_bar_vaddr + reg_offset);
}

u32 gpio_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.gpio_bar_vaddr +
        reg_offset));
}

void vpp0_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.vpp0_bar_vaddr + reg_offset);
}

u32 vpp0_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.vpp0_bar_vaddr +
        reg_offset));
}

void vpp1_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.vpp1_bar_vaddr + reg_offset);
}

u32 vpp1_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.vpp1_bar_vaddr +
        reg_offset));
}

void uart_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.uart_bar_vaddr + reg_offset);
}

u32 uart_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.uart_bar_vaddr +
        reg_offset));
}

void wdt_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.wdt_bar_vaddr + reg_offset);
}

u32 wdt_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.wdt_bar_vaddr +
        reg_offset));
}

void tpu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr + reg_offset);
}

u32 tpu_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr +
        reg_offset));
}

void gdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr + reg_offset);
}

u32 gdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr +
        reg_offset));
}

void spacc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.spacc_bar_vaddr + reg_offset);
}

u32 spacc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.spacc_bar_vaddr +
        reg_offset));
}

void pka_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.pka_bar_vaddr + reg_offset);
}

u32 pka_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.pka_bar_vaddr +
        reg_offset));
}

void efuse_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	local_iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.efuse_bar_vaddr + reg_offset);
}

u32 efuse_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return READ_REGISTER_ULONG(
        (volatile ULONG *)(bmdi->cinfo.bar_info.io_bar_vaddr.efuse_bar_vaddr +
        reg_offset));
}


/*set module's vaddr acording to phyaddr*/
void io_init(struct bm_device_info *bmdi)
{
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->shmem_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->top_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gp_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->pwm_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->cdma_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.cdma_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->bdc_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->smmu_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->intc_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.intc_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->nv_timer_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gpio_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gpio_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->vpp0_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.vpp0_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->vpp1_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.vpp1_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->uart_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.uart_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->wdt_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.wdt_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->spacc_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.spacc_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->pka_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.pka_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->efuse_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.efuse_bar_vaddr);

#ifndef SOC_MODE
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->dev_info_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->i2c_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->ddr_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->spi_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.spi_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->mcu_info_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.mcu_info_bar_vaddr);
#endif
}

int bm_get_reg(struct bm_device_info *bmdi, struct bm_reg *reg)
{
	reg->reg_value = bm_read32(bmdi, reg->reg_addr);
	return 0;
}

int bm_set_reg(struct bm_device_info *bmdi, struct bm_reg *reg)
{
#ifdef PR_DEBUG
	bm_write32(bmdi, reg->reg_addr, reg->reg_value);
#else
    UNREFERENCED_PARAMETER(bmdi);
    UNREFERENCED_PARAMETER(reg);
#endif
	return 0;
}
