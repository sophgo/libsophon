#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include "84x6_reg.h"
#include "84x6_card.h"
#include "bm_io.h"
#include "bm_pcie.h"
#include "bm_common.h"
#include "bm_card.h"
#include "bm_memcpy.h"

/* forward declarations */
static int bm84x6_pcie_config_port_code(struct bm_device_info *bmdi, int func);
static void bm84x6_pcie_config_upstream_port(struct bm_device_info *bmdi);
static inline u64 bm84x6_ep_iatu_addr(int chip, int func, int bar);
static void bm84x6_pcie_check_boot_from_pcie(struct bm_device_info *bmdi);

static const struct bm_bar_info bm84x6_pcie_x8_0_bar_layout[] = {
    {
     .bar0_len = 0x400000,
     .bar0_dev_start = BM84X6_PCIE_DBI_X8_0,  // PCIE_x8_0 base addr

     .bar1_len = 0x400000,
     .bar1_dev_start = 0x00000000,
     .bar1_part_info[0] = { 0x000000, 0x004000, 0x28100000},
     .bar1_part_info[1] = { 0x004000, 0x004000, 0x21300000}, // x4_0 ctrl
     .bar1_part_info[2] = { 0x008000, 0x004000, 0x21308000}, // x8_0 ctrl
     .bar1_part_info[3] = { 0x00C000, 0x010000, 0x21790000}, // pcie_sub_cfg
     .bar1_part_info[4] = { 0x01C000, 0x001000, 0x217a0000}, // msi cfg
     /* C2C topology matrix window: BAR1 off 0x1D000 -> dev 0x1004000000 */
     .bar1_part_info[5] = { 0x01D000, 0x004000, 0x1004000000},
     .bar1_part_info[6] = { 0x021000, 0x004000, 0x20b00000}, // x4_0 atu
     .bar1_part_info[7] = { 0x025000, 0x001000, 0x20800000}, // x4_0 dbi

     .bar2_len = 0x4000,
     .bar2_dev_start = 0,

     .bar4_len = 0x100000,
     .bar4_dev_start = 0,
    },
    {
     .bar0_len = 0x400000,
     .bar0_dev_start = BM84X6_PCIE_DBI_X8_0,  // PCIE_x8_0 base addr

     .bar1_len = 0x400000,
     .bar1_dev_start = 0x00000000,
     .bar1_part_info[0] = { 0x000000, 0x004000, 0x28100000},
     .bar1_part_info[1] = { 0x004000, 0x004000, 0x21300000},
     .bar1_part_info[2] = { 0x008000, 0x044000, 0x270D0000},
     .bar1_part_info[3] = { 0x04C000, 0x010000, 0x21790000},
     .bar1_part_info[4] = { 0x05C000, 0x001000, 0x217a0000},
     .bar1_part_info[5] = { 0x05D000, 0x001000, 0x27090000},
     .bar1_part_info[6] = { 0x05E000, 0x002000, 0x05025000},
     .bar1_part_info[7] = { 0x060000, 0x001000, 0x21000000},
     /* C2C topology matrix window: BAR1 off 0x61000 -> dev 0x1004000000 */
     .bar1_part_info[8] = { 0x061000, 0x004000, 0x1004000000},
     /* TPU_SYS_REG (gating/soft-reset ctrl): BAR1 off 0x65000 -> dev 0x26d09000 */
     .bar1_part_info[9] = { 0x065000, 0x001000, 0x26d09000},
     /* per-core C906 windows (start_c906): TPU_REG + SCH_FIFO/TXP_SYS(incl. RVBA) */
     .bar1_part_info[10] = { 0x066000, 0x001000, 0x24410000}, // core0 TPU_REG
     .bar1_part_info[11] = { 0x067000, 0x00a000, 0x24500000}, // core0 SCH_FIFO + TXP_SYS
     .bar1_part_info[12] = { 0x071000, 0x001000, 0x24c10000}, // core1 TPU_REG
     .bar1_part_info[13] = { 0x072000, 0x00a000, 0x24d00000}, // core1 SCH_FIFO + TXP_SYS
     .bar1_part_info[14] = { 0x07c000, 0x001000, 0x25410000}, // core2 TPU_REG
     .bar1_part_info[15] = { 0x07d000, 0x00a000, 0x25500000}, // core2 SCH_FIFO + TXP_SYS
     .bar1_part_info[16] = { 0x087000, 0x001000, 0x25c10000}, // core3 TPU_REG
     .bar1_part_info[17] = { 0x088000, 0x00a000, 0x25d00000}, // core3 SCH_FIFO + TXP_SYS
     /* TSH cfg/done block (TSH_RST_CLK..TSH_DISABLE_CORE, TSH_HWQx_DONE_CNT,
      * TSH_DONE, TPUSYS_MSG): BAR1 off 0x092000 -> dev 0x26d06000.
      * iATU index == part index; 19-21 are free (18 = BAR4->C2C, 22-30 multi-function). */
     .bar1_part_info[19] = { 0x092000, 0x003000, 0x26d06000},
     /* host<->fw shared mem (API packages, 256*4KB): BAR1 off 0x095000 -> dev 0x24000000 */
     .bar1_part_info[20] = { 0x095000, 0x100000, 0x24000000},

     .bar2_len = 0x4000,
     .bar2_dev_start = 0,

     .bar4_len = 0x100000,
     .bar4_dev_start = 0,
    }
};

static const struct bm_bar_info bm84x6_pcie_x4_0_bar_layout[] = {
    {
     .bar0_len = 0x400000,
     .bar0_dev_start = 0x20800000,  //PCIE4 base addr

     .bar1_len = 0x400000,
     .bar1_dev_start = 0x00000000,
     .bar1_part_info[0] = { 0x000000, 0x004000, 0x28100000}, // top
     .bar1_part_info[1] = { 0x004000, 0x004000, 0x21308000}, // x8_0 ctrl
     .bar1_part_info[2] = { 0x008000, 0x004000, 0x21300000}, // x4_0 ctrl
     .bar1_part_info[3] = { 0x00C000, 0x010000, 0x21790000}, // pcie_sub_cfg
     .bar1_part_info[4] = { 0x01C000, 0x001000, 0x217a0000}, // msi cfg
     /* C2C topology matrix window: BAR1 off 0x1D000 -> dev 0x1004000000 */
     .bar1_part_info[5] = { 0x01D000, 0x004000, 0x1004000000},

     .bar1_part_info[6] = { 0x021000, 0x004000, 0x20300000}, // x8_0 atu
     .bar1_part_info[7] = { 0x025000, 0x001000, 0x20000000}, // x8_0 dbi

     .bar2_len = 0x400000,
     .bar2_dev_start = 0,
     .bar2_part_info[0] = { 0x0, 0x0, 0x0},

     .bar4_len = 0x100000,
     .bar4_dev_start = 0,
      },
    {
     .bar0_len = 0x400000,
     .bar0_dev_start = 0x20800000,  //PCIE4 base addr

     .bar1_len = 0x400000,
     .bar1_dev_start = 0x00000000,
     .bar1_part_info[0] = { 0x000000, 0x004000, 0x28100000},
     .bar1_part_info[1] = { 0x004000, 0x004000, 0x21300000},
     .bar1_part_info[2] = { 0x008000, 0x044000, 0x270D0000},
     .bar1_part_info[3] = { 0x04C000, 0x010000, 0x21790000},
     .bar1_part_info[4] = { 0x05C000, 0x001000, 0x217a0000},
     .bar1_part_info[5] = { 0x05D000, 0x001000, 0x27090000},
     .bar1_part_info[6] = { 0x05E000, 0x002000, 0x05025000},
     .bar1_part_info[7] = { 0x060000, 0x001000, 0x21000000},
     /* C2C topology matrix window: BAR1 off 0x61000 -> dev 0x1004000000 */
     .bar1_part_info[8] = { 0x061000, 0x004000, 0x1004000000},
     /* TPU_SYS_REG (gating/soft-reset ctrl): BAR1 off 0x65000 -> dev 0x26d09000 */
     .bar1_part_info[9] = { 0x065000, 0x001000, 0x26d09000},
     /* per-core C906 windows (start_c906): TPU_REG + SCH_FIFO/TXP_SYS(incl. RVBA) */
     .bar1_part_info[10] = { 0x066000, 0x001000, 0x24410000}, // core0 TPU_REG
     .bar1_part_info[11] = { 0x067000, 0x00a000, 0x24500000}, // core0 SCH_FIFO + TXP_SYS
     .bar1_part_info[12] = { 0x071000, 0x001000, 0x24c10000}, // core1 TPU_REG
     .bar1_part_info[13] = { 0x072000, 0x00a000, 0x24d00000}, // core1 SCH_FIFO + TXP_SYS
     .bar1_part_info[14] = { 0x07c000, 0x001000, 0x25410000}, // core2 TPU_REG
     .bar1_part_info[15] = { 0x07d000, 0x00a000, 0x25500000}, // core2 SCH_FIFO + TXP_SYS
     .bar1_part_info[16] = { 0x087000, 0x001000, 0x25c10000}, // core3 TPU_REG
     .bar1_part_info[17] = { 0x088000, 0x00a000, 0x25d00000}, // core3 SCH_FIFO + TXP_SYS
     /* TSH cfg/done block (TSH_RST_CLK..TSH_DISABLE_CORE, TSH_HWQx_DONE_CNT,
      * TSH_DONE, TPUSYS_MSG): BAR1 off 0x092000 -> dev 0x26d06000.
      * iATU index == part index; 19-21 are free (18 = BAR4->C2C, 22-30 multi-function). */
     .bar1_part_info[19] = { 0x092000, 0x003000, 0x26d06000},
     /* host<->fw shared mem (API packages, 256*4KB): BAR1 off 0x095000 -> dev 0x24000000 */
     .bar1_part_info[20] = { 0x095000, 0x100000, 0x24000000},

     .bar2_len = 0x4000,
     .bar2_dev_start = 0,
     .bar2_part_info[0] = { 0x0, 0x0, 0x0},

     .bar4_len = 0x100000,
     .bar4_dev_start = 0,
      },
};

void bm84x6_pcie_get_outbound_base(struct bm_device_info *bmdi)
{
    u32 ctrl_sel = (bmdi->cinfo.mode >> BM84X6_PCIE_EP_SEL_SHIFT) & BM84X6_PCIE_EP_SEL_MASK;
    u32 route_id = ctrl_sel + 1;

    bmdi->cinfo.ob_base = 0xe000000 | (route_id << 28);
}

static void bm84x6_debug_dump_atu(struct bm_device_info *bmdi)
{
    struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
    void __iomem *atu_base = bari->bar0_vaddr + BM84X6_OFFSET_PCIE_iATU;
    void __iomem *c2c = bari->bar1_vaddr + bari->bar1_part_info[3].offset;
    int i;
    u32 ctrl2;

    pr_info("===== ATU DUMP =====\n");

    /* SNPS inbound iATU */
    pr_info("--- SNPS IB iATU (non-zero) ---\n");
    for (i = 0; i < 31; i++) {
        ctrl2 = REG_READ32(atu_base, PCIE_IB_IATU_REGION_CTRL_2(i));
        if (ctrl2 == 0)
            continue;
        pr_info(" IB[%2d]: ctrl1=0x%08x ctrl2=0x%08x "
                "base=0x%08x_%08x limit=0x%08x_%08x "
                "target=0x%08x_%08x\n",
                i,
                REG_READ32(atu_base, PCIE_IB_IATU_REGION_CTRL_1(i)),
                ctrl2,
                REG_READ32(atu_base, PCIE_IB_IATU_UPPER_BASE_ADDR(i)),
                REG_READ32(atu_base, PCIE_IB_IATU_LWR_BASE_ADDR(i)),
                REG_READ32(atu_base, PCIE_IB_IATU_UPPR_LIMIT_ADDR(i)),
                REG_READ32(atu_base, PCIE_IB_IATU_LIMIT_ADDR(i)),
                REG_READ32(atu_base, PCIE_IB_IATU_UPPER_TARGET_ADDR(i)),
                REG_READ32(atu_base, PCIE_IB_IATU_LWR_TARGET_ADDR(i)));
    }

    /* SNPS outbound iATU */
    pr_info("--- SNPS OB iATU (non-zero) ---\n");
    for (i = 0; i < 31; i++) {
        ctrl2 = REG_READ32(atu_base, PCIE_OB_IATU_REGION_CTRL_2(i));
        if (ctrl2 == 0)
            continue;
        pr_info(" OB[%2d]: ctrl1=0x%08x ctrl2=0x%08x "
                "base=0x%08x_%08x limit=0x%08x_%08x "
                "target=0x%08x_%08x\n",
                i,
                REG_READ32(atu_base, PCIE_OB_IATU_REGION_CTRL_1(i)),
                ctrl2,
                REG_READ32(atu_base, PCIE_OB_IATU_UPPER_BASE_ADDR(i)),
                REG_READ32(atu_base, PCIE_OB_IATU_LWR_BASE_ADDR(i)),
                REG_READ32(atu_base, PCIE_OB_IATU_UPPR_LIMIT_ADDR(i)),
                REG_READ32(atu_base, PCIE_OB_IATU_LIMIT_ADDR(i)),
                REG_READ32(atu_base, PCIE_OB_IATU_UPPER_TARGET_ADDR(i)),
                REG_READ32(atu_base, PCIE_OB_IATU_LWR_TARGET_ADDR(i)));
    }

    /* C2C inbound ATU */
    pr_info("--- C2C IB ATU ---\n");
    for (i = 0; i < 4; i++) {
        pr_info(" C2C_IB[%d]: up=0x%08x low=0x%08x ctrl=0x%08x dst=0x%08x\n",
                i,
                REG_READ32(c2c, C2C_IB_ATU_ADDR_UP(i)),
                REG_READ32(c2c, C2C_IB_ATU_ADDR_LOW(i)),
                REG_READ32(c2c, C2C_IB_ATU_CTRL(i)),
                REG_READ32(c2c, C2C_IB_ATU_DST_ADDR(i)));
    }
    pr_info(" C2C_IB_SEL = 0x%08x\n", REG_READ32(c2c, C2C_IB_ATU_SEL));

    /* C2C outbound ATU to PC */
    pr_info("--- C2C OB ATU to PC ---\n");
    for (i = 0; i < 5; i++) {
        pr_info(" C2C_OB_PC[%d]: up=0x%08x low=0x%08x ctrl=0x%08x\n",
                i,
                REG_READ32(c2c, C2C_OB_ATU_PC_ADDR_UP(i)),
                REG_READ32(c2c, C2C_OB_ATU_PC_ADDR_LOW(i)),
                REG_READ32(c2c, C2C_OB_ATU_PC_CTRL(i)));
    }

    pr_info("===== ATU DUMP END =====\n");
}

void bm84x6_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
    struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
    void __iomem *cfg_base_addr = bari->bar0_vaddr;
    void __iomem *atu_base_addr;
    void __iomem *c2c_us;
    u64 base_addr = 0;
    u64 bar1_start = 0;
    int function_num = 0;
    int max_function_num = 0x0;
    int chip;
    u32 pcie_timeout_config = 0x0;
    u32 value;
    u16 index;
    u64 bar0, bar1, bar4;

    /* [0] determine function number */
    if (bmdi->cinfo.pcie_func_index > 0) {
        function_num = bmdi->cinfo.pcie_func_index;
    } else {
        function_num = (pdev->devfn & 0x7);
    }

    /* [1] DBI reads must happen before unmap — BAR0 access at offset 0x14
     *     may not work after iATU is cleared.
     */
    if (function_num == 0x0) {
        bar1_start = REG_READ32(cfg_base_addr, 0x14) & ~0xf;
    } else {
        bar1_start = 0x80400000;
    }

    /* [2] DBI timeout config (BAR4 resize moved to firmware) */
    REG_WRITE32(bari->bar0_vaddr, 0x8bc,
                (REG_READ32(bari->bar0_vaddr, 0x8bc) | 0x1));
    pcie_timeout_config = REG_READ32(bari->bar0_vaddr, 0x98);
    pcie_timeout_config &= ~0xf;
    pcie_timeout_config |= 0xa;
    REG_WRITE32(bari->bar0_vaddr, 0x98, pcie_timeout_config);
    REG_WRITE32(bari->bar0_vaddr, 0x8bc,
                (REG_READ32(bari->bar0_vaddr, 0x8bc) & (~0x1)));

    /* [3] clear stale iATU entries from warm reset / reload */
    bm84x6_unmap_bar(bmdi);

    atu_base_addr = bari->bar0_vaddr + BM84X6_OFFSET_PCIE_iATU;

    /* [4] INBOUND: BAR1 per-IP windows (SETUP layout, single inbound pass).
     *     Only write entries with non-zero length to avoid polluting iATU.
     */
    for (index = 0; index < PCIE_BAR1_PART_MAX; index++) {
        if (bari->bar1_part_info[index].len == 0)
            continue;
        base_addr = bar1_start + bari->bar1_part_info[index].offset;
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_BASE_ADDR(index), (u32) (base_addr & 0xffffffff));
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_BASE_ADDR(index), base_addr >> 32);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LIMIT_ADDR(index), (u32) ((base_addr & 0xffffffff) + bari->bar1_part_info[index].len - 1));
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(index), (u32) (bari->bar1_part_info[index].dev_start & 0xffffffff));
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(index), (bari->bar1_part_info[index].dev_start >> 32));
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(index), 0x80000100);
    }

    /* [5] get_mode (depends on [4] BAR1 window to top reg 0x28100004) */
    if (bm84x6_pcie_get_mode(bmdi) < 0) {
        pr_err("bm84x6_map_bar: get_mode failed\n");
        return;
    }

    /* [6] ob_base (depends on [5] cinfo.mode) */
    bm84x6_pcie_get_outbound_base(bmdi);

    /* [6b] decide whether host loads c2c/ddr fw, per trap boot_sel */
    bm84x6_pcie_check_boot_from_pcie(bmdi);

    /* [7] BAR4 -> C2C (func0 only). Use iATU index 18: BAR1 parts 0..17 own
     *     iATU 0..17; multi-function fixed indexes own 22-30.
     */
    c2c_us = bari->bar1_vaddr + bari->bar1_part_info[3].offset;
    if (function_num == 0) {
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(18), 0x0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(18), 0x24000);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(18), 0x002000);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(18), 0xc0080400);

        REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_UP(0), 0x24000);
        REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_LOW(0), 0);
        REG_WRITE32(c2c_us, C2C_IB_ATU_CTRL(0), 0xa0010024);
        REG_WRITE32(c2c_us, C2C_IB_ATU_DST_ADDR(0), 0);
        value = REG_READ32(c2c_us, C2C_IB_ATU_SEL);
        value &= ~(0x3 << 0);
        value |= (0x1 << 0);
        REG_WRITE32(c2c_us, C2C_IB_ATU_SEL, value);
    }

    /* [8] Multi-function inbound (func0 only): fixed IB iATU indexes.
     *     func1: [30]=BAR0 [29]=BAR1 [28]=BAR4
     *     func2: [27]=BAR0 [26]=BAR1 [25]=BAR4
     *     func3: [24]=BAR0 [23]=BAR1 [22]=BAR4
     *     Only inbound SNPS IB iATU + C2C IB ATU; downstream OB iATU is done by firmware.
     */
    if (function_num == 0) {
        max_function_num = (bmdi->cinfo.mode & 0xc0) >> 6;
        chip = pdev->devfn & 0x7;

        if (max_function_num >= 1) {
            bar0 = bm84x6_ep_iatu_addr(chip, 1, 0);
            bar1 = bm84x6_ep_iatu_addr(chip, 1, 1);
            bar4 = bm84x6_ep_iatu_addr(chip, 1, 4);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(30), bar0 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(30), bar0 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(30), 0x102000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(30), 0xc0080000);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(29), bar1 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(29), bar1 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(29), 0x102000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(29), 0xc0080100);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(28), bar4 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(28), bar4 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(28), 0x102000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(28), 0xc0080400);

            if (chip == 0) {
                REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_UP(1), bar4 >> 32);
                REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_LOW(1), bar4 & 0xffffffff);
                REG_WRITE32(c2c_us, C2C_IB_ATU_CTRL(1), 0xc4020024);
                REG_WRITE32(c2c_us, C2C_IB_ATU_DST_ADDR(1), 0);
                value = REG_READ32(c2c_us, C2C_IB_ATU_SEL);
                value &= ~(0x3 << 2);
                value |= (0x1 << 2);
                REG_WRITE32(c2c_us, C2C_IB_ATU_SEL, value);
            }
        }

        if (max_function_num >= 2) {
            bar0 = bm84x6_ep_iatu_addr(chip, 2, 0);
            bar1 = bm84x6_ep_iatu_addr(chip, 2, 1);
            bar4 = bm84x6_ep_iatu_addr(chip, 2, 4);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(27), bar0 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(27), bar0 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(27), 0x202000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(27), 0xc0080000);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(26), bar1 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(26), bar1 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(26), 0x202000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(26), 0xc0080100);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(25), bar4 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(25), bar4 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(25), 0x202000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(25), 0xc0080400);

            if (chip == 0) {
                REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_UP(2), bar4 >> 32);
                REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_LOW(2), bar4 & 0xffffffff);
                REG_WRITE32(c2c_us, C2C_IB_ATU_CTRL(2), 0x88030024);
                REG_WRITE32(c2c_us, C2C_IB_ATU_DST_ADDR(2), 0);
                value = REG_READ32(c2c_us, C2C_IB_ATU_SEL);
                value &= ~(0x3 << 4);
                value |= (0x1 << 4);
                REG_WRITE32(c2c_us, C2C_IB_ATU_SEL, value);
            }
        }

        if (max_function_num >= 3) {
            bar0 = bm84x6_ep_iatu_addr(chip, 3, 0);
            bar1 = bm84x6_ep_iatu_addr(chip, 3, 1);
            bar4 = bm84x6_ep_iatu_addr(chip, 3, 4);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(24), bar0 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(24), bar0 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(24), 0x302000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(24), 0xc0080000);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(23), bar1 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(23), bar1 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(23), 0x302000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(23), 0xc0080100);

            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(22), bar4 & 0xffffffff);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(22), bar4 >> 32);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(22), 0x302000);
            REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(22), 0xc0080400);

            if (chip == 0) {
                REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_UP(3), bar4 >> 32);
                REG_WRITE32(c2c_us, C2C_IB_ATU_ADDR_LOW(3), bar4 & 0xffffffff);
                REG_WRITE32(c2c_us, C2C_IB_ATU_CTRL(3), 0xac040024);
                REG_WRITE32(c2c_us, C2C_IB_ATU_DST_ADDR(3), 0);
                value = REG_READ32(c2c_us, C2C_IB_ATU_SEL);
                value &= ~(0x3 << 6);
                value |= (0x1 << 6);
                REG_WRITE32(c2c_us, C2C_IB_ATU_SEL, value);
            }
        }
    }
    /* [9] func!=0: nothing (BAR4 resize handed to firmware) */

    /* [10] PORT CODE (depends on [4] BAR1 inbound) */
    bm84x6_pcie_config_port_code(bmdi, function_num);

    /* [11] OB CONFIG: CDMA / AP / MSI (depends on [4] BAR1 inbound) */
    if (function_num == 0)
        bm84x6_pcie_config_upstream_port(bmdi);

    bm84x6_debug_dump_atu(bmdi);
}

void bm84x6_unmap_bar(struct bm_device_info *bmdi)
{
    struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
    void __iomem *atu_base_addr = bari->bar0_vaddr + BM84X6_OFFSET_PCIE_iATU;
    int index = 0;

    for (index = 0; index < 31; index++) {
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_2(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_REGION_CTRL_1(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_BASE_ADDR(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_BASE_ADDR(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LIMIT_ADDR(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPR_LIMIT_ADDR(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_LWR_TARGET_ADDR(index), 0);
        REG_WRITE32(atu_base_addr, PCIE_IB_IATU_UPPER_TARGET_ADDR(index), 0);
    }
}

void bm84x6_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi)
{
    struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
    void __iomem *cfg_core_ctrl;
    int max_payload = 0x0;
    int max_rd_req = 0x0;
    int total_func_num = 0;
    int i = 0;
    int temp_value = 2;
    int temp_low = 2;
    struct bm_card *bmcd = NULL;

    total_func_num = ((bmdi->cinfo.mode >> BM84X6_PCIE_FUNC_NUM_SHIFT) & 0x3) + 1;

    cfg_core_ctrl = bari->bar1_vaddr + bari->bar1_part_info[1].offset + 0x1000;
    max_payload = (REG_READ32(cfg_core_ctrl, 0x1d4) >> 20) & 0xfff;
    max_rd_req = (REG_READ32(cfg_core_ctrl, 0x1d4) >> 8) & 0xfff;
    for (i = 0; i < total_func_num; i++) {
        temp_value = max_rd_req >> (i * 0x3);
        temp_value &= 0x7;
        if (temp_value < temp_low)
            temp_low = temp_value;
        temp_value = max_payload >> (i * 0x3);
        temp_value &= 0x7;
        if (temp_value < temp_low)
            temp_low = temp_value;
    }

    bmcd = bmdrv_card_get_bm_card(bmdi);
    if (bmcd != NULL) {
        temp_value = bmcd->cdma_max_payload & 0x7;
        if (temp_value < temp_low)
            temp_low = temp_value;
    }

    bmdi->memcpy_info.cdma_max_payload = temp_low | (temp_low << 3);

    pr_info
        ("max_payload = 0x%x, max_rd_req = 0x%x, total_func_num = 0x%x, max_paload = 0x%x \n",
         max_payload, max_rd_req, total_func_num,
         bmdi->memcpy_info.cdma_max_payload);
}

int bm84x6_setup_bar_dev_layout(struct bm_device_info *bmdi,
                                BAR_LAYOUT_TYPE type)
{
    struct bm_bar_info *bar_info = &bmdi->cinfo.bar_info;
    void __iomem *atu_base_addr =
        bar_info->bar0_vaddr + BM84X6_OFFSET_PCIE_iATU;
    const struct bm_bar_info *bar_layout = NULL;
    int index;

    bar_info->bar0_dev_start = REG_READ32(atu_base_addr, 0x114);
    if (bar_info->bar0_dev_start == BM84X6_PCIE_DBI_X8_0)
        bar_layout = &bm84x6_pcie_x8_0_bar_layout[type];
    else
        bar_layout = &bm84x6_pcie_x4_0_bar_layout[type];

    if (bar_layout->bar1_len == bar_info->bar1_len) {

        bar_info->bar1_dev_start = bar_layout->bar1_dev_start;
        for (index = 0; index < PCIE_BAR1_PART_MAX; index++) {
            bar_info->bar1_part_info[index].dev_start =
                bar_layout->bar1_part_info[index].dev_start;
            bar_info->bar1_part_info[index].offset =
                bar_layout->bar1_part_info[index].offset;
            bar_info->bar1_part_info[index].len =
                bar_layout->bar1_part_info[index].len;
        }

        bar_info->bar2_dev_start = bar_layout->bar2_dev_start;
        bar_info->bar2_len = bar_layout->bar2_len;

        //bar_info->bar4_dev_start = bar_layout->bar4_dev_start;
        //bar_info->bar4_len = bar_layout->bar4_len;
        return 0;
    }

    return -1;
}

int bm84x6_pcie_get_mode(struct bm_device_info *bmdi)
{
    u32 value = 0x0;

    value = bm_read32(bmdi, BM84X6_PCIE_TRAP_REG);
    if (value == 0xffffffff) {
        pr_err("pcie get mode fail\n");
        return -1;
    }

    bmdi->cinfo.mode = (value>>BM84X6_PCIE_MODE_SHIFT) & BM84X6_PCIE_MODE_MASK;
    pr_info("pcie mode 0x%x \n", bmdi->cinfo.mode);

    return 0;
}

/* boot_sel field [20:18] of trap reg == 0x5 means boot_from_pcie.
 * Only then should the host load the c2c/ddr firmware; otherwise mark the
 * FW_ONESHOT_C2C bit so the fw-load loop skips that entry.
 */
static void bm84x6_pcie_check_boot_from_pcie(struct bm_device_info *bmdi)
{
    u32 trap = bm_read32(bmdi, BM84X6_PCIE_TRAP_REG);
    u32 boot_sel;

    if (trap == 0xffffffff)
        return;

    boot_sel = (trap >> BM84X6_PCIE_BOOT_SEL_SHIFT) & BM84X6_PCIE_BOOT_SEL_MASK;
    if (boot_sel != BM84X6_PCIE_BOOT_FROM_PCIE) {
        bmdi->fw_oneshot_mask |= FW_ONESHOT_C2C;
        pr_info("bm84x6: boot_sel=0x%x (not pcie), skip c2c/ddr fw\n", boot_sel);
    }
}

static u32 bm84x6_pcie_port_code_list[4] = {
    0x13336,
    0x13361,
    0x13611,
    0x16111
};

static int bm84x6_pcie_config_port_code(struct bm_device_info *bmdi, int func)
{
    struct bm_card *bmcd = bmdrv_card_get_bm_card(bmdi);
    u32 card_size, card_id;
    u32 value = 0x0;

    if (bmcd != NULL) {
        card_size = bmcd->chip_num;
        card_id = bmcd->card_index;
    } else {
        card_size = ((bmdi->cinfo.mode >> BM84X6_PCIE_FUNC_NUM_SHIFT) & 0x3) + 1;
        if (bm_get_available_card_index() >= 0)
            card_id = bm_get_available_card_index();
        else
            card_id = 0x0;
    }

    value = bm_read32(bmdi, BM84X6_CHAIN_REGMAP_CTRL);
    value &= ~(0x3ff << 20);
    value |= (card_size << 27) | (card_id << 20);
    bm_write32(bmdi, BM84X6_CHAIN_REGMAP_CTRL, value);

    value = bm_read32(bmdi, BM84X6_PORT_CODE_LIST_X8_0);
    value &= ~0xfffff;
    value |= bm84x6_pcie_port_code_list[func];
    bm_write32(bmdi, BM84X6_PORT_CODE_LIST_X8_0, value);

    value = bm_read32(bmdi, BM84X6_PORT_CODE_LIST_X4_0);
    value &= ~0xfffff;
    value |= bm84x6_pcie_port_code_list[func];
    bm_write32(bmdi, BM84X6_PORT_CODE_LIST_X4_0, value);

    return 0;
}

static void bm84x6_pcie_set_cdma_ob_atu(struct bm_device_info *bmdi)
{
    struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
    void __iomem *c2c = bari->bar1_vaddr + bari->bar1_part_info[3].offset;

    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_UP(0), 0x0);
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_LOW(0), 0x0);
    REG_WRITE32(c2c, C2C_OB_ATU_PC_CTRL(0), 0xc000002f);
}

static void bm84x6_pcie_set_msi_ob_atu(struct bm_device_info *bmdi)
{
    struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
    void __iomem *c2c = bari->bar1_vaddr + bari->bar1_part_info[3].offset;
    u64 src_addr;

    /* chip 0 MSI gen to PC */
    src_addr = (0x50ULL << 32) | (0x0ULL << 46) | (0x1ULL << 45) | (0x7ULL << 49);
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_UP(1), (src_addr >> 32));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_LOW(1), (src_addr & 0xffffffff));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_CTRL(1), 0xe0500020);

    /* chip 1 MSI gen to PC */
    src_addr = (0x51ULL << 32) | (0x1ULL << 46) | (0x1ULL << 45) | (0x7ULL << 49);
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_UP(2), (src_addr >> 32));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_LOW(2), (src_addr & 0xffffffff));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_CTRL(2), 0xe1510020);

    /* chip 2 MSI gen to PC */
    src_addr = (0x52ULL << 32) | (0x2ULL << 46) | (0x1ULL << 45) | (0x7ULL << 49);
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_UP(3), (src_addr >> 32));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_LOW(3), (src_addr & 0xffffffff));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_CTRL(3), 0xe2520020);

    /* chip 3 MSI gen to PC */
    src_addr = (0x53ULL << 32) | (0x3ULL << 46) | (0x1ULL << 45) | (0x7ULL << 49);
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_UP(4), (src_addr >> 32));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_ADDR_LOW(4), (src_addr & 0xffffffff));
    REG_WRITE32(c2c, C2C_OB_ATU_PC_CTRL(4), 0xe3530020);
}

static void bm84x6_pcie_set_ap_ob_atu(struct bm_device_info *bmdi)
{
}

static void bm84x6_pcie_config_upstream_port(struct bm_device_info *bmdi)
{
    bm84x6_pcie_set_cdma_ob_atu(bmdi);
    bm84x6_pcie_set_ap_ob_atu(bmdi);
    bm84x6_pcie_set_msi_ob_atu(bmdi);
}

static inline u64 bm84x6_ep_iatu_addr(int chip, int func, int bar)
{
    u64 base = BM84X6_EP_BASE + ((u64)chip + func - 1) * BM84X6_SLOT_STRIDE;

    switch (bar) {
    case 0:  return base;
    case 1:  return base + BM84X6_BAR1_OFFSET;
    case 4:  return base + BM84X6_BAR4_OFFSET;
    default: return 0;
    }
}

int bm84x6_c2c_post_load(struct bm_device_info *bmdi)
{
    u32 val;
    int timeout = 5000;

    val = bm_read32(bmdi, BM84X6_PCIE_STATUS_REG);
    bm_write32(bmdi, BM84X6_PCIE_STATUS_REG, val | BM84X6_PCIE_INIT_START);

    while (!(bm_read32(bmdi, BM84X6_PCIE_STATUS_REG) & BM84X6_PCIE_DDR_INITIALIZED)) {
        mdelay(10);
        if (--timeout <= 0) {
            pr_err("bm84x6: C2C/DDR init timeout, status=0x%x\n",
                   bm_read32(bmdi, BM84X6_PCIE_STATUS_REG));
            return -EBUSY;
        }
    }
    pr_info("bm84x6: C2C/DDR init done\n");
    return 0;
}

int bm84x6_config_iatu_for_function_x(struct pci_dev *pdev,
                                      struct bm_device_info *bmdi,
                                      struct bm_bar_info *bari)
{
    bmdi->cinfo.pcie_func_index = (pdev->devfn & 0x7);

    /* link check: BAR0 DBI must be accessible before probe continues */
    if (REG_READ32(bari->bar0_vaddr, 0x14) == 0xffffffff) {
        pr_info("pcie link may error, abort probe\n");
        return -1;
    }

    return 0;
}

void bm84x6_pci_slider_bar4_config_device_addr(struct bm_bar_info *bari,
                                               u32 addr)
{
    void __iomem *atu_base_addr;
    u32 dst_addr = 0;
    u32 temp_addr = 0;
    atu_base_addr = bari->bar0_vaddr + BM84X6_OFFSET_PCIE_iATU;
    dst_addr = REG_READ32(atu_base_addr, 0x3714);

    if ((addr > (dst_addr + 0xFFFFF)) || (addr < dst_addr)) {
        temp_addr = addr & (~0xfffff);
        REG_WRITE32(atu_base_addr, 0x3714, temp_addr);
        temp_addr = REG_READ32(atu_base_addr, 0x3714);
    }
}
