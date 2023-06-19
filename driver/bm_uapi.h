#ifndef _BM_UAPI_H_
#define _BM_UAPI_H_
#include <linux/types.h>
#include <bm_msg.h>
#include "vpp/vpp_platform.h"

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
	unsigned int mem_total;
	unsigned int mem_avail;
	unsigned int mem_used;
};

typedef struct bm_dev_stat {
	int mem_total;
	int mem_used;
	int tpu_util;
	int heap_num;
	struct bm_heap_stat heap_stat[4];
} bm_dev_stat_t;

typedef struct bm_heap_stat_byte {
	unsigned int  heap_id;
	unsigned long long mem_total;
	unsigned long long mem_avail;
	unsigned long long mem_used;
	unsigned long long mem_start_addr;
} bm_heap_stat_byte_t;

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
	unsigned long chipid_bit_mask;
	unsigned int driver_version;
	int domain_bdf; /*[31:16]-domin,[15:8]-bus_id,[7:3]-device_id,[2:0]-func_num*/
	int board_version; /*hardware board version [23:16]-mcu sw version, [15:8]-board type, [7:0]-hw version*/
	int a53_enable;
	int dyn_enable;
};


/*
 * bm boot info
 */
#define BOOT_INFO_VERSION       0xabcd
#define BOOT_INFO_VERSION_V1    0xabcd0001

struct boot_info_append_v1 {
	unsigned int a53_enable;
	unsigned int dyn_enable;
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

#define BMDEV_IOCTL_MAGIC  'p'
#define BMDEV_MEMCPY			_IOW('p', 0x00, unsigned long)

#define BMDEV_ALLOC_GMEM		_IOWR('p', 0x10, unsigned long)
#define BMDEV_FREE_GMEM			_IOW('p', 0x11, unsigned long)
#define BMDEV_TOTAL_GMEM		_IOWR('p', 0x12, unsigned long)
#define BMDEV_AVAIL_GMEM		_IOWR('p', 0x13, unsigned long)
#define BMDEV_REQUEST_ARM_RESERVED	_IOWR('p', 0x14, unsigned long)
#define BMDEV_RELEASE_ARM_RESERVED	_IOW('p',  0x15, unsigned long)
#define BMDEV_MAP_GMEM			_IOWR('p', 0x16, unsigned long) //not used, map in mmap
#define BMDEV_INVALIDATE_GMEM		_IOWR('p', 0x17, unsigned long)
#define BMDEV_FLUSH_GMEM		_IOWR('p', 0x18, unsigned long)
#define BMDEV_ALLOC_GMEM_ION		_IOW('p', 0x19, unsigned long)
#define BMDEV_GMEM_ADDR		        _IOW('p', 0x1a, unsigned long)

#define BMDEV_SEND_API			_IOW('p', 0x20, unsigned long)
#define BMDEV_THREAD_SYNC_API		_IOW('p', 0x21, unsigned long)
#define BMDEV_DEVICE_SYNC_API		_IOW('p', 0x23, unsigned long)
#define BMDEV_HANDLE_SYNC_API		_IOW('p', 0x27, unsigned long)
#define BMDEV_SEND_API_EXT		_IOW('p', 0x28, unsigned long)
#define BMDEV_QUERY_API_RESULT		_IOW('p', 0x29, unsigned long)

#define BMDEV_GET_MISC_INFO		_IOWR('p', 0x30, unsigned long)
#define BMDEV_UPDATE_FW_A9		_IOW('p',  0x31, unsigned long)
#define BMDEV_GET_PROFILE		_IOWR('p', 0x32, unsigned long)
#define BMDEV_PROGRAM_A53               _IOWR('p', 0x33, unsigned long)
#define BMDEV_GET_BOOT_INFO		_IOWR('p', 0x34, unsigned long)
#define BMDEV_UPDATE_BOOT_INFO		_IOWR('p', 0x35, unsigned long)
#define BMDEV_SN                        _IOWR('p', 0x36, unsigned long)
#define BMDEV_MAC0                      _IOWR('p', 0x37, unsigned long)
#define BMDEV_MAC1                      _IOWR('p', 0x38, unsigned long)
#define BMDEV_BOARD_TYPE                _IOWR('p', 0x39, unsigned long)
#define BMDEV_PROGRAM_MCU               _IOWR('p', 0x3a, unsigned long)
#define BMDEV_CHECKSUM_MCU              _IOWR('p', 0x3b, unsigned long)
#define BMDEV_SET_REG                   _IOWR('p', 0x3c, unsigned long)
#define BMDEV_GET_REG                   _IOWR('p', 0x3d, unsigned long)
#define BMDEV_GET_DEV_STAT              _IOWR('p', 0x3e, unsigned long)

#define BMDEV_TRACE_ENABLE		_IOW('p',  0x40, unsigned long)
#define BMDEV_TRACE_DISABLE		_IOW('p',  0x41, unsigned long)
#define BMDEV_TRACEITEM_NUMBER		_IOWR('p', 0x42, unsigned long)
#define BMDEV_TRACE_DUMP		_IOWR('p', 0x43, unsigned long)
#define BMDEV_TRACE_DUMP_ALL		_IOWR('p', 0x44, unsigned long)
#define BMDEV_ENABLE_PERF_MONITOR       _IOWR('p', 0x45, unsigned long)
#define BMDEV_DISABLE_PERF_MONITOR      _IOWR('p', 0x46, unsigned long)
#define BMDEV_GET_DEVICE_TIME           _IOWR('p', 0x47, unsigned long)

#define BMDEV_SET_TPU_DIVIDER		_IOWR('p', 0x50, unsigned long)
#define BMDEV_SET_MODULE_RESET		_IOWR('p', 0x51, unsigned long)
#define BMDEV_SET_TPU_FREQ		_IOWR('p', 0x52, unsigned long)
#define BMDEV_GET_TPU_FREQ		_IOWR('p', 0x53, unsigned long)

#define BMDEV_TRIGGER_VPP               _IOWR('p', 0x60, unsigned long)
#define BMDEV_TRIGGER_SPACC             _IOWR('p', 0x61, unsigned long)
#define BMDEV_SECKEY_VALID              _IOWR('p', 0x62, unsigned long)

#define BMDEV_EFUSE_WRITE               _IOWR('p', 0x66, unsigned long)
#define BMDEV_EFUSE_READ                _IOWR('p', 0x67, unsigned long)

#define BMDEV_BASE64_PREPARE            _IOWR('p', 0x70, unsigned long)
#define BMDEV_BASE64_START              _IOWR('p', 0x71, unsigned long)
#define BMDEV_BASE64_CODEC              _IOWR('p', 0x72, unsigned long)

#define BMDEV_I2C_READ_SLAVE            _IOWR('p', 0x73, unsigned long)
#define BMDEV_I2C_WRITE_SLAVE           _IOWR('p', 0x74, unsigned long)

#define BMDEV_GET_HEAP_STAT_BYTE        _IOWR('p', 0x75, unsigned long)
#define BMDEV_GET_HEAP_NUM              _IOWR('p', 0x76, unsigned long)

#define BMDEV_I2C_SMBUS_ACCESS          _IOWR('p', 0x77, unsigned long)

#define BMDEV_SET_BMCPU_STATUS          _IOWR('p', 0x7F, unsigned long)
#define BMDEV_GET_BMCPU_STATUS          _IOWR('p', 0xAB, unsigned long)
#define BMDEV_TRIGGER_BMCPU             _IOWR('p', 0x80, unsigned long)
#define BMDEV_SETUP_VETH                _IOWR('p', 0xA0, unsigned long)
#define BMDEV_RMDRV_VETH                _IOWR('p', 0xA1, unsigned long)
#define BMDEV_FORCE_RESET_A53           _IOWR('p', 0xA2, unsigned long)
#define BMDEV_SET_FW_MODE               _IOWR('p', 0xA3, unsigned long)
#define BMDEV_GET_VETH_STATE            _IOWR('p', 0xA4, unsigned long)
#define BMDEV_COMM_READ                 _IOWR('p', 0xA5, unsigned long)
#define BMDEV_COMM_WRITE                _IOWR('p', 0xA6, unsigned long)
#define BMDEV_COMM_READ_MSG             _IOWR('p', 0xA7, unsigned long)
#define BMDEV_COMM_WRITE_MSG            _IOWR('p', 0xA8, unsigned long)
#define BMDEV_COMM_CONNECT_STATE        _IOWR('p', 0xA9, unsigned long)
#define BMDEV_COMM_SET_CARDID           _IOWR('p', 0xAA, unsigned long)
#define BMDEV_SET_IP			_IOWR('p', 0xAC, unsigned long)
#define BMDEV_SET_GATE                  _IOWR('p', 0xAD, unsigned long)

#define BMDEV_GET_TPUC                  _IOWR('p', 0x81, unsigned long)
#define BMDEV_GET_MAXP                  _IOWR('p', 0x82, unsigned long)
#define BMDEV_GET_BOARDP                _IOWR('p', 0x83, unsigned long)
#define BMDEV_GET_FAN                   _IOWR('p', 0x84, unsigned long)
#define BMDEV_GET_CORRECTN              _IOR('p', 0x85, unsigned long)
#define BMDEV_GET_12V_ATX               _IOR('p', 0x86, unsigned long)
#define BMDEV_GET_SN                    _IOR('p', 0x87, unsigned long)
#define BMDEV_GET_STATUS                _IOR('p', 0x88, unsigned long)
#define BMDEV_GET_TPU_MINCLK            _IOR('p', 0x89, unsigned long)
#define BMDEV_GET_TPU_MAXCLK            _IOR('p', 0x8A, unsigned long)
#define BMDEV_GET_DRIVER_VERSION        _IOR('p', 0x8B, unsigned long)
#define BMDEV_GET_BOARD_TYPE            _IOR('p', 0x8C, unsigned long)
#define BMDEV_GET_BOARDT                _IOR('p', 0x8D, unsigned long)
#define BMDEV_GET_CHIPT                 _IOR('p', 0x8E, unsigned long)
#define BMDEV_GET_TPU_P                 _IOR('p', 0x8F, unsigned long)
#define BMDEV_GET_TPU_V                 _IOR('p', 0x90, unsigned long)
#define BMDEV_GET_CARD_ID               _IOR('p', 0x91, unsigned long)
#define BMDEV_GET_DYNFREQ_STATUS        _IOR('p', 0x92, unsigned long)
#define BMDEV_CHANGE_DYNFREQ_STATUS     _IOR('p', 0x93, unsigned long)
#define BMDEV_GET_VERSION           	_IOR('p', 0x94, unsigned long)
#define BMDEV_LOADED_LIB		_IOR('p', 0x95, unsigned long)
#define BMDEV_GET_SMI_ATTR              _IOR('p', 0x96, unsigned long)

#define BMCTL_GET_DEV_CNT               _IOR('q', 0x0, unsigned long)
#define BMCTL_GET_SMI_ATTR              _IOWR('q', 0x01, unsigned long)
#define BMCTL_SET_LED                   _IOWR('q', 0x02, unsigned long)
#define BMCTL_TEST_I2C1                 _IOWR('q', 0x9, unsigned long) // test
#define BMCTL_SET_ECC                   _IOWR('q', 0x03, unsigned long)
#define BMCTL_GET_PROC_GMEM             _IOWR('q', 0x04, unsigned long)
#define BMCTL_DEV_RECOVERY              _IOWR('q', 0x05, unsigned long)
#define BMCTL_GET_DRIVER_VERSION        _IOR('q', 0x06, unsigned long)
#define BMCTL_GET_CARD_INFO             _IOR('q', 0x07, unsigned long)
#define BMCTL_GET_CARD_NUM              _IOR('q', 0x08, unsigned long)

#ifdef TEST_VPU_ONECORE_FPGA
#define MAX_NUM_VPU_CORE                1               /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP           MAX_NUM_VPU_CORE
#define MAX_NUM_VPU_CORE_BM1686         1               /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP_BM1686    MAX_NUM_VPU_CORE_BM1686
#else
#define MAX_NUM_VPU_CORE                5               /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP           MAX_NUM_VPU_CORE
#define MAX_NUM_VPU_CORE_BM1686         3               /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP_BM1686    MAX_NUM_VPU_CORE_BM1686
#endif
#define MAX_NUM_JPU_CORE 4
#define MAX_NUM_JPU_CORE_BM1686 2
struct bm_smi_attr {
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
	char board_type[6];

	/* vpu mem and instant info*/
	int vpu_instant_usage[MAX_NUM_VPU_CORE];
	int64_t vpu_mem_total_size[2];
	int64_t vpu_mem_free_size[2];
	int64_t vpu_mem_used_size[2];

	int jpu_core_usage[MAX_NUM_JPU_CORE];

	/*if or not to display board endline and board attr*/
	int board_endline;
	int board_attr;

	/* vpp chronic and instant usage info*/
	int vpp_instant_usage[VPP_CORE_MAX];
	int vpp_chronic_usage[VPP_CORE_MAX];

	bm_dev_stat_t stat;
};

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
	uint32_t     ctrl;
	uint32_t     alg;
	uint64_t     next;
	uint64_t     src;
	uint64_t     dst;
	uint64_t     len;
	union {
		uint8_t      key[32];
		uint64_t     dstlen; //destination amount, only used in base64
	};
	uint8_t      iv[16];
};

struct ce_base {
	uint64_t     src;
	uint64_t     dst;
	uint64_t     len;
	bool direction;
};

struct ce_reg {
	uint32_t ctrl; //control
	uint32_t intr_enable; //interrupt mask
	uint32_t desc_low; //descriptor base low 32bits
	uint32_t desc_high; //descriptor base high 32bits
	uint32_t intr_raw; //interrupt raw, write 1 to clear
	uint32_t se_key_valid; //secure key valid, read only
	uint32_t cur_desc_low; //current reading descriptor
	uint32_t cur_desc_high; //current reading descriptor
	uint32_t r1[24]; //reserved
	uint32_t desc[22]; //PIO mode descriptor register
	uint32_t r2[10]; //reserved
	uint32_t key0[8];//keys
	uint32_t key1[8];//keys
	uint32_t key2[8];//keys		//keys
	uint32_t r3[8]; //reserved
	uint32_t iv[12]; //iv
	uint32_t r4[4]; //reserved
	uint32_t sha_param[8]; //sha parameter
	uint32_t sm3_param[8]; //sm3 parameter
};

//for i2c
struct bm_i2c_param {
	int i2c_slave_addr;
	int i2c_index;
	int i2c_cmd;
	int value;
	int op;
};

struct bm_i2c_smbus_ioctl_info
{
  int i2c_slave_addr; /*i2c slave address to read or write*/
  int i2c_index; /*i2c id*/
  int i2c_cmd; /*pointer register*/
  int length; /*length of data to read or write*/
  unsigned char data[32]; /*buf of read or write*/
  int op; /*0:write or 1:read*/
};

struct bm_efuse_param{
	unsigned char addr;
	unsigned int *data;
	unsigned int datalen;
};

#endif /* _BM_UAPI_H_ */
