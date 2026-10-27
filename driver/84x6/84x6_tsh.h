#ifndef __84X6_TSH_H__
#define __84X6_TSH_H__

#include <linux/types.h>
#include <linux/interrupt.h>
#include "bm_common.h"

#define TPUSYS_BASE_ADDR 0x24000000
#define TXPSYS_OFFSET 0x800000

#define TXPSYS0_TPU_AXI_LMEM 0x24000000
#define TXPSYS_TPU_AXI_LMEM_SIZE 0x400000

#define TXPSYS0_TPU_AXI_SMEM 0x24400000
#define TXPSYS_TPU_AXI_SMEM_SIZE 0x10000

#define TPUSYS_L2M0 0x26d20000
#define TPUSYS_L2M0_SIZE 0x40000
#define TPUSYS_L2M_OFFSET 0x40000

#define SYS_CTRL_BASE 0x28100000    // top misc reg

#define TXPSYS0_REG 0x24509000

#define TXPSYS0_GDMA_REG 0x24510000

#define TXPSYS0_TPU_REG 0x24410000

#define TXPSYS0_TSH_FIFO_INSTR 0x24502000

#define TXPSYS0_TPU_SCH_FIFO_AXI 0x24501000 // read only

#define TXPSYS0_TPU_SCH_FIFO_REG 0x24500000

#define TSH_CFG_ADDR 0x26d06000

#define TSH_DONE 0x26d07000UL

#define TPUSYS_MSG 0x26d08000   // h410 write 1; h414 rw

#define TPUSYS_HAU_REG 0x26D10000

#define TXPSYS0_IFE_APB_REG 0x24503000  // ? 8byte align,rw h3504-h3508,h3534-h3538

#define TPU_MONITOR0 0x26F00000 // TPU_MONITOR0-TPU_MONITOR5
#define TPU_MONITOR_SIZE 0x1000

#define TPU_FIREWALL0 0x26F06000    // TPU_FIREWALL0-TPU_FIREWALL5
#define TPU_FIREWALL_SIZE 0x1000

/* TPUSYS REG */
#define TPU_SYS_BASE 0x24000000
#define TPU_SYS_REG_BASE 0x26D09000
#define TPU_SYS_GATING_CTRL 0x0 // 0xff
#define TPU_SYS_SOFT_RESET_CTRL 0x4 // 0x0
#define TPU_SYS_INT_CLR 0xf4

/* TXPSYS REG */
#define TXP_SYS0_BASE 0x24509000
#define TXP_SYS_OFFSET 0x800000
#define TXP_SYS_SCALAR_RVBA_L 0x58
#define TXP_SYS_SCALAR_RVBA_H 0x5c
#define TXP_SYS_GATING_CTRL 0x0 // 0xff
#define TXP_SYS_SOFT_RESET_CTRL 0x4 // 0x0
#define TPU_SYS_SCALAR_RETIRE_PC_L 0x70
#define TPU_SYS_SCALAR_RETIRE_PC_H 0x78
#define TXPSYS0_TPU_SCH_FIFO_REG 0x24500000
#define TXPSYS0_TSH_FIFO_INSTR 0x24502000

/* TSH REG */
#define TSH_RST_CLK_REG (TSH_CFG_ADDR + 0x0)
#define TSH_MODE_REG (TSH_CFG_ADDR + 0x4)
#define TSH_TIMING_SYNC_REG (TSH_CFG_ADDR + 0x8)
#define TSH_SYNC_MAP_DIR_REG (TSH_CFG_ADDR + 0xc)
#define TSH_ASYNC_CONFIG01 (TSH_CFG_ADDR + 0x10)
#define TSH_ASYNC_CONFIG23 (TSH_CFG_ADDR + 0x14)
#define TSH_ASYNC_CONFIG45 (TSH_CFG_ADDR + 0x18)
#define TSH_ASYNC_CONFIG67 (TSH_CFG_ADDR + 0x1c)
#define TSH_CORE_STATUS (TSH_CFG_ADDR + 0x20)
#define TSH_HWQ_DEPTH0123 (TSH_CFG_ADDR + 0x24)
#define TSH_HWQ_DEPTH4567 (TSH_CFG_ADDR + 0x28)

#define TSH_KENEL_PACKET_HWQ0 (TSH_CFG_ADDR + 0x30)
#define TSH_KENEL_PACKET_HWQ1 (TSH_CFG_ADDR + 0x34)
#define TSH_KENEL_PACKET_HWQ2 (TSH_CFG_ADDR + 0x38)
#define TSH_KENEL_PACKET_HWQ3 (TSH_CFG_ADDR + 0x3c)
#define TSH_KENEL_PACKET_HWQ4 (TSH_CFG_ADDR + 0x40)
#define TSH_KENEL_PACKET_HWQ5 (TSH_CFG_ADDR + 0x44)
#define TSH_KENEL_PACKET_HWQ6 (TSH_CFG_ADDR + 0x48)
#define TSH_KENEL_PACKET_HWQ7 (TSH_CFG_ADDR + 0x4c)

/* outbox buffer could write data numver, 8 bits per outbox */
#define TSH_OUTBOX_DEPTH0123 (TSH_CFG_ADDR + 0x50)
#define TSH_OUTBOX_DEPTH4567 (TSH_CFG_ADDR + 0x54)

/* outbox write data */
#define TSH_OUTBOX0_PACKET (TSH_CFG_ADDR + 0x58)
#define TSH_OUTBOX1_PACKET (TSH_CFG_ADDR + 0x60)
#define TSH_OUTBOX2_PACKET (TSH_CFG_ADDR + 0x68)
#define TSH_OUTBOX3_PACKET (TSH_CFG_ADDR + 0x70)
#define TSH_OUTBOX4_PACKET (TSH_CFG_ADDR + 0x78)
#define TSH_OUTBOX5_PACKET (TSH_CFG_ADDR + 0x80)
#define TSH_OUTBOX6_PACKET (TSH_CFG_ADDR + 0x88)
#define TSH_OUTBOX7_PACKET (TSH_CFG_ADDR + 0x90)

#define TSH_TCORE0_BASE_ADDR (TSH_CFG_ADDR + 0x98)
#define TSH_TCORE1_BASE_ADDR (TSH_CFG_ADDR + 0xa0)
#define TSH_TCORE2_BASE_ADDR (TSH_CFG_ADDR + 0xa8)
#define TSH_TCORE3_BASE_ADDR (TSH_CFG_ADDR + 0xb0)

#define TSH_TIMEOUT_EN (TSH_CFG_ADDR + 0x198)
#define TSH_124CORE_TIMEOUR_CYCLE_THR (TSH_CFG_ADDR + 0x19c)
#define TSH_HWQ0_DONE_CNT (TSH_CFG_ADDR + 0x1ac)    // 1bit for 1 hwq
#define TSH_HWQ0_7_DONE_CNT_CLR (TSH_CFG_ADDR + 0x1cc)
#define TSH_MSGID_THR (TSH_CFG_ADDR + 0x1d0)
#define TSH_DISABLE_CORE (TSH_CFG_ADDR + 0x1d4)
/* TSH FIFO REG*/
#define TXPSYS0_TSH_FIFO_TSH_BASE_ADDR_L (TXPSYS0_TPU_SCH_FIFO_REG + 0x8)
#define TXPSYS0_TSH_FIFO_TSH_BASE_ADDR_H (TXPSYS0_TPU_SCH_FIFO_REG + 0xc)



#define OUTBOX_NUM 8
#define HWQ_NUM 8
#define MAX_CORE_NUM 4
#define HWQ_DEPTH 16
#define OUTBOX_DEPTH 64
#define TSH_FIFO_DEPTH 16
//#define BIT(n) (1U << (n))
//#define GENMASK(h, l) ((~0U << (l)) & (~0U >> (31 - (h))))

#define KERNEL_PACKET_OFFSET 0x2000
#define KERNEL_PACKET_SIZE (0x1180000000UL)

//#define pr_err(fmt, ...) printf("ERROR: " fmt "\n", ##__VA_ARGS__)

typedef enum tsh_mode {
    TSH_ASYNC_MODE = 0,
    TSH_SYNC_MODE = 1,
    TSH_BYPASS_MODE = 2,
    TSH_MAX_NUM_MODE
} en_tsh_mode;

enum tsh_sync_map_dir {
    RECORD_IDX_DIR = 0,
    RECORD_DEPTH_DIR = 1,
};

enum task_phase {
    TASK_PHASE_PREPARE = 0,
    TASK_PHASE_MAIN = 1,
    TASK_PHASE_FULL = 2,
};

enum task_sync {
    TASK_ASYNC_MULTICORE_SYNC = 1,  // async mode 下groupidx为0时设定
};

enum task_tag {
    TASK_TAG_CUSTOM = 1,        // 自定义header
};

union tsh_hwq_packet {
    struct {
        uint32_t block_num:5;
        uint32_t group_num:5;
        uint32_t packet_addr:22;
    };
    uint32_t packet;
};

typedef struct {
    uint64_t phase:2;
    uint64_t packet_addr:22;
    uint64_t sync:1;
    uint64_t msgid:9;
    uint64_t group_idx:5;
    uint64_t hwq_idx:3;
    uint64_t tag:1;
    uint64_t block_num:5;
    uint64_t group_num:5;
    uint64_t send_core_num:5;
    uint64_t start_core_idx:5;
    uint64_t reserve:1;
} tsh_outbox_packet_fields;


union tsh_outbox_packet {
    tsh_outbox_packet_fields fields;
    uint64_t packet;
};

// 改为genmask

union tsh_async_hwq_config {
    struct {
        uint32_t first_hwq_tpu_core_num:5;
        uint32_t reserved0:3;
        uint32_t first_hwq_start_core_id:5;
        uint32_t reserved1:3;
        uint32_t second_hwq_tpu_core_num:5;
        uint32_t reserved2:3;
        uint32_t second_hwq_start_core_id:5;
        uint32_t reserved3:3;
    } __attribute__((packed));
    uint32_t config;
};

union tsh_hwq_wdn {
    struct {
        uint8_t hwq0_wdn;
        uint8_t hwq1_wdn;
        uint8_t hwq2_wdn;
        uint8_t hwq3_wdn;
        uint8_t hwq4_wdn;
        uint8_t hwq5_wdn;
        uint8_t hwq6_wdn;
        uint8_t hwq7_wdn;
    } __attribute__((packed));
    struct {
        uint32_t hwq0123_wdn;
        uint32_t hwq4567_wdn;
    } __attribute__((packed));
    uint8_t hwq_wdn[HWQ_NUM];
};

union tsh_outbox_wdn {
    struct {
        uint8_t outbox0_wdn;
        uint8_t outbox1_wdn;
        uint8_t outbox2_wdn;
        uint8_t outbox3_wdn;
        uint8_t outbox4_wdn;
        uint8_t outbox5_wdn;
        uint8_t outbox6_wdn;
        uint8_t outbox7_wdn;
    } __attribute__((packed));
    struct {
        uint32_t outbox0123_wdn;
        uint32_t outbox4567_wdn;
    } __attribute__((packed));
    uint8_t outbox_wdn[OUTBOX_NUM];
};

struct wdn_depth_info {
    // TODO: driver实际用不到tsh_fifo_wdn,后续设计结构体可以union前两部分，并且用函数指针区分获取func
    union tsh_hwq_wdn hwq_wdn;
    union tsh_outbox_wdn outbox_wdn;
    union {
        struct {
            uint8_t tsh_fifo0_wdn;
            uint8_t tsh_fifo1_wdn;
            uint8_t tsh_fifo2_wdn;
            uint8_t tsh_fifo3_wdn;
        } __attribute__((packed));
        uint8_t tsh_fifo_wdn[MAX_CORE_NUM];
    };
};

struct tsh_config {
    enum tsh_mode mode;
    enum tsh_sync_map_dir sync_map_dir;
    uint8_t active_core_num;
    uint8_t active_core_mask;
    uint8_t async_hwq_start_core_conf[HWQ_NUM];
    uint8_t async_hwq_core_num_conf[HWQ_NUM];
    union {
        uint32_t hwq_packet[HWQ_NUM];
        uint64_t outbox_packet[OUTBOX_NUM];
    };
    union {
        uint8_t active_outbox_mask;
        uint8_t active_hwq_mask;
    };
};

union kernel_packet {
    struct {
        union tsh_hwq_packet hwq_packet;
        uint8_t hwq_idx;
    };
    union tsh_outbox_packet outbox_packet;
};

struct test_cfg {
    struct tsh_config tsh_cfg;
    int packet_num;
    bool is_kernel_executed;    // true表示tscalar要做处理并产生done信号,在某个设定的memory address设定
    bool is_packet_verified;    // true表示tscalar回传packet,验证内容
    bool is_group_num_random;
    union kernel_packet kernel_packets[80];
};



int tsh_get_irq_hwq_index(void);

int tsh_init(struct bm_device_info *bmdi, struct tsh_config *cfg);
int tsh_setmode(en_tsh_mode mode);
en_tsh_mode tsh_getmode(struct bm_device_info *bmdi);
int tsh_get_active_queue_mask(struct bm_device_info *bmdi);
int tsh_get_hwq_free_slots(struct bm_device_info *bmdi, int idx);
int tsh_get_outbox_free_slots(struct bm_device_info *bmdi, int idx);
int tsh_write_hwq(struct bm_device_info *bmdi, int idx, u32 packet);
int tsh_write_outbox(struct bm_device_info *bmdi, int idx, u64 packet);
int tsh_active_core_num(struct bm_device_info *bmdi);
u64 tsh_get_task_done_count(struct bm_device_info *bmdi, int idx);
void tsh_clear_irq(struct bm_device_info *bmdi);
int tsh_clr_done_count(struct bm_device_info *bmdi, int idx);
void tpusys_write_32(struct bm_device_info *bmdi, u32 addr, u32 val);
void tpusys_clrbits_32(struct bm_device_info *bmdi, u32 addr, u32 mask);
void tpusys_setbits_32(struct bm_device_info *bmdi, u32 addr, u32 mask);
u32 tpusys_read_32(struct bm_device_info *bmdi, u32 addr);


#endif /* __84X6_TSH_H__ */
