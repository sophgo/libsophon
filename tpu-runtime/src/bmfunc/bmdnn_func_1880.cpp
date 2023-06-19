#include "bmfunc/bmfunc.h"
#include <iostream>

namespace bmruntime {

bm_status_t bmdnn_func_1880::_bmdnn_multi_fullnet_(
        bm_handle_t handle,
        int input_num,
        u64* user_input_global_offset,
        u64* cmd_input_global_offset,
        int* input_n,
        int* input_length,
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
        int cmdgroup_num
        )
{
    BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
    u32 api_buffer_size = sizeof(int) + (input_num * (sizeof(u64) * 2 + sizeof(int) * 2 + sizeof(unsigned short) + sizeof(unsigned char) * 2)) + //api buffer size for input
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
        *(int*)p_api = input_length[i];
        p_api = (int*)p_api + 1;
        *(unsigned short*)p_api = input_data_type[i];
        p_api = (unsigned short*)p_api + 1;
        *(unsigned char*)p_api = input_st_mode[i];
        p_api = (unsigned char*)p_api + 1;
        BMRT_ASSERT_INFO(input_st_mode[i] != 1,"input_st_mode[%d] shouldn't be 2N\n",i);
        *(unsigned char*)p_api = real_in_stmode[i];
        p_api = (unsigned char*)p_api + 1;
        BMRT_ASSERT_INFO(real_in_stmode[i] != 1,"real_in_stmode[%d] shouldn't be 2N\n",i);
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
        BMRT_ASSERT_INFO(output_st_mode[i] != 1,"output_st_mode[%d] shouldn't be 2N\n",i);
        *(unsigned char*)p_api = force_out_stmode[i];
        p_api = (unsigned char*)p_api + 1;
        BMRT_ASSERT_INFO(force_out_stmode[i] != 1,"force_out_stmode[%d] shouldn't be 2N\n",i);
        
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

    bm_status_t status = BM_SUCCESS;
    //status = (bm_status_t)bm_multi_fullnet(handle, api_buffer, api_buffer_size);
    status = (bm_status_t)bm_send_api(handle, (bm_api_id_t)0xfff, api_buffer, api_buffer_size);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_multi_fullnet run failed, status:%d", status);
    }

    delete [] api_buffer;
    return status;
}

/*
 * dynamic fullnet mode
 */
bm_status_t bmdnn_func_1880::_bmdnn_dynamic_fullnet_v2_(
        bm_handle_t handle,
        unsigned long long compiled_ir_global_addr,
        unsigned int compiled_ir_length,
        unsigned int input_num,
        const unsigned long long *input_addrs,
        const unsigned long long *input_middle_addrs,
        const int * const * input_shapes,
        const int * input_dims,
        unsigned int output_num,
        const unsigned long long *output_addrs,
        const unsigned long long *output_middle_addrs,
        unsigned long long apd_ctx_mem_offset,
        unsigned long long apd_coeff_mem_offset,
        bool need_middle_buff_flag,
        unsigned int* output_need_middle_buff_flag,
        bool get_output_shape,
        unsigned long long output_shape_global_addr,
        unsigned int using_arm_buffer_size //current use 0
    )
{
     BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
     u32 api_buffer_size = sizeof(u64) +sizeof(u32) +  //compiled_ir addr, length
                           //input info
                           (sizeof(u32) + sizeof(u32) + input_num *
                           (sizeof(u64) * (need_middle_buff_flag ? 2 : 1) + sizeof(int) + sizeof(int) * BM_MAX_DIMS_NUM)) +
                           //output info
                           (sizeof(u32) + output_num * (sizeof(u32) + 2 * sizeof(u64))) +
                           //get_output_shape, global_shape_mem_addr, apd_ctx_mem_offset, apd_coeff_mem_offset,
                           sizeof(u32) + sizeof(u64) + sizeof(u64) + sizeof(u64) +
                           //arm_reserved_addr, arm_reserved_size
                           sizeof(u64) + sizeof(u32);

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

       *(u32*)p_api = input_dims[i];
       p_api = (u32*)p_api + 1;
       for(int j=0;j<BM_MAX_DIMS_NUM; j++){
         *(u32 *)p_api = (j < input_dims[i]) ? input_shapes[i][j] : 1;
         p_api = (u32 *)p_api + 1;
       }
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
     *(u64*)p_api = apd_ctx_mem_offset;
     p_api = (u64*)p_api + 1;

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

     bm_status_t status = BM_SUCCESS;
     #if 0
     bm_status_t status =
         bm_send_api(handle, (bm_api_id_t)BM_API_ID_DYNAMIC_FULLNET, api_buffer, api_buffer_size);
     if (BM_SUCCESS != status) {
       BMRT_LOG(ERROR) << "bm_send_api failed, api id:" << BM_API_ID_DYNAMIC_FULLNET
                    << ", status:" << status;
     } else {
       status = bm_sync_api(handle);
       if (BM_SUCCESS != status) {
         BMRT_LOG(ERROR) << "bm_sync_api failed, api id:" << BM_API_ID_DYNAMIC_FULLNET
                      << ", status:" << status;
       }
     }

     bm_gmem_arm_reserved_release(handle);
     #endif

     delete[] api_buffer;
     return status;
}

}
