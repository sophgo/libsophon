#ifndef _VPP_PLATFORM
#define _VPP_PLATFORM

#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/wait.h>

#define VPP_CORE_MAX (2)
#define MAX_VPP_STAT_WIN_SIZE  100

struct bm_device_info;
typedef struct vpp_statistic_info {
	uint64_t vpp_working_time_in_ms[VPP_CORE_MAX];
	uint64_t vpp_total_time_in_ms[VPP_CORE_MAX];
	atomic_t vpp_busy_status[VPP_CORE_MAX];
	char vpp_status_array[VPP_CORE_MAX][MAX_VPP_STAT_WIN_SIZE];
	int vpp_status_index[VPP_CORE_MAX];
	int vpp_core_usage[VPP_CORE_MAX];
	int vpp_instant_interval;
}vpp_statistic_info_t;

typedef struct vpp_drv_context {
  struct semaphore vpp_core_sem;
  unsigned long vpp_idle_bit_map;
  wait_queue_head_t  wq_vpp[VPP_CORE_MAX];
  int got_event_vpp[VPP_CORE_MAX];
  vpp_statistic_info_t s_vpp_usage_info;

  int  (*trigger_vpp)(struct bm_device_info *bmdi, unsigned long arg);
  void (*bm_vpp_request_irq)(struct bm_device_info *bmdi);
  void (*bm_vpp_free_irq)(struct bm_device_info *bmdi);
  int  (*vpp_init)(struct bm_device_info *bmdi);
  void (*vpp_exit)(struct bm_device_info *bmdi);
} vpp_drv_context_t;


int  trigger_vpp(struct bm_device_info *bmdi, unsigned long arg);
void bm_vpp_request_irq(struct bm_device_info *bmdi);
void bm_vpp_free_irq(struct bm_device_info *bmdi);
int  vpp_init(struct bm_device_info *bmdi);
void vpp_exit(struct bm_device_info *bmdi);
int  bm_vpp_check_usage_info(struct bm_device_info *bmdi);
void vpp_usage_info_init(struct bm_device_info *bmdi);

int  bm1684_trigger_vpp(struct bm_device_info *bmdi, unsigned long arg);
void bm1684_vpp_request_irq(struct bm_device_info *bmdi);
void bm1684_vpp_free_irq(struct bm_device_info *bmdi);
int  bm1684_vpp_init(struct bm_device_info *bmdi);
void bm1684_vpp_exit(struct bm_device_info *bmdi);

int  bm1686_trigger_vpp(struct bm_device_info *bmdi, unsigned long arg);
void bm1686_vpp_request_irq(struct bm_device_info *bmdi);
void bm1686_vpp_free_irq(struct bm_device_info *bmdi);
int  bm1686_vpp_init(struct bm_device_info *bmdi);
void bm1686_vpp_exit(struct bm_device_info *bmdi);

#endif

