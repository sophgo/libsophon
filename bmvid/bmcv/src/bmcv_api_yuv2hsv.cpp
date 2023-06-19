#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"

static bm_status_t bmcv_yuv2hsv_check(
        bm_handle_t handle,
        bmcv_rect_t rect,
        bm_image input,
        bm_image output) {
    if (handle == NULL) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_image_format_ext fmt_i = input.image_format;
    bm_image_format_ext fmt_o = output.image_format;
    bm_image_data_format_ext type_i = input.data_type;
    bm_image_data_format_ext type_o = output.data_type;
    if (fmt_i != FORMAT_NV12 && fmt_i != FORMAT_NV21 && fmt_i != FORMAT_YUV420P) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "input image format NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (fmt_o != FORMAT_HSV_PLANAR) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "output image format NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (type_i != DATA_TYPE_EXT_1N_BYTE || type_i != type_o) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "data type NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (!bm_image_is_attached(input)) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "input not malloc device memory!\r\n");
        return BM_ERR_PARAM;
    }
    if (rect.start_x < 0 || rect.start_x + rect.crop_w > input.width ||
        rect.start_y < 0 || rect.start_y + rect.crop_h > input.height) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "ROI out of input image!\r\n");
        return BM_ERR_PARAM;
    }
    if (rect.crop_w > output.width || rect.crop_h > output.height ||
        (output.width - rect.crop_w) % 2 || (output.height - rect.crop_h) % 2) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "output size should greater than ROI size!\r\n");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}


bm_status_t bmcv_image_yuv2hsv(bm_handle_t     handle,
                               bmcv_rect_t     rect,
                               bm_image        input,
                               bm_image        output) {

    if (handle == NULL) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }

    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_yuv2hsv_check(handle, rect, input, output);
    if (BM_SUCCESS != ret) {
        return ret;
    }

    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
        output_alloc_flag = true;
    }

    int plane_num = bm_image_get_plane_num(input);
    bm_device_mem_t in_dev_mem[3], out_dev_mem[3];
    unsigned long long src_addr[3], dst_addr[3];
    bm_image_get_device_mem(input, in_dev_mem);
    bm_image_get_device_mem(output, out_dev_mem);
    for (int p = 0; p < plane_num; p++) {
        src_addr[p] = bm_mem_get_device_addr(in_dev_mem[p]);
        dst_addr[p] = bm_mem_get_device_addr(out_dev_mem[p]);
    }
    int stride_i[3], stride_o[3];
    bm_image_get_stride(input, stride_i);
    bm_image_get_stride(output, stride_o);
    int pad_h = (output.height - rect.crop_h) / 2;
    int pad_w = (output.width - rect.crop_w) / 2;
    // if pad, memset the whole memory
    if (pad_h || pad_w) {
        for (int p = 0; p < plane_num; p++) {
            bm_memset_device(handle, 0, out_dev_mem[p]);
        }
    }
    // construct api info
    bm_api_cv_yuv2hsv_t api;
    api.src_format = input.image_format;  // 0:FORMAT_YUV420P  3:FORMAT_NV12  4:FORMAT_NV21
    api.dst_format = output.image_format;
    api.src_addr[0] = src_addr[0] + rect.start_y * stride_i[0] + rect.start_x;
    api.src_addr[1] = input.image_format == FORMAT_YUV420P ?
                      src_addr[1] + rect.start_y / 2 * stride_i[1] + rect.start_x / 2 :
                      src_addr[1] + rect.start_y / 2 * stride_i[1] + rect.start_x;
    api.src_addr[2] = input.image_format == FORMAT_YUV420P ?
                      src_addr[2] + rect.start_y / 2 * stride_i[2] + rect.start_x / 2 :
                      0;
    api.width = rect.crop_w;
    api.height = rect.crop_h;
    api.stride_i = stride_i[0];
    api.ow = stride_o[0];
    api.oh = output.height;
    api.dst_addr[0] = dst_addr[0] + pad_h * stride_o[0] + pad_w;
    api.dst_addr[1] = dst_addr[1] + pad_h * stride_o[1] + pad_w;
    api.dst_addr[2] = dst_addr[2] + pad_h * stride_o[2] + pad_w;
    ret = bm_send_api(handle,  BM_API_ID_CV_YUV2HSV, (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "yuv2hsv send api error\n");
        if (output_alloc_flag) {
            for (int p = 0; p < plane_num; p++) {
                bm_free_device(handle, out_dev_mem[p]);
            }
        }
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("YUV2HSV", BMLIB_LOG_ERROR, "yuv2hsv sync api error\n");
        if (output_alloc_flag) {
            for (int p = 0; p < plane_num; p++) {
                bm_free_device(handle, out_dev_mem[p]);
            }
        }
        return ret;
    }
    return BM_SUCCESS;
}

