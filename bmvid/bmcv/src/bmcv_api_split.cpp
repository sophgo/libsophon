#include <math.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"


bm_status_t bmcv_split_check(bm_image input,
                             bm_image* output) {
    int src_w = input.width;
    int src_h = input.height;
    int dst_w = output[0].width;
    int dst_h = output[0].height;
    int sec_w = src_w / dst_w;
    int sec_h = src_h / dst_h;
    if (src_w % dst_w || src_h % dst_h) {
        BMCV_ERR_LOG("[Split] src_width \% dst_width != 0 or src_height \% dst_height != 0\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (!bm_image_is_attached(output[0])) {
        BMCV_ERR_LOG("[Split] output bm_image should use bm_image_alloc_contiguous_mem to alloc mem\r\n");
        return BM_NOT_SUPPORTED;
    }
    for (int i = 0; i < sec_w * sec_h; i++) {
        if ((input.data_type != output[i].data_type) ||
            (input.image_format != output[i].image_format)) {
            BMCV_ERR_LOG(
                "[Split] input data_type and image_format must be same to outputs!\r\n");
            return BM_NOT_SUPPORTED;
        }
        if (dst_w != output[i].width || dst_h != output[i].height) {
            BMCV_ERR_LOG("[Split] all outputs size should be equal\r\n");
            return BM_ERR_FAILURE;
        }
    }
    // image format check
    if ((input.image_format != FORMAT_RGB_PACKED) &&
        (input.image_format != FORMAT_BGR_PACKED)) {
        BMCV_ERR_LOG("[Split] image format only support RGB_PACKED/BGR_PACKED\r\n");
        return BM_NOT_SUPPORTED;
    }
    if ((input.data_type != DATA_TYPE_EXT_1N_BYTE_SIGNED) &&
        (input.data_type != DATA_TYPE_EXT_1N_BYTE)) {
        BMCV_ERR_LOG("[Split] image data type only support 1N_BYTE and 1N_BYTE_SIGNED\r\n");
        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_image_split(bm_handle_t         handle,
                             bm_image            input,
                             bm_image *          output) {
    if (handle == NULL) {
        BMCV_ERR_LOG("[Split] Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_split_check(input, output);
    if (BM_SUCCESS != ret) {
        BMCV_ERR_LOG("[Split] bmcv_split_check error!\r\n");
        return ret;
    }
    int src_w = input.width;
    int src_h = input.height;
    int dst_w = output[0].width;
    int dst_h = output[0].height;

    bm_device_mem_t dev_mem_i, dev_mem_o;
    bm_image_get_device_mem(input, &dev_mem_i);
    bm_image_get_device_mem(output[0], &dev_mem_o);
    bm_api_cv_split_t arg;
    arg.src_global_offset = bm_mem_get_device_addr(dev_mem_i);
    arg.dst_global_offset = bm_mem_get_device_addr(dev_mem_o);
    arg.src_h = src_h;
    arg.src_w = src_w * 3;
    arg.dst_h = dst_h;
    arg.dst_w = dst_w * 3;
    arg.type_size = input.data_type == DATA_TYPE_EXT_FLOAT32 ? 4 : 1;

    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_SPLIT, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("split send api error\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("split sync api error\r\n");
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

