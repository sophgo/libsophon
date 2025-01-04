#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmlib_runtime.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include <memory>
#include <vector>
#include <iostream>
#include <stdio.h>

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

// Expanded by first row|A|
float getA(float arcs[3][3],int n)
{
    if(n == 1)
        return arcs[0][0];
    float ans = 0;
    float temp[3][3];
    int i, j, k;
    for(i = 0; i < n; i++){
        for(j = 0; j < n-1; j++)
            for(k = 0; k < n-1; k++)
                temp[j][k] = arcs[j+1][(k>=i)?k+1:k];
        float t = getA(temp,n-1);
        if(i % 2 == 0)
            ans += arcs[0][i]*t;
        else
            ans -= arcs[0][i]*t;
    }
    return ans;
}

//Calculate the remainder equation corresponding to each element in each column of each row to form A*
void getAStart(float arcs[3][3],int n,float ans[3][3])
{
    if(n == 1){
        ans[0][0] = 1;
        return;
    }
    int i, j, k, t;
    float temp[3][3];
    for(i = 0; i < n; i++)
        for(j = 0; j < n; j++){
            for(k = 0; k < n-1; k++)
                for(t = 0; t< n-1; t++)
                    temp[k][t] = arcs[k>=i?k+1:k][t>=j?t+1:t];
            ans[j][i] = getA(temp,n-1);
            if((i + j) % 2 == 1){
                ans[j][i] =- ans[j][i];
            }
        }
}

void inverse_matrix(int n, float arcs[3][3], float astar[3][3])
{
    int i, j;
    float a = getA(arcs, n);
    if(a == 0)
        printf("can not transform!\n");
    else{
        getAStart(arcs, n, astar);
        for(i = 0; i < n; i++)
            for(j = 0; j < n; j++)
                astar[i][j] = astar[i][j]/a;
    }
}

static bm_status_t bmcv_perspective_check(
    bm_handle_t                    handle,
    int                            image_num,
    bmcv_perspective_image_matrix  matrix[4],
    bm_image*                      input,
    bm_image*                      output)
{
    if (handle == NULL) {
        bmlib_log("PERSPECTIVE", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
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
    float m[9];
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
    for (int i = 0; i < image_num; i++) {
        for (int j = 0; j < matrix[i].matrix_num; j++) {
            for(int a=0;a<9;a++)
                m[a]=matrix[i].matrix[j].m[a];
            int left_top_x = (int)((m[0] * 0 + m[1] * 0 + m[2]) / (m[6] * 0 + m[7] * 0 + m[8]));
            int left_top_y = (int)((m[3] * 0 + m[4] * 0 + m[5]) / (m[6] * 0 + m[7] * 0 + m[8]));
            int left_btm_x = (int)((m[0] * 0 + m[1] * (image_dh - 1) + m[2]) / (m[6] * 0 + m[7] * (image_dh - 1) + m[8]));
            int left_btm_y = (int)((m[3] * 0 + m[4] * (image_dh - 1) + m[5]) / (m[6] * 0 + m[7] * (image_dh - 1) + m[8]));
            int right_top_x = (int)((m[0] * (image_dw - 1) + m[1] * 0 + m[2]) / (m[6] * (image_dw - 1) + m[7] * 0 + m[8]));
            int right_top_y = (int)((m[3] * (image_dw - 1) + m[4] * 0 + m[5]) / (m[6] * (image_dw - 1) + m[7] * 0 + m[8]));
            int right_btm_x = (int)((m[0] * (image_dw - 1) + m[1] * (image_dh - 1) + m[2]) / (m[6] * (image_dw - 1) + m[7] * (image_dh - 1) + m[8]));
            int right_btm_y = (int)((m[3] * (image_dw - 1) + m[4] * (image_dh - 1) + m[5]) / (m[6] * (image_dw - 1) + m[7] * (image_dh - 1) + m[8]));
            if (left_top_x < 0 || left_top_x > image_sw ||
                left_top_y < 0 || left_top_y > image_sh ||
                left_btm_x < 0 || left_btm_x > image_sw ||
                left_btm_y < 0 || left_btm_y > image_sh ||
                right_top_x < 0 || right_top_x > image_sw ||
                right_top_y < 0 || right_top_y > image_sh ||
                right_btm_x < 0 || right_btm_x > image_sw ||
                right_btm_y < 0 || right_btm_y > image_sh) {

                bmlib_log("BMCV", BMLIB_LOG_ERROR, "output image is out of input image range %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
    }

    return BM_SUCCESS;
}

static void get_perspective_transform(int* sx, int* sy, int dw, int dh, float* matrix) {
    int A = sx[3] + sx[0] - sx[1] - sx[2];
    int B = sy[3] + sy[0] - sy[1] - sy[2];
    int C = sx[2] - sx[3];
    int D = sy[2] - sy[3];
    int E = sx[1] - sx[3];
    int F = sy[1] - sy[3];
    matrix[8] = 1;
    matrix[7] = ((float)(A * F - B * E) / (float)dh) / (float)(C * F - D * E);
    matrix[6] = ((float)(A * D - B * C) / (float)dw) / (float)(D * E - C * F);
    matrix[0] = (matrix[6] * dw * sx[1] + sx[1] - sx[0]) / dw;
    matrix[1] = (matrix[7] * dh * sx[2] + sx[2] - sx[0]) / dh;
    matrix[2] = sx[0];
    matrix[3] = (matrix[6] * dw * sy[1] + sy[1] - sy[0]) / dw;
    matrix[4] = (matrix[7] * dh * sy[2] + sy[2] - sy[0]) / dh;
    matrix[5] = sy[0];
}

bm_status_t bmcv_image_warp_perspective_1684(
    bm_handle_t                   handle,
    int                           image_num,
    bmcv_perspective_image_matrix matrix[4],
    bm_image *                    input,
    bm_image *                    output,
    int                           use_bilinear)
{
    bm_device_mem_t     tensor_P;

    if(BM_SUCCESS != bmcv_perspective_check(handle, image_num, matrix, input, output)) {
        return BM_ERR_FAILURE;
    }

    bm_image_data_format_ext data_type = input[0].data_type;
    int matrix_sum = 0;
    int matrix_num[4] = {0};

    for (int i = 0; i < image_num; i++) {
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

    int msg_size = output_num * sizeof(u64) + matrix_sum * sizeof(bmcv_perspective_matrix);
    char *msg_buf = new char[msg_size];
    u64 *out_dev = (u64 *)msg_buf;
    bmcv_perspective_matrix *in_matrix = (bmcv_perspective_matrix *)(out_dev + output_num);
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
        memcpy(in_matrix, matrix[i].matrix, matrix[i].matrix_num * sizeof(bmcv_perspective_matrix));
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
    api.type = use_bilinear ? 3 : 1;  // 0: affine_nearest  1: perspective_nearest
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

    int total_capacity = L2_SRAM_USER_SIZE;
    int max_h = WARP_MAX_H;
    int max_w = WARP_MAX_W;
    int y_loop = (image_dh + max_h - 1) / max_h;
    int x_loop = (image_dw + max_w - 1) / max_w;

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
                float m2 = matrix[i].matrix[j].m[2];
                float m3 = matrix[i].matrix[j].m[3];
                float m4 = matrix[i].matrix[j].m[4];
                float m5 = matrix[i].matrix[j].m[5];
                float m6 = matrix[i].matrix[j].m[6];
                float m7 = matrix[i].matrix[j].m[7];
                float m8 = matrix[i].matrix[j].m[8];
                int x1, x2, x3, x4;
                int max_x, min_x, max_h_per_line;
                int y11, y12, y21, y22;
                int crop_w;
                int l2_capacity = (total_capacity - idx_l2_size - dst_l2_size) / image_c;
                // left top block
                x1 = (int)((m0 * 0 + m1 * 0 + m2) /
                           (m6 * 0 + m7 * 0 + m8));
                x2 = (int)((m0 * 0 + m1 * (dh_max - 1) + m2) /
                           (m6 * 0 + m7 * (dh_max - 1) + m8));
                x3 = (int)((m0 * (dw_max - 1) + m1 * 0 + m2) /
                           (m6 * (dw_max - 1) + m7 * 0 + m8));
                x4 = (int)((m0 * (dw_max - 1) + m1 * (dh_max - 1) + m2) /
                           (m6 * (dw_max - 1) + m7 * (dh_max - 1) + m8));
                max_x = bm_max(x1, x2);
                max_x = bm_max(max_x, x3);
                max_x = bm_max(max_x, x4);
                min_x = bm_min(x1, x2);
                min_x = bm_min(min_x, x3);
                min_x = bm_min(min_x, x4);

                crop_w = max_x - min_x + 1;

                y11 = (int)((m3 * 0 + m4 * 0 + m5) /
                            (m6 * 0 + m7 * 0 + m8));
                y12 = (int)((m3 * (dw_max - 1) + m4 * 0 + m5) /
                            (m6 * (dw_max - 1) + m7 * 0 + m8));
                y21 = (int)((m3 * 0 + m4 * (dh_max - 1) + m5) /
                            (m6 * 0 + m7 * (dh_max - 1) + m8));
                y22 = (int)((m3 * (dw_max - 1) + m4 * (dh_max - 1) + m5) /
                            (m6 * (dw_max - 1) + m7 * (dh_max - 1) + m8));
                max_h_per_line = bm_max(ABS(y22 - y21), ABS(y12 - y11));

                // TODO: 1.2 is estimated, in order to make mem enough
                if (l2_capacity < crop_w * (max_h_per_line + 1) * 1.2) {
                    max_h -= (max_h > 16 ? 16 : 1);
                    max_w -= (max_w > 16 ? 16 : 1);
                    flag = false;
                    break;
                }
                // right top block
                x1 = (int)((m0 * (image_dw - dw_max) + m1 * 0 + m2) /
                           (m6 * (image_dw - dw_max) + m7 * 0 + m8));
                x2 = (int)((m0 * (image_dw - dw_max) + m1 * (dh_max - 1) + m2) /
                           (m6 * (image_dw - dw_max) + m7 * (dh_max - 1) + m8));
                x3 = (int)((m0 * (image_dw - 1) + m1 * 0 + m2) /
                           (m6 * (image_dw - 1) + m7 * 0 + m8));
                x4 = (int)((m0 * (image_dw - 1) + m1 * (dh_max - 1) + m2) /
                           (m6 * (image_dw - 1) + m7 * (dh_max - 1) + m8));
                max_x = bm_max(x1, x2);
                max_x = bm_max(max_x, x3);
                max_x = bm_max(max_x, x4);
                min_x = bm_min(x1, x2);
                min_x = bm_min(min_x, x3);
                min_x = bm_min(min_x, x4);

                crop_w = max_x - min_x + 1;

                y11 = (int)((m3 * (image_dw - dw_max) + m4 * 0 + m5) /
                            (m6 * (image_dw - dw_max) + m7 * 0 + m8));
                y12 = (int)((m3 * (image_dw - 1) + m4 * 0 + m5) /
                            (m6 * (image_dw - 1) + m7 * 0 + m8));
                y21 = (int)((m3 * (image_dw - dw_max) + m4 * (dh_max - 1) + m5) /
                            (m6 * (image_dw - dw_max) + m7 * (dh_max - 1) + m8));
                y22 = (int)((m3 * (image_dw - 1) + m4 * (dh_max - 1) + m5) /
                            (m6 * (image_dw - 1) + m7 * (dh_max - 1) + m8));
                max_h_per_line = bm_max(ABS(y22 - y21), ABS(y12 - y11));

                if (l2_capacity < crop_w * (max_h_per_line + 1) * 1.2) {
                    max_h -= (max_h > 16 ? 16 : 1);
                    max_w -= (max_w > 16 ? 16 : 1);
                    flag = false;
                    break;
                }
                // left bottom block
                x1 = (int)((m0 * 0 + m1 * (image_dh - dh_max) + m2) /
                           (m6 * 0 + m7 * (image_dh - dh_max) + m8));
                x2 = (int)((m0 * 0 + m1 * (image_dh - 1) + m2) /
                           (m6 * 0 + m7 * (image_dh - 1) + m8));
                x3 = (int)((m0 * (dw_max - 1) + m1 * (image_dh - dh_max) + m2) /
                           (m6 * (dw_max - 1) + m7 * (image_dh - dh_max) + m8));
                x4 = (int)((m0 * (dw_max - 1) + m1 * (image_dh - 1) + m2) /
                           (m6 * (dw_max - 1) + m7 * (image_dh - 1) + m8));
                max_x = bm_max(x1, x2);
                max_x = bm_max(max_x, x3);
                max_x = bm_max(max_x, x4);
                min_x = bm_min(x1, x2);
                min_x = bm_min(min_x, x3);
                min_x = bm_min(min_x, x4);

                crop_w = max_x - min_x + 1;

                y11 = (int)((m3 * 0 + m4 * (image_dh - dh_max) + m5) /
                            (m6 * 0 + m7 * (image_dh - dh_max) + m8));
                y12 = (int)((m3 * (dw_max - 1) + m4 * (image_dh - dh_max) + m5) /
                            (m6 * (dw_max - 1) + m7 * (image_dh - dh_max) + m8));
                y21 = (int)((m3 * 0 + m4 * (image_dh - 1) + m5) /
                            (m6 * 0 + m7 * (image_dh - 1) + m8));
                y22 = (int)((m3 * (dw_max - 1) + m4 * (image_dh - 1) + m5) /
                            (m6 * (dw_max - 1) + m7 * (image_dh - 1) + m8));
                max_h_per_line = bm_max(ABS(y22 - y21), ABS(y12 - y11));

                if (l2_capacity < crop_w * (max_h_per_line + 1) * 1.2) {
                    max_h -= (max_h > 16 ? 16 : 1);
                    max_w -= (max_w > 16 ? 16 : 1);
                    flag = false;
                    break;
                }
                // right bottom block
                x1 = (int)((m0 * (image_dw - dw_max) + m1 * (image_dh - dh_max) + m2) /
                           (m6 * (image_dw - dw_max) + m7 * (image_dh - dh_max) + m8));
                x2 = (int)((m0 * (image_dw - dw_max) + m1 * (image_dh - 1) + m2) /
                           (m6 * (image_dw - dw_max) + m7 * (image_dh - 1) + m8));
                x3 = (int)((m0 * (image_dw - 1) + m1 * (image_dh - dh_max) + m2) /
                           (m6 * (image_dw - 1) + m7 * (image_dh - dh_max) + m8));
                x4 = (int)((m0 * (image_dw - 1) + m1 * (image_dh - 1) + m2) /
                           (m6 * (image_dw - 1) + m7 * (image_dh - 1) + m8));
                max_x = bm_max(x1, x2);
                max_x = bm_max(max_x, x3);
                max_x = bm_max(max_x, x4);
                min_x = bm_min(x1, x2);
                min_x = bm_min(min_x, x3);
                min_x = bm_min(min_x, x4);

                crop_w = max_x - min_x + 1;

                y11 = (int)((m3 * (image_dw - dw_max) + m4 * (image_dh - dh_max) + m5) /
                            (m6 * (image_dw - dw_max) + m7 * (image_dh - dh_max) + m8));
                y12 = (int)((m3 * (image_dw - 1) + m4 * (image_dh - dh_max) + m5) /
                            (m6 * (image_dw - 1) + m7 * (image_dh - dh_max) + m8));
                y21 = (int)((m3 * (image_dw - dw_max) + m4 * (image_dh - 1) + m5) /
                            (m6 * (image_dw - dw_max) + m7 * (image_dh - 1) + m8));
                y22 = (int)((m3 * (image_dw - 1) + m4 * (image_dh - 1) + m5) /
                            (m6 * (image_dw - 1) + m7 * (image_dh - 1) + m8));
                max_h_per_line = bm_max(ABS(y22 - y21), ABS(y12 - y11));

                if (l2_capacity < crop_w * (max_h_per_line + 1) * 1.2) {
                    max_h -= (max_h > 16 ? 16 : 1);
                    max_w -= (max_w > 16 ? 16 : 1);
                    flag = false;
                    break;
                }
            }
            if (!flag) break;
        }
        if (!flag) continue;
        else break;
    }
    // printf("max_h = %d   max_w = %d \n", max_h , max_w);
    if (max_h < 1 || max_w < 1) {
        BMCV_ERR_LOG("L2 SRAM is NOT enough to do this operation!\n");
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
                BMCV_ERR_LOG("warp perspective send api error\r\n");
                bm_free_device(handle, tensor_P);
                return BM_ERR_FAILURE;
            }
            if (BM_SUCCESS != bm_sync_api(handle)) {
                BMCV_ERR_LOG("warp perspective sync api error\r\n");
                bm_free_device(handle, tensor_P);
                return BM_ERR_FAILURE;
            }
        }
    }

    bm_free_device(handle, tensor_P);
    return BM_SUCCESS;
}

static bm_status_t per_image_deal_nearest(bm_handle_t handle,
                        int image_dh,
                        int image_dw,
                        bm_image input,
                        bm_image output,
                        bm_device_mem_t tensor_output,
                        bmcv_perspective_image_matrix matrix){
    int image_c = 3;
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t tensor_S;
    bm_device_mem_t tensor_temp;
    bm_device_mem_t tensor_out_align;
    bm_device_mem_t tensor_input;
    bm_image_get_device_mem(input, &tensor_input);
    bm_image_get_device_mem(output, &tensor_output);
    // ret = bm_malloc_device_byte(handle, &tensor_S, image_dh * image_dw * image_c * 4);
    int index_size_temp = image_dw > image_dh ? ALIGN(image_dw, 64) : ALIGN(image_dh, 64);
    ret = bm_malloc_device_byte(handle, &tensor_S, index_size_temp * index_size_temp * image_c * 4);
    if(BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return ret;
    }

    sg_api_cv_warp_perspective_1684x_t param;
    param.image_n  = NO_USE;
    param.image_sh = input.height;
    param.image_sw = input.width;
    param.image_dh = image_dh;
    param.image_dw = image_dw;
    param.type     = 1;   // 1: perspective_nearest

    param.input_image_addr = bm_mem_get_device_addr(tensor_input);
    ret = bm_malloc_device_byte(handle, &tensor_temp, input.height * input.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, tensor_S);
        return ret;
    }
    ret = bm_malloc_device_byte(handle, &tensor_out_align, output.height * output.width * 2);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "bm_malloc error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, tensor_S);
        bm_free_device(handle, tensor_temp);
        return ret;
    }
    param.output_image_addr = bm_mem_get_device_addr(tensor_output);
    param.index_image_addr = bm_mem_get_device_addr(tensor_S);
    param.input_image_addr_align = bm_mem_get_device_addr(tensor_temp);
    param.out_image_addr_align = bm_mem_get_device_addr(tensor_out_align);
    bm_image_get_stride(input, &(param.src_w_stride));
    bm_image_get_stride(output, &(param.dst_w_stride));

    for (int i = 0;i < 9;i++){
        param.m.m[i] = matrix.matrix->m[i];
    }
    ret = bm_tpu_kernel_launch(handle, "sg_cv_warp_perspective_1684x", &param, sizeof(param));
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "sg_cv_warp_perspective_1684x error, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, tensor_S);
        bm_free_device(handle, tensor_temp);
        return ret;
    }
    bm_free_device(handle, tensor_S);
    bm_free_device(handle, tensor_temp);
    bm_free_device(handle, tensor_out_align);
    return BM_SUCCESS;
}

static bm_status_t per_image_deal_bilinear(bm_handle_t handle,
                        int image_dh,
                        int image_dw,
                        bm_image input,
                        bm_image output,
                        bm_device_mem_t tensor_output,
                        bmcv_perspective_image_matrix matrix){
    int image_c = 3;
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t tensor_S;
    bm_device_mem_t tensor_temp;
    bm_device_mem_t tensor_out_align;
    bm_device_mem_t tensor_input;
    bm_image_get_device_mem(input, &tensor_input);
    bm_image_get_device_mem(output, &tensor_output);
    // ret = bm_malloc_device_byte(handle, &tensor_S, image_dh * image_dw * image_c * 4);
    int index_size_temp = image_dw > image_dh ? ALIGN(image_dw, 64) : ALIGN(image_dh, 64);
    ret = bm_malloc_device_byte(handle, &tensor_S, index_size_temp * index_size_temp * image_c * 4);
    if (BM_SUCCESS != ret) return ret;

    sg_api_cv_warp_perspective_1684x_t param;
    param.image_n  = image_c;
    param.image_sh = input.height;
    param.image_sw = input.width;
    param.image_dh = image_dh;
    param.image_dw = image_dw;
    param.type     = 3;   // 0: affine_nearest  1: perspective_nearest
                          // 2: affine_bilinear 3: perspective_bilinear
    param.input_image_addr = bm_mem_get_device_addr(tensor_input);
    ret = bm_malloc_device_byte(handle, &tensor_temp, input.height * input.width * 2);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, tensor_S);
        return ret;
    }
    ret = bm_malloc_device_byte(handle, &tensor_out_align, output.height * output.width * 2);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, tensor_S);
        bm_free_device(handle, tensor_temp);
        return ret;
    }
    param.output_image_addr = bm_mem_get_device_addr(tensor_output);
    param.index_image_addr = bm_mem_get_device_addr(tensor_S);
    param.input_image_addr_align = bm_mem_get_device_addr(tensor_temp);
    param.out_image_addr_align = bm_mem_get_device_addr(tensor_out_align);
    bm_image_get_stride(input, &(param.src_w_stride));
    bm_image_get_stride(output, &(param.dst_w_stride));

    for (int i = 0;i < 9;i++){
        param.m.m[i] = matrix.matrix->m[i];
    }
    ret = bm_tpu_kernel_launch(handle, "cv_api_warp_perspective_bilinear_1684x", &param, sizeof(param));
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, tensor_S);
        bm_free_device(handle, tensor_temp);
	bm_free_device(handle, tensor_out_align);
        return ret;
    }
    bm_free_device(handle, tensor_S);
    bm_free_device(handle, tensor_temp);
    bm_free_device(handle, tensor_out_align);
    return BM_SUCCESS;
}
bm_status_t bmcv_image_warp_perspective_1684X(
    bm_handle_t handle,
    int image_num,
    bmcv_perspective_image_matrix matrix[4],
    int image_dh,
    int image_dw,
    bm_image* input,
    bm_image* output,
    int use_bilinear)
{
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t tensor_output[4];
    #ifdef __linux__
        bool output_alloc_flag[image_num];
    #else
        std::shared_ptr<bool> output_alloc_flag_(new bool[image_num], std::default_delete<bool[]>());
        bool*                 output_alloc_flag = output_alloc_flag_.get();
    #endif
    for (int num = 0;num < image_num;num++) {
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

    for (int num = 0;num < image_num;num++) {
        int stride = 0;
        bm_image_get_stride(input[num], &stride);
        input[num].width = stride;

        if (!use_bilinear) {
        ret = per_image_deal_nearest(handle, image_dh, image_dw, input[num], output[num],
                    tensor_output[num], matrix[num]);
        } else {
        ret = per_image_deal_bilinear(handle, image_dh, image_dw, input[num], output[num],
                    tensor_output[num], matrix[num]);
        }
        if (BM_SUCCESS != ret) {
            for (int free_idx = 0; free_idx < num; free_idx ++) {
                if (output_alloc_flag[free_idx])
                    bm_image_detach(output[free_idx]);
            }
            return ret;
        }
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_warp_perspective_with_coordinate(
    bm_handle_t                       handle,
    int                               image_num,
    bmcv_perspective_image_coordinate coord[4],
    bm_image *                        input,
    bm_image *                        output,
    int                               use_bilinear)
{
    bmcv_perspective_image_matrix matrix[4];
    int dh = output[0].height;
    int dw = output[0].width;
    int coord_sum = 0;
    bm_handle_check_2(handle, input[0], output[0]);
    for (int i = 0; i < image_num; i++) {
        coord_sum += coord[i].coordinate_num;
    }
    #ifdef __linux__
    bmcv_perspective_matrix mat[coord_sum];
    #else
    std::shared_ptr<bmcv_perspective_matrix> mat_(new bmcv_perspective_matrix[coord_sum], std::default_delete<bmcv_perspective_matrix[]>());
    bmcv_perspective_matrix* mat = mat_.get();
    #endif
    int idx = 0;
    for (int i = 0; i < image_num; i++) {
        int index = idx;
        matrix[i].matrix_num = coord[i].coordinate_num;
        for (int j = 0; j < matrix[i].matrix_num; j++) {
            get_perspective_transform(coord[i].coordinate[j].x,
                                      coord[i].coordinate[j].y,
                                      dw - 1,
                                      dh - 1,
                                      mat[idx].m);
            idx++;
        }
        matrix[i].matrix = mat + index;
    }

    return bmcv_image_warp_perspective(handle,
                                       image_num,
                                       matrix,
                                       input,
                                       output,
                                       use_bilinear);
}

bm_status_t bmcv_image_warp_perspective_similar_to_opencv(
    bm_handle_t                       handle,
    int                               image_num,
    bmcv_perspective_image_matrix     matrix[4],
    bm_image *                        input,
    bm_image *                        output,
    int                               use_bilinear)
{
    float matrix_tem[3][3];
    float matrix_tem_inv[3][3];

    bm_handle_check_2(handle, input[0], output[0]);
    for (int i = 0; i < image_num; i++) {
        for(int matrix_no = 0; matrix_no < matrix[i].matrix_num; matrix_no++){
            memset(matrix_tem, 0, sizeof(matrix_tem));
            memset(matrix_tem_inv, 0, sizeof(matrix_tem_inv));
            for(int a=0;a<9;a++){
                    matrix_tem[(a/3)][(a%3)]=matrix[i].matrix->m[a];
            }
            inverse_matrix(3, matrix_tem, matrix_tem_inv);
            for(int a=0;a<9;a++){
                    matrix[i].matrix->m[a]=matrix_tem_inv[(a/3)][(a%3)];
            }
        }
    }

    return bmcv_image_warp_perspective(handle,
                                       image_num,
                                       matrix,
                                       input,
                                       output,
                                       use_bilinear);
}

bm_status_t bmcv_image_warp_perspective(
    bm_handle_t                   handle,
    int                           image_num,
    bmcv_perspective_image_matrix matrix[4],
    bm_image *                    input,
    bm_image *                    output,
    int                           use_bilinear)
{
    unsigned int chipid;
    bm_handle_check_2(handle, input[0], output[0]);
    bm_get_chipid(handle, &chipid);
    if (chipid == BM1684X){
        return bmcv_image_warp_perspective_1684X(
                handle,
                image_num,
                matrix,
                output->height,
                output->width,
                input,
                output,
                use_bilinear);
    }
    else if (chipid == 0x1684){
    return bmcv_image_warp_perspective_1684(
            handle,
            image_num,
            matrix,
            input,
            output,
            use_bilinear);
    }else{
        printf(" NOT SUPPORT! \n");
        return BM_ERR_FAILURE;
    }
}
