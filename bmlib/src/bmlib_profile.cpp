#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <time.h>
#include "bmlib_internal.h"
#include "bmlib_profile.h"
#include "bmlib_utils.h"
#include "api.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include<direct.h>
#endif

using std::string;
using std::vector;

#define PROFILE_INFO(fmt, ...)  printf("[perf][info] " fmt, ##__VA_ARGS__);
#define PROFILE_ERROR(fmt, ...)  printf("[perf][error] " fmt, ##__VA_ARGS__);

#ifdef _WIN32
#define BILLION (1E9)
static BOOL          g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

int clock_gettime(int dummy, struct timespec* ct)
{
	LARGE_INTEGER count;

	if (g_first_time) {
		g_first_time = 0;

		if (0 == QueryPerformanceFrequency(&g_counts_per_sec)) {
		g_counts_per_sec.QuadPart = 0;
		}
	}

	if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) ||
		(0 == QueryPerformanceCounter(&count))) {
		return -1;
	}

	ct->tv_sec  = count.QuadPart / g_counts_per_sec.QuadPart;
	ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) /
		g_counts_per_sec.QuadPart;

	return 0;
}

int gettimeofday(struct timeval* tp, void* tzp)
{
	struct timespec clock;
	clock_gettime(0, &clock);
	tp->tv_sec = clock.tv_sec;
	tp->tv_usec = clock.tv_nsec / 1000;
	return 0;
}


void timeradd(struct timeval* a, struct timeval* b, struct timeval* res)
{
	res->tv_sec  = a->tv_sec + b->tv_sec;
	res->tv_usec = a->tv_usec + b->tv_usec;
}

void timersub(struct timeval* a, struct timeval* b, struct timeval* res)
{
	res->tv_sec  = a->tv_sec - b->tv_sec;
	res->tv_usec = a->tv_usec - b->tv_usec;
}
#endif

//////////////////////////////////////////
// some useful utilities
#define TIME_TO_USECS(t) (t.tv_sec*1000000+ t.tv_usec)

inline u64 get_usec()
{
	timeval time;
	gettimeofday(&time, NULL);
	return TIME_TO_USECS(time);
}

static int bm_mkdir(const char *dirname, bool must_new)
{
	string dname = dirname;
	struct stat st;
	dname += "/";
	if (stat(dname.c_str(), &st) == -1) {
#ifdef __linux__
		mkdir(dname.c_str(), 0777);
#else
		_mkdir(dname.c_str());
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

//////////////////////////////////////////////////////////
static void write_block(FILE* profile_fp, u32 type, size_t len, const void *data)
{
	if(len == 0) return;
	// BMRT_LOG(INFO, "%s: type=%d, len=%d", __func__, type, (int)len);
	fwrite(&type, sizeof(type), 1, profile_fp);
	fwrite(&len, sizeof(u32), 1, profile_fp);
	fwrite(data, len, 1, profile_fp);
}

#define ENV_PREFIX "BMLIB_"
#define ENV_GDMA_SIZE (ENV_PREFIX "GDMA_RECORD_SIZE")
#define ENV_ARM_SIZE (ENV_PREFIX "ARM_RECORD_SIZE")
#define ENV_BDC_SIZE (ENV_PREFIX "BDC_RECORD_SIZE")
#define ENV_SHOW_SIZE (ENV_PREFIX "PERF_SHOW_SIZE")
#define ENV_SHOW_RAW_DATA (ENV_PREFIX "PERF_SHOW_RAW_DATA")
#define ENV_ENABLE_PROFILE (ENV_PREFIX "ENABLE_PROFILE")
#define ENV_PROFILE_PER_API (ENV_PREFIX "PROFILE_PER_API")

void free_buffer(bm_handle_t handle, buffer_pair_t *bp) {
	if(bp->ptr){
		delete [] bp->ptr;
		bp->ptr =nullptr;
		bm_free_device(handle, bp->mem);
	}
}

void must_alloc_buffer(bm_handle_t handle, buffer_pair_t *bp, size_t size) {
	if (bp->size != size || !bp->ptr) {
		free_buffer(handle, bp);
	}
	if (!bp->ptr) {
		bp->ptr= (u8 *)new u8[size];
		if (!bp->ptr) {
			PROFILE_ERROR("malloc system buffer failed for profile\n");
			exit(-1);
		}

		bm_status_t status = bm_malloc_device_byte(handle, &bp->mem, size);
		if (BM_SUCCESS != status)  {
			PROFILE_ERROR("device mem alloc failed: size=%d[0x%x], status=%d\n", (int)size, (int)size, (int)status);
			exit(-1);
		}
		bp->size = size;
	}
}

static inline int get_arch_code(bm_handle_t handle)
{
	(void)handle;
#ifdef USING_CMODEL
	int arch_code = handle->bm_dev->chip_id;
#else
	int arch_code = handle->misc_info.chipid;
#endif
	return arch_code;
}

static void show_profile_tips()
{
	PROFILE_INFO("=== this program is under PROFILE mode, which will cost extra time ===\n");
	PROFILE_INFO("=== close profile by unset 'BMLIB_ENABLE_ALL_PROFILE' and 'unset BMLIB_ENABLE_PROFILE' ===\n");
}

////////////////////////////////////////////////////////
// for BM1684

static bm_status_t __bm1684_set_profile_enable(bm_handle_t handle, bool enable)
{
	ASSERT(handle);
	u32 api_buffer_size = sizeof(u32);
	u32 profile_enable = enable;
	bm_status_t status = bm_send_api(handle, (sglib_api_id_t)BM_API_ID_SET_PROFILE_ENABLE,
										(u8*)&profile_enable, api_buffer_size);
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("bm_send_api failed, api id:%d, status:%d\n", BM_API_ID_SET_PROFILE_ENABLE, status);
	} else {
		status = bm_sync_api(handle);
		if (BM_SUCCESS != status) {
		PROFILE_ERROR("bm_sync_api failed, api id:%d, status:%d\n", BM_API_ID_SET_PROFILE_ENABLE, status);
		}
	}
	return status;
}

static bm_status_t __bm1684_get_profile_data(
    bm_handle_t handle,
    unsigned long long output_global_addr,
    unsigned int output_max_size,
    unsigned int byte_offset,
    unsigned int data_category //0: profile time records, 1: extra data
    )
{

	ASSERT(handle);
#pragma pack(1)
	struct {
		u64 mcu_reserved_addr;
		u64 output_global_addr;
		u32 output_size;
		u32 byte_offset;
		u32 data_category; //0: profile_data, 1: profile extra data
	} api_data;
#pragma pack()

	const u32 api_buffer_size = sizeof(api_data);

	api_data.mcu_reserved_addr = bm_gmem_arm_reserved_request(handle);
	api_data.output_global_addr = output_global_addr;
	api_data.output_size = output_max_size;
	api_data.byte_offset = byte_offset;
	api_data.data_category = data_category;

	sglib_api_id_t api_code = (sglib_api_id_t)BM_API_ID_GET_PROFILE_DATA;
	bm_status_t status = bm_send_api(handle, api_code, (u8*)&api_data, api_buffer_size);
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("bm_send_api failed, api id:%d, status:%d\n", api_code, status);
	} else {
		status = bm_sync_api(handle);
		if (BM_SUCCESS != status) {
			PROFILE_ERROR("bm_sync_api failed, api id:%d, status:%d\n",api_code, status);
		}
	}
	bm_gmem_arm_reserved_release(handle);
	return status;
}

#define BM1684_GDMA_FREQ_MHZ 575
#define BM1684_TPU_FREQ_MHZ 550

#pragma pack(1)
typedef struct {
	unsigned int inst_start_time;
	unsigned int inst_end_time;
	unsigned long inst_id : 16;
	unsigned long long computation_load : 48;
	unsigned int num_read;
	unsigned int num_read_stall;
	unsigned int num_write;
	unsigned int reserved;
} BM1684_TPU_PROFILE_FORMAT;

typedef struct {
	unsigned int inst_start_time;
	unsigned int inst_end_time;
	unsigned int inst_id : 16;
	unsigned int reserved: 16;
	unsigned int d0_aw_bytes;
	unsigned int d0_wr_bytes;
	unsigned int d0_ar_bytes;
	unsigned int d1_aw_bytes;
	unsigned int d1_wr_bytes;
	unsigned int d1_ar_bytes;
	unsigned int gif_aw_bytes;
	unsigned int gif_wr_bytes;
	unsigned int gif_ar_bytes;
	unsigned int d0_wr_valid_cyc;
	unsigned int d0_rd_valid_cyc;
	unsigned int d1_wr_valid_cyc;
	unsigned int d1_rd_valid_cyc;
	unsigned int gif_wr_valid_cyc;
	unsigned int gif_rd_valid_cyc;
	unsigned int d0_wr_stall_cyc;
	unsigned int d0_rd_stall_cyc;
	unsigned int d1_wr_stall_cyc;
	unsigned int d1_rd_stall_cyc;
	unsigned int gif_wr_stall_cyc;
	unsigned int gif_rd_stall_cyc;
	unsigned int d0_aw_end;
	unsigned int d0_aw_st;
	unsigned int d0_ar_end;
	unsigned int d0_ar_st;
	unsigned int d0_wr_end;
	unsigned int d0_wr_st;
	unsigned int d0_rd_end;
	unsigned int d0_rd_st;
	unsigned int d1_aw_end;
	unsigned int d1_aw_st;
	unsigned int d1_ar_end;
	unsigned int d1_ar_st;
	unsigned int d1_wr_end;
	unsigned int d1_wr_st;
	unsigned int d1_rd_end;
	unsigned int d1_rd_st;
	unsigned int gif_aw_reserved1;
	unsigned int gif_aw_reserved2;
	unsigned int gif_ar_end;
	unsigned int gif_ar_st;
	unsigned int gif_wr_end;
	unsigned int gif_wr_st;
	unsigned int gif_rd_end;
	unsigned int gif_rd_st;
} BM1684_GDMA_PROFILE_FORMAT;
#pragma pack()

static char num2char(int num)
{
	if(num<0){
		return '-';
	} else if(num>16){
		return '*';
	} else if(num>9){
		return num-10+'A';
	}
	return num + '0';
}
static void show_raw_data(const unsigned char* data, size_t len)
{
	std::string raw_str = "     len=";
	raw_str += std::to_string(len);
	raw_str += " [";
	for(size_t i=0; i<len; i++){
		auto c = data[i];
		raw_str += num2char(c>>4&0xf);
		raw_str += num2char(c&0xf);
		raw_str += " ";
	}
	raw_str += " ]\n";
	PROFILE_INFO("%s\n", raw_str.c_str());
}

void show_bm1684_gdma_item(const BM1684_GDMA_PROFILE_FORMAT* data, int len, float freq_MHz, float time_offset=0)
{
	if (len==0 || !data) return;
	int max_print = get_env_int_value(ENV_SHOW_SIZE, -1);
	bool show_raw = get_env_bool_value(ENV_SHOW_RAW_DATA, false);
	if (max_print<=0) return;
	int real_print = max_print>len?len: max_print;
	float period = 1/freq_MHz;
	PROFILE_INFO("Note: gdma record time_offset=%fus, freq=%gMHz, period=%.3fus\n", time_offset, freq_MHz, period);
	unsigned int cycle_offset = data[0].inst_start_time;
	for (int i=0; i<real_print; i++) {
		auto& p = data[i];
		auto total_time =  (p.inst_end_time - p.inst_start_time) * period;
		auto total_bytes = p.d0_wr_bytes + p.d1_wr_bytes + p.gif_wr_bytes;
		auto speed = total_time>0?total_bytes/total_time: -1000.0;
		PROFILE_INFO("---> gdma record #%d,"
					" inst_id=%d, cycle=%d, start=%.3fus,"
					" end=%.3fus, interval=%.3fus"
					" bytes=%d, speed=%.3fGB/s"
					"\n",
					i, p.inst_id,
					p.inst_end_time - p.inst_start_time,
					(p.inst_start_time - cycle_offset)* period + time_offset,
					(p.inst_end_time - cycle_offset) * period + time_offset,
					total_time,
					total_bytes,
					speed/1000.0);
		if (show_raw) {
			show_raw_data((const unsigned char*) &p, sizeof(p));
		}
	}
	if (real_print<len) {
		PROFILE_INFO("... (%d gdma records are not shown)\n", len-real_print);
	}
}

void show_bm1684_tiu_item(const BM1684_TPU_PROFILE_FORMAT* data, int len, float freq_MHz, float time_offset=0)
{
	if (len==0 || !data) return;
	int max_print = get_env_int_value(ENV_SHOW_SIZE, -1);
	bool show_raw = get_env_bool_value(ENV_SHOW_RAW_DATA, false);
	if (max_print<=0) return;
	int real_print = max_print>len?len: max_print;
	float period = 1/freq_MHz;
	PROFILE_INFO("Note: bdc record time_offset=%fus, freq=%gMHz, period=%.3fus\n", time_offset, freq_MHz, period);
	unsigned int cycle_offset = data[0].inst_start_time;
	for (int i=0; i<real_print; i++) {
		auto& p = data[i];
		PROFILE_INFO("---> bdc record #%d, inst_id=%d, cycle=%d, start=%.3fus, end=%.3fus, interval=%.3fus\n",
					i, (int)(p.inst_id), (int)(p.inst_end_time - p.inst_start_time),
					(p.inst_start_time - cycle_offset)* period + time_offset,
					(p.inst_end_time - cycle_offset) * period + time_offset,
					(p.inst_end_time - p.inst_start_time) * period);
		if(show_raw) {
			show_raw_data((const unsigned char*) &p, sizeof(p));
		}
	}
	if (real_print<len) {
		PROFILE_INFO("... (%d bdc records are not shown)\n", len-real_print);
	}
}

// fix bm1684 gdma perf data
const int   BLOCK_SIZE = 4096;
const int   BURST_LEN  = 192;
static void __fix_data(unsigned char* aligned_data, unsigned int len, size_t offset)
{
	if (len < BLOCK_SIZE)
		return;
	__fix_data(&aligned_data[BLOCK_SIZE], len - BLOCK_SIZE, offset + BLOCK_SIZE);
	unsigned int max_copy_len = (offset + BLOCK_SIZE + BURST_LEN - 1) / BURST_LEN * BURST_LEN - offset - BLOCK_SIZE;
	unsigned int copy_len = (max_copy_len < len - BLOCK_SIZE) ? max_copy_len : (len - BLOCK_SIZE);
	if (copy_len > 0) {
		memcpy(&aligned_data[BLOCK_SIZE], aligned_data, copy_len);
	}
	return;
}

static unsigned int fix_bm1684_gdma_monitor_data(unsigned char* aligned_data, unsigned int len)
{
	unsigned int valid_len = len;
	unsigned int item_size = sizeof(BM1684_GDMA_PROFILE_FORMAT);
	while (valid_len != 0 && aligned_data[valid_len-1]==0xFF) valid_len--;
	valid_len = (valid_len + item_size-1)/item_size * item_size;
	valid_len += item_size; //to fix the last record
	while (valid_len>len) valid_len -= item_size;
	__fix_data(aligned_data, valid_len, 0);
	unsigned int left_len = valid_len;
	unsigned int copy_len = BLOCK_SIZE;
	while (copy_len%BURST_LEN != 0) copy_len += BLOCK_SIZE;
	unsigned int invalid_size = (BURST_LEN+item_size -1)/item_size * item_size;
	copy_len -= invalid_size;
	unsigned char* src_ptr = aligned_data + invalid_size;
	unsigned char* dst_ptr = aligned_data;
	valid_len = 0;
	while (left_len>invalid_size) {
		unsigned int real_copy_len = left_len>(copy_len+invalid_size)?copy_len:(left_len - invalid_size);
		memcpy(dst_ptr, src_ptr, real_copy_len);
		dst_ptr += real_copy_len;
		src_ptr += copy_len + invalid_size;
		left_len -= (real_copy_len + invalid_size);
		valid_len += real_copy_len;
	}
	while(valid_len != 0 && aligned_data[valid_len-1]==0xFF) valid_len--;
	valid_len = valid_len/item_size * item_size;
	return valid_len;
}

bm_status_t bm1684_final_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	return BM_SUCCESS;
}

bm_status_t bm1684_final_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	return BM_SUCCESS;
}

bm_status_t bm1684_mcu_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	return __bm1684_set_profile_enable(handle, true);
}

bm_status_t bm1684_gdma_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	// set gdma_start_offset to aligned(start,4K)+192 to assure the front of data is right,
	// not to override by hw bug.
	const int align_number = BLOCK_SIZE;
	const int burst_len = BURST_LEN;
	u64 real_gdma_mem_addr = bm_mem_get_device_addr(profile->cores[core_id].gdma_buffer.mem);
	u64 real_gdma_mem_size = bm_mem_get_device_size(profile->cores[core_id].gdma_buffer.mem);
	u64 aligned_addr = ALIGN(real_gdma_mem_addr, align_number);
	u64 aligned_size = real_gdma_mem_size + real_gdma_mem_addr - aligned_addr;
	profile->cores[core_id].gdma_aligned_mem = profile->cores[core_id].gdma_buffer.mem;
	bm_mem_set_device_addr(&profile->cores[core_id].gdma_aligned_mem, aligned_addr);
	bm_mem_set_device_size(&profile->cores[core_id].gdma_aligned_mem, aligned_size);

	auto& gdma_perf_monitor = profile->cores[core_id].gdma_perf_monitor;
	gdma_perf_monitor.buffer_start_addr = aligned_addr + burst_len;
	gdma_perf_monitor.buffer_size = aligned_size - burst_len;
	gdma_perf_monitor.monitor_id = PERF_MONITOR_GDMA;

	memset(profile->cores[core_id].gdma_buffer.ptr, -1, profile->cores[core_id].gdma_buffer.size);
	u32 ret = bm_memcpy_s2d(handle, profile->cores[core_id].gdma_aligned_mem,
							profile->cores[core_id].gdma_buffer.ptr);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
	}
	return bm_enable_perf_monitor(handle, &gdma_perf_monitor);
}

bm_status_t bm1684_tiu_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	auto& tpu_perf_monitor = profile->cores[core_id].tpu_perf_monitor;
	tpu_perf_monitor.buffer_start_addr = bm_mem_get_device_addr(profile->cores[core_id].tiu_buffer.mem);
	tpu_perf_monitor.buffer_size = bm_mem_get_device_size(profile->cores[core_id].tiu_buffer.mem);
	tpu_perf_monitor.monitor_id = PERF_MONITOR_TPU;
	memset(profile->cores[core_id].tiu_buffer.ptr, -1, profile->cores[core_id].tiu_buffer.size);
	auto ret = bm_memcpy_s2d(handle, profile->cores[core_id].tiu_buffer.mem,
								profile->cores[core_id].tiu_buffer.ptr);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
		exit(-1);
	}
	return bm_enable_perf_monitor(handle, &tpu_perf_monitor);
}

bm_status_t bm1684_mcu_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	for(int i=0; i<2; i++){
		vector<u8> data;
		size_t offset = 0;
		size_t total_len = 0;
		u32 block_type = (i == 0) ? BLOCK_DYN_DATA : BLOCK_DYN_EXTRA;
		while(1) {
			auto status = __bm1684_get_profile_data(
					handle,
					bm_mem_get_device_addr(profile->cores[core_id].mcu_buffer.mem),
					bm_mem_get_device_size(profile->cores[core_id].mcu_buffer.mem),
					offset, i);
			ASSERT(status == BM_SUCCESS);
			status = bm_memcpy_d2s(handle, profile->cores[core_id].mcu_buffer.ptr,
									profile->cores[core_id].mcu_buffer.mem);
			ASSERT(status == BM_SUCCESS);
			auto u32_ptr = (u32*)profile->cores[core_id].mcu_buffer.ptr;
			auto read_len = u32_ptr[0];
			if (total_len==0) {
				total_len = u32_ptr[1];
			}
			auto data_ptr = (u8*)&u32_ptr[2];
			data.insert(data.end(), data_ptr, data_ptr + read_len);
			offset += read_len;
			if (offset>=total_len) break;
		}
		write_block(fp, block_type, data.size(), data.data());
	}
	return __bm1684_set_profile_enable(handle, false);
}

bm_status_t bm1684_tiu_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	bm_disable_perf_monitor(handle, &profile->cores[core_id].tpu_perf_monitor);
	auto ret = bm_memcpy_d2s(handle, profile->cores[core_id].tiu_buffer.ptr,
								profile->cores[core_id].tiu_buffer.mem);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("Get monitor profile from device to system failed, ret = %d\n", ret);
		exit(-1);
	}
	auto tpu_data = (BM1684_TPU_PROFILE_FORMAT *)profile->cores[core_id].tiu_buffer.ptr;
	u32 valid_len = 0;
	u32 tiu_record_len = profile->cores[core_id].tiu_buffer.size/sizeof(BM1684_TPU_PROFILE_FORMAT);
	while (tpu_data[valid_len].inst_start_time != 0xFFFFFFFF &&
			tpu_data[valid_len].inst_end_time != 0xFFFFFFFF &&
			valid_len < tiu_record_len)
		valid_len++;
	PROFILE_INFO("bdc record_num=%d, max_record_num=%d\n", (int)valid_len, (int)tiu_record_len);
	show_bm1684_tiu_item(tpu_data, valid_len, BM1684_TPU_FREQ_MHZ);
	write_block(fp, BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
	return BM_SUCCESS;
}

bm_status_t bm1684_gdma_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	bm_disable_perf_monitor(handle, &profile->cores[core_id].gdma_perf_monitor);
	auto ret = bm_memcpy_d2s(handle, profile->cores[core_id].gdma_buffer.ptr,
								profile->cores[core_id].gdma_aligned_mem);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("Get monitor profile from device to system failed, ret = %d\n", ret);
		exit(-1);
	}
	u32 valid_len = fix_bm1684_gdma_monitor_data(profile->cores[core_id].gdma_buffer.ptr,
												bm_mem_get_device_size(profile->cores[core_id].gdma_aligned_mem));
	u32 gdma_record_len = profile->cores[core_id].gdma_buffer.size/sizeof(BM1684_GDMA_PROFILE_FORMAT);
	u32 gdma_num = (valid_len/sizeof(BM1684_GDMA_PROFILE_FORMAT));
	PROFILE_INFO("gdma record_num=%d, max_record_num=%d\n",
				(int)gdma_num, (int)gdma_record_len);
	show_bm1684_gdma_item((BM1684_GDMA_PROFILE_FORMAT*)profile->cores[core_id].gdma_buffer.ptr,
							gdma_num, BM1684_GDMA_FREQ_MHZ);
	write_block(fp, BLOCK_MONITOR_GDMA, valid_len, profile->cores[core_id].gdma_buffer.ptr);
	return BM_SUCCESS;
}

////////////////////////////////////////////////////////
// for BM1686
bm_status_t bm1686_final_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	return BM_SUCCESS;
}

bm_status_t bm1686_final_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	return BM_SUCCESS;
}

static bm_status_t __bm1686_set_profile_enable(bm_handle_t handle, bool enable, int core_id)
{
	if (!(handle && handle->profile && handle->profile->cores[core_id].enable_profile_func_id))
		return BM_SUCCESS;
	u32 api_buffer_size = sizeof(u32);
	u32 profile_enable = enable;
	bm_status_t status = tpu_kernel_launch(handle, handle->profile->cores[core_id].enable_profile_func_id,
											&profile_enable, api_buffer_size);
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("%s tpu kernel launch failed, status:%d\n", __func__, status);
	}
	return status;
}

bm_status_t bm1686_mcu_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	if(!handle || !profile || !profile->cores[core_id].enable_profile_func_id) return BM_SUCCESS;
	auto status = __bm1686_set_profile_enable(handle, true, core_id);
	return status;
}

static bm_status_t __bm1686_get_profile_data(
    bm_handle_t handle,
    bmlib_profile_t* profile,
    unsigned long long output_global_addr,
    unsigned int output_max_size,
    unsigned int byte_offset,
    unsigned int data_category, //0: profile time records, 1: extra data
    int core_id
    )
{
	ASSERT_INFO(handle && profile, "handle=%p, profile=%p", handle, profile);
	if (!handle->profile->cores[core_id].get_profile_func_id) return BM_ERR_DEVNOTREADY;
#pragma pack(1)
	struct {
		u64 mcu_reserved_addr;
		u64 output_global_addr;
		u32 output_size;
		u32 byte_offset;
		u32 data_category; //0: profile_data, 1: profile extra data
	} api_data;
#pragma pack()

	const u32 api_buffer_size = sizeof(api_data);

	api_data.mcu_reserved_addr = bm_gmem_arm_reserved_request(handle);
	api_data.output_global_addr = output_global_addr;
	api_data.output_size = output_max_size;
	api_data.byte_offset = byte_offset;
	api_data.data_category = data_category;

	sglib_api_id_t api_code = (sglib_api_id_t)BM_API_ID_GET_PROFILE_DATA;
	bm_status_t status = tpu_kernel_launch(handle, handle->profile->cores[core_id].get_profile_func_id,
											&api_data, sizeof(api_data));
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("%s tpu kernel launch failed, status:%d\n", __func__, status);
	}
	bm_gmem_arm_reserved_release(handle);
	return status;
}

#pragma pack(1)
#define BM1686_GDMA_ITEM_SIZE 256
typedef BM1684_TPU_PROFILE_FORMAT BM1686_TPU_PROFILE_FORMAT;
typedef struct {
	BM1684_GDMA_PROFILE_FORMAT valid_data;
	unsigned char reserved[BM1686_GDMA_ITEM_SIZE-sizeof(BM1684_GDMA_PROFILE_FORMAT)];
} BM1686_GDMA_PROFILE_FORMAT;
#pragma pack()

bm_status_t bm1686_gdma_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	auto& perf_monitor = profile->cores[core_id].gdma_perf_monitor;
	perf_monitor.buffer_start_addr = bm_mem_get_device_addr(profile->cores[core_id].gdma_buffer.mem);
	perf_monitor.buffer_size = bm_mem_get_device_size(profile->cores[core_id].gdma_buffer.mem);
	perf_monitor.monitor_id = PERF_MONITOR_GDMA;
	memset(profile->cores[core_id].gdma_buffer.ptr, -1, profile->cores[core_id].gdma_buffer.size);
	auto ret = bm_memcpy_s2d(handle, profile->cores[core_id].gdma_buffer.mem,
								profile->cores[core_id].gdma_buffer.ptr);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
		exit(-1);
	}
	return bm_enable_perf_monitor(handle, &perf_monitor);
}

bm_status_t bm1686_tiu_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	auto& tpu_perf_monitor = profile->cores[core_id].tpu_perf_monitor;
	tpu_perf_monitor.buffer_start_addr = bm_mem_get_device_addr(profile->cores[core_id].tiu_buffer.mem);
	tpu_perf_monitor.buffer_size = bm_mem_get_device_size(profile->cores[core_id].tiu_buffer.mem);
	tpu_perf_monitor.monitor_id = PERF_MONITOR_TPU;
	memset(profile->cores[core_id].tiu_buffer.ptr, -1, profile->cores[core_id].tiu_buffer.size);
	auto ret = bm_memcpy_s2d(handle, profile->cores[core_id].tiu_buffer.mem,
								profile->cores[core_id].tiu_buffer.ptr);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
		exit(-1);
	}
	return bm_enable_perf_monitor(handle, &tpu_perf_monitor);
}

bm_status_t bm1686_mcu_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	for (int i=0; i<2; i++) {
		vector<u8> data;
		size_t offset = 0;
		size_t total_len = 0;
		u32 block_type = (i == 0) ? BLOCK_DYN_DATA : BLOCK_DYN_EXTRA;
		while (1) {
			auto status = __bm1686_get_profile_data(
					handle, profile,
					bm_mem_get_device_addr(profile->cores[core_id].mcu_buffer.mem),
					bm_mem_get_device_size(profile->cores[core_id].mcu_buffer.mem),
					offset, i,
					core_id);
			if (status == BM_ERR_DEVNOTREADY) break;
			ASSERT(status == BM_SUCCESS);
			status = bm_memcpy_d2s(handle, profile->cores[core_id].mcu_buffer.ptr, profile->cores[core_id].mcu_buffer.mem);
			ASSERT(status == BM_SUCCESS);
			auto u32_ptr = (u32*)profile->cores[core_id].mcu_buffer.ptr;
			auto read_len = u32_ptr[0];
			if (total_len==0) {
				total_len = u32_ptr[1];
			}
			PROFILE_INFO("total_len= %d, offset=%d\n", (int)total_len, (int)offset);
			auto data_ptr = (u8*)&u32_ptr[2];
			data.insert(data.end(), data_ptr, data_ptr + read_len);
			offset += read_len;
			if(offset>=total_len) break;
		}
		PROFILE_INFO("dyn_data = %d bytes\n", (int)data.size());
		write_block(fp, block_type, data.size(), data.data());
	}
	return __bm1686_set_profile_enable(handle, false, core_id);
}

bm_status_t bm1686_tiu_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	bm_disable_perf_monitor(handle, &profile->cores[core_id].tpu_perf_monitor);
	auto ret = bm_memcpy_d2s(handle, profile->cores[core_id].tiu_buffer.ptr, profile->cores[core_id].tiu_buffer.mem);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("Get bdc monitor profile from device to system failed, ret = %d\n", ret);
		exit(-1);
	}
	auto tpu_data = (BM1686_TPU_PROFILE_FORMAT *)profile->cores[core_id].tiu_buffer.ptr;
	u32 valid_len = 0;
	u32 tiu_record_len = profile->cores[core_id].tiu_buffer.size/sizeof(BM1686_TPU_PROFILE_FORMAT);
	while (tpu_data[valid_len].inst_start_time != 0xFFFFFFFF &&
			tpu_data[valid_len].inst_end_time != 0xFFFFFFFF &&
			valid_len < tiu_record_len)
		valid_len++;
	PROFILE_INFO("bdc record_num=%d, max_record_num=%d\n", (int)valid_len, (int)tiu_record_len);

	int freq_MHz = 0;
	bm_get_clk_tpu_freq(handle, &freq_MHz);
	show_bm1684_tiu_item(tpu_data, valid_len, freq_MHz);

	write_block(fp, BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
	return BM_SUCCESS;
}

bm_status_t bm1686_gdma_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	bm_disable_perf_monitor(handle, &profile->cores[core_id].gdma_perf_monitor);
	auto ret = bm_memcpy_d2s(handle, profile->cores[core_id].gdma_buffer.ptr,
								profile->cores[core_id].gdma_buffer.mem);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("Get gdma monitor profile from device to system failed, ret = %d\n", ret);
		exit(-1);
	}
	auto gdma_data = (BM1686_GDMA_PROFILE_FORMAT *)profile->cores[core_id].gdma_buffer.ptr;
	u32 valid_len = 0;
	u32 record_len = profile->cores[core_id].gdma_buffer.size/sizeof(BM1686_GDMA_PROFILE_FORMAT);
	while (gdma_data[valid_len].valid_data.inst_start_time != 0xFFFFFFFF &&
			gdma_data[valid_len].valid_data.inst_end_time != 0xFFFFFFFF &&
			valid_len < record_len)
		valid_len++;
	PROFILE_INFO("gdma record_num=%d, max_record_num=%d\n", (int)valid_len, (int)record_len);

	std::vector<decltype(gdma_data->valid_data)> valid_data(valid_len);
	for (u32 i=0; i<valid_len; i++) {
		valid_data[i] = gdma_data[i].valid_data;
	}
	int freq_MHz = 0;
	bm_get_clk_tpu_freq(handle, &freq_MHz);
	show_bm1684_gdma_item(valid_data.data(), valid_data.size(), freq_MHz);
	write_block(fp, BLOCK_MONITOR_GDMA, valid_len * sizeof(gdma_data->valid_data), valid_data.data());
	return BM_SUCCESS;
}


///////////////////////////////////////////////////////////////
/// bm1688 interface
///
#pragma pack(1)
typedef struct {
	// 0x00-0x0F
	unsigned int inst_start_time;
	unsigned int inst_end_time;
	unsigned int inst_id : 20;
	unsigned int reserved0 : 12;
	unsigned int axi_d0_aw_bytes;

	// 0x10-0x1F
	unsigned int axi_d0_wr_bytes;
	unsigned int axi_d0_ar_bytes;
	unsigned int axi_d1_aw_bytes;
	unsigned int axi_d1_wr_bytes;

	// 0x20-0x2F
	unsigned int axi_d1_ar_bytes;
	unsigned int gif_fmem_aw_bytes;
	unsigned int gif_fmem_wr_bytes;
	unsigned int gif_fmem_ar_bytes;

	// 0x30-0x3F
	unsigned int gif_l2sram_aw_bytes;
	unsigned int gif_l2sram_wr_bytes;
	unsigned int gif_l2sram_ar_bytes;
	unsigned int reserved1;

	// 0x40-0x4F
	unsigned int axi_d0_wr_valid_bytes;
	unsigned int axi_d0_rd_valid_bytes;
	unsigned int axi_d1_wr_valid_bytes;
	unsigned int axi_d1_rd_valid_bytes;

	// 0x50-0x5F
	unsigned int gif_fmem_wr_valid_bytes;
	unsigned int gif_fmem_rd_valid_bytes;
	unsigned int gif_l2sram_wr_valid_bytes;
	unsigned int gif_l2sram_rd_valid_bytes;

	// 0x60-0x6F
	unsigned int axi_d0_wr_stall_bytes;
	unsigned int axi_d0_rd_stall_bytes;
	unsigned int axi_d1_wr_stall_bytes;
	unsigned int axi_d1_rd_stall_bytes;

	// 0x70-0x7F
	unsigned int gif_fmem_wr_stall_bytes;
	unsigned int gif_fmem_rd_stall_bytes;
	unsigned int gif_l2sram_wr_stall_bytes;
	unsigned int gif_l2sram_rd_stall_bytes;

	// 0x80-0x8F
	unsigned int axi_d0_aw_end;
	unsigned int axi_d0_aw_st;
	unsigned int axi_d0_ar_end;
	unsigned int axi_d0_ar_st;

	// 0x90-0x9F
	unsigned int axi_d0_wr_end;
	unsigned int axi_d0_wr_st;
	unsigned int axi_d0_rd_end;
	unsigned int axi_d0_rd_st;

	// 0xA0-0xAF
	unsigned int axi_d1_aw_end;
	unsigned int axi_d1_aw_st;
	unsigned int axi_d1_ar_end;
	unsigned int axi_d1_ar_st;

	// 0xB0-0xBF
	unsigned int axi_d1_wr_end;
	unsigned int axi_d1_wr_st;
	unsigned int axi_d1_rd_end;
	unsigned int axi_d1_rd_st;

	// 0xC0-0xCF
	unsigned int reserved2;
	unsigned int reserved3;
	unsigned int gif_fmem_ar_end;
	unsigned int gif_fmem_ar_st;

	// 0xD0-0xDF
	unsigned int gif_fmem_wr_end;
	unsigned int gif_fmem_wr_st;
	unsigned int gif_fmem_rd_end;
	unsigned int gif_fmem_rd_st;

	// 0xE0-0xEF
	unsigned int reserved4;
	unsigned int reserved5;
	unsigned int gif_l2sram_ar_end;
	unsigned int gif_l2sram_ar_st;

	// 0xF0-0xFF
	unsigned int gif_l2sram_wr_end;
	unsigned int gif_l2sram_wr_st;
	unsigned int gif_l2sram_rd_end;
	unsigned int gif_l2sram_rd_st;
} BM1688_GDMA_PROFILE_FORMAT;

typedef struct {
	unsigned int inst_start_time;
	unsigned int inst_end_time;
	unsigned long long inst_id : 16;
	unsigned long long computation_load : 48;
	unsigned int num_read;
	unsigned int num_read_stall;
	unsigned int num_write;
	unsigned int reserved;
} BM1688_TPU_PROFILE_FORMAT;
#pragma pack()

#pragma pack(1)
typedef struct {
	int engine;
	unsigned long long addr;
	unsigned long long size;
} sg_api_engine_profile_param_t;
#pragma pack()

#define PROFILE_ENGINE_MCU 0
#define PROFILE_ENGINE_GDMA 1
#define PROFILE_ENGINE_TIU 2

static bm_status_t __bm1688_set_engine_profile_param(bm_handle_t handle, int core, int engine_type,
														unsigned long long addr, unsigned long long size)
{
	if(!handle || !handle->profile) return BM_SUCCESS;
	auto set_engine_param_func_id = handle->profile->cores[core].set_engine_profile_param_func_id;
	if(!set_engine_param_func_id) return BM_SUCCESS;

	sg_api_engine_profile_param_t param={
		.engine = engine_type,
		.addr = addr,
		.size = size
	};

	bm_status_t status = tpu_kernel_launch_async_from_core(handle, set_engine_param_func_id, (u8*)&param,
															sizeof(param), core);
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("%s failed, api id:%d, status:%d", __func__, BM_API_ID_SET_ENGINE_PROFILE_PARAM, status);
	}
	return status;
}

static bm_status_t __bm1688_set_profile_enable(bm_handle_t handle, unsigned enable_bits, int core_id)
{
	if(!handle || !handle->profile) return BM_SUCCESS;
	auto set_profile_func_id = handle->profile->cores[core_id].enable_profile_func_id;
	if(!set_profile_func_id) return BM_SUCCESS;
	u32 api_buffer_size = sizeof(u32);
	u32 profile_enable = enable_bits;

	bm_status_t status = tpu_kernel_launch_async_from_core(handle, set_profile_func_id, (void*)&profile_enable,
															api_buffer_size, core_id);
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("%s failed, api id:%d, status:%d", __func__, BM_API_ID_SET_PROFILE_ENABLE, status);
	}
	return status;
}

bm_status_t bm1688_final_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id) {
	if(!handle || !profile) return BM_SUCCESS;
	u32 enable_bits = 0;
	if(profile->cores[core_id].tiu_buffer.size>0){
		enable_bits |= 1<<PROFILE_ENGINE_TIU;
	}
	if(profile->cores[core_id].gdma_buffer.size>0){
		enable_bits |= 1<<PROFILE_ENGINE_GDMA;
	}
	if(profile->cores[core_id].mcu_buffer.size>0) {
		enable_bits |= 1<<PROFILE_ENGINE_MCU;
	}
	if(!enable_bits) return BM_SUCCESS;
	return __bm1688_set_profile_enable(handle, enable_bits, core_id);
}

bm_status_t bm1688_final_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	return __bm1688_set_profile_enable(handle, 0, core_id);
}

void show_bm1688_gdma_item(const BM1688_GDMA_PROFILE_FORMAT* data, int len, float freq_MHz, int core_id, float time_offset=0)
{
	if (len==0 || !data) return;
	int max_print = get_env_int_value(ENV_SHOW_SIZE, -1);
	bool show_raw = get_env_bool_value(ENV_SHOW_RAW_DATA, false);
	if (max_print<=0) return;
	int real_print = max_print>len?len: max_print;
	float period = 1/freq_MHz;
	PROFILE_INFO("Note: core_id=%d, gdma record time_offset=%fus, freq=%gMHz, period=%.3fus\n", core_id,
					time_offset, freq_MHz, period);
	unsigned int cycle_offset = data[0].inst_start_time;
	for (int i=0; i<real_print; i++) {
		auto& p = data[i];
		auto total_time =  (p.inst_end_time - p.inst_start_time) * period;
		auto total_bytes = p.axi_d0_wr_bytes + p.axi_d1_wr_bytes + p.gif_fmem_wr_bytes;
		auto speed = total_time>0?total_bytes/total_time: -1000.0;
		PROFILE_INFO("[%d] ---> gdma record #%d,"
					" inst_id=%d, cycle=%d, start=%.3fus,"
					" end=%.3fus, interval=%.3fus"
					" bytes=%d, "
					"speed=%.3fGB/s, "
					"gif_wr=%d, gif_rd=%d, d0_wr=%d, d1_wr=%d, d0_rd=%d, d1_rd=%d"
					"\n",
					core_id, i, p.inst_id,
					p.inst_end_time - p.inst_start_time,
					(p.inst_start_time - cycle_offset)* period + time_offset,
					(p.inst_end_time - cycle_offset) * period + time_offset,
					total_time,
					total_bytes,
					speed/1000.0,
					p.gif_fmem_wr_bytes,
					p.gif_fmem_ar_bytes,
					p.axi_d0_wr_bytes,
					p.axi_d1_wr_bytes,
					p.axi_d0_ar_bytes,
					p.axi_d1_ar_bytes
					);
		if (show_raw) {
			show_raw_data((const unsigned char*) &p, sizeof(p));
		}
	}

	if (real_print<len) {
		PROFILE_INFO("[%d] ... (%d gdma records are not shown)\n", core_id, len-real_print);
	}
}

void show_bm1688_tiu_item(const BM1688_TPU_PROFILE_FORMAT* data, int len, float freq_MHz,
							int core_id, float time_offset=0)
{
	if (len==0 || !data) return;
	int max_print = get_env_int_value(ENV_SHOW_SIZE, -1);
	bool show_raw = get_env_bool_value(ENV_SHOW_RAW_DATA, false);
	if (max_print<=0) return;
	int real_print = max_print>len?len: max_print;
	float period = 1/freq_MHz;
	PROFILE_INFO("Note: core_id=%d, bdc record time_offset=%fus, freq=%gMHz, period=%.3fus\n", core_id,
					time_offset, freq_MHz, period);
	unsigned int cycle_offset = data[0].inst_start_time;
	for (int i=0; i<real_print; i++) {
		auto& p = data[i];
		PROFILE_INFO("[%d] ---> bdc record #%d, inst_id=%d, cycle=%d, start=%.3fus, end=%.3fus, interval=%.3fus\n",
					core_id, i,
					(int)(p.inst_id), (int)(p.inst_end_time - p.inst_start_time),
					(p.inst_start_time - cycle_offset)* period + time_offset,
					(p.inst_end_time - cycle_offset) * period + time_offset,
					(p.inst_end_time - p.inst_start_time) * period);
		if (show_raw) {
			show_raw_data((const unsigned char*) &p, sizeof(p));
		}
	}

	if (real_print<len) {
		PROFILE_INFO("[%d] ... (%d bdc records are not shown)\n", core_id, len-real_print);
	}
}

bm_status_t bm1688_mcu_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	return BM_SUCCESS;
}

static bm_status_t __bm1688_get_profile_data(
    bm_handle_t handle,
    bmlib_profile_t* profile,
    unsigned long long output_global_addr,
    unsigned int output_max_size,
    unsigned int byte_offset,
    unsigned int data_category, //0: profile time records, 1: extra data
    int core_id
    )
{
	ASSERT_INFO(handle && profile, "handle=%p, profile=%p", handle, profile);
	if (!handle->profile->cores[core_id].get_profile_func_id) return BM_SUCCESS;
#pragma pack(1)
	struct {
		u64 mcu_reserved_addr;
		u64 output_global_addr;
		u32 output_size;
		u32 byte_offset;
		u32 data_category; //0: profile_data, 1: profile extra data
	} api_data;
#pragma pack()
	auto get_profile_func_id = handle->profile->cores[core_id].get_profile_func_id;

	const u32 api_buffer_size = sizeof(api_data);

	api_data.mcu_reserved_addr = -1;
	api_data.output_global_addr = output_global_addr;
	api_data.output_size = output_max_size;
	api_data.byte_offset = byte_offset;
	api_data.data_category = data_category;

	bm_status_t status = tpu_kernel_launch_async_from_core(handle, get_profile_func_id,
											(void*)&api_data, sizeof(api_data), core_id);
	if (BM_SUCCESS != status) {
		PROFILE_ERROR("%s tpu kernel launch failed, status:%d\n", __func__, status);
	}
	return status;
}

bm_status_t bm1688_gdma_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	u64 buffer_start_addr = bm_mem_get_device_addr(profile->cores[core_id].gdma_buffer.mem);
	u64 buffer_size = bm_mem_get_device_size(profile->cores[core_id].gdma_buffer.mem);
	memset(profile->cores[core_id].gdma_buffer.ptr, 0, profile->cores[core_id].gdma_buffer.size);
	auto ret = bm_memcpy_s2d(handle, profile->cores[core_id].gdma_buffer.mem,
								profile->cores[core_id].gdma_buffer.ptr);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
		exit(-1);
	}

	return __bm1688_set_engine_profile_param(handle, core_id, PROFILE_ENGINE_GDMA,
												buffer_start_addr, buffer_size);
}

bm_status_t bm1688_tiu_init(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	u64 buffer_start_addr = bm_mem_get_device_addr(profile->cores[core_id].tiu_buffer.mem);
	u64 buffer_size = bm_mem_get_device_size(profile->cores[core_id].tiu_buffer.mem);
	memset(profile->cores[core_id].tiu_buffer.ptr, 0, profile->cores[core_id].tiu_buffer.size);
	auto ret = bm_memcpy_s2d(handle, profile->cores[core_id].tiu_buffer.mem,
								profile->cores[core_id].tiu_buffer.ptr);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
		exit(-1);
	}

	return __bm1688_set_engine_profile_param(handle, core_id, PROFILE_ENGINE_TIU,
												buffer_start_addr, buffer_size);
}

bm_status_t bm1688_mcu_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	for (int i=0; i<2; i++) {
		vector<u8> data;
		size_t offset = 0;
		size_t total_len = 0;
		u32 block_type = (i == 0) ? BLOCK_DYN_DATA : BLOCK_DYN_EXTRA;
		while(1){
			u64 buffer_addr = bm_mem_get_device_addr(profile->cores[core_id].mcu_buffer.mem);
			size_t buffer_size = bm_mem_get_device_size(profile->cores[core_id].mcu_buffer.mem);
			auto status = __bm1688_get_profile_data(
					handle, profile,
					buffer_addr, buffer_size,
					offset, i,
					core_id);
			if (status == BM_ERR_DEVNOTREADY) break;
			ASSERT(status == BM_SUCCESS);
			status = bm_memcpy_d2s(handle, profile->cores[core_id].mcu_buffer.ptr,
									profile->cores[core_id].mcu_buffer.mem);
			ASSERT(status == BM_SUCCESS);
			auto u32_ptr = (u32*)profile->cores[core_id].mcu_buffer.ptr;
			auto read_len = u32_ptr[0];
			if (total_len==0) {
				total_len = u32_ptr[1];
			}
			auto data_ptr = (u8*)&u32_ptr[2];
			data.insert(data.end(), data_ptr, data_ptr + read_len);
			offset += read_len;
			if(offset>=total_len) break;
		}
		PROFILE_INFO("dyn_type=%d, dyn_data = %d bytes\n", i, (int)data.size());
		write_block(fp, block_type, data.size(), data.data());
	}
	return BM_SUCCESS;
}

bm_status_t bm1688_tiu_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	auto ret = bm_memcpy_d2s(handle, profile->cores[core_id].tiu_buffer.ptr,
								profile->cores[core_id].tiu_buffer.mem);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("Get bdc monitor profile from device to system failed, ret = %d\n", ret);
		exit(-1);
	}

	auto tpu_data = (BM1688_TPU_PROFILE_FORMAT *)profile->cores[core_id].tiu_buffer.ptr;
	u32 valid_len = 0;
	u32 tiu_record_len = profile->cores[core_id].tiu_buffer.size/sizeof(BM1688_TPU_PROFILE_FORMAT);
	while (tpu_data[valid_len].inst_start_time != 0 &&
			tpu_data[valid_len].inst_end_time != 0 &&
			valid_len < tiu_record_len)
		valid_len++;
	PROFILE_INFO("bdc record_num=%d, max_record_num=%d\n", (int)valid_len, (int)tiu_record_len);

	int tiu_freq_MHz = 1000;
	bm_get_clk_tpu_freq(handle, &tiu_freq_MHz);
	// TODO: remove the hack
	if(tiu_freq_MHz==0) tiu_freq_MHz = 1000;
	show_bm1688_tiu_item(tpu_data, valid_len, tiu_freq_MHz, core_id);

	write_block(fp, BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
	return BM_SUCCESS;
}

bm_status_t bm1688_gdma_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id)
{
	auto ret = bm_memcpy_d2s(handle, profile->cores[core_id].gdma_buffer.ptr,
								profile->cores[core_id].gdma_buffer.mem);
	if (ret != BM_SUCCESS) {
		PROFILE_ERROR("Get gdma monitor profile from device to system failed, ret = %d\n", ret);
		exit(-1);
	}

	auto gdma_data = (BM1688_GDMA_PROFILE_FORMAT *)profile->cores[core_id].gdma_buffer.ptr;
	u32 valid_len = 0;
	u32 record_len = profile->cores[core_id].gdma_buffer.size/sizeof(BM1688_GDMA_PROFILE_FORMAT);
	while (gdma_data[valid_len].inst_start_time != 0 &&
			gdma_data[valid_len].inst_end_time != 0 && valid_len < record_len)
		valid_len++;
	PROFILE_INFO("gdma record_num=%d, max_record_num=%d\n", (int)valid_len, (int)record_len);

	int tiu_freq_MHz = 0;
	bm_get_clk_tpu_freq(handle, &tiu_freq_MHz);
	// TODO: remove the hack
	if(tiu_freq_MHz==0) tiu_freq_MHz = 1000;

	// Note: GDMA and TIU in BM1688 have different frequence.
	// The config is from PMT
	int gdma_freq_MHz = 750;
	if(tiu_freq_MHz == 375) gdma_freq_MHz = 500;
	show_bm1688_gdma_item(gdma_data, valid_len, gdma_freq_MHz, core_id);
	write_block(fp, BLOCK_MONITOR_GDMA, valid_len * sizeof(gdma_data[0]), gdma_data);
	return BM_SUCCESS;
}

///////////////////////////////////////////////////////////////
/// BMProfile framework
///

typedef bm_status_t (*bm_profile_init_t)(bm_handle_t handle, bmlib_profile_t* profile, int core_id);
typedef bm_status_t (*bm_profile_deinit_t)(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp, int core_id);

struct bm_arch_profile_info_t {
	int arch_value;
	bm_profile_init_t tiu_init;
	bm_profile_init_t gdma_init;
	bm_profile_init_t mcu_init;
	bm_profile_deinit_t tiu_deinit;
	bm_profile_deinit_t gdma_deinit;
	bm_profile_deinit_t mcu_deinit;
	bm_profile_init_t final_init;
	bm_profile_deinit_t final_deinit;
	u32 tpu_record_size;
	u32 gdma_record_size;
};

#define ARCH_ITEM(arch, code, value)                   \
{                                                    \
	code,                                              \
	{                                                  \
	value,                                           \
		bm##arch##_tiu_init,                         \
		bm##arch##_gdma_init,                        \
		bm##arch##_mcu_init,                         \
		bm##arch##_tiu_deinit,                       \
		bm##arch##_gdma_deinit,                      \
		bm##arch##_mcu_deinit,                       \
		bm##arch##_final_init,                       \
		bm##arch##_final_deinit,                     \
		(u32)sizeof(BM##arch##_TPU_PROFILE_FORMAT),  \
		(u32)sizeof(BM##arch##_GDMA_PROFILE_FORMAT), \
	}                                                  \
}

std::map<int, bm_arch_profile_info_t> _global_arch_profile_map = {
	ARCH_ITEM(1684, 0x1684, 1),
	ARCH_ITEM(1686, 0x1686, 3),
	ARCH_ITEM(1688, 0x1686a200, 4),
};


void bm_profile_init_record(bm_handle_t handle)
{
	auto profile = handle->profile;
	auto& arch_info = _global_arch_profile_map.at(profile->arch_code);
	for(unsigned int core_id=0; core_id<profile->cores.size(); core_id++) {
		profile->cores[core_id].summary.interval.begin_usec = get_usec();
		profile->cores[core_id].summary.send_info.clear();
		profile->cores[core_id].summary.copy_info.clear();
		profile->cores[core_id].summary.sync_info.clear();
		profile->cores[core_id].summary.mark_info.clear();
		profile->cores[core_id].summary.mem_info.clear();
		profile->cores[core_id].summary.extra_info.clear();

		if(profile->cores[core_id].tiu_buffer.size>0 && arch_info.tiu_init){
			arch_info.tiu_init(handle, profile, core_id);
		}

		if(profile->cores[core_id].gdma_buffer.size>0 && arch_info.gdma_init){
			arch_info.gdma_init(handle, profile, core_id);
		}

		if(profile->cores[core_id].mcu_buffer.size>0 && arch_info.mcu_init) {
			arch_info.mcu_init(handle, profile, core_id);
		}
	}

	for(unsigned int core_id=0; core_id<profile->cores.size(); core_id++) {
		arch_info.final_init(handle, profile, core_id);
	}
}

bm_status_t bm_profile_create(bm_handle_t handle, bool force_enable)
{
	auto arch_code = get_arch_code(handle);
	if(_global_arch_profile_map.count(arch_code) == 0) return BM_NOT_SUPPORTED;
	auto& arch_info = _global_arch_profile_map.at(arch_code);
	// judge if need profile
	int gdma_record_len = get_env_int_value(ENV_GDMA_SIZE, 128*1024);
	int tiu_record_len = get_env_int_value(ENV_BDC_SIZE, 128*1024);
	int dyn_max_size = get_env_int_value(ENV_ARM_SIZE, 1024*1024);
	bool enable_mcu_perf =  dyn_max_size > 0;
	bool enable_tiu_perf =  tiu_record_len > 0;
	bool enable_gdma_perf = gdma_record_len > 0;
	bool enable_profile = (enable_mcu_perf || enable_tiu_perf || enable_gdma_perf) &&
		(force_enable || get_env_bool_value(ENV_ENABLE_PROFILE, false));
	if (!enable_profile) return BM_SUCCESS;

	// set bmprofile struct
	bmlib_profile_t *profile = new bmlib_profile_t;
	if(!profile) return BM_ERR_FAILURE;

	PROFILE_INFO("Profiling init for %x\n", arch_code);
	profile->arch_code = arch_code;

	static int profile_count = 0;
	profile->id = profile_count++;

	unsigned int core_num = 1;
	bm_get_tpu_scalar_num(handle, &core_num);
	profile->cores.resize(core_num);
	profile->dir = "bmprofile_data-" + std::to_string(handle->dev_id);
	bm_mkdir(profile->dir.c_str(), false);
	handle->profile = profile;

	for (size_t core_id=0; core_id < core_num; core_id++){
		u64 gdma_device_buffer_size = gdma_record_len * arch_info.gdma_record_size;
		must_alloc_buffer(handle, &profile->cores[core_id].gdma_buffer, gdma_device_buffer_size);
		u64 tpu_device_buffer_size = tiu_record_len * arch_info.tpu_record_size;
		must_alloc_buffer(handle, &profile->cores[core_id].tiu_buffer, tpu_device_buffer_size);
		must_alloc_buffer(handle, &profile->cores[core_id].mcu_buffer, dyn_max_size);
		profile->cores[core_id].save_per_api_mode = get_env_bool_value(ENV_PROFILE_PER_API, false);
		profile->cores[core_id].save_id = 0;
		profile->cores[core_id].is_dumped = false;
		profile->cores[core_id].profile_running = false;
		profile->cores[core_id].summary.copy_info.reserve(128);
		profile->cores[core_id].summary.send_info.reserve(128);
		profile->cores[core_id].summary.sync_info.reserve(128);
		profile->cores[core_id].summary.mark_info.reserve(128);
		profile->cores[core_id].summary.mem_info.reserve(128);
		profile->cores[core_id].summary.extra_info.reserve(4096);
		profile->cores[core_id].summary.interval.begin_usec = get_usec();
	}
	return BM_SUCCESS;
}

bm_status_t bm_profile_init(bm_handle_t handle, bool force_enable)
{
	auto ret = bm_profile_create(handle, force_enable);
	if(ret != BM_SUCCESS) return ret;

	bm_profile_init_record(handle);
	show_profile_tips();
	return BM_SUCCESS;
}

void bm_profile_save_summary_data(bmlib_profile_t * profile, FILE* fp, int core_id)
{
	u32 num_send = profile->cores[core_id].summary.send_info.size();
	u32 num_mark= profile->cores[core_id].summary.mark_info.size();
	u32 num_sync = profile->cores[core_id].summary.sync_info.size();
	u32 num_mem  = profile->cores[core_id].summary.mem_info.size();
	u32 num_copy = profile->cores[core_id].summary.copy_info.size();
	u32 summary_size = sizeof(bm_profile_interval_t) +
		sizeof(u32) + num_send * sizeof(bm_profile_send_api_t) +
		sizeof(u32) + num_mark * sizeof(bm_profile_mark_t)     +
		sizeof(u32) + num_sync * sizeof(bm_profile_interval_t) +
		sizeof(u32) + num_mem  * sizeof(bm_profile_mem_info_t) +
		sizeof(u32) + num_copy * sizeof(bm_profile_memcpy_t);
	u32 block_type = BLOCK_BMLIB;

	fwrite(&block_type, sizeof(u32), 1, fp);
	fwrite(&summary_size, sizeof(u32), 1, fp);
	fwrite(&profile->cores[core_id].summary.interval, sizeof(bm_profile_interval_t), 1, fp);
	fwrite(&num_send, sizeof(u32), 1, fp);
	fwrite(profile->cores[core_id].summary.send_info.data(), sizeof(bm_profile_send_api_t), num_send, fp);
	fwrite(&num_sync, sizeof(u32), 1, fp);
	fwrite(profile->cores[core_id].summary.sync_info.data(), sizeof(bm_profile_interval_t), num_sync, fp);
	fwrite(&num_mark, sizeof(u32), 1, fp);
	fwrite(profile->cores[core_id].summary.mark_info.data(), sizeof(bm_profile_mark_t), num_mark, fp);
	fwrite(&num_copy, sizeof(u32), 1, fp);
	fwrite(profile->cores[core_id].summary.copy_info.data(), sizeof(bm_profile_memcpy_t), num_copy, fp);
	fwrite(&num_mem, sizeof(u32), 1, fp);
	fwrite(profile->cores[core_id].summary.mem_info.data(), sizeof(bm_profile_mem_info_t), num_mem, fp);

	block_type = BLOCK_BMLIB_EXTRA;
	fwrite(&block_type, sizeof(u32), 1, fp);
	u32 extra_size = profile->cores[core_id].summary.extra_info.size();
	fwrite(&extra_size, sizeof(u32), 1, fp);
	fwrite(profile->cores[core_id].summary.extra_info.data(), extra_size, 1, fp);
}

void bm_profile_deinit_record(bm_handle_t handle, const std::string& file_path, int core_id)
{
	FILE *fp = fopen(file_path.c_str(), "wb");
	ASSERT(fp);

	auto profile = handle->profile;
	bm_profile_save_summary_data(profile, fp, core_id);
	bool enable_mcu_perf = profile->cores[core_id].mcu_buffer.size>0;
	bool enable_tiu_perf = profile->cores[core_id].tiu_buffer.size>0;
	bool enable_gdma_perf = profile->cores[core_id].gdma_buffer.size>0;
	auto& arch_info = _global_arch_profile_map.at(profile->arch_code);
	profile->cores[core_id].profile_running = true;

	if (enable_gdma_perf && arch_info.gdma_deinit){
		arch_info.gdma_deinit(handle, profile, fp, core_id);
	}

	if (enable_tiu_perf && arch_info.tiu_deinit){
		arch_info.tiu_deinit(handle, profile, fp, core_id);
	}

	if (enable_mcu_perf && arch_info.mcu_deinit) {
		arch_info.mcu_deinit(handle, profile, fp, core_id);
	}

	arch_info.final_deinit(handle, profile, fp, core_id);

	fclose(fp);
	profile->cores[core_id].profile_running = false;
}

void bm_profile_destroy(bm_handle_t handle)
{
	auto profile = handle->profile;
	for(size_t core_id=0; core_id<profile->cores.size(); core_id++){
		free_buffer(handle, &profile->cores[core_id].mcu_buffer);
		free_buffer(handle, &profile->cores[core_id].tiu_buffer);
		free_buffer(handle, &profile->cores[core_id].gdma_buffer);
	}
	delete profile;
	handle->profile = NULL;
}

void bm_profile_write_global_info(bm_handle_t handle, bm_arch_profile_info_t arch_info, int core_id)
{
	auto profile = handle->profile;
	string global_path = profile->dir + "/global.profile";
	if (core_id>0) {
		global_path = profile->dir + "/global_"+std::to_string(core_id)+".profile";
	}
	FILE* global_fp = fopen(global_path.c_str(), "a");
	int freq_MHz = 0;
	bm_get_clk_tpu_freq(handle, &freq_MHz);
	fprintf(global_fp, "arch=%d\n", arch_info.arch_value);
	fprintf(global_fp, "tpu_freq=%d\n", freq_MHz);
	fclose(global_fp);
}

void bm_profile_dump_core(bm_handle_t handle, int core_id)
{
	if (!handle->profile) return;
	auto profile = handle->profile;
	if (profile->cores[core_id].is_dumped) return;
	profile->cores[core_id].summary.interval.end_usec = get_usec();

	auto arch_code = get_arch_code(handle);
	auto& arch_info = _global_arch_profile_map.at(arch_code);
	bm_profile_write_global_info(handle, arch_info, core_id);

	string file_path = profile->dir + "/bmlib_" + std::to_string(profile->id)+".profile";
	if (core_id > 0) {
		file_path = profile->dir + "/bmlib_" + std::to_string(profile->id)+"_"+std::to_string(core_id)+".profile";
	}
	bm_profile_deinit_record(handle, file_path, core_id);
	profile->cores[core_id].is_dumped = true;
}

void bm_profile_deinit(bm_handle_t handle)
{
	if(!handle->profile) return;
	size_t core_num = handle->profile->cores.size();
	for(size_t core_id=0; core_id<core_num; core_id++){
		bm_profile_dump_core(handle, core_id);
	}
	bm_profile_destroy(handle);
	show_profile_tips();
}

#undef ENV_PREFIX
#undef ENV_GDMA_SIZE
#undef ENV_ARM_SIZE
#undef ENV_BDC_SIZE
#undef ENV_SHOW_SIZE
#undef ENV_ENABLE_PROFILE

void bm_profile_save_profile_per_api(bm_handle_t handle, bmlib_profile_t* profile, int core_id)
{
	if(!profile->cores[core_id].save_per_api_mode) return;
	if(profile->cores[core_id].summary.send_info.empty()) return;
	profile->cores[core_id].summary.interval.end_usec = get_usec();
	std::string filename = profile->dir + "/bmlib_" + std::to_string(profile->id)+"_api_" + std::to_string(profile->cores[core_id].save_id) + ".profile";
	if (core_id > 0) {
		filename = profile->dir + "/bmlib_" + std::to_string(profile->id)+"_api_" + std::to_string(profile->cores[core_id].save_id) + "_"+ std::to_string(core_id) +".profile";
	}
	bm_profile_deinit_record(handle, filename, core_id);
	profile->cores[core_id].save_id++;
	bm_profile_init_record(handle);
}

void bm_profile_record_sync_begin(bm_handle_t handle, int core_id)
{
	if(!(handle && handle->profile)) return;
	if(handle->profile->cores[core_id].profile_running) return;
	auto& info = handle->profile->cores[core_id].summary.sync_info;
	info.resize(info.size()+1);
	info.back().begin_usec = get_usec();
}

void bm_profile_record_sync_end(bm_handle_t handle, int core_id)
{
	if (!(handle && handle->profile)) return;
	if(handle->profile->cores[core_id].profile_running) return;
	auto end_usec = get_usec();
	auto& info = handle->profile->cores[core_id].summary.sync_info;
	info.back().end_usec = end_usec;
	bm_profile_save_profile_per_api(handle, handle->profile, core_id);
}


typedef struct {
	tpu_kernel_function_t func_id;
	int param_size;
	char data[0];
} launch_func_param_t;

void bm_profile_record_send_api(bm_handle_t handle, u32 api_id, const void* param, int core_id)
{
	u64 begin_usec = get_usec();
	if(!(handle && handle->profile)) return;
	auto profile = handle->profile;
	if(profile->cores[core_id].profile_running) return;

	if(api_id == BM_API_ID_TPUSCALER_LAUNCH_FUNC){
		auto launch_param = (const launch_func_param_t*)param;
		if(launch_param->func_id == profile->cores[core_id].get_profile_func_id || launch_param->func_id == profile->cores[core_id].enable_profile_func_id){
		return;
		}
		auto& info = handle->profile->cores[core_id].summary.send_info;
		info.resize(info.size()+1);
		info.back().begin_usec = begin_usec;
		info.back().api_id = api_id;
		if(profile->cores[core_id].func_name_map.count(launch_param->func_id)) {
		bm_profile_add_extra(profile, SEND_EXTRA, profile->cores[core_id].func_name_map.at(launch_param->func_id), core_id);
		}
		return;
	}
	auto& info = handle->profile->cores[core_id].summary.send_info;
	info.resize(info.size()+1);
	info.back().begin_usec = begin_usec;
	info.back().api_id = api_id;
}

bm_profile_mark_t *bm_profile_mark_begin(bm_handle_t handle, u64 id, int core_id)
{
	if(!handle->profile) return NULL;
	auto& info = handle->profile->cores[core_id].summary.mark_info;
	info.resize(info.size()+1);
	bm_profile_mark_t* mark = &info.back();
	mark->id = id;
	mark->begin_usec = get_usec();
	mark->end_usec = 0;
	return mark;
}

void bm_profile_mark_end(bm_handle_t handle, bm_profile_mark_t *mark)
{
	if(!handle->profile || !mark) return;
	mark->end_usec = get_usec();
}

void bm_profile_record_memcpy_end(bm_handle_t handle, u64 src_addr, u64 dst_addr, u32 size, u32 dir)
{
	if(!(handle && handle->profile)) return;
	auto& copy_info = handle->profile->cores[0].summary.copy_info.back();
	copy_info.end_usec = get_usec();
	copy_info.src_addr = src_addr;
	copy_info.dst_addr = dst_addr;
	copy_info.size = size;
	copy_info.dir = dir;
}

void bm_profile_record_memcpy_begin(bm_handle_t handle)
{
	if(!(handle && handle->profile)) return;
	auto& info = handle->profile->cores[0].summary.copy_info;
	info.resize(info.size()+1);
	auto& copy_info = info.back();
	copy_info.begin_usec = get_usec();
}

void bm_profile_record_mem_end(bm_handle_t handle, bm_mem_op_type_t type, u64 device_addr, u32 size)
{
	if(!(handle && handle->profile)) return;
	auto& mem_info = handle->profile->cores[0].summary.mem_info.back();
	mem_info.end_usec = get_usec();
	mem_info.device_addr = device_addr;
	mem_info.type = type;
	mem_info.size = size;
}

void bm_profile_record_mem_begin(bm_handle_t handle) {
	if(!(handle && handle->profile)) return;
	auto& info = handle->profile->cores[0].summary.mem_info;
	info.resize(info.size()+1);
	auto& mem_info = info.back();
	mem_info.begin_usec = get_usec();
}

void bm_profile_add_extra(bmlib_profile_t* profile, bmlib_extra_type_t type, const std::string& str, int core_id)
{
	if(!profile) return;
		int index = 0;

	if(type == SEND_EXTRA){
		index = profile->cores[core_id].summary.send_info.size()-1;
	} else if(type == SYNC_EXTRA){
		index = profile->cores[core_id].summary.sync_info.size()-1;
	} else if(type == MEM_EXTRA) {
		index = profile->cores[core_id].summary.mem_info.size()-1;
	} else if (type == COPY_EXTRA) {
		index = profile->cores[core_id].summary.copy_info.size()-1;
	} else if (type == MARK_EXTRA) {
		index = profile->cores[core_id].summary.mark_info.size()-1;
	}

	auto& info = profile->cores[core_id].summary.extra_info;
	typedef struct {
	int type;
	int index;
	u32 size;
	} extra_info_header_t;
	extra_info_header_t header;
	header.type = type;
	header.index = index;
	header.size = str.size();
	auto old_size = info.size();
	info.resize(old_size + sizeof(header) + str.size());
	auto ptr = info.data() + old_size;
	memcpy(ptr, &header, sizeof(header));
	memcpy(ptr + sizeof(header), str.c_str(), str.size());
}

void bm_profile_func_map(bm_handle_t handle, tpu_kernel_function_t func, const std::string& name, int core_id)
{
	if (handle->profile) {
		handle->profile->cores[core_id].func_name_map[func] = name;
	}
}

void bm_profile_load_module(bm_handle_t handle, tpu_kernel_module_t p_module, int core_id)
{
	if (!handle->profile) return;
	auto profile = handle->profile;
	if (!profile->cores[core_id].current_module) {
		auto& arch_info = _global_arch_profile_map.at(profile->arch_code);
		profile->cores[core_id].current_module = p_module;
		profile->cores[core_id].set_engine_profile_param_func_id = tpu_kernel_get_function_from_core(handle, p_module, "sg_api_set_engine_profile_param", core_id);
		profile->cores[core_id].enable_profile_func_id = tpu_kernel_get_function_from_core(handle, p_module, "sg_api_set_profile", core_id);
		profile->cores[core_id].get_profile_func_id = tpu_kernel_get_function_from_core(handle, p_module, "sg_api_get_profile_data", core_id);
		ASSERT(handle->profile->cores[core_id].enable_profile_func_id);
		ASSERT(handle->profile->cores[core_id].get_profile_func_id);
		PROFILE_INFO("Profile load module: core_id=%d, %s: param_func_id=%d, enable_func_id=%d, data_func_id=%d\n", core_id,
					p_module->lib_name,
					profile->cores[core_id].set_engine_profile_param_func_id,
					profile->cores[core_id].enable_profile_func_id,
					profile->cores[core_id].get_profile_func_id);
		arch_info.tiu_init(handle, profile, core_id);
		arch_info.gdma_init(handle, profile, core_id);
		arch_info.mcu_init(handle, handle->profile, core_id);
		arch_info.final_init(handle, profile, core_id);
	} else {
		PROFILE_INFO("Only profile on the first module, the current module %s cannot be profiled\n", p_module->lib_name);
	}
}
void bm_profile_unload_module(bm_handle_t handle, tpu_kernel_module_t p_module, int core_id)
{
	if(!handle->profile) return;
	if(p_module != handle->profile->cores[core_id].current_module) return;
	PROFILE_INFO("Profile unload module: core_id=%d, %s\n", core_id, p_module->lib_name);
	bm_profile_dump_core(handle, core_id);
	handle->profile->cores[core_id].current_module = nullptr;
	handle->profile->cores[core_id].func_name_map.clear();
}
