#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include <memory>
#include <vector>
#include <iostream>
#include <stdio.h>
#include "bmlib_runtime.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#define NO_USE 0

template<typename T>
class Blob {
  public:
    T *data      = NULL;
    int   blob_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data      = NULL;
            blob_size = 0;
            return;
        }
        data      = new T[size];
        blob_size = size;
    }
    ~Blob() {
        if (data) {
            delete[] data;
            data = NULL;
            blob_size = 0;
        }
    }
};

typedef std::shared_ptr<Blob<u8>> u8_data;
typedef std::shared_ptr<Blob<int>> int_data;
typedef std::shared_ptr<Blob<float>> float_data;
typedef std::shared_ptr<Blob<bm_image>> image_data;

#define MAKE_BLOB(T, size) std::make_shared<Blob<T>>(size)

static int store_mode_transfer(bm_image_data_format_ext data_format)
{
    switch(data_format)
    {
        case DATA_TYPE_EXT_FLOAT32:
        return STORAGE_MODE_1N_FP32;
        case DATA_TYPE_EXT_4N_BYTE:
        return STORAGE_MODE_4N_INT8;
        default:
        return STORAGE_MODE_1N_INT8;
    }
}

static float det(float arcs[3][3],int n){
    if(n == 1){
        return arcs[0][0];
    }
    float ans = 0;
    float temp[3][3];
    int i,j,k;
    for(i = 0;i < n;i++){
        for(j = 0;j < n - 1;j++){
            for(k = 0;k < n - 1;k++){
                temp[j][k] = arcs[j+1][(k >= i) ? k+1 : k];
            }
        }
        float t = det(temp, n-1);
        if(i%2 == 0){
            ans += arcs[0][i] * t;
        }
        else{
            ans -=  arcs[0][i] * t;
        }
    }
    return ans;
}

static void inverse_matrix(float matrix[3][3], float matrix_inv[2][3]){
    float det_value = det(matrix, 3);

    matrix_inv[0][0] = matrix[1][1] / det_value;
    matrix_inv[0][1] = -matrix[0][1] / det_value;
    matrix_inv[0][2] = (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1]) / det_value;
    matrix_inv[1][0] = - matrix[1][0] / det_value;
    matrix_inv[1][1] = matrix[0][0] / det_value;
    matrix_inv[1][2] = (matrix[0][2] * matrix[1][0] - matrix[0][0] * matrix[1][2]) / det_value;
}

bm_status_t bmcv_image_warp_affine_1684(
    bm_handle_t handle,
    int image_num,
    bmcv_affine_image_matrix matrix[4],
    bm_image *input,
    bm_image *output,
    int use_bilinear){
    bm_device_mem_t     tensor_P;
    bm_image_data_format_ext data_type = input[0].data_type;
    int matrix_sum = 0;
    int matrix_num[4] = {0};

    for (int i = 0; i < image_num; i++)
    {
        matrix_num[i] = matrix[i].matrix_num;
        matrix_sum += matrix_num[i];
    }
    int output_num = (data_type == DATA_TYPE_EXT_4N_BYTE) ? ALIGN(matrix_sum,4)/4 : matrix_sum;
    int input_num = (data_type == DATA_TYPE_EXT_4N_BYTE) ? ALIGN(image_num, 4)/4 : image_num;

    #ifdef __linux__
    bool output_alloc_flag[output_num];
    #else
    std::shared_ptr<bool> output_alloc_flag_(new bool[output_num], std::default_delete<bool[]>());
    bool*                 output_alloc_flag = output_alloc_flag_.get();
    #endif
    for(int i = 0; i < output_num; i++) {
        output_alloc_flag[i] = false;
        if(!bm_image_is_attached(output[i])) {
            if(BM_SUCCESS !=bm_image_alloc_dev_mem(output[i], BMCV_HEAP_ANY)) {
                BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");
                for (int free_idx = 0; free_idx < i; free_idx ++) {
                    if (output_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(output[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }

                return BM_ERR_FAILURE;
            }
            output_alloc_flag[i] = true;
        }
    }

    int msg_size = output_num * sizeof(u64) + matrix_sum * sizeof(bmcv_affine_matrix);
    char *msg_buf = new char[msg_size];
    u64 *out_dev = (u64 *)msg_buf;
    bmcv_affine_matrix *in_matrix = (bmcv_affine_matrix *)(out_dev + output_num);
    //output device memory.output
    for (int i = 0; i < output_num; i++) {
        bm_device_mem_t dev_mem;
        if(BM_SUCCESS !=bm_image_get_device_mem(output[i], &dev_mem)) {
            BMCV_ERR_LOG("bm_image_get_device_mem error\r\n");
            for (int free_idx = 0; free_idx < output_num; free_idx ++) {
                if (output_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }

            return BM_ERR_FAILURE;
        }
        out_dev[i] = bm_mem_get_device_addr(dev_mem);
    }
    //matrix
    for (int i = 0; i < image_num; i++) {
        memcpy(in_matrix, matrix[i].matrix, matrix[i].matrix_num * sizeof(bmcv_affine_matrix));
        in_matrix += matrix[i].matrix_num;
    }
    //copy the parameter to device memory.
    if(BM_SUCCESS !=bm_malloc_device_byte(handle, &tensor_P, msg_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte_heap error\r\n");
        for (int free_idx = 0; free_idx < output_num; free_idx ++) {
            if (output_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }
        delete[] msg_buf;

        return BM_ERR_FAILURE;
    }
    if(BM_SUCCESS !=bm_memcpy_s2d(handle, tensor_P, msg_buf)) {
        BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
        for (int free_idx = 0; free_idx < output_num; free_idx ++) {
            if (output_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }
        bm_free_device(handle, tensor_P);
        delete[] msg_buf;

        return BM_ERR_FAILURE;
    }
    delete [] msg_buf;

    u64 input_global_offset[4] = { 0 };
    for(int i = 0; i < input_num; i++) {
        bm_device_mem_t tensor_input;
        if(BM_SUCCESS !=bm_image_get_device_mem(input[i], &tensor_input)) {
            BMCV_ERR_LOG("bm_image_get_device_mem error\r\n");
            for (int free_idx = 0; free_idx < output_num; free_idx ++) {
                if (output_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
                bm_free_device(handle, tensor_P);

            return BM_ERR_FAILURE;
        }
        input_global_offset[i] = bm_mem_get_device_addr(tensor_input);
    }

    int image_c = 3;
    int image_sh = input[0].height;
    int image_sw = input[0].width;
    int image_dh = output[0].height;
    int image_dw = output[0].width;

    bm_api_cv_warp_t api;
    api.P_global_offset = bm_mem_get_device_addr(tensor_P);
    api.src_mode = store_mode_transfer(data_type);
    api.image_n = image_num;
    api.image_c = image_c;
    api.image_sh = image_sh;
    api.image_sw = image_sw;
    api.image_dh_real = image_dh;
    api.image_dw_real = image_dw;
    api.type = use_bilinear ? 2 : 0;   // 0: affine_nearest  1: perspective_nearest
                                    // 2: affine_bilinear 3: perspective_bilinear

    for (int i = 0; i < 4; i++) {
        api.S_global_offset[i] = input_global_offset[i];
        api.matrix_num[i] = matrix_num[i];
        if(i < input_num)
        {
            bm_image_get_stride(input[i], api.src_stride + i);
        }
    }
    // support output image with stride. limit: all outputs has the same stride
    bm_image_get_stride(output[0], &(api.image_dw_real));
    api.image_dw_real = (api.src_mode == STORAGE_MODE_4N_INT8) ?
                        api.image_dw_real / 4 : api.image_dw_real;

    int max_h = WARP_MAX_H;
    int max_w = WARP_MAX_W;
    int x_loop, y_loop;
    int total_capacity = L2_SRAM_USER_SIZE;

    while (max_h && max_w) {
        bool flag = true;
        y_loop = (image_dh + max_h - 1) / max_h;
        x_loop = (image_dw + max_w - 1) / max_w;
        int dh_max = y_loop == 1 ? image_dh : max_h;
        int dw_max = x_loop == 1 ? image_dw : max_w;
        int input_w = dw_max;
        int input_h = ceiling_func_shift(dh_max, NPU_SHIFT);
        int input_c = ceiling_func(dh_max, input_h);
        int idx_l2_size   = (input_c * input_h * input_w) * sizeof(int);
        int dst_l2_size   = (image_c * dh_max * dw_max);
        for (int i = 0; i < image_num; i++) {
            for (int j = 0; j < matrix[i].matrix_num; j++) {
                float m0 = matrix[i].matrix[j].m[0];
                float m1 = matrix[i].matrix[j].m[1];
                float m3 = matrix[i].matrix[j].m[3];
                float m4 = matrix[i].matrix[j].m[4];
                int crop_w = ABS(m0) * (dw_max - 1) + ABS(m1) * (dh_max - 1) + 1;
                int l2_capacity = total_capacity - idx_l2_size - dst_l2_size;
                l2_capacity /= image_c;
                long long step_h =
                    (l2_capacity / crop_w - ABS(m3) * dw_max - 2) / (ABS(m4) + 0.000001);
                if (step_h < 1) {
                    max_h -= (max_h > 16 ? 16 : 1);
                    max_w -= (max_w > 16 ? 16 : 1);
                    flag = false;
                    break;
                }
            }
            if (!flag) break;
        }
        if (flag) break;
        else continue;
    }

    if (max_h < 1 || max_w < 1) {
        printf("L2 SRAM is NOT enough to do this operation!\n");
        return BM_NOT_SUPPORTED;
    }

    for (int y_idx = 0; y_idx < y_loop; y_idx++) {
        int dh = (y_idx == y_loop - 1) ? (image_dh - y_idx * max_h)
                                    : max_h;
        for (int x_idx = 0; x_idx < x_loop; x_idx++) {
            int dw = (x_idx == x_loop - 1) ? (image_dw - x_idx * max_w)
                                        : max_w;
            api.blockIdx_x = x_idx * max_w;
            api.blockIdx_y = y_idx * max_h;
            api.image_dh   = dh;
            api.image_dw   = dw;
            if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_WARP, (uint8_t *)&api, sizeof(api))) {
                BMCV_ERR_LOG("warp affine send api error\r\n");
                bm_free_device(handle, tensor_P);
                return BM_ERR_FAILURE;
            }
            if (BM_SUCCESS != bm_sync_api(handle)) {
                BMCV_ERR_LOG("warp affine sync api error\r\n");
                bm_free_device(handle, tensor_P);
                return BM_ERR_FAILURE;
            }
        }
    }

    bm_free_device(handle, tensor_P);
    return BM_SUCCESS;
}

static bm_status_t per_image_deal(bm_handle_t handle,
                        int image_dh,
                        int image_dw,
                        bm_image input,
                        bm_image output,
                        bm_device_mem_t tensor_output,
                        bmcv_affine_image_matrix matrix){
    int image_c = 3;
    bm_device_mem_t tensor_input;
    bm_device_mem_t tensor_temp;
    bm_device_mem_t tensor_S;
    bm_device_mem_t tensor_out_align;
    bm_status_t ret = BM_SUCCESS;
    bm_image_get_device_mem(input, &tensor_input);
    bm_image_get_device_mem(output, &tensor_output);
    sg_api_cv_warp_t param;
    int image_sh = input.height;
    int image_sw = input.width;
    param.output_image_addr = bm_mem_get_device_addr(tensor_output);
    param.input_image_addr = bm_mem_get_device_addr(tensor_input);
    ret = bm_malloc_device_byte(handle, &tensor_temp, input.height * input.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return ret;
    }
    ret = bm_malloc_device_byte(handle, &tensor_out_align, output.height * output.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, tensor_temp);
        return ret;
    }
    param.input_image_addr_align = bm_mem_get_device_addr(tensor_temp);
    param.out_image_addr_align   = bm_mem_get_device_addr(tensor_out_align);
    param.image_n  = NO_USE;
    param.image_sh = image_sh;
    param.image_sw = image_sw;
    param.image_dh = image_dh;
    param.image_dw = image_dw;
    param.type = 0;   // 0: affine_nearest

    for (int i = 0;i < 6;i++){
        param.m.m[i] = matrix.matrix->m[i];
    }

    bm_image_get_stride(input, &(param.src_w_stride));
    ret = bm_malloc_device_byte(handle, &tensor_S, image_dh * image_dw * image_c * 4);
    if(BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, tensor_temp);
        bm_free_device(handle, tensor_out_align);
        return ret;
    }
    param.index_image_addr = bm_mem_get_device_addr(tensor_S);
    bm_image_get_stride(output, &(param.dst_w_stride));
    ret = bm_tpu_kernel_launch(handle, "sg_cv_warp_affine_1684x", &param, sizeof(param));
    if(BM_SUCCESS != ret){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "sg_cv_warp_affine_1684x error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, tensor_temp);
        bm_free_device(handle, tensor_out_align);
        return ret;
    } 

    bm_free_device(handle, tensor_S);
    bm_free_device(handle, tensor_temp);
    bm_free_device(handle, tensor_out_align);
    return BM_SUCCESS;
}

static bm_status_t per_image_deal(bm_handle_t handle,
                        int image_dh,
                        int image_dw,
                        bm_image input,
                        bm_image output,
                        bm_device_mem_t tensor_output,
                        bmcv_affine_image_matrix matrix,
                        bm_image_data_format_ext input_format,
                        bm_image_data_format_ext output_format){
    int image_c = 3;
    bm_device_mem_t tensor_input;
    bm_device_mem_t tensor_temp_r;
    bm_device_mem_t tensor_temp_g;
    bm_device_mem_t tensor_temp_b;
    bm_device_mem_t tensor_S;
    bm_device_mem_t tensor_sys_lu;
    bm_device_mem_t tensor_sys_ld;
    bm_device_mem_t tensor_sys_ru;
    bm_device_mem_t tensor_sys_rd;
    bm_device_mem_t tensor_out_align_a;
    bm_device_mem_t tensor_out_align_b;
    bm_status_t ret = BM_SUCCESS;
    bm_image_get_device_mem(input, &tensor_input);
    bm_image_get_device_mem(output, &tensor_output);
    sg_api_cv_warp_bilinear_t param;
    int image_sh = input.height;
    int image_sw = input.width;
    param.output_image_addr = bm_mem_get_device_addr(tensor_output);
    param.input_image_addr = bm_mem_get_device_addr(tensor_input);

    std::vector<bm_device_mem_t*> internal_mem_v;
    ret = bm_malloc_device_byte(handle, &tensor_temp_r, input.height * input.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_temp_r);

    ret = bm_malloc_device_byte(handle, &tensor_temp_g, input.height * input.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_temp_g);

    ret = bm_malloc_device_byte(handle, &tensor_temp_b, input.height * input.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_temp_b);

    ret = bm_malloc_device_byte(handle, &tensor_out_align_a, output.height * output.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_out_align_a);

    ret = bm_malloc_device_byte(handle, &tensor_out_align_b, output.height * output.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_out_align_b);

    param.input_image_addr_align_r = bm_mem_get_device_addr(tensor_temp_r);
    param.input_image_addr_align_g = bm_mem_get_device_addr(tensor_temp_g);
    param.input_image_addr_align_b = bm_mem_get_device_addr(tensor_temp_b);
    param.out_image_addr_align_a   = bm_mem_get_device_addr(tensor_out_align_a);
    param.out_image_addr_align_b   = bm_mem_get_device_addr(tensor_out_align_b);
    param.image_sh = image_sh;
    param.image_sw = image_sw;
    param.image_dh = image_dh;
    param.image_dw = image_dw;
    param.image_c = (input.image_format == FORMAT_BGR_PLANAR || input.image_format == FORMAT_RGB_PLANAR) ? 3 : 1;
    param.input_format  = (bm_image_data_format_ext)input_format;
    param.output_format = (bm_image_data_format_ext)output_format;
    param.type = 0;   // 0: affine_nearest

    for (int i = 0;i < 6;i++){
        param.m.m[i] = matrix.matrix->m[i];
    }

    ret = bm_malloc_device_byte(handle, &tensor_S, image_dh * image_dw * image_c * 4);
    if(BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_S);

    ret = bm_malloc_device_byte(handle, &tensor_sys_lu, image_dh * image_dw * image_c * 4);
    if(BM_SUCCESS != ret) {
       bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_sys_lu);

    ret = bm_malloc_device_byte(handle, &tensor_sys_ld, image_dh * image_dw * image_c * 4);
    if(BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_sys_ld);

    ret = bm_malloc_device_byte(handle, &tensor_sys_ru, image_dh * image_dw * image_c * 4);
    if(BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_sys_ru);

    ret = bm_malloc_device_byte(handle, &tensor_sys_rd, image_dh * image_dw * image_c * 4);
    if(BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }
    internal_mem_v.push_back(&tensor_sys_rd);

    param.index_image_addr = bm_mem_get_device_addr(tensor_S);
    param.system_image_addr_lu = bm_mem_get_device_addr(tensor_sys_lu);
    param.system_image_addr_ld = bm_mem_get_device_addr(tensor_sys_ld);
    param.system_image_addr_ru = bm_mem_get_device_addr(tensor_sys_ru);
    param.system_image_addr_rd = bm_mem_get_device_addr(tensor_sys_rd);

    ret = bm_tpu_kernel_launch(handle, "cv_api_warp_affine_bilinear_1684x", &param, sizeof(param));
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "cv_api_warp_affine_bilinear_1684x error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        goto free_devmem;
    }

free_devmem:
  for (auto mem : internal_mem_v) {
    bm_free_device(handle, *mem);
  }

  return ret;
}

 bm_status_t bmcv_image_warp_affine_1684X(
    bm_handle_t handle,
    int image_num,
    bmcv_affine_image_matrix matrix[4],
    int image_dh,
    int image_dw,
    bm_image* input,
    bm_image* output,
    int use_bilinear){
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t tensor_output[4];
    #ifdef __linux__
    bool output_alloc_flag[image_num];
    #else
    std::shared_ptr<bool> output_alloc_flag_(new bool[image_num], std::default_delete<bool[]>());
    bool*                 output_alloc_flag = output_alloc_flag_.get();
    #endif
    for (int num = 0;num < image_num;num++){
        output_alloc_flag[num] = false;
        if(!bm_image_is_attached(output[num])) {
            if(BM_SUCCESS !=bm_image_alloc_dev_mem(output[num], BMCV_HEAP_ANY)) {
                std::cout << "bm_image_alloc_dev_mem error\r\n" << std::endl;
                for (int free_idx = 0; free_idx < num; free_idx ++) {
                    if (output_alloc_flag[free_idx]) {
                        bm_image_detach(output[num]);
                    }
                }
                return BM_ERR_FAILURE;
            }
            output_alloc_flag[num] = true;
        }
    }

    for (int num = 0;num < image_num;num++){
        if(use_bilinear){
            bm_image_data_format_ext input_format = input[0].data_type;
            bm_image_data_format_ext output_format = output[0].data_type;
            ret = per_image_deal(handle, image_dh, image_dw, input[num], output[num],
                    tensor_output[num], matrix[num], input_format, output_format);
        }else{
            ret = per_image_deal(handle, image_dh, image_dw, input[num], output[num],
                    tensor_output[num], matrix[num]);
        }
        if (BM_SUCCESS != ret){
            for (int free_idx = 0; free_idx < num; free_idx ++) {
                if (output_alloc_flag[free_idx]) {
                    bm_image_detach(output[free_idx]);
                }
            }
            return ret;
        }
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_warp_check(
    bm_handle_t              handle,
    int                      image_num,
    bmcv_affine_image_matrix      matrix[4],
    bm_image*                input,
    bm_image*                output)
{
    if (handle == NULL) {
        bmlib_log("WARP", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_image_format_ext src_format = input[0].image_format;
    bm_image_data_format_ext src_type = input[0].data_type;
    bm_image_format_ext dst_format = output[0].image_format;
    bm_image_data_format_ext dst_type = output[0].data_type;
    int image_sh = input[0].height;
    int image_sw = input[0].width;
    int image_dh = output[0].height;
    int image_dw = output[0].width;
    int dw_stride = 0;
    bm_image_get_stride(output[0], &dw_stride);

    if ((input[0].data_type == DATA_TYPE_EXT_FP16) ||
        (output[0].data_type == DATA_TYPE_EXT_FP16)||
        (input[0].data_type == DATA_TYPE_EXT_BF16) ||
        (output[0].data_type == DATA_TYPE_EXT_BF16)){
        BMCV_ERR_LOG("data type not support\n");

        return BM_NOT_SUPPORTED;
    }

    if (!input || !output) {
        return BM_ERR_PARAM;
    }
    if (image_num < 0 || image_num > 4) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "expect 1 <= image_num <= 4 %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (src_format != dst_format || src_type != dst_type) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "expect  the same input / output image format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (src_format != FORMAT_RGB_PLANAR && src_format != FORMAT_BGR_PLANAR) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "Not supported input image format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    /*
    if (image_dw > WARP_MAX_W || image_dh > WARP_MAX_H) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "warp output image size too big %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }*/
    ASSERT(bm_image_is_attached(input[0]));
    if (src_type == DATA_TYPE_EXT_1N_BYTE) {
        for (int i = 1; i < image_num; i++) {
            if (src_format != input[i].image_format || src_type != input[i].data_type) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "expected consistant input image format %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            if (image_sh != input[i].height || image_sw != input[i].width) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "expected consistant input image size %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            ASSERT(bm_image_is_attached(input[i]));
        }
    }
    int out_num = 0;
    for (int i = 0; i < image_num; i++) {
        out_num += matrix[i].matrix_num;
    }
    if (out_num <= 0) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "illegal out_num %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    out_num = (src_type == DATA_TYPE_EXT_4N_BYTE) ? ALIGN(out_num, 4)/4 : out_num;
    if (dst_type == DATA_TYPE_EXT_1N_BYTE) {
        for (int i = 1; i < out_num; i++) {
            if (src_format != output[i].image_format || src_type != output[i].data_type) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "expect consistant output image format %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            if (image_dh != output[i].height || image_dw != output[i].width) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "expect  consistant output image size %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            int stride = 0;
            bm_image_get_stride(output[i], &stride);
            if (dw_stride != stride) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "expect  consistant output image stride %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
    }

    return BM_SUCCESS;
}

inline int warp_get_idx(int input, int matrix_sigma[4], int image_num)
{
    for (int i = 0; i < image_num; i++) {
        if (input < matrix_sigma[i]) {
            return i;
        }
    }
    return 0;
}

bm_status_t bmcv_image_warp_affine_similar_to_opencv(
    bm_handle_t handle,
    int image_num,
    bmcv_affine_image_matrix matrix[4],
    bm_image *input,
    bm_image *output,
    int use_bilinear)
{
    UNUSED(use_bilinear);
    float matrix_tem[3][3];
    float matrix_tem_inv[2][3];
    for (int i = 0; i < image_num; i++) {
        for(int matrix_no = 0; matrix_no < matrix[i].matrix_num; matrix_no++){
            memset(matrix_tem, 0, sizeof(matrix_tem));
            memset(matrix_tem_inv, 0, sizeof(matrix_tem_inv));
            for(int a = 0;a < 6;a++){
                matrix_tem[(a/3)][(a%3)] = matrix[i].matrix->m[a];
            }
            matrix_tem[2][0] = 0;
            matrix_tem[2][1] = 0;
            matrix_tem[2][2] = 1;
            inverse_matrix(matrix_tem, matrix_tem_inv);
            for(int a = 0;a < 6;a++){
                float temp = matrix_tem_inv[(a/3)][(a%3)];
                matrix[i].matrix->m[a] = temp;
            }
        }
    }

    return bmcv_image_warp_affine(handle,
            image_num,
            &matrix[0],
            input, output,use_bilinear);
}

bm_status_t bmcv_image_warp_affine(
    bm_handle_t handle,
    int image_num,
    bmcv_affine_image_matrix matrix[4],
    bm_image *input,
    bm_image *output,
    int use_bilinear)
{
    if(BM_SUCCESS !=bmcv_warp_check(handle, image_num, matrix, input, output)) {
        BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
        return BM_ERR_FAILURE;
    }

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    if (chipid == BM1684X){
        return bmcv_image_warp_affine_1684X(handle,
                image_num,
                &matrix[0],
                output->height,
                output->width,
                input,
                output,
                use_bilinear);
    }
    else if (chipid == 0x1684){
        return bmcv_image_warp_affine_1684(handle,
                                image_num,
                                &matrix[0],
                                input,
                                output,
                                use_bilinear);
    }else{
        printf(" NOT SUPPORT! \n");
        return BM_ERR_FAILURE;
    }
}

bm_status_t bmcv_image_warp(bm_handle_t            handle,
                            int                    image_num,
                            bmcv_affine_image_matrix matrix[4],
                            bm_image *             input,
                            bm_image *             output) {
    return bmcv_image_warp_affine(handle, image_num, matrix, input, output);
}
