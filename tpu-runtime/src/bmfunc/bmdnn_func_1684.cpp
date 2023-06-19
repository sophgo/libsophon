#include "bmfunc/bmfunc.h"
#include <iostream>

namespace bmruntime {

bm_status_t bmdnn_func_1684::_bmdnn_multi_fullnet_(
        bm_handle_t handle,
        int input_num,
        u64* user_input_global_offset,
        u64* cmd_input_global_offset,
        int* input_n,
        int* input_c,
        int* input_h,
        int* input_w,
        unsigned short* input_data_type, //0: FP32, 1: FP16, 2: INT8, 3: UINT8, 4: INT16, 5: UINT16
        unsigned char* input_st_mode,    //0: 1N, 1: 2N, 2: 4N
        unsigned char* real_in_stmode,   //0: 1N, 1: 2N, 2: 4N
        int output_num,
        u64* user_output_global_offset,
        u64* cmd_output_global_offset,
        int* output_n,
        int* output_length,
        unsigned short* output_data_type, //0: FP32, 1: FP16, 2: INT8, 3: UINT8, 4: INT16, 5: UINT16
        unsigned char* output_st_mode,    //0: 1N, 1: 2N, 2: 4N
        unsigned char* force_out_stmode,   //0: 1N, 1: 2N, 2: 4N
        u64 bdc_cmd_offset,
        u64 gdma_cmd_offset,
        int* bdc_cmd_num,
        int* gdma_cmd_num,
        int cmdgroup_num,
        u32* input_pad_h
        )
{
    BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
    u32 api_buffer_size = sizeof(int) + (input_num * (sizeof(u64) * 2 + sizeof(int) * 4 + sizeof(unsigned short) + sizeof(unsigned char) * 2 + sizeof(int))) + //api buffer size for input
                          sizeof(int) + (output_num * (sizeof(u64) * 2 + sizeof(int) * 2 + sizeof(unsigned short) + sizeof(unsigned char) * 2)) + //api buffer size for output
                          sizeof(u64) * 2 + sizeof(int) * 2 * cmdgroup_num + sizeof(int);

    u8* api_buffer = new u8 [api_buffer_size];

    void* p_api = api_buffer;
    //input global offset process
    *(int*)p_api = input_num;
    p_api = (int*)p_api + 1;
    for (int i = 0; i < input_num; ++i) {
        *(u64*)p_api = user_input_global_offset[i];
        p_api = (u64*)p_api + 1;
        *(u64*)p_api = cmd_input_global_offset[i];
        p_api = (u64*)p_api + 1;
        *(int*)p_api = input_n[i];
        p_api = (int*)p_api + 1;
        *(int*)p_api = input_c[i];
        p_api = (int*)p_api + 1;
        *(int*)p_api = input_h[i];
        p_api = (int*)p_api + 1;
        *(int*)p_api = input_w[i];
        p_api = (int*)p_api + 1;
        *(unsigned short*)p_api = input_data_type[i];
        p_api = (unsigned short*)p_api + 1;
        *(unsigned char*)p_api = input_st_mode[i];
        p_api = (unsigned char*)p_api + 1;
        *(unsigned char*)p_api = real_in_stmode[i];
        p_api = (unsigned char*)p_api + 1;
        *(u32*)p_api = input_pad_h[i];
        p_api = (u32*)p_api + 1;
    }

    //output global offset process
    *(int*)p_api = output_num;
    p_api = (int*)p_api + 1;
    for (int i = 0; i < output_num; ++i) {
        *(u64*)p_api = user_output_global_offset[i];
        p_api = (u64*)p_api + 1;
        *(u64*)p_api = cmd_output_global_offset[i];
        p_api = (u64*)p_api + 1;
        *(int*)p_api = output_n[i];
        p_api = (int*)p_api + 1;
        *(int*)p_api = output_length[i];
        p_api = (int*)p_api + 1;
        *(unsigned short*)p_api = output_data_type[i];
        p_api = (unsigned short*)p_api + 1;
        *(unsigned char*)p_api = output_st_mode[i];
        p_api = (unsigned char*)p_api + 1;
        *(unsigned char*)p_api = force_out_stmode[i];
        p_api = (unsigned char*)p_api + 1;
    }

    //memcpy cmd offset and num
    *(u64*)p_api = bdc_cmd_offset;
    p_api = (u64*)p_api + 1;
    *(u64*)p_api = gdma_cmd_offset;
    p_api = (u64*)p_api + 1;
    *(int*)p_api = cmdgroup_num;
    for (int i = 0; i < cmdgroup_num; i++) {
        p_api = (int*)p_api + 1;
        *(int*)p_api = bdc_cmd_num[i];
        p_api = (int*)p_api + 1;
        *(int*)p_api = gdma_cmd_num[i];
    }

    bm_status_t status = bm_send_api(handle, (bm_api_id_t)BM_API_ID_MULTI_FULLNET, api_buffer, api_buffer_size);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d", BM_API_ID_MULTI_FULLNET, status);
    }
    delete [] api_buffer;
    return status;
}

/*
 * dynamic fullnet mode
 */
bm_status_t bmdnn_func_1684::_bmdnn_dynamic_fullnet_v2_(
        bm_handle_t handle,
        unsigned long long compiled_ir_global_addr,
        unsigned int compiled_ir_length, //unit dword
        unsigned int input_num,
        const unsigned long long *input_addrs,
        const unsigned long long *input_middle_addrs,
        const int * const * input_shapes,
        const int * input_elem_nums,
        const int * input_dtype_and_dims,
        unsigned int output_num,
        const unsigned long long *output_addrs,
        const unsigned long long *output_middle_addrs,
        unsigned long long apd_ctx_start,
        std::vector<unsigned long long> apd_ctx_mem_borders,
        std::vector<unsigned long long> apd_ctx_mem_offset,
        unsigned long long apd_coeff_mem_offset,
        bool need_middle_buff_flag,
        unsigned int* output_need_middle_buff_flag,
        bool get_output_shape,
        unsigned long long output_shape_global_addr,
        unsigned int using_arm_buffer_size //current use 0
    )
{
    BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
    BMRT_ASSERT_INFO(
        apd_ctx_mem_borders.size() == apd_ctx_mem_offset.size(),
        "ctx borders and offset should have same size");

     size_t ctx_num = apd_ctx_mem_borders.size();
     u32 api_buffer_size = sizeof(u64) +sizeof(u32) +  //compiled_ir addr, length
                           //input info
                           (sizeof(u32) + sizeof(u32) + input_num *
                           (sizeof(u64) * (need_middle_buff_flag ? 2 : 1) + sizeof(int) + sizeof(int) * BM_MAX_DIMS_NUM + sizeof(int))) +
                           //output info
                           (sizeof(u32) + output_num * (sizeof(u32) + 2 * sizeof(u64))) +
                           //get_output_shape, global_shape_mem_addr, apd_ctx_start, (ctx_num, apd_ctx_mem_borders, apd_ctx_mem_offset),
                           sizeof(u32) + sizeof(u64) + sizeof(u64) + ( sizeof(u32)+sizeof(u64)*ctx_num*2 ) +
                           //apd_coeff_mem_offset, arm_reserved_addr, arm_reserved_size
                           sizeof(u64) + sizeof(u64) + sizeof(u32);

     if (api_buffer_size > MAX_API_MSG_SIZE) {
       //decrease the api buffer size
       for (u32 i = 0; i < input_num; ++i) {
         u32 cur_dim = (u32)(input_dtype_and_dims[i] & 0xFFFF);
         api_buffer_size -= (BM_MAX_DIMS_NUM - cur_dim) * sizeof(int);
       }
     }

     u8* api_buffer = new u8 [api_buffer_size];

     void* p_api = api_buffer;
     //compiled ir information
     *(u64*)p_api = compiled_ir_global_addr;
     p_api = (u64*)p_api + 1;
     *(u32*)p_api = compiled_ir_length;
     p_api = (u32*)p_api + 1;

     //input information
     *(u32*)p_api = input_num;
     p_api = (u32*)p_api + 1;

     *(u32*)p_api = (u32)need_middle_buff_flag;
     p_api = (u32*)p_api + 1;

     for(u32 i = 0; i < input_num; ++i){
       *(u64*)p_api = input_addrs[i];
       p_api = (u64*)p_api + 1;

       if(need_middle_buff_flag) {
         *(u64*)p_api = input_middle_addrs[i];
         p_api = (u64*)p_api + 1;
       }

       *(u32*)p_api = input_dtype_and_dims[i];
       p_api = (u32*)p_api + 1;
       u32 cur_dim = (u32)(input_dtype_and_dims[i] & 0xFFFF);
       for(u32 j = 0; j < cur_dim; j++){
         *(u32 *)p_api = (u32)input_shapes[i][j];
         p_api = (u32 *)p_api + 1;
       }
       *(u32*)p_api = input_elem_nums[i];
       p_api = (u32*)p_api + 1;
     }
     //output information
     *(u32*)p_api = output_num;
     p_api = (u32*)p_api + 1;

     for(u32 i = 0; i < output_num; ++i) {
       *(u32*)p_api = (u32)output_need_middle_buff_flag[i];
       p_api = (u32*)p_api + 1;
     }

     for(u32 i = 0; i < output_num; ++i){
       *(u64*)p_api = output_addrs[i];
       p_api = (u64*)p_api + 1;

       *(u64*)p_api = output_middle_addrs[i];
       p_api = (u64*)p_api + 1;
     }
     //output shape info related
     *(u32*)p_api = (u32)get_output_shape;
     p_api = (u32*)p_api + 1;
     *(u64*)p_api = output_shape_global_addr;
     p_api = (u64*)p_api + 1;

     //The memory address in cmd gdma need to be offset when append context,here is the offset value.
     *(u64*)p_api = apd_ctx_start;
     p_api = (u64*)p_api + 1;

     *(u32*)p_api = ctx_num;
     p_api = (u32*)p_api + 1;

     for (size_t i = 0; i < ctx_num; ++i)
     {
       *(u64*)p_api = apd_ctx_mem_borders[i];
       p_api = (u64*)p_api + 1;
     }
     for (size_t i = 0; i < ctx_num; ++i)
     {
       *(u64*)p_api = apd_ctx_mem_offset[i];
       p_api = (u64*)p_api + 1;
     }

     *(u64*)p_api = apd_coeff_mem_offset;
     p_api = (u64*)p_api + 1;

     u64 arm_reserved_addr = -1;
     u32 arm_reserved_size = 0;

     arm_reserved_addr = bm_gmem_arm_reserved_request(handle);
     arm_reserved_size = 0; //64M
     BMRT_ASSERT_INFO(using_arm_buffer_size<=arm_reserved_size,"using_arm_buffer_size:%d is larger than arm_reserved_size:%d",\
using_arm_buffer_size,arm_reserved_size);
     *(u64*)p_api = arm_reserved_addr;
     p_api = (u64*)p_api + 1;
     *(u32*)p_api = arm_reserved_size;
     p_api = (u64*)p_api + 1;

     bm_status_t status =
         bm_send_api(handle, (bm_api_id_t)BM_API_ID_DYNAMIC_FULLNET, api_buffer, api_buffer_size);
     if (BM_SUCCESS != status) {
       BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d", BM_API_ID_DYNAMIC_FULLNET, status);
     } else {
       status = bm_sync_api(handle);
       if (BM_SUCCESS != status) {
         BMRT_LOG(WRONG, "bm_sync_api failed, api id:%d, status:%d", BM_API_ID_DYNAMIC_FULLNET, status); 
       }
     }

     bm_gmem_arm_reserved_release(handle);

     delete[] api_buffer;
     return status;
}

bm_status_t  bmdnn_func_1684::_bmdnn_set_profile_enable_(bm_handle_t handle, bool enable){
     BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
     u32 api_buffer_size = sizeof(u32);
     u32 profile_enable = enable;
     bm_status_t status = bm_send_api(handle, (bm_api_id_t)BM_API_ID_SET_PROFILE_ENABLE, (u8*)&profile_enable, api_buffer_size);
     if (BM_SUCCESS != status) {
       BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d", BM_API_ID_SET_PROFILE_ENABLE, status);
     }
     return status;
}
bm_status_t bmdnn_func_1684::_bmdnn_get_profile_data_(
        bm_handle_t handle,
        unsigned long long output_global_addr,
        unsigned int output_max_size,
        unsigned int byte_offset,
        unsigned int data_category //0: profile time records, 1: extra data
        ){
      BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
#pragma pack(1)
     struct {
      u64 arm_reserved_addr;
      u64 output_global_addr;
      u32 output_size;
      u32 byte_offset;
      u32 data_category; //0: profile_data, 1: profile extra data
     } api_data;
#pragma pack()

     const u32 api_buffer_size = sizeof(api_data);

     api_data.arm_reserved_addr = bm_gmem_arm_reserved_request(handle);
     api_data.output_global_addr = output_global_addr;
     api_data.output_size = output_max_size;
     api_data.byte_offset = byte_offset;
     api_data.data_category = data_category;

     bm_api_id_t api_code = (bm_api_id_t)BM_API_ID_GET_PROFILE_DATA;
     bm_status_t status =
         bm_send_api(handle, api_code, (u8*)&api_data, api_buffer_size);
     if (BM_SUCCESS != status) {
       BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d", api_code, status);
     } else {
       status = bm_sync_api(handle);
       if (BM_SUCCESS != status) {
         BMRT_LOG(WRONG, "bm_sync_api failed, api id:%d, status:%d", api_code, status);
       }
     }
     bm_gmem_arm_reserved_release(handle);
     return status;
}

}
