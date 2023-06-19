#ifndef BM_INTERNAL_H_
#define BM_INTERNAL_H_

//#include <set>
#include "bmlib_type.h"
#include "bmlib_runtime.h"
#include "bmlib_profile.h"
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>
#include <guiddef.h>
#include "window/bmlib_ioctl.h"
//#include "..\..\common\bm1684\include_win\common_win.h"
#else
#include "linux/bmlib_ioctl.h"
#endif
#ifdef USING_CMODEL
#include "bmlib_device.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef _WIN32
struct ion_allocation_data {
    unsigned long long len;
    unsigned int       heap_id_mask;
    unsigned int       flags;
    unsigned int       fd;
    unsigned int       heap_id;
    unsigned long long paddr;
};
#endif

#define ION_MAX_HEAP_CNT    3

#define RDBUF_SIZE 672
struct product_config {
  char sn[32];
  char mac0[32];
  char mac1[32];
  char product_type[32];
  char aging_flag[32];
  char reserve_string_2[32];
  char reserve_string_3[32];
  char reserve_string_4[32];
  char ddr_type[32];
  char board_type[32];
  char bom[32];
  char module_type[32];
  char ex_module_type[32];
  char product[32];
  char vender[32];
  char algorithm[32];
  char manufacture[32];
  char date_production[32];
  char passw_ssh[32];
  char user_name[32];
  char passw[32];
};

typedef enum {
  BMLIB_NOT_USE_IOMMU = 0,
  BMLIB_PRE_ALLOC_IOMMU,
  BMLIB_USER_SETUP_IOMMU
} bm_cdma_iommu_mode;

#define DRIVER_MAJOR_MIN 1  // min of required drvier version
#define DRIVER_MINOR_MIN 0

#define DRIVER_MAJOR_MAX 1  // max of required driver version
#define DRIVER_MINOR_MAX 1

#define DRIVER_MAJOR_VERSION(a) ((((u32)a) & 0xFFFF0000) >> 16)
#define DRIVER_MINOR_VERSION(a) (((u32)a) & 0x0000FFFF)

#define DRIVER_VERSION(major, minor) ((major << 16) | minor)

#define DRIVER_VERSION_MIN  DRIVER_VERSION(DRIVER_MAJOR_MIN, DRIVER_MINOR_MIN)
#define DRIVER_VERSION_MAX  DRIVER_VERSION(DRIVER_MAJOR_MAX, DRIVER_MINOR_MAX)

#define BMLIB_VERSION_MAJOR 1
#define BMLIB_VERSION_MINOR 0
#define BMLIB_VERSION          (BMLIB_VERSION_MAJOR << 16 | BMLIB_VERSION_MINOR)

typedef struct vpp_fd_st {
    int dev_fd;/*vpp dev fd*/
    char *name;/*if u fill in the value of vpp_dev_fd,u must fill in vpp_dev_name as bm-vpp*/
}vpp_fd_t;

typedef struct bm_context {
#ifdef __linux__
 /* reserved memory table for cv */
  bm_device_mem_t    bilinear_coeff_table_dev_mem;
  bm_device_mem_t    image_scale_reserved_mem;
  bm_device_mem_t    warp_affine_constant_mem;
  /* reserved memory table for blas */
  bm_device_mem_t    index_table_dev_mem;

	#ifdef USING_CMODEL
	  bm_device *bm_dev;
	#else
	  int dev_fd;
	  int ion_fd;
	  int spacc_fd;
	  int heap_cnt;
	  int carveout_heap_id[ION_MAX_HEAP_CNT];
	  struct bm_misc_info misc_info;
	  bm_cdma_iommu_mode cdma_iommu_mode;
	  vpp_fd_t vpp_fd;
	#endif
  	int dev_id;
 #else
    u32 dev_id;
	HDEVINFO                         hDevInfo;
    PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetail;
    HANDLE                           hDevice;
    struct bm_misc_info              misc_info;
    bm_cdma_iommu_mode               cdma_iommu_mode;
#endif
    bmlib_profile_t *profile;
} bm_context_t, *bm_handle_t;

DECL_EXPORT bm_status_t bm_send_api(
  bm_handle_t  handle,
  int api_id,
  const u8     *api,
  u32          size);

DECL_EXPORT bm_status_t bm_send_api_ext(
  bm_handle_t  handle,
  int api_id,
  const u8     *api,
  u32          size,
  u64 *api_handle);

struct ce_cipher {
	  int			  alg;
	  //set 0 to decryption, otherwise encryption
	  int	  enc;
	  void			  *iv;
	  void			  *key;
	  void			  *src;
	  void			  *dst;
	  unsigned long   len;
	  void		  *iv_out;
};

struct ce_hash {
	  int		  alg;
	  void			  *init;
	  void			  *src;
	  unsigned long 	  len;
	  void			  *hash;
};

  //Base N -- Base16 Base32 Base64
  //Now only stdardard base64 is supported which defined in RFC4648 section 4
struct ce_base64 {
	  int			  alg;
	  //set 0 to decryption, otherwise encryption
	  int	  enc;
	  void			  *src;
	  void			  *dst;
	  unsigned long   len;
	  //only used in decode mode, crypto engine is a dma device.
	  //it accepts physicall address, so it cannot access data using cpu.
	  //so the caller should tell me real output size.
	  unsigned long   dstlen;
};

typedef enum ce_type {
	  CE_CIPER=0,
	  CE_HASH,
	  CE_BASE
}ce_type_t;

struct spacc_batch {
	  ce_type_t cetype;
	  void* user_dst;
	  union{
	  struct ce_cipher ce_cipher_t;
	  struct ce_hash ce_hash_t;
	  struct ce_base64 ce_base_t;
	  }u;
};

struct bm_efuse_param{
	unsigned char addr;
	unsigned int *data;
	unsigned int datalen;
};

typedef struct loaded_lib {
  unsigned char md5[MD5SUM_LEN];
  int loaded;
} loaded_lib_t;

DECL_EXPORT bm_status_t bm_sync_api(bm_handle_t handle);

u64 bm_get_version(bm_handle_t handle);

bm_status_t bm1682_dev_request(bm_context_t *ctx);
void bm1682_dev_free(bm_context_t *ctx);

bm_status_t bm_set_clk_tpu_divider(bm_handle_t handle, int divider);

/*
 * bm boot info
 */
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
  unsigned int ddr_mode;/*[31:16]-ddr_gmem_mdoe, [15:0]-ddr_power_mode*/
  unsigned int ddr_rank_mode;
  unsigned int tpu_max_clk;
  unsigned int tpu_min_clk;
  unsigned int temp_sensor_exist;
  unsigned int chip_power_sensor_exist;
  unsigned int board_power_sensor_exist;
  unsigned int fan_exist;
  unsigned int max_board_power;
  unsigned int boot_info_version; /*[31:16]-0xabcd,[15:0]-version num*/
  union {
    struct boot_info_append_v1 append_v1;
  } append;
};

bm_status_t bm_get_boot_info(bm_handle_t handle, bm_boot_info *pboot_info);
bm_status_t bm_update_boot_info(bm_handle_t handle, bm_boot_info *pboot_info);
bm_status_t bm_get_device_time_us(bm_handle_t handle,
                                            unsigned long *time_us);

void bm_enable_iommu(bm_handle_t handle);
void bm_disable_iommu(bm_handle_t handle);

struct bm_reg {
  int reg_addr;
  int reg_value;
};

struct bm_card {
        int card_index;
        int chip_num;
        int running_chip_num;
        int dev_start_index;
        int board_power;
        int fan_speed;
        int atx12v_curr;
        int board_max_power;
	int cdma_max_payload;
        char sn[18];
        struct bm_device_info *sc5p_mcu_bmdi;
        struct bm_device_info *card_bmdi[32];
};

bm_status_t bm_get_reg(bm_handle_t handle, struct bm_reg *p_reg);
bm_status_t bm_set_reg(bm_handle_t handle, struct bm_reg *p_reg);

typedef struct bm_fw_desc {
  unsigned int *itcm_fw;
  int itcmfw_size;  // bytes
  unsigned int *ddr_fw;
  int ddrfw_size;  // bytes
} bm_fw_desc, *pbm_fw_desc;
bm_status_t bm_update_firmware_a9(bm_handle_t handle, pbm_fw_desc pfw);

bm_status_t bmkernel_init(bm_handle_t *handle, int dev_id);
void bmkernel_deinit(bm_handle_t handle);


struct bm_trace_item_data {
  int trace_type; /*0---cdma; 1---api*/
  #ifdef __linux__
  unsigned long sent_time;
  unsigned long start_time;
  unsigned long end_time;
  #else
  unsigned long long sent_time;
  unsigned long long start_time;
  unsigned long long end_time;
  #endif
  int api_id;
  int cdma_dir;
};

struct bm_smi_attr_t {
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
  u32 tpu_power;
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
  int vpu_instant_usage[5];
  signed long long vpu_mem_total_size[2];
  signed long long vpu_mem_free_size[2];
  signed long long vpu_mem_used_size[2];

  int jpu_core_usage[4];

  /*if or not to display board endline and board attr*/
  int board_endline;
  int board_attr;

  int vpp_instant_usage[2];
  int vpp_chronic_usage[2];

  bm_dev_stat_t stat;
};

/*this struct is also defined in driver/bm_uapi.h.*/
struct bm_i2c_param {
  int i2c_slave_addr; /*i2c slave address to be read*/
  int i2c_index; /*number of i2c*/
  int i2c_cmd; /*pointer register*/
  int value; /*return value read write by i2c*/
  int op; /*0:read or 1:write*/
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

/**
 * @name    bm_trace_enable
 * @brief   To enable trace for the current thread.
 * @ingroup bmlib_runtime
 *
 * @parama [in] handle  The device handle
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
bm_status_t bm_trace_enable(bm_handle_t handle);

/**
 * @name    bm_trace_disable
 * @brief   To disable trace for the current thread.
 * @ingroup bmlib_runtime
 *
 * @parama [in] handle  The device handle
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
bm_status_t bm_trace_disable(bm_handle_t handle);

/**
 * @name    bm_traceitem_number
 * @brief   To get the number of traced items of the current thread
 * @ingroup bmlib_runtime
 *
 * @param [in]  handle  The device handle
 * @param [out] number  The number of traced items
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
bm_status_t bm_traceitem_number(bm_handle_t handle, long long *number);

/**
 * @name    bm_trace_dump
 * @brief   To get one traced item data
 *          (the oldest recorded item of the current thread).
 *          Once fetched, this item data is no longer recorded in kernel.
 * @ingroup  bmlib_runtime
 *
 * @param [in]  handle     The device handle
 * @param [out] trace_data The traced data
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
bm_status_t bm_trace_dump(bm_handle_t handle, struct bm_trace_item_data *trace_data);

/**
 * @name    bm_trace_dump_all
 * @brief   To get all the traced item data of the current thread.
 *          Once fetched, the item data is no longer recorded in kernel.
 *          (bm_traceitem_number should be called first to determine how many
 *          items are available. Buffer should be allocated to store the
 *          traced item data).
 * @ingroup  bmlib_runtime
 *
 * @param [in]  handle     The device handle
 * @param [out] trace_data The traced data
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
bm_status_t bm_trace_dump_all(bm_handle_t handle,
                              struct bm_trace_item_data *trace_data);


/**
 * @name    bm_set_module_reset
 * @brief   To reset TPU module. (Only valid in PCIE mode)
 * @ingroup bmlib_runtime
 *
 * @param [in]  handle  The device handle
 * @param [in]  module  The ID of module to reset
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
bm_status_t bm_set_module_reset(bm_handle_t handle, MODULE_ID module);

/**
 * @name    bm_trigger_vpp
 * @brief   start vpp hw
 * @ingroup bmlib_runtime
 *
 * @param [in]  handle  The device handle
 * @param [in]  vpp param
 * @retval  BM_SUCCESS  Succeeds.
 *          Other code  Fails.
 */
DECL_EXPORT bm_status_t bm_trigger_vpp(bm_handle_t handle, struct vpp_batch_n* batch);

DECL_EXPORT bm_status_t bm_trigger_spacc(bm_handle_t handle, struct spacc_batch* batch);

DECL_EXPORT bm_status_t bm_is_seckey_valid(bm_handle_t handle, int* is_valid);

DECL_EXPORT bm_status_t bm_efuse_write(bm_handle_t handle, struct bm_efuse_param *efuse_param);

DECL_EXPORT bm_status_t bm_efuse_read(bm_handle_t handle, struct bm_efuse_param *efuse_param);

bm_status_t bm_handle_i2c_read(bm_handle_t handle, struct bm_i2c_param *i2c_param);

bm_status_t bm_handle_i2c_write(bm_handle_t handle, struct bm_i2c_param *i2c_param);

bm_status_t bm_handle_i2c_access(bm_handle_t handle, struct bm_i2c_smbus_ioctl_info *i2c_buf);

bm_status_t bmlib_log_mutex_lock(void);

bm_status_t bmlib_log_mutex_unlock(void);

DECL_EXPORT int platform_ioctl(bm_handle_t handle, u32 commandID, void *param);

#ifdef _WIN32
DECL_EXPORT int gettimeofday(struct timeval *tp, void *tzp);

DECL_EXPORT int  clock_gettime(int dummy, struct timespec *ct);

DECL_EXPORT void timeradd(struct timeval *a, struct timeval *b, struct timeval *res);

DECL_EXPORT void timersub(struct timeval *a, struct timeval *b, struct timeval *res);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* BM_INTERNAL_H_ */
