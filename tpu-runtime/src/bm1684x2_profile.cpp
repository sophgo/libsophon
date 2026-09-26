#include <memory.h>
#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include "bmruntime.h"
#include "bm1684x2_profile.h"
#include "backend/launcher.hpp"

namespace bm1684x2_profile {

#define PROFILE_ENGINE_MCU 0
#define PROFILE_ENGINE_GDMA 1
#define PROFILE_ENGINE_TIU 2
#define PROFILE_ENGINE_CDMA 3
#define PROFILE_ENGINE_TGS 4
#define PROFILE_PAUSE 5
#define CDMANum 2

// Packed engine-profile param entry, mirrors firmware sg_api_engine_profile_param_t.
#pragma pack(1)
typedef struct bm_api_engine_profile_param {
  int engine;
  unsigned long long addr;
  unsigned long long size;
} bm_api_engine_profile_param_t;
#pragma pack()

BMProfileDevice::BMProfileDevice(BMProfile* profile)
    : bmruntime::BMProfileDeviceBase(profile) {
  // tpuv7-compatible profile controls:
  //   ENABLE_ALL_PROFILE=1  -> turn profiling on (also honor the libsophon
  //                            legacy BMRUNTIME_ENABLE_PROFILE)
  //   PROFILE_MODE=0/1/2    -> 0: pmu-only, 1: +condensed node stream,
  //                            2: +detailed node stream (incl. cmd binary)
  //   PROFILE_RECORD_SIZE=N -> record count per engine buffer
  bool env_enable = profile->getenv_bool("ENABLE_ALL_PROFILE", false);
  env_enable = env_enable || profile->getenv_bool("BMRUNTIME_ENABLE_PROFILE", false);
  mMode = profile->getenv_int("PROFILE_MODE", 0);
  mRecordNum = profile->getenv_int("PROFILE_RECORD_SIZE", 0x10000);
  enable = env_enable;
  enable_gdma = enable;
  enable_bdc = enable;
  enable_arm = (mMode >= 1);
  if (enable) {
    BMRT_LOG(INFO, "bm1684x2 profile enabled: mode=%d, record_num=0x%zx",
             mMode, mRecordNum);
  }
}

bool BMProfileDevice::init()
{
    if(!enable) {
        return false;
    }
    // Mirror tpuv7: buffer for every physical core regardless of how many
    // cores the bmodel actually runs on. set_pmu_param broadcasts once and
    // each core self-indexes by CORE_ID, so all physical cores need a slot.
    auto phys_cores = profile->get_bmrt()->backend()->core_num();
    this->buffers.resize(phys_cores);
    for(auto& buffer: this->buffers){
        if(enable_arm){
            profile->alloc_buffer(&buffer.mcu, mRecordNum * sizeof(u32), "dyn_profile");
        }
        if(enable_bdc){
            u64 tiu_size = mRecordNum * sizeof(tiu_pmu_item_t);
            profile->alloc_buffer(&buffer.tiu, tiu_size, "bdc_perf_monitor");
        }
        if(enable_gdma){
            u64 gdma_size = mRecordNum * sizeof(gdma_pmu_item_t);
            profile->alloc_buffer(&buffer.gdma, gdma_size, "gdma_perf_monitor");
        }
    }
    // Contiguous buffer for broadcast DYN reads: each core writes its slice to
    // mcu_all + CORE_ID * mcu_size.  Sized phys_cores * per-core mcu_size.
    if (enable_arm) {
        profile->alloc_buffer(&mcu_all,
            phys_cores * mRecordNum * sizeof(u32), "dyn_profile_all");
    }
    return true;
}

bool BMProfileDevice::begin(net_ctx_t* net_ctx)
{
    auto handle = profile->get_handle();
    auto& core_list = profile->get_core_list();
    auto phys_cores = profile->get_bmrt()->backend()->core_num();
    // enable bits follow the tpuv7 AKSV convention consumed by the bm1684x2
    // firmware sg_api_set_profile:
    //   bit PROFILE_ENGINE_TGS (4) -> PMU master enable (TIU+GDMA monitors)
    //   bits 0-1                    -> node-stream mode (1: condensed, 2: detailed)
    unsigned int enable_bits = 1u << PROFILE_ENGINE_TGS;
    if (mMode == 1) {
        enable_bits |= 1u << PROFILE_ENGINE_MCU;         // bit0
    } else if (mMode >= 2) {
        enable_bits |= 1u << (PROFILE_ENGINE_MCU + 1);   // bit1
    }
    BMRT_LOG(INFO, "[prof] begin: enable_bits=0x%x mode=%d phys_cores=%zu "
             "bmodel_cores=%zu enable_bdc=%d enable_gdma=%d", enable_bits, mMode,
             phys_cores, core_list.size(), enable_bdc, enable_gdma);
    auto set_func_ids = net_ctx->kernel_module_->get_set_engine_profile_param_func_id(core_list);
    auto enable_func_ids = net_ctx->kernel_module_->get_enable_profile_func_id(core_list);
    auto &launcher = profile->get_bmrt()->backend()->launcher();

    // Phase 1: zero + upload every core's PMU buffer, then build ONE vParams
    // array (CDMA placeholders + per-core TIU/GDMA) and broadcast it once.
    // Mirrors tpuv7 setPmuParam: the firmware sg_api_set_engine_profile_param
    // indexes the array by physical CORE_ID, so all physical cores must have a
    // slot even if the bmodel runs on fewer cores.
    uint64_t tiuSize = mRecordNum * sizeof(tiu_pmu_item_t);
    uint64_t dmaSize = mRecordNum * sizeof(gdma_pmu_item_t);
    for (size_t i = 0; i < phys_cores; i++) {
        if (enable_bdc) {
            auto& tiu_buffer = buffers[i].tiu;
            memset(tiu_buffer.ptr, 0, tiu_buffer.size);
            bm_status_t ret = bm_memcpy_s2d(handle, tiu_buffer.mem, tiu_buffer.ptr);
            if (ret != BM_SUCCESS) {
                BMRT_LOG(FATAL, "init tiu profile buffer failed, ret = %d\n", ret);
            }
        }
        if (enable_gdma) {
            auto& gdma_buffer = buffers[i].gdma;
            memset(gdma_buffer.ptr, 0, gdma_buffer.size);
            bm_status_t ret = bm_memcpy_s2d(handle, gdma_buffer.mem, gdma_buffer.ptr);
            if (ret != BM_SUCCESS) {
                BMRT_LOG(FATAL, "init gdma profile buffer failed, ret = %d\n", ret);
            }
        }
    }
    // Build vParams exactly like tpuv7 setPmuParam, minus SDMA (bm1684x2 has none).
    // Layout consumed by firmware sg_api_set_engine_profile_param:
    //   [0 .. CDMANum-1]                      -> CDMA placeholders (size==0)
    //   [CDMANum + core_idx*2 + 0]            -> core_idx's TIU
    //   [CDMANum + core_idx*2 + 1]            -> core_idx's GDMA
    std::vector<bm_api_engine_profile_param_t> vParams;
    bm_api_engine_profile_param_t param;
    for (int port = 0; port < CDMANum; port++) {
        param.engine = PROFILE_ENGINE_CDMA;
        param.addr = 0;
        param.size = 0;
        vParams.push_back(param);
    }
    for (size_t core_idx = 0; core_idx < phys_cores; core_idx++) {
        param.engine = PROFILE_ENGINE_TGS;  // host marker, firmware ignores (same as v7)
        if (enable_bdc) {
            param.addr = bm_mem_get_device_addr(buffers[core_idx].tiu.mem);
            param.size = tiuSize;
        } else {
            param.addr = 0;
            param.size = 0;
        }
        vParams.push_back(param);
        if (enable_gdma) {
            param.addr = bm_mem_get_device_addr(buffers[core_idx].gdma.mem);
            param.size = dmaSize;
        } else {
            param.addr = 0;
            param.size = 0;
        }
        vParams.push_back(param);
    }
    uint32_t elt_size = sizeof(bm_api_engine_profile_param_t);
    BMRT_LOG(INFO, "[prof] set_param broadcast: set_func_id=%u vParams=%zu elt=%u "
             "total=%zu", set_func_ids[0], vParams.size(), elt_size,
             vParams.size() * elt_size);
    bm_status_t ret = dynamic_cast<Launcher_BM1684X2*>(launcher.get())->_bmdnn_set_engine_profile_param_(
        handle, set_func_ids[0], vParams.data(), vParams.size() * elt_size);
    if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "set engine profile param broadcast failed, ret = %d\n", ret);
    }
    for (size_t i = 0; i < phys_cores; i++) {
        bm_thread_sync_from_core(handle, i);
    }
    BMRT_LOG(INFO, "[prof] set_param broadcast + synced for %zu cores, now enabling", phys_cores);

    // Phase 2: enable PMU -- single broadcast (mirrors tpuv7 setProfile).
    // enable_pmu RMW inter-core clobber is deferred per user direction; verify
    // PMU data completeness first, fall back to "all cores enable all cores" if
    // cores 0/1/2 come back all-zero.
    BMRT_LOG(INFO, "[prof] enable broadcast: enable_func_id=%u enable_bits=0x%x",
             enable_func_ids[0], enable_bits);
    ret = dynamic_cast<Launcher_BM1684X2*>(launcher.get())->_bmdnn_set_profile_enable_(
        handle, enable_func_ids[0], enable_bits);
    if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "enable profile broadcast failed, ret = %d\n", ret);
    }
    for (size_t i = 0; i < phys_cores; i++) {
        bm_thread_sync_from_core(handle, i);
    }
    return true;
}

static bool allZero(const unsigned char *data, int len)
{
    for (int i = 0; i < len; i++) {
        if (data[i] != 0) return 0;
    }
    return 1;
}

std::string BMProfileDevice::get_folder()
{
    if (mFolder.empty()) {
        char dir[256];
        snprintf(dir, sizeof(dir), "cdm_profile_data_dev%d", profile->get_bmrt()->get_devid());
        mFolder = dir;
        std::string rm = "rm -rf " + mFolder + " && mkdir -p " + mFolder;
        system(rm.c_str());
    }
    return mFolder;
}

std::string BMProfileDevice::get_file_name(int core_id, bool global)
{
    if (global) return get_folder() + "/global.profile";
    return get_folder() + "/cdmlib0_" + std::to_string(core_id) + ".profile";
}

void BMProfileDevice::write_global_file()
{
    FILE* fp = fopen(get_file_name(0, true).c_str(), "w");
    if (!fp) return;
    fprintf(fp, "bmodel\n");
    fprintf(fp, "arch=%d\n", 7);
    int freq = 1000;
    bm_get_clk_tpu_freq(profile->get_handle(), &freq);
    if (freq <= 0) freq = 1000;
    fprintf(fp, "tpu_freq=%d\n", freq);
    fclose(fp);
}

void BMProfileDevice::write_pmu_block(FILE* fp, int block_type, const buffer_pair_t& buf, size_t elt_size)
{
    size_t max_len = buf.size / elt_size;
    size_t valid_len = 0;
    while (valid_len < max_len) {
        if (allZero((const unsigned char*)buf.ptr + valid_len * elt_size, elt_size)) break;
        valid_len++;
    }
    if (valid_len) {
        size_t bytes = valid_len * elt_size;
        fwrite(&block_type, sizeof(block_type), 1, fp);
        u32 len = (u32)bytes;
        fwrite(&len, sizeof(len), 1, fp);
        fwrite(buf.ptr, bytes, 1, fp);
        BMRT_LOG(INFO, "[prof] pmu block=%d, record=%zu", block_type, valid_len);
    } else {
        BMRT_LOG(INFO, "[prof] pmu block=%d skipped (all zero, max=%zu)",
                 block_type, max_len);
    }
}

void BMProfileDevice::write_des_block(FILE* fp, net_ctx_t* net_ctx, int block_type, int core_idx)
{
    // The instruction descriptor (BLOCK_DES_BDC/BLOCK_DES_GDMA) is the raw
    // command binary uploaded to device, cached at load time by
    // BMProfile::record_cmd_data and keyed by device address.
    const profile_cmd_info_t* cmd_info = profile->get_cmd_info(core_idx);
    if (!cmd_info) {
        BMRT_LOG(INFO, "[prof] des block=%d skipped: no cmd_info for core %d", block_type, core_idx);
        return;
    }
    int engine = (block_type == BLOCK_DES_BDC) ? (int)ENGINE_BD : (int)ENGINE_GDMA;
    u64 lookup_addr = (block_type == BLOCK_DES_BDC) ? cmd_info->bdc_base_addr : cmd_info->gdma_base_addr;
    BMRT_LOG(INFO, "[prof] des block=%d core=%d lookup addr=0x%llx engine=%d",
             block_type, core_idx, (unsigned long long)lookup_addr, engine);
    const std::vector<char>* data = profile->get_cmd_data(lookup_addr, core_idx, engine);
    if (!data || data->empty()) {
        BMRT_LOG(INFO, "[prof] des block=%d skipped: cmd_data miss (addr=0x%llx,core=%d,eng=%d)",
                 block_type, (unsigned long long)lookup_addr, core_idx, engine);
        return;
    }
    fwrite(&block_type, sizeof(block_type), 1, fp);
    u32 len = (u32)data->size();
    fwrite(&len, sizeof(len), 1, fp);
    fwrite(data->data(), data->size(), 1, fp);
    BMRT_LOG(INFO, "[prof] des block=%d, bytes=%zu", block_type, data->size());
}

// Broadcast sg_api_get_profile_data once per pagination step.  On 84x6 every
// core runs the API (bmlib hardcodes a 1,4 fan-out), and the firmware
// self-indexes by CORE_ID so each core writes its slice to
//   mcu_all + CORE_ID * slice_size.
// One broadcast therefore collects all physical cores' DYN data in a single
// pass.  We paginate with a shared byte_offset: cores with less data return
// read_len=0 once their offset passes their total, so we advance by the max
// read_len this round and stop when every core has got >= its total.
// Returns one vector per physical core (indexed by physical core number).
std::vector<std::vector<u8>> BMProfileDevice::collect_mcu_allcores(
    net_ctx_t* net_ctx, int data_category)
{
    auto phys_cores = profile->get_bmrt()->backend()->core_num();
    auto handle = profile->get_handle();
    auto& core_list = profile->get_core_list();
    auto get_func_ids = net_ctx->kernel_module_->get_get_profile_func_id(core_list);
    auto &launcher = profile->get_bmrt()->backend()->launcher();

    size_t total_size = bm_mem_get_device_size(mcu_all.mem);
    size_t slice_size = total_size / phys_cores;  // per-core chunk
    std::vector<std::vector<u8>> per_core_data(phys_cores);
    std::vector<size_t> per_core_total(phys_cores, 0);
    std::vector<size_t> per_core_got(phys_cores, 0);
    size_t byte_offset = 0;
    BMRT_LOG(INFO, "[prof] collect_mcu_allcores: category=%d phys_cores=%zu "
             "slice_size=%zu", data_category, phys_cores, slice_size);
    while (true) {
        bm_status_t status = dynamic_cast<Launcher_BM1684X2*>(launcher.get())->_bmdnn_get_profile_data_(
            handle, 0, get_func_ids[0],
            bm_mem_get_device_addr(mcu_all.mem), (unsigned)slice_size,
            (unsigned)byte_offset, data_category);
        if (status != BM_SUCCESS) {
            BMRT_LOG(FATAL, "get profile data broadcast failed, ret = %d\n", status);
        }
        status = bm_memcpy_d2s(handle, mcu_all.ptr, mcu_all.mem);
        if (status != BM_SUCCESS) {
            BMRT_LOG(FATAL, "copy profile data from device failed, ret = %d\n", status);
        }
        size_t max_read = 0;
        bool all_done = true;
        for (size_t c = 0; c < phys_cores; c++) {
            u8* slice = mcu_all.ptr + c * slice_size;
            auto u32_ptr = (u32*)slice;
            auto read_len = u32_ptr[0];
            auto total_len = u32_ptr[1];
            if (total_len == 0) {
                per_core_total[c] = 0;
                continue;
            }
            if (per_core_total[c] == 0) per_core_total[c] = total_len;
            if (read_len > 0) {
                u8* data_ptr = (u8*)&u32_ptr[2];
                per_core_data[c].insert(per_core_data[c].end(), data_ptr, data_ptr + read_len);
                per_core_got[c] += read_len;
            }
            if (per_core_got[c] < per_core_total[c]) all_done = false;
            if (read_len > max_read) max_read = read_len;
        }
        if (all_done) break;
        if (max_read == 0) break;  // no progress this round
        byte_offset += max_read;
    }
    for (size_t c = 0; c < phys_cores; c++) {
        BMRT_LOG(INFO, "[prof] collect_mcu_allcores: core=%zu category=%d "
                 "total=%zu got=%zu", c, data_category, per_core_total[c],
                 per_core_got[c]);
    }
    return per_core_data;
}

bool BMProfileDevice::end(net_ctx_t* net_ctx)
{
    auto handle = profile->get_handle();
    auto& core_list = profile->get_core_list();
    auto phys_cores = profile->get_bmrt()->backend()->core_num();
    auto enable_func_ids = net_ctx->kernel_module_->get_enable_profile_func_id(core_list);
    auto &launcher = profile->get_bmrt()->backend()->launcher();

    // (a) Pause broadcast -- freeze PMU so d2s reads a stable snapshot.
    // Mirrors tpuv7 disable(): setProfile(false, true) sets the PAUSE bit.
    unsigned int pause_bits = 1u << PROFILE_PAUSE;
    BMRT_LOG(INFO, "[prof] disable(pause) broadcast: enable_func_id=%u bits=0x%x",
             enable_func_ids[0], pause_bits);
    bm_status_t ret = dynamic_cast<Launcher_BM1684X2*>(launcher.get())->_bmdnn_set_profile_enable_(
        handle, enable_func_ids[0], pause_bits);
    if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "pause profile broadcast failed, ret = %d\n", ret);
    }
    for (size_t i = 0; i < phys_cores; i++) {
        bm_thread_sync_from_core(handle, i);
    }

    write_global_file();

    // (b) Collect DYN (MCU) data for all physical cores via broadcast reads.
    // The firmware self-indexes by CORE_ID, so collect_mcu_allcores returns
    // one vector per physical core (indexed by physical core number, NOT
    // core_list position).  Collect before the per-core write loop so the
    // broadcasts happen while PMU is paused.
    std::vector<std::vector<u8>> dyn_data;
    std::vector<std::vector<u8>> dyn_extra;
    if (enable_arm) {
        if (mMode >= 1) {
            dyn_data = collect_mcu_allcores(net_ctx, 0);   // BLOCK_DYN_DATA
        }
        if (mMode >= 2) {
            dyn_extra = collect_mcu_allcores(net_ctx, 1);  // BLOCK_DYN_EXTRA
        }
    }

    // (c) Dump PMU for all physical cores; DES/MCU only for cores the bmodel
    // actually ran on (cmd_infos/cmd_data_map are indexed by core_list position
    // idx, not physical core number).  DYN data is indexed by physical core.
    for (int core = 0; core < (int)phys_cores; core++) {
        FILE* fp = fopen(get_file_name(core, false).c_str(), "wb");
        if (!fp) {
            BMRT_LOG(WRONG, "open profile file %s failed", get_file_name(core, false).c_str());
            continue;
        }
        if (enable_bdc) {
            bm_status_t r = bm_memcpy_d2s(handle, buffers[core].tiu.ptr, buffers[core].tiu.mem);
            if (r != BM_SUCCESS) {
                BMRT_LOG(FATAL, "copy tiu pmu from device failed, ret = %d\n", r);
            }
            write_pmu_block(fp, BLOCK_MONITOR_BDC, buffers[core].tiu, sizeof(tiu_pmu_item_t));
        }
        if (enable_gdma) {
            bm_status_t r = bm_memcpy_d2s(handle, buffers[core].gdma.ptr, buffers[core].gdma.mem);
            if (r != BM_SUCCESS) {
                BMRT_LOG(FATAL, "copy gdma pmu from device failed, ret = %d\n", r);
            }
            write_pmu_block(fp, BLOCK_MONITOR_GDMA, buffers[core].gdma, sizeof(gdma_pmu_item_t));
        }
        // DES/MCU blocks: only for cores in core_list, indexed by position.
        auto it = std::find(core_list.begin(), core_list.end(), core);
        if (it != core_list.end()) {
            int idx = (int)(it - core_list.begin());
            write_des_block(fp, net_ctx, BLOCK_DES_BDC, idx);
            write_des_block(fp, net_ctx, BLOCK_DES_GDMA, idx);
        }
        // DYN blocks: indexed by physical core number (firmware self-indexes
        // by CORE_ID).  Written for every physical core that has data, even
        // if not in core_list, to match the PMU dump policy.
        if (mMode >= 1 && !dyn_data.empty() && !dyn_data[core].empty()) {
            int bt = BLOCK_DYN_DATA;
            fwrite(&bt, sizeof(bt), 1, fp);
            u32 len = (u32)dyn_data[core].size();
            fwrite(&len, sizeof(len), 1, fp);
            fwrite(dyn_data[core].data(), dyn_data[core].size(), 1, fp);
            BMRT_LOG(INFO, "[prof] mcu block=%d core=%d, bytes=%zu",
                     bt, core, dyn_data[core].size());
        }
        if (mMode >= 2 && !dyn_extra.empty() && !dyn_extra[core].empty()) {
            int bt = BLOCK_DYN_EXTRA;
            fwrite(&bt, sizeof(bt), 1, fp);
            u32 len = (u32)dyn_extra[core].size();
            fwrite(&len, sizeof(len), 1, fp);
            fwrite(dyn_extra[core].data(), dyn_extra[core].size(), 1, fp);
            BMRT_LOG(INFO, "[prof] mcu block=%d core=%d, bytes=%zu",
                     bt, core, dyn_extra[core].size());
        }
        fclose(fp);
    }

    // (d) Full-off broadcast -- mirrors tpuv7 setProfile(false, false).
    BMRT_LOG(INFO, "[prof] disable(off) broadcast: enable_func_id=%u bits=0",
             enable_func_ids[0]);
    ret = dynamic_cast<Launcher_BM1684X2*>(launcher.get())->_bmdnn_set_profile_enable_(
        handle, enable_func_ids[0], 0);
    if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "off profile broadcast failed, ret = %d\n", ret);
    }
    for (size_t i = 0; i < phys_cores; i++) {
        bm_thread_sync_from_core(handle, i);
    }
    return true;
}

void BMProfileDevice::deinit()
{
  if (!enable) return;
  for (auto& buffer : buffers){
    profile->free_buffer(&buffer.tiu);
    profile->free_buffer(&buffer.gdma);
    profile->free_buffer(&buffer.mcu);
  }
  if (mcu_all.size) {
    profile->free_buffer(&mcu_all);
  }
}

bool BMProfileDevice::enabled()
{
    return enable;
}

}
