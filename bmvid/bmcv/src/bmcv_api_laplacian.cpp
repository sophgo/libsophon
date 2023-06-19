#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include <memory>
#include <vector>
#include <cstring>

#define KERNEL_LENGTH 9
static bm_status_t check(
        bm_handle_t handle,
        bm_image input,
        bm_image output) {
    if (handle == NULL) {
        bmlib_log("Crop", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_image_format_ext fmt = input.image_format;
    bm_image_data_format_ext type = input.data_type;
    if (fmt != FORMAT_GRAY ) {
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "image format NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (type != DATA_TYPE_EXT_1N_BYTE && type != DATA_TYPE_EXT_1N_BYTE_SIGNED) {
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "data type NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (!bm_image_is_attached(input)) {
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "input not malloc device memory!\r\n");
        return BM_ERR_PARAM;
    }

    if (!bm_image_is_attached(output)) {
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "output not malloc device memory!\r\n");
        return BM_ERR_PARAM;
    }

    if (output.image_format != fmt || output.data_type != type) {
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "output and input NOT consistant!\r\n");
        return BM_ERR_PARAM;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_laplacian_check(int ksize){
    if (ksize !=1 && ksize != 3){
        return BM_ERR_PARAM;
    }

    return BM_SUCCESS;

}

bm_status_t  bmcv_image_laplacian_bm1684(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        unsigned int ksize) {
    bm_status_t ret = check(handle, input, output);
    if (BM_SUCCESS != ret){
        return ret;
    }

    ret= bmcv_laplacian_check(ksize);
    if (ret != BM_SUCCESS) return ret;

    float kernel[KERNEL_LENGTH]={2, 0, 2, 0, -8, 0, 2, 0, 2}; //default ksize = 3
    //float *kernel = new float[KERNEL_LENGTH];
    if (ksize == 1){
        float kernel_ksize_1[KERNEL_LENGTH] = {0, 1, 0, 1, -4, 1, 0, 1, 0};
        memcpy(kernel, kernel_ksize_1, sizeof(float) * KERNEL_LENGTH);
    }

    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, KERNEL_LENGTH*sizeof(float));
    if (BM_SUCCESS != ret) return ret;

    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret){
        bm_free_device(handle, kernel_mem);
        return ret;
    }

    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            bm_free_device(handle, kernel_mem);
            return ret;
        }
        output_alloc_flag = true;
    }

    int kw = ksize;
    int kh = ksize;

    int plane_num = bm_image_get_plane_num(input);
    bm_device_mem_t in_dev_mem[3], out_dev_mem[3];
    unsigned long long src_addr[3];
    bm_image_get_device_mem(input, in_dev_mem);
    for (int p = 0; p < plane_num; p++) {
        src_addr[p] = bm_mem_get_device_addr(in_dev_mem[p]);
    }
    int stride_i, stride_o;
    bm_image_get_stride(input, &stride_i);
    // construct api info
    bm_api_laplacian_t api;

    api.channel = 1;
    api.is_packed = 0;
    api.delta = 0;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.input_addr[0] = src_addr[0];
    api.kw=kw;
    api.kh=kh;

    bm_image_get_stride(output, &stride_o);
    bm_image_get_device_mem(output, out_dev_mem);
    api.height[0] = input.height;
    api.width[0] =input.width;
    api.stride_i[0] = stride_i;
    api.stride_o[0] = stride_o;
    api.output_addr[0] = bm_mem_get_device_addr(out_dev_mem[0]);

    ret = bm_send_api(handle,  BM_API_ID_CV_LAPLACIAN, (uint8_t*)&api, sizeof(api));
    if (ret != BM_SUCCESS){
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "laplacian send api error\n");
        bm_free_device(handle, kernel_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, out_dev_mem[0]);
        }
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("LAPLACIAN", BMLIB_LOG_ERROR, "laplacian sync api error\n");
        bm_free_device(handle, kernel_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, out_dev_mem[0]);
        }
        return ret;
    }

    bm_free_device(handle, kernel_mem);
    //delete [] kernel;
    return BM_SUCCESS;
}

bm_status_t bmcv_image_laplacian_bm1684X(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        unsigned int ksize)
{
    bm_status_t ret = check(handle, input, output);
    if (BM_SUCCESS != ret){
        return ret;
    }

    ret= bmcv_laplacian_check(ksize);
    if (ret != BM_SUCCESS) return ret;

    int8_t kernel[KERNEL_LENGTH] = { 2, 0, 2, 0, -8, 0, 2, 0, 2};
    if (ksize == 1){
        int8_t kernel_tmp[KERNEL_LENGTH] = {0, 1, 0, 1, -4, 1, 0, 1, 0};
        memcpy(kernel, kernel_tmp, sizeof(int8_t) * KERNEL_LENGTH);
    }
    //tpu_api_cv_laplacian_t param = {};
    bm_api_laplacian_t param;
    memset(&param, 0, sizeof(bm_api_laplacian_t));
    bm_device_mem_t input_mem, kernel_mem, output_mem;

    if (input.image_format != FORMAT_GRAY || output.image_format != FORMAT_GRAY)
        return BM_ERR_DATA;

    ret = bm_malloc_device_byte(handle, &kernel_mem, KERNEL_LENGTH * sizeof(int));
    if (BM_SUCCESS != ret) return ret;
    if (input.image_format == FORMAT_GRAY) {
        param.channel = 1;
    } else {
        printf("Image format not support, only support FORMAT_GRAY\n");
        return BM_NOT_SUPPORTED;
    }

    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret){
        bm_free_device(handle, kernel_mem);
        return ret;
    }
    bm_image_get_device_mem(input, &input_mem);
    bm_image_get_device_mem(output, &output_mem);

    param.input_addr[0] = bm_mem_get_device_addr(input_mem);
    param.output_addr[0] = bm_mem_get_device_addr(output_mem);
    param.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    param.width[0] = input.width;
    param.height[0] = input.height;
    param.kw = ksize;
    param.kh = ksize;
#if 0 // for diff input/output type
    param.input_dtype = input.data_type;
    param.output_dtype = output.data_type;
#endif
    //ret = tpu_kernel_launch_sync(handle, "cv_laplacian", &param, sizeof(param));
    ret = bm_tpu_kernel_launch(handle, "cv_laplacian", &param, sizeof(param));
    bm_free_device(handle, kernel_mem);
    if (ret != BM_SUCCESS)
        printf("tpu_kernel_launch failed");

    return ret;
}

bm_status_t  bmcv_image_laplacian(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        unsigned int ksize) {

    unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_laplacian_bm1684(handle, input, output, ksize);
        break;

      case BM1684X:
        ret = bmcv_image_laplacian_bm1684X(handle, input, output, ksize);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}
