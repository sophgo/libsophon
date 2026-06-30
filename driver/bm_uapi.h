#ifndef _BM_UAPI_H_
#define _BM_UAPI_H_
#include <linux/types.h>

typedef struct bm_profile {
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


/*
 * bm misc info
 */
struct bm_misc_info {
	int pcie_soc_mode; /*0---pcie; 1---soc*/
	unsigned int chipid;
	int tpu_core_num;
#define SGTPUV8_CHIPID_BIT_MASK	(0X1 << 3)
	unsigned long chipid_bit_mask;
	unsigned int driver_version;
	int domain_bdf; /*[31:16]-domin,[15:8]-bus_id,[7:3]-device_id,[2:0]-func_num*/
	int board_version; /*hardware board version [23:16]-mcu sw version, [15:8]-board type, [7:0]-hw version*/
	int a53_enable;
	int dyn_enable;
};
struct bm_heap_info {
	unsigned int heap_id;
	unsigned long long mem_start_addr;
	unsigned long long mem_size;
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

#define BMDEV_IOCTL_MAGIC  'p'

#define BMDEV_ALLOC_GMEM		_IOWR('p', 0x10, unsigned long)
#define BMDEV_FREE_GMEM			_IOW('p', 0x11, unsigned long)
#define BMDEV_TOTAL_GMEM		_IOWR('p', 0x12, unsigned long)
#define BMDEV_AVAIL_GMEM		_IOWR('p', 0x13, unsigned long)
#define BMDEV_REQUEST_ARM_RESERVED	_IOWR('p', 0x14, unsigned long)
#define BMDEV_ALLOC_GMEM_ION		_IOW('p', 0x19, unsigned long)
#define BMDEV_GMEM_ADDR		        _IOW('p', 0x1a, unsigned long)
#define BMDEV_GET_MISC_INFO            _IOWR('p', 0x30, unsigned long)
#define BMDEV_GET_DEV_STAT              _IOWR('p', 0x3e, unsigned long)
#define BMDEV_SET_TPU_DIVIDER		_IOWR('p', 0x50, unsigned long)
#define BMDEV_SET_MODULE_RESET		_IOWR('p', 0x51, unsigned long)
#define BMDEV_GET_TPUC                  _IOWR('p', 0x81, unsigned long)
#define BMDEV_GET_BOARDP                _IOWR('p', 0x83, unsigned long)
#define BMDEV_GET_STATUS                _IOR('p', 0x88, unsigned long)
#define BMDEV_GET_BOARD_TYPE            _IOR('p', 0x8C, unsigned long)
#define BMDEV_GET_DRIVER_VERSION        _IOR('p', 0x8B, unsigned long)
#define BMDEV_GET_BOARDT                _IOR('p', 0x8D, unsigned long)
#define BMDEV_GET_CHIPT                 _IOR('p', 0x8E, unsigned long)
#define BMDEV_GET_TPU_P                 _IOR('p', 0x8F, unsigned long)
#define BMDEV_GET_TPU_V                 _IOR('p', 0x90, unsigned long)
#define BMDEV_GET_CARD_ID               _IOR('p', 0x91, unsigned long)
#define BMDEV_GET_DYNFREQ_STATUS        _IOR('p', 0x92, unsigned long)
#define BMDEV_CHANGE_DYNFREQ_STATUS     _IOR('p', 0x93, unsigned long)
#define BMDEV_SET_IOMAP_TPYE            _IOWR('p', 0x99, u_int)
#define BMDEV_SET_TPU_EVENT             _IOW('p', 0x105, unsigned long)
#define BMCTL_GET_DEV_CNT               _IOR('q', 0x0, unsigned long)
#define BMCTL_GET_SMI_ATTR              _IOWR('q', 0x01, unsigned long)
#define BMDEV_GET_HEAP_INFO                   _IOWR('q', 0x02, unsigned long)
#define BMCTL_GET_PROC_GMEM             _IOWR('q', 0x04, unsigned long)
#define BMCTL_GET_DRIVER_VERSION        _IOR('q', 0x06, unsigned long)
#define BMCTL_GET_CARD_NUM              _IOR('q', 0x08, unsigned long)
#define BMCTL_SET_GDMA_EVENT            _IOW('q', 0x09, unsigned long)
#define BMDEV_FORCE_RESET_TPU         _IOWR('p', 0xAE, unsigned long)



#ifdef TEST_VPU_ONECORE_FPGA
#define MAX_NUM_VPU_CORE                1               /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP           MAX_NUM_VPU_CORE
#else
#define MAX_NUM_VPU_CORE                5               /* four wave cores */
#define MAX_NUM_VPU_CORE_CHIP           MAX_NUM_VPU_CORE
#endif

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
	char sn[18];
	char board_type[6];
	/*if or not to display board endline and board attr*/
	int board_endline;
	int board_attr;
	bm_dev_stat_t stat;
};

struct bm_smi_proc_gmem {
	int dev_id;
	pid_t pid[128];
	u64 gmem_used[128];
	int proc_cnt;
};

#endif /* _BM_UAPI_H_ */
