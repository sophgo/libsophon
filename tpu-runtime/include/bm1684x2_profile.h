#ifndef BM1684X2_PROFILE_H
#define BM1684X2_PROFILE_H
#include "bmruntime_profile.h"

using namespace bmruntime;
namespace bm1684x2_profile {

// PMU record layout is taken verbatim from the bm1684x2 firmware
// (TPU1686/bm1684x2/firmware_base/src/firmware/firmware_pmu.c). Keep these
// structs byte-identical with the firmware so the perf-monitor buffers copied
// back from device parse correctly.
#pragma pack(1)
typedef struct {
    unsigned int inst_start_time;
    unsigned int inst_end_time;
    unsigned int inst_id;
    // lower 1 bit: thread_id; higher 31 bits: bank_conflict
    unsigned int thread_id_and_bank_conflict;
} tiu_pmu_item_t;   // 16 bytes

typedef struct {
    // H0
    unsigned int inst_start_time;
    unsigned int inst_end_time;
    unsigned int inst_id;
    unsigned int thread_id : 1;
    unsigned int ar_latency_cnt : 19;
    unsigned int rip_valid_latency : 12;
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
} gdma_pmu_item_t;  // 64 bytes
#pragma pack()

typedef struct {
    buffer_pair_t tiu;
    buffer_pair_t gdma;
    buffer_pair_t mcu;
} profile_core_buffer_t;

class BMProfileDevice : public BMProfileDeviceBase {
    // BMProfileDeviceBase interface
public:
    BMProfileDevice(BMProfile* profile);
    bool init();
    bool begin(net_ctx_t* net_ctx);
    bool end(net_ctx_t* net_ctx);
    void deinit();
    bool enabled();

private:
    // Output uses the tpuv7 AKSV layout so tpuv7 tooling can parse it:
    //   cdm_profile_data_dev<devid>/global.profile
    //   cdm_profile_data_dev<devid>/cdmlib0_<core>.profile
    std::string get_folder();
    std::string get_file_name(int core_id, bool global);
    void write_global_file();
    void write_pmu_block(FILE* fp, int block_type, const buffer_pair_t& buf, size_t elt_size);
    void write_des_block(FILE* fp, net_ctx_t* net_ctx, int block_type, int core_idx);
    // Broadcast sg_api_get_profile_data once per pagination step; the firmware
    // self-indexes by CORE_ID (each core writes to output + CORE_ID*slice_size).
    // Returns one data vector per physical core for the given data_category
    // (0 = DYN time records, 1 = DYN extra data).
    std::vector<std::vector<u8>> collect_mcu_allcores(net_ctx_t* net_ctx, int data_category);

    std::vector<profile_core_buffer_t> buffers;
    buffer_pair_t mcu_all; // contiguous phys_cores*mcu_size buffer for broadcast reads
    int mMode = 0;              // PROFILE_MODE: 0 pmu-only, 1 condensed, 2 detailed
    size_t mRecordNum = 0;      // PROFILE_RECORD_SIZE: record count per engine buffer
    std::string mFolder;
};

}
#endif // BM1684X2_PROFILE_H
