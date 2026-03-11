#ifndef CV184X_PROFILE_H
#define CV184X_PROFILE_H
#include "bmruntime_profile.h"

using namespace bmruntime;
namespace  cv184x_profile {

#pragma pack(1)
typedef struct {
  // H0
  unsigned int inst_start_time;
  unsigned int inst_end_time;
  unsigned int inst_id;
  uint32_t thread_id: 1;
  uint32_t ar_latency_cnt: 19;
  uint32_t rip_valid_latency: 12;
  // H1
  unsigned int gif_wr_rd_stall_cntr;
  unsigned int axi_d0_w_cntr;
  unsigned int axi_d0_ar_cntr;
  unsigned int axi_d0_aw_cntr;
  // H2
  unsigned int axi_d0_wr_stall_cntr;
  unsigned int axi_d0_rd_stall_cntr;
  unsigned int gif_mem_w_cntr;
  unsigned int gif_mem_ar_cntr;
  // H3
  unsigned int axi_d0_wr_vaild_cntr;
  unsigned int axi_d0_rd_vaild_cntr;
  unsigned int gif_wr_valid_cntr;
  unsigned int gif_rd_valid_cntr;
}GDMA_PROFILE_FORMAT;

typedef struct {
  unsigned int inst_id : 20;
  unsigned int reserved : 12;
  unsigned int thread_id : 1;
  unsigned int bank_conflict : 31;
  unsigned int inst_start_time;
  unsigned int inst_end_time;
} TPU_PROFILE_FORMAT;
#pragma pack()

typedef struct {
    buffer_pair tiu;
    buffer_pair gdma;
    buffer_pair mcu;
} profile_core_buffer_t;

using namespace bmruntime;
class BMProfileDevice:public BMProfileDeviceBase {

    // BMProfileDeviceBase interface
public:
    BMProfileDevice(BMProfile* profile):bmruntime::BMProfileDeviceBase(profile) {
        // not used dynamic subnet profile
        enable_arm = false;
     }
    bool init();
    bool begin(net_ctx_t* net_ctx);
    bool end(net_ctx_t* net_ctx);
    void deinit();
    bool enabled();

private:
    std::vector<profile_core_buffer_t> buffers;
protected:
    size_t gdma_record_len = 16 * 1024;
    size_t bdc_record_len = 16 * 1024;
};

}
#endif // CV184X_PROFILE_H
