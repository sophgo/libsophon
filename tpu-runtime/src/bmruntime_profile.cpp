#include <sys/stat.h>
#include <stdlib.h>
#include "bmruntime.h"
#include "bm1682_profile.h"
#include "bm1684_profile.h"
#include "bm1684x_profile.h"
#ifndef __linux__
#include <direct.h>
#endif

namespace bmruntime {


#ifdef __linux__
inline u64 get_usec(){
    timeval time;
    gettimeofday(&time, NULL);
    return TIME_TO_USECS(time);
}
#else
inline u64 get_usec(){
  timespec time;
  bmrt_clock_gettime(0, &time);
  return TIME_TO_USECS(time);
}
#endif

int bm_mkdir(const char *dirname, bool must_new)
{
    string dname = dirname;
    struct stat st;
    dname += "/";
    if (stat(dname.c_str(), &st) == -1) {
        #ifdef __linux__
        int mkdir_status = mkdir(dname.c_str(), 0777);
        BMRT_ASSERT_INFO(mkdir_status == 0, \
            "Failed to create directory named %s",dname.c_str());
        #else
        int mkdir_status = _mkdir(dname.c_str());
        BMRT_ASSERT_INFO(mkdir_status == 0, \
            "Failed to create directory named %s",dname.c_str());
        #endif
        return 1;
    }
    if(must_new){
        string cmd = "rm ";
        cmd += dname + "*";
        system(cmd.c_str());
    }
    return 0;
}


BMProfile::BMProfile(Bmruntime* p_bmrt): p_bmrt(p_bmrt), enabled(false) {
    set_save_dir("bmprofile_data");
    handle = p_bmrt->get_bm_handle();
    devid = p_bmrt->get_devid();
    auto arch = bmrt_arch_info::get_bmtpu_arch();
    if(arch== BM1684){
      device = decltype(device)(new bm1684_profile::BMProfileDevice(this));
    } else if(arch == BM1684X){
      device = decltype(device)(new bm1684x_profile::BMProfileDevice(this));
    } else {
      BMRT_LOG(WARNING, "Not support profile for arch=%d",  arch);
    }
    enabled = device && device->enabled();
}

BMProfile::~BMProfile(){
    if(mem_info.empty()) return;
    auto filename = get_global_filename();
    FILE* fp = fopen(filename.c_str(), "a");
    if(!fp){
        BMRT_LOG(WRONG, "writing file %s failed!", filename.c_str());
        return;
    }
    for(auto& minfo: mem_info){
        fprintf(fp, "[bmprofile] mtype=%d addr=%lld size=%d alloc=%lld free=%lld desc=%s\n",
            minfo.type, minfo.addr, minfo.size, minfo.alloc_usec, minfo.free_usec, minfo.desc.c_str());
    }
    fclose(fp);
}

void BMProfile::deinit()
{
  if (is_enabled()) print_note();
  if (device) {
    device->deinit();
    }
}

void BMProfile::print_note()
{
    BMRT_LOG(INFO, "*****************************************************************");
    BMRT_LOG(INFO, "* PROFILE MODE due to %s=1", ENV_ENABLE_PROFILE);
    BMRT_LOG(INFO, "* Note: BMRuntime will collect time data during running         *");
    BMRT_LOG(INFO, "*       that will cost extra time.                              *");
    BMRT_LOG(INFO, "* Close PROFILE Mode by \"unset %s\"", ENV_ENABLE_PROFILE);
    BMRT_LOG(INFO, "*****************************************************************");
}

void BMProfile::record_alloc_device_mem(const bm_device_mem_t &mem, const std::string &desc)
{
    if(!enabled) return;
    record_mem(MEM_GLOBAL, bm_mem_get_device_addr(mem), bm_mem_get_device_size(mem), desc);
}

void BMProfile::record_free_device_mem(const bm_device_mem_t &mem){
    if(!enabled) return;
    auto addr = bm_mem_get_device_addr(mem);
    bool find = false;
    for(auto& mi: mem_info){
        if(mi.type==MEM_GLOBAL && mi.alloc_usec>0 && mi.free_usec==0 && mi.addr == addr){
            mi.free_usec = get_usec();
            find = true;
            break;
        }
    }
    if(!find){
        BMRT_LOG(WARNING, "[bmprofile] free mem without alloc: addr=0x%llx", addr);
    }
}

profile_cmd_num_t *BMProfile::record_subnet_cmd_info(u64 gdma_addr, u64 gdma_offset, u64 bdc_addr, u64 bdc_offset, u32 group_num)
{
    if(!current_enabled) return nullptr;
    u8* info_buffer = new u8[sizeof(profile_cmd_info_t)+group_num*sizeof(profile_cmd_num_t)];
    cmd_info = (profile_cmd_info_t*)info_buffer;
    cmd_info->gdma_base_addr = gdma_addr;
    cmd_info->gdma_offset = gdma_offset;
    cmd_info->bdc_base_addr = bdc_addr;
    cmd_info->bdc_offset = bdc_offset;
    cmd_info->group_num = group_num;
    return cmd_info->cmd_num;
}

void BMProfile::record_cmd_data(ENGINE_ID engine, const void *cmd_ptr, u32 cmd_len, u64 store_addr)
{
    if(!enabled) return;
    char filename[256] = {0};
    sprintf(filename, "cmd_%llx_%d.dat", store_addr, engine);
    auto path = get_save_dir();
    path += "/";
    path += filename;
    FILE* fp = fopen(path.c_str(),"wb");
    BMRT_ASSERT_INFO(fp != nullptr, "fp shouldn't be nullptr");
    fwrite(cmd_ptr, 1, cmd_len, fp);
    fclose(fp);
}

void BMProfile::record_cpu_mem(const void *ptr, u32 len, const std::string &desc)
{
    if(!enabled) return;
    record_mem(MEM_CPU, (u64)ptr, len, desc);
}

void BMProfile::record_mem(PROFILE_MEM_TYPE_T mtype, u64 addr, u32 size, const std::string &desc)
{
    if(!enabled) return;
    profile_mem_info_t info;
    info.addr = addr;
    info.size = size;
    info.desc = desc;
    info.type = mtype;
    info.alloc_usec = get_usec();
    info.free_usec = 0;
//    BMRT_LOG(INFO, "%s: addr=0x%llx, end=0x%llx, size=%d", desc.c_str(), addr, addr+size, (int)size);
    mem_info.push_back(info);
}

string BMProfile::get_save_dir() const
{
    bm_mkdir(save_dir.c_str(), false);
    return save_dir;
}

void BMProfile::set_save_dir(const string &value)
{
    save_dir = value;
    save_dir += std::to_string(devid);
}

std::string BMProfile::get_global_filename()
{
    return get_save_dir() + "/" + "global.profile";
}

void BMProfile::init(const string& net_name, const vector<u8>& data, const vector<u8>& stat)
{
    if (!is_enabled()) return;
    auto arch = bmrt_arch_info::get_bmtpu_arch();
    string filename = get_global_filename();
    auto fp = fopen(filename.c_str(), "wb");
    int freq = 0;
    bm_get_clk_tpu_freq(handle, &freq);
    fprintf(fp, "[bmprofile] arch=%d\n", arch);
    fprintf(fp, "[bmprofile] net_name=%s\n", net_name.c_str());
    fprintf(fp, "[bmprofile] tpu_freq=%d\n", freq);
    if(data.size()>0){
        fwrite(data.data(), data.size(), 1, fp);
    }
    fclose(fp);
    if(stat.size()>0){
        std::string stat_filename = get_save_dir() + "/" + "net_stat.sim";
        auto fp = fopen(stat_filename.c_str(), "wb");
        fwrite(stat.data(), stat.size(), 1, fp);
        fclose(fp);
    }

    if(arch == BM1682) {
        bm_set_debug_mode(handle, 1);
        bmlib_log_set_level(BMLIB_LOG_DEBUG);
        bmlib_api_dbg_callback call_back = bm1682_profile::bm_log_profile_callback;
        bmlib_set_api_dbg_callback(call_back);
    } else if (device) {
        device->init();
    } else {
        BMRT_LOG(FATAL, "Not support chip arch");
    }
}

void BMProfile::alloc_buffer(buffer_pair *bp, size_t size, const string& desc)
{
    if(bp->size != size || !bp->ptr){
        free_buffer(bp);
    }
    if(!bp->ptr) {
        bp->ptr= (u8 *)new u8[size];
        if (!bp->ptr) {
            BMRT_LOG(FATAL, "malloc system buffer failed for profile");
        }
        p_bmrt->must_alloc_device_mem(&bp->mem, size, desc);
        bp->size = size;
    }
}

void BMProfile::free_buffer(buffer_pair *bp)
{
    if(bp->ptr){
        delete [] bp->ptr;
        bp->ptr =nullptr;
        p_bmrt->must_free_device_mem(bp->mem);
    }
}

void BMProfile::create_file()
{
    if(!current_enabled) return;
    auto filename = get_save_dir() + "/iter" + std::to_string(summary.iteration) + ".profile";
    profile_fp = fopen(filename.c_str(), "wb");
    BMRT_ASSERT_INFO(profile_fp, \
        "can't open file named:%s",filename.c_str());
}

void BMProfile::close_file()
{
    fclose(profile_fp);
    profile_fp = nullptr;
}

int BMProfile::getenv_int(const char *name, int default_val)
{
    auto value_str = getenv(name);
    int value = default_val;
    if(value_str){
        value = std::stoi(value_str);
        BMRT_LOG(INFO, "ENV %s=%s, value=%d", name, value_str, value);
    }
    return value;
}

bool BMProfile::getenv_bool(const char *name, bool default_val)
{
    auto value_str = getenv(name);
    bool value = default_val;
    if(value_str){
        value = value_str[0] == 'T' || value_str[0] == 't' || value_str[0] == '1' || std::stoi(value_str) != 0;
        BMRT_LOG(INFO, "ENV %s=%s, value=%d", name, value_str, value);
    }
    return value;
}

void BMProfile::save_cmd_profile()
{
    if(!current_enabled) return;
    if(!cmd_info) return;
    write_block(BLOCK_CMD, sizeof(profile_cmd_info_t)+cmd_info->group_num*sizeof(profile_cmd_num_t), cmd_info);
    delete []((u8*)cmd_info);
    cmd_info = nullptr;
}

void BMProfile::write_block(u32 type, size_t len, const void *data)
{
    if(len == 0) return;
    BMRT_LOG(INFO, "%s: type=%d, len=%d", __func__, type, (int)len);
    fwrite(&type, sizeof(type), 1, profile_fp);
    fwrite(&len, sizeof(u32), 1, profile_fp);
    fwrite(data, len, 1, profile_fp);
}

bool BMProfile::need_profile(int iteration, int subnet_id, int subnet_mode)
{
    if (!enabled) return false;
    if(!enabled_iterations.empty()){
        return enabled_iterations.count(iteration);
    }
    if(!enabled_subnet_ids.empty()){
        return enabled_subnet_ids.count(subnet_id);
    }
    if(!enabled_subnet_modes.empty()){
        return enabled_subnet_modes.count(subnet_mode);
    }
    return true;
}

void BMProfile::begin_subnet(net_ctx_t* net_ctx, int iteration, int subnet_id, int subnet_mode)
{
    current_enabled = need_profile(iteration, subnet_id, subnet_mode);
    if(!current_enabled) return;

    summary.iteration = iteration;
    summary.subnet_id = subnet_id;
    summary.subnet_type = subnet_mode;
    summary.extra_data = 0;
    summary.start_usec = get_usec();
    if (arch == BM1682) {
        bmfunc::bmdnn_1682()->set_bmdnn_func_profile(1);
    } else if (device) {
        device->begin(net_ctx);
    }
}

void BMProfile::set_extra_data(u64 data){
    if(!enabled || !current_enabled) return;
    summary.extra_data = data;
}

void BMProfile::end_subnet(net_ctx_t* net_ctx)
{
    if(!enabled || !current_enabled) return;
    summary.end_usec = get_usec();
    end_profile(net_ctx);
}

void BMProfile::set_enable(bool is_enable, set<int> iterations, set<int> subnet_ids, set<int> subnet_modes)
{
    enabled = is_enable && device && device->enabled();
    enabled_iterations = iterations;
    enabled_subnet_ids = subnet_ids;
    enabled_subnet_modes = subnet_modes;
    if(enabled){
        print_note();
    }
}

void BMProfile::end_profile(net_ctx_t* net_ctx)
{
    create_file();
    write_block(BLOCK_SUMMARY, sizeof(summary), &summary);
    auto arch = bmrt_arch_info::get_bmtpu_arch();
    if (arch == BM1682) {
        auto& v_log = bm1682_profile::get_log();
        write_block(BLOCK_FIRMWARE_LOG, v_log.size(), v_log.data());
        v_log.clear();
    } else {
        save_cmd_profile();
        if(device) device->end(net_ctx);
    }
    close_file();
}

}
