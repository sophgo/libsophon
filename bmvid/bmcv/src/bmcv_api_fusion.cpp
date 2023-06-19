#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "device_mem_allocator.h"
#include "pcie_cpu/bmcv_api_struct.h"
#include <memory>
#include <vector>
#include <cmath>
#include <thread>

using namespace std;

static bm_status_t bmcv_fusion_check(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output,
        bm_thresh_type_t type) {
    if (handle == NULL) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (type >= BM_THRESH_TYPE_MAX) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "Threshold type is error!\r\n");
        return BM_ERR_PARAM;
    }
    if (input1.height != output.height ||
        input1.width != output.width ||
        input2.height != output.height ||
        input2.width != output.width) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_NOT_SUPPORTED;
    }
    if (input1.image_format != FORMAT_GRAY ||
        input2.image_format != FORMAT_GRAY) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "Not supported input image format, only GRAY supported!\n");
        return BM_NOT_SUPPORTED;
    }
    if (input1.image_format != output.image_format ||
        input2.image_format != output.image_format) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "input and output image format should be same\n");
        return BM_NOT_SUPPORTED;
    }
    if (input1.data_type != DATA_TYPE_EXT_1N_BYTE ||
        input2.data_type != DATA_TYPE_EXT_1N_BYTE ||
        output.data_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}


bm_status_t bmcv_image_fusion_bm1684(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output,
        unsigned char thresh,
        unsigned char max_value,
        bm_thresh_type_t type,
        int kw,
        int kh,
        bm_device_mem_t kmem) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_fusion_check(handle, input1, input2, output, type);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
    }
    // initial the parameters from input and output
    // int channel = bm_image_get_plane_num(input1);
    int input1_str, input2_str, output_str;
    bm_image_get_stride(input1, &input1_str);
    bm_image_get_stride(input2, &input2_str);
    bm_image_get_stride(output, &output_str);
    bm_device_mem_t input1_mem;
    bm_image_get_device_mem(input1, &input1_mem);
    bm_device_mem_t input2_mem;
    bm_image_get_device_mem(input2, &input2_mem);
    bm_device_mem_t output_mem;
    bm_image_get_device_mem(output, &output_mem);

    // if kernel shape is rectangle
    unsigned char* kernel = new unsigned char [kh * kw];
    ret = bm_memcpy_d2s(handle, kernel, kmem);
    if (BM_SUCCESS != ret) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "kernel d2s failed!\r\n");
        delete [] kernel;
        return BM_ERR_FAILURE;
    }
    bm_api_cv_fusion_t api;
    api.input1_addr = bm_mem_get_device_addr(input1_mem);
    api.input2_addr = bm_mem_get_device_addr(input2_mem);
    api.output_addr = bm_mem_get_device_addr(output_mem);
    api.height = input1.height;
    api.width = input1.width;
    api.input1_str = input1_str;
    api.input2_str = input2_str;
    api.output_str = output_str;
    api.type = type;
    api.thresh = thresh,
    api.max_value = max_value;
    api.kh = kh;
    api.kw = kw;
    api.format = input1.image_format;
    ret = bm_send_api(handle,  BM_API_ID_CV_FUSION, (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "FUSION send api error\n");
        delete [] kernel;
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("FUSION", BMLIB_LOG_ERROR, "FUSION sync api error\n");
        delete [] kernel;
        return ret;
    }
    return BM_SUCCESS;
}


bm_status_t bmcv_image_fusion(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output,
        unsigned char thresh,
        unsigned char max_value,
        bm_thresh_type_t type,
        int kw,
        int kh,
        bm_device_mem_t kmem) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_fusion_bm1684(handle, input1, input2,
                                        output, thresh, max_value,
                                        type, kw, kh, kmem);
        break;

      case BM1684X:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}
