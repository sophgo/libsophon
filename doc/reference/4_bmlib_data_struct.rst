相关数据结构定义
================

bm_status_t
-----------

typedef enum {

BM_SUCCESS = 0,

BM_ERR_DEVNOTREADY = 1, /\* Device not ready yet \*/

BM_ERR_FAILURE = 2, /\* General failure \*/

BM_ERR_TIMEOUT = 3, /\* Timeout \*/

BM_ERR_PARAM = 4, /\* Parameters invalid \*/

BM_ERR_NOMEM = 5, /\* Not enough memory \*/

BM_ERR_DATA = 6, /\* Data error \*/

BM_ERR_BUSY = 7, /\* Busy \*/

BM_ERR_NOFEATURE = 8, /\* Not supported yet \*/

BM_NOT_SUPPORTED = 9

} bm_status_t;

bm_mem_type_t
-------------

typedef enum {

BM_MEM_TYPE_DEVICE = 0,

BM_MEM_TYPE_HOST = 1,

BM_MEM_TYPE_SYSTEM = 2,

BM_MEM_TYPE_INT8_DEVICE = 3,

BM_MEM_TYPE_INVALID = 4

} bm_mem_type_t;

bm_mem_flags_t
--------------

typedef union {

struct {

bm_mem_type_t mem_type : 3;

unsigned int reserved : 29;

} u;

unsigned int rawflags;

} bm_mem_flags_t;

bm_mem_desc_t
-------------

typedef struct bm_mem_desc {

union {

struct {

unsigned long device_addr;

unsigned int reserved;

int dmabuf_fd;

} device;

struct {

void \*system_addr;

unsigned int reserved0;

int reserved1;

} system;

} u;

bm_mem_flags_t flags;

unsigned int size;

} bm_mem_desc_t;

bm_misc_info
------------

struct bm_misc_info {

int pcie_soc_mode; /*0---pcie; 1---soc*/

int ddr_ecc_enable; /*0---disable; 1---enable*/

unsigned int chipid;

#define BM1682_CHIPID_BIT_MASK (0X1 << 0)

#define BM1684_CHIPID_BIT_MASK (0X1 << 1)

unsigned long chipid_bit_mask;

unsigned int driver_version;

int domain_bdf;

};

bm_profile_t
------------

typedef struct bm_profile {

unsigned long cdma_in_time;

unsigned long cdma_in_counter;

unsigned long cdma_out_time;

unsigned long cdma_out_counter;

unsigned long tpu_process_time;

unsigned long sent_api_counter;

unsigned long completed_api_counter;

} bm_profile_t;

bm_heap_stat
------------

struct bm_heap_stat {

unsigned int mem_total;

unsigned int mem_avail;

unsigned int mem_used;

}

bm_dev_stat_t
-------------

typedef struct bm_dev_stat {

int mem_total;

int mem_used;

int tpu_util;

int heap_num;

struct bm_heap_stat heap_stat[4];

} bm_dev_stat_t;

bm_log_level
------------

#define BMLIB_LOG_QUIET -8

#define BMLIB_LOG_PANIC 0

#define BMLIB_LOG_FATAL 8

#define BMLIB_LOG_ERROR 16

#define BMLIB_LOG_WARNING 24

#define BMLIB_LOG_INFO 32

#define BMLIB_LOG_VERBOSE 40

#define BMLIB_LOG_DEBUG 48

#define BMLIB_LOG_TRACE 56
