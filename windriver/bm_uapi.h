#ifndef _BM_UAPI_H_
#define _BM_UAPI_H_

typedef enum {
    BMCPU_IDLE    = 0,
    BMCPU_RUNNING = 1,
    BMCPU_FAULT   = 2
} bm_cpu_status_t;

typedef struct bm_api {
	bm_api_id_t    api_id;
	u8 *api_addr;
	u32 api_size;
} bm_api_t;

typedef struct bm_api_ext {
	bm_api_id_t    api_id;
	u8 *api_addr;
	u32 api_size;
	u64 api_handle;
} bm_api_ext_t;

typedef struct bm_api_data {
	bm_api_id_t    api_id;
	u64 api_handle;
	u64 data;
	s32 timeout;
} bm_api_data_t;

typedef struct bm_profile {
	u64 cdma_in_time;
	u64 cdma_in_counter;
	u64 cdma_out_time;
	u64 cdma_out_counter;
	u64 tpu_process_time;
	u64 sent_api_counter;
	u64 completed_api_counter;
} bm_profile_t;

struct bm_heap_stat {
	u32 mem_total;
	u32 mem_avail;
	u32 mem_used;
};

typedef struct bm_dev_stat {
	u32 mem_total;
	u32 mem_used;
	u32 tpu_util;
	u32 heap_num;
	struct bm_heap_stat heap_stat[4];
} bm_dev_stat_t;

/*
 * bm misc info
 */
#define BM_DRV_SOC_MODE 1
#define BM_DRV_PCIE_MODE 0
struct bm_misc_info {
	int pcie_soc_mode; /*0---pcie; 1---soc*/
	int ddr_ecc_enable; /*0---disable; 1---enable*/
	long long ddr0a_size;
	long long ddr0b_size;
	long long ddr1_size;
	long long ddr2_size;
	unsigned int chipid;
#define BM1682_CHIPID_BIT_MASK	(0X1 << 0)
#define BM1684_CHIPID_BIT_MASK	(0X1 << 1)
#define BM1684X_CHIPID_BIT_MASK	(0X1 << 2)
	unsigned long long chipid_bit_mask;
	unsigned int driver_version;
	int domain_bdf; /*[31:16]-domin,[15:8]-bus_id,[7:3]-device_id,[2:0]-func_num*/
	int board_version; /*hardware board version [23:16]-mcu sw version, [15:8]-board type, [7:0]-hw version*/
	int a53_enable;
};


/*
 * bm boot info
 */
#define BOOT_INFO_VERSION       0xabcd
#define BOOT_INFO_VERSION_V1    0xabcd0001

struct boot_info_append_v1 {
	unsigned int a53_enable;
	unsigned long long heap2_size;
};

struct bm_boot_info {
	unsigned int deadbeef;
	unsigned int ddr_ecc_enable; /*0---disable; 1---enable*/
	unsigned long long ddr_0a_size;
	unsigned long long ddr_0b_size;
	unsigned long long ddr_1_size;
	unsigned long long ddr_2_size;
	unsigned int ddr_vendor_id;
	unsigned int ddr_mode;/*[31:16]-ddr_gmem_mode, [15:0]-ddr_power_mode*/
	unsigned int ddr_rank_mode;
	unsigned int tpu_max_clk;
	unsigned int tpu_min_clk;
	unsigned int temp_sensor_exist;
	unsigned int tpu_power_sensor_exist;
	unsigned int board_power_sensor_exist;
	unsigned int fan_exist;
	unsigned int max_board_power;
	unsigned int boot_info_version; /*[31:16]-0xabcd,[15:0]-version num*/
	union {
		struct boot_info_append_v1 append_v1;
	} append;
};

typedef enum {
	PERF_MONITOR_GDMA = 0,
	PERF_MONITOR_TPU = 1
} PERF_MONITOR_ID;

/*
* bm performace monitor
*/
struct bm_perf_monitor {
	long long buffer_start_addr;
	int buffer_size;
	PERF_MONITOR_ID monitor_id;
};

struct bm_reg {
	int reg_addr;
	int reg_value;
};

#define BMDEV_MEMCPY                                                           \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_ALLOC_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_FREE_GMEM                                                        \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TOTAL_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_AVAIL_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_REQUEST_ARM_RESERVED                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x14, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_RELEASE_ARM_RESERVED                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x15, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_MAP_GMEM                                                         \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x16, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_INVALIDATE_GMEM                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x17, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_FLUSH_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x18, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_ALLOC_GMEM_ION                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x19, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_SEND_API                                                         \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_THREAD_SYNC_API                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x21, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_DEVICE_SYNC_API                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x23, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_HANDLE_SYNC_API                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x27, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SEND_API_EXT                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x28, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_QUERY_API_RESULT                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x29, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_GET_MISC_INFO                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_UPDATE_FW_A9                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_PROFILE                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x32, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_PROGRAM_A53                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x33, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOOT_INFO                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x34, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_UPDATE_BOOT_INFO                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x35, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SN                                                               \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x36, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_MAC0                                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x37, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_MAC1                                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x38, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_BOARD_TYPE                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x39, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_PROGRAM_MCU                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_CHECKSUM_MCU                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_REG                                                          \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_REG                                                          \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DEV_STAT                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3e, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_TRACE_ENABLE                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x40, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_DISABLE                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x41, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACEITEM_NUMBER                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x42, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_DUMP                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x43, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_DUMP_ALL                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x44, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_ENABLE_PERF_MONITOR                                              \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x45, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_DISABLE_PERF_MONITOR                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x46, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DEVICE_TIME                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x47, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_SET_TPU_DIVIDER                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x50, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_MODULE_RESET                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x51, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_TPU_FREQ                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x52, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_FREQ                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x53, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_TRIGGER_VPP                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_BASE64_PREPARE                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x70, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_BASE64_START                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x71, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_BASE64_CODEC                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x72, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_I2C_READ_SLAVE                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x73, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_I2C_WRITE_SLAVE                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x74, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_GET_HEAP_STAT_BYTE                                               \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x75, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_HEAP_NUM                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x76, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_BMCPU_STATUS                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x7F, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRIGGER_BMCPU                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_GET_TPUC                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x81, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_MAXP                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x82, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOARDP               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x83, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_FAN                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x84, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_CORRECTN             CTL_CODE(FILE_DEVICE_UNKNOWN, 0x85, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_12V_ATX              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x86, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_SN                   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x87, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_STATUS               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x88, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_MINCLK           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x89, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_MAXCLK           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DRIVER_VERSION       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOARD_TYPE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0X8C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOARDT               CTL_CODE(FILE_DEVICE_UNKNOWN, 0X8D, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_CHIPT                CTL_CODE(FILE_DEVICE_UNKNOWN, 0X8E, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_P                CTL_CODE(FILE_DEVICE_UNKNOWN, 0X8F, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_V                CTL_CODE(FILE_DEVICE_UNKNOWN, 0X90, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_CARD_ID              CTL_CODE(FILE_DEVICE_UNKNOWN, 0X91, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DYNFREQ_STATUS       CTL_CODE(FILE_DEVICE_UNKNOWN, 0X92, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_CHANGE_DYNFREQ_STATUS    CTL_CODE(FILE_DEVICE_UNKNOWN, 0X93, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_VERSION    CTL_CODE(FILE_DEVICE_UNKNOWN, 0X94, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_LOADED_LIB    CTL_CODE(FILE_DEVICE_UNKNOWN, 0X95, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_SMI_ATTR    CTL_CODE(FILE_DEVICE_UNKNOWN, 0X96, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define BMCTL_GET_DEV_CNT                                                      \
    CTL_CODE(FILE_DEVICE_BEEP, 0x0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_SMI_ATTR                                                     \
    CTL_CODE(FILE_DEVICE_BEEP, 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_SET_LED                                                          \
    CTL_CODE(FILE_DEVICE_BEEP, 0x02, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_TEST_I2C1                                                        \
    CTL_CODE(FILE_DEVICE_BEEP, 0x9, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_SET_ECC                                                          \
    CTL_CODE(FILE_DEVICE_BEEP, 0x03, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_PROC_GMEM                                                    \
    CTL_CODE(FILE_DEVICE_BEEP, 0x04, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_DEV_RECOVERY                                                     \
    CTL_CODE(FILE_DEVICE_BEEP, 0x05, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_DRIVER_VERSION                                               \
    CTL_CODE(FILE_DEVICE_BEEP, 0x06, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_CARD_INFO                                                    \
    CTL_CODE(FILE_DEVICE_BEEP, 0x07, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_CARD_NUM                                                     \
    CTL_CODE(FILE_DEVICE_BEEP, 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef TEST_VPU_ONECORE_FPGA
#define MAX_NUM_VPU_CORE 1 /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP MAX_NUM_VPU_CORE
#define MAX_NUM_VPU_CORE_BM1686 1 /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP_BM1686 MAX_NUM_VPU_CORE_BM1686
#else
#define MAX_NUM_VPU_CORE 5 /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP MAX_NUM_VPU_CORE
#define MAX_NUM_VPU_CORE_BM1686 3 /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP_BM1686 MAX_NUM_VPU_CORE_BM1686
#endif

#define MAX_NUM_JPU_CORE 4
#define MAX_NUM_JPU_CORE_BM1686 2

typedef struct bm_smi_attr {
	int dev_id;
	int chip_id;
	int chip_mode;  /*0---pcie; 1---soc*/
	int domain_bdf;
	int status;
	int card_index;
	int chip_index_of_card;

	int mem_used;
	int mem_total;
	int tpu_util;

	int board_temp;
	int chip_temp;
	int board_power;
	int tpu_power;
	int fan_speed;

	int vdd_tpu_volt;
	int vdd_tpu_curr;
	int atx12v_curr;

	int tpu_min_clock;
	int tpu_max_clock;
	int tpu_current_clock;
	int board_max_power;

	int ecc_enable;
	int ecc_correct_num;

	char sn[18];
	char board_type[5];

	/* vpu mem and instant info*/
    int     vpu_instant_usage[MAX_NUM_VPU_CORE];
    s64     vpu_mem_total_size[2];
    s64     vpu_mem_free_size[2];
    s64     vpu_mem_used_size[2];

	/*if or not to display board endline and board attr*/
	int board_endline;
	int board_attr;
    bm_dev_stat_t stat;
} BM_SMI_ATTR, *BM_SMI_PATTR;

struct bm_smi_proc_gmem {
	int dev_id;
	pid_t pid[128];
	u64 gmem_used[128];
	int proc_cnt;
};

/*
* definations used by base64
*/

struct ce_desc {
	u32     ctrl;
	u32     alg;
	u64     next;
	u64     src;
	u64     dst;
	u64     len;
    union {
        u8  key[32];
        u64 dstlen;  // destination amount, only used in base64
    }key_dstlen;
	u8      iv[16];
};

struct ce_base {
	u64     src;
	u64     dst;
	u64     len;
	bool direction;
};

struct ce_reg {
	u32 ctrl; //control
	u32 intr_enable; //interrupt mask
	u32 desc_low; //descriptor base low 32bits
	u32 desc_high; //descriptor base high 32bits
	u32 intr_raw; //interrupt raw, write 1 to clear
	u32 se_key_valid; //secure key valid, read only
	u32 cur_desc_low; //current reading descriptor
	u32 cur_desc_high; //current reading descriptor
	u32 r1[24]; //reserved
	u32 desc[22]; //PIO mode descriptor register
	u32 r2[10]; //reserved
	u32 key[24];		//keys
	u32 r3[8]; //reserved
	u32 iv[12]; //iv
	u32 r4[4]; //reserved
	u32 sha_param[8]; //sha parameter
};

//for i2c
struct bm_i2c_param {
	int i2c_slave_addr;
	int i2c_index;
	int i2c_cmd;
	int value;
	int op;
};

typedef struct bm_heap_stat_byte {
		unsigned int  heap_id;
        unsigned long long mem_total;
        unsigned long long mem_avail;
        unsigned long long mem_used;
        unsigned long long mem_start_addr;
} bm_heap_stat_byte_t;

#endif /* _BM_UAPI_H_ */
