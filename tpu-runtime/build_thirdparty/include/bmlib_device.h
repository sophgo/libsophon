#ifndef BM_DEVICE_H_
#define BM_DEVICE_H_

#ifdef USING_CMODEL
#include <dlfcn.h>
#include "bmlib_runtime.h"
#include "bmlib_mmpool.h"
#include "bmlib_internal.h"
#include "bmlib_type.h"
#include <map>
#include <queue>
#include <atomic>

#ifdef __cplusplus
extern "C"
{
#endif

#define DEVICE_SYNC_MARKER 0xFFFF
#define MAX_DEVICE_NUM 0x2

  struct thread_api_info
  {
    pthread_t thd_id;
    u32 last_seq;
    u32 cpl_seq;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    thread_api_info(pthread_t tid, u32 lsq, u32 csq)
        : thd_id(tid), last_seq(lsq), cpl_seq(csq)
    {
    }
  };

  struct api_queue_entry
  {
    pthread_t thd_id;
    u32 thd_seq;
    u32 dev_seq;
  };

  class bm_device
  {
  public:
    bm_device(int _dev_id);
    ~bm_device();
    int bm_device_id()
    {
      return dev_id;
    }
    int bm_core_num() { return core_num; }
    bm_status_t bm_device_send_api(int api_id, const u8 *api, u32 size, int core_idx = 0);
    bm_status_t bm_device_sync();
    bm_status_t bm_device_thread_sync_from_core(int core_idx);
    u64 bm_device_alloc_mem(u64 size);
    void bm_device_free_mem(u64 addr);
    bm_status_t bm_device_memcpy_s2d(bm_device_mem_t dst, void *src, int core_idx = 0);
    bm_status_t bm_device_memcpy_d2s(void *dst, bm_device_mem_t src, int core_idx = 0);
    bm_status_t bm_device_memcpy_s2d_u64(bm_device_mem_u64_t dst, void *src, int core_idx = 0);
    bm_status_t bm_device_memcpy_d2s_u64(void *dst, bm_device_mem_u64_t src, int core_idx = 0);
    bm_status_t sg_device_memcpy_s2d(sg_device_mem_t dst, void *src, int core_idx = 0);
    bm_status_t sg_device_memcpy_d2s(void *dst, sg_device_mem_t src, int core_idx = 0);
    u64 bm_device_arm_reserved_req();
    void bm_device_arm_reserved_rel();
    void cmodel_setup(void);
    typedef void *(*t_get_global_memaddr)(int);
    typedef int (*t_cmodel_init)(int, unsigned long long);
    typedef void (*t_set_cur_nodechip_idx)(int);
    typedef void (*t_cmodel_nodechip_runtime_init)(int);
    typedef void (*t_cmodel_nodechip_runtime_exit)(int);
    typedef void (*t_cmodel_deinit)(int);
    typedef u32 *(*t_cmodel_get_share_memory_addr)(u32, int);
    typedef void (*t_cmodel_write_share_reg)(u32, u32, int);
    typedef void (*t_cmodel_write_share_memory)(u32, u32, int);
    typedef u32 (*t_cmodel_read_share_reg)(u32, int);
    typedef void (*t_host_dma_copy_s2d_cmodel)(u64, void *, u64, u32);
    typedef void (*t_host_dma_copy_d2s_cmodel)(void *, u64, u64, u32);
    typedef void (*t_cmodel_api_poll)(int);
    typedef u32 (*t_cmodel_get_chip_id)(void);
    typedef u32 (*t_cmodel_share_reg_func)();
    typedef void (*t_cmodel_wait_)(void *, u64, u64, u32);
    typedef void (*t_cmodel_wait_share_reg_equal)(u32, int, int, int, int);
    typedef int (*t_cmodel_get_total_nodechip_num)(void);
    typedef u64 (*t_cmodel_get_gmem_start_addr)();
    typedef int (*t_cmodel_get_last_func_id)(int);

    t_cmodel_get_gmem_start_addr cmodel_get_gmem_start_addr_;
    t_get_global_memaddr get_global_memaddr_;
    t_cmodel_init cmodel_init_;
    t_set_cur_nodechip_idx set_cur_nodechip_idx_;
    t_cmodel_nodechip_runtime_init cmodel_nodechip_runtime_init_;
    t_cmodel_nodechip_runtime_exit cmodel_nodechip_runtime_exit_;
    t_cmodel_deinit cmodel_deinit_;
    t_cmodel_get_share_memory_addr cmodel_get_share_memory_addr_;
    t_cmodel_write_share_reg cmodel_write_share_reg_;
    t_cmodel_write_share_memory cmodel_write_share_memory_;
    t_cmodel_read_share_reg cmodel_read_share_reg_;
    t_host_dma_copy_d2s_cmodel host_dma_copy_d2s_cmodel_;
    t_host_dma_copy_s2d_cmodel host_dma_copy_s2d_cmodel_;
    t_cmodel_api_poll cmodel_api_poll_;
    t_cmodel_api_poll cmodel_api_signal_;
    t_cmodel_api_poll cmodel_api_signal_begin_;
    t_cmodel_get_chip_id cmodel_get_chip_id_;

    t_cmodel_share_reg_func cmodel_share_reg_message_rp_;
    t_cmodel_share_reg_func cmodel_share_reg_message_wp_;
    t_cmodel_share_reg_func cmodel_share_reg_fw_status_;
    t_cmodel_wait_share_reg_equal cmodel_wait_share_reg_equal_;
    t_cmodel_get_total_nodechip_num cmodel_get_total_nodechip_num_;
    t_cmodel_get_last_func_id cmodel_get_last_func_id;

    u64 global_gmem_size;
    u32 share_reg_message_wp;
    u32 share_reg_message_rp;
    u32 sharemem_size_bit;
    u32 sharemem_mask;
    u32 api_message_empty_slot_num;
    u32 share_reg_fw_status;
    u32 last_ini_reg_val;
    u32 reserved_ddr_arm;
    u32 chip_id;

  private:
    /* add lock */
    void _write_share_mem(u32 offset, u32 data, int core_idx);
    void _write_share_reg(u32 idx, u32 data, int core_idx);
    u32 _read_share_reg(u32 idx, int core_idx);

    u32 _poll_message_fifo_cnt(int core_idx);
    void copy_message_to_sharemem(const u32 *src_msg_buf,
                                  u32 *wp, u32 size, u32 api_id, int core_idx);

    bm_status_t bm_send_quit_message(int core_idx);
    void bm_wait_fwinit_done(int core_idx);

    bm_status_t bm_alloc_arm_reserved();
    void bm_free_arm_reserved();
    bm_status_t bm_alloc_instr_reserved();
    void bm_free_instr_reserved();
    bm_status_t bm_alloc_iommu_reserved();
    void bm_free_iommu_reserved();
    bm_status_t bm_malloc_device_dword(bm_device_mem_t *pmem, int cnt);
    void bm_free_device(bm_device_mem_t mem);
    bm_status_t bm_init_l2_sram();

    thread_api_info *bm_get_thread_api_info(pthread_t thd_id, int core_idx);
    bm_status_t bm_add_thread_api_info(pthread_t thd_id, int core_idx);
    bm_status_t bm_remove_thread_api_info(pthread_t thd_id, int core_idx);

    static void *bm_msg_done_poll(void *arg);

    int dev_id;
    int core_num;
    // u64                device_mem_size;
    bm_mem_pool device_mem_pool;
    bm_device_mem_t instr_reserved_mem;
    bm_device_mem_t arm_reserved_dev_mem;
    bm_device_mem_t iommu_reserved_dev_mem;
    /* arm reserved memory lock */
    pthread_mutex_t arm_reserved_lock;

    struct msg_done_poll_param_t
    {
      bm_device *dev;
      int core_idx;
    };
    pthread_t *msg_poll_threads;
    msg_done_poll_param_t *msg_done_poll_params;

    pthread_mutex_t *api_locks;
    std::map<pthread_t, thread_api_info> *thread_api_tables;
    std::queue<api_queue_entry> *pending_api_queues;
    volatile u32 *device_sync_last;
    std::atomic<u32> *device_sync_cpl;
  };

  class bm_device_manager
  {
  public:
    bm_device *get_bm_device(int dev_id);
    void free_bm_device(int dev_id);
    static bm_device_manager *get_dev_mgr();
    static void destroy_dev_mgr();

  private:
    bm_device_manager(int max_dev_cnt);
    ~bm_device_manager();

    int dev_cnt;
    int max_dev_cnt;
    bm_device **bm_dev_list;
    int *dev_user;
    static bm_device_manager *bm_dev_mgr;
    static pthread_mutex_t init_lock;
  };

  class bm_device_manager_control
  {
  public:
    bm_device_manager_control()
    {
    }
    ~bm_device_manager_control()
    {
      bm_device_manager::destroy_dev_mgr();
    }
  };

#ifdef __cplusplus
}
#endif
#endif
#endif
