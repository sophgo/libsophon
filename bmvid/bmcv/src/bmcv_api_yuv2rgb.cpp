#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_internal.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

#ifndef USING_CMODEL
#include "vpplib.h"
#endif
#ifdef USING_CMODEL_BM1684
// #define FORCE_VPP
#endif

static bm_status_t bmcv_yuv2bgr_check(int                   input_n,
                                      int                   input_h,
                                      int                   input_w,
                                      bm_image_format_ext src_image_format,
                                      bm_image_data_format_ext  src_data_format,
                                      bm_image_format_ext dst_image_format,
                                      bm_image_data_format_ext  dst_data_format) {
    if (input_n < 1 || input_h < 1 || input_w < 1) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "illegal image_num, image size %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (input_w % 2 != 0 || input_h % 2 != 0)
    {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "input width and height should 2 aligned %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if (src_data_format != DATA_TYPE_EXT_1N_BYTE)
    {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "input  data size should be DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }


    if (!(dst_data_format == DATA_TYPE_EXT_FLOAT32 ||
          dst_data_format == DATA_TYPE_EXT_1N_BYTE ||
          dst_data_format == DATA_TYPE_EXT_4N_BYTE))
    {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "not supported output data type %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if ((dst_data_format == DATA_TYPE_EXT_FP16) ||
        (dst_data_format == DATA_TYPE_EXT_FP16)||
        (dst_data_format == DATA_TYPE_EXT_BF16) ||
        (dst_data_format == DATA_TYPE_EXT_BF16)){
        BMCV_ERR_LOG("data type not support\n");

        return BM_NOT_SUPPORTED;
    }

    if (!(src_image_format == FORMAT_NV12 || src_image_format == FORMAT_NV21 ||
          src_image_format == FORMAT_NV16 || src_image_format == FORMAT_NV61 ||
          src_image_format == FORMAT_YUV420P || src_image_format == FORMAT_YUV422P))
    {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "not supported input image format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (!(dst_image_format == FORMAT_RGB_PLANAR || dst_image_format == FORMAT_BGR_PLANAR))
    {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "not supported output image format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_yuv2bgr_ext_(bm_handle_t  handle,
                                    int          image_num,
                                    bm_image *   input,
                                    bm_image *   output) {
    if (handle == NULL) {
        bmlib_log("YUV2BGR", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    int                   width            = input[0].width;
    int                   height           = input[0].height;
    bm_image_format_ext src_image_format = input[0].image_format;
    bm_image_data_format_ext  src_data_format  = input[0].data_type;

    bm_image_format_ext dst_image_format = output[0].image_format;
    bm_image_data_format_ext  dst_data_format  = output[0].data_type;

    int output_image_num =
        dst_data_format == DATA_TYPE_EXT_4N_BYTE ? 1 : image_num;

    if (!bm_image_is_attached(input[0])) {
        bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "input not attached data!\r\n");
        return BM_ERR_PARAM;
    }
    for (int i = 1; i < image_num; i++) {
        if (width != input[i].width || height != input[i].height) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "inputs shape not same!\r\n");
            return BM_ERR_PARAM;
        }
        if (src_data_format != input[i].data_type) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "inputs data type not same!\r\n");
            return BM_ERR_PARAM;
        }
        if (src_image_format != input[i].image_format) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "inputs format not same!\r\n");
            return BM_ERR_PARAM;
        }
        if (!bm_image_is_attached(input[i])) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "input %d not attached data!\r\n", i);
            return BM_ERR_PARAM;
        }
    }

    for (int i = 0; i < output_image_num; i++) {
        if (width != output[i].width || height != output[i].height) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "outputs shape not same!\r\n");
            return BM_ERR_PARAM;
        }
        if (dst_data_format != output[i].data_type) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "outputs data type not same!\r\n");
            return BM_ERR_PARAM;
        }
        if (dst_image_format != output[i].image_format) {
            bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "outputs format not same!\r\n");
            return BM_ERR_PARAM;
        }
    }

    bm_status_t result = bmcv_yuv2bgr_check(image_num,
                                            height,
                                            width,
                                            src_image_format,
                                            src_data_format,
                                            dst_image_format,
                                            dst_data_format);
    if (result == BM_NOT_SUPPORTED) {
        return result;
    }

    int src_stride[3] = {0};
    bm_image_get_stride(input[0], src_stride);
    for (int i = 1; i < image_num; i++)
    {
        int stride[3] = {0};
        bm_image_get_stride(input[i], stride);
        for (int k = 0; k < 3; k++)
        {
            if (src_stride[k] != stride[k]) {
                bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "src stride not same!\r\n");
                return BM_ERR_PARAM;
            }
        }
    }

    int dst_stride[3] = {0};
    bm_image_get_stride(output[0], dst_stride);
    for (int i = 1; i < output_image_num; i++)
    {
        int stride[3] = {0};
        bm_image_get_stride(output[i], stride);
        for (int k = 0; k < 3; k++)
        {
            if (dst_stride[k] != stride[k]) {
                bmlib_log("YUV2RGB", BMLIB_LOG_ERROR, "dst stride not same!\r\n");
                return BM_ERR_PARAM;
            }
        }
    }

    int input_plane_size[3];
    bm_image_get_byte_size(input[0], input_plane_size);

    #ifdef __linux__
    bool output_alloc_flag[output_image_num];
    #else
    std::shared_ptr<bool> output_alloc_flag_(new bool[output_image_num], std::default_delete<bool[]>());
    bool*                 output_alloc_flag = output_alloc_flag_.get();
    #endif
    for (int i = 0; i < output_image_num; i++) {
        if (!bm_image_is_attached(output[i])) {
            output_alloc_flag[i] = false;
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

#ifndef USING_CMODEL
    bool try_use_vpp = true;
    for (int i = 0; i < image_num; i++) {
        bm_device_mem_t mem[3];
        bm_image_get_device_mem(input[i], mem);
        for (int k = 0; k < bm_image_get_plane_num(input[i]); k++)
        {
            if(bm_mem_get_device_addr(mem[k]) < 0x300000000)  // DDR1_MEM_START_ADDR
            {
                try_use_vpp = false;
            }
        }
    }

    for (int i = 0; i < output_image_num; i++) {
        if (output[i].data_type == DATA_TYPE_EXT_4N_BYTE || output[i].data_type == DATA_TYPE_EXT_FLOAT32) {
            try_use_vpp = false;
        }
    }

    if(try_use_vpp)
    {
        bool success = true;
        bmcv_rect_t rect = {0, 0, input[0].width, input[0].height};
        for (int i = 0; i < image_num; i++)
        {
            #ifdef __linux__
            if (bmcv_image_vpp_convert(handle, 1, input[i], output + i, &rect) != BM_SUCCESS)
            {
                bmlib_log("BMCV", BMLIB_LOG_INFO, "can't use vpp, go through tpu path\n");
                success = false;
                break;
            }
            #else
            return BM_NOT_SUPPORTED;
            #endif
        }
        if(success)
        {
            return BM_SUCCESS;
        }
    }

#endif

    bm_image_format_info info_in, info_out;
    bm_image_get_format_info(input, &info_in);
    bm_image_get_format_info(output, &info_out);

    if(!info_in.default_stride || !info_out.default_stride)
    {
        return bmcv_image_storage_convert(handle, image_num, input, output);
    }

    bm_device_mem_t tensor_input[4][3];
    bm_device_mem_t tensor_output[4][1];
    u64             input_dev_addr[4][3]       = {0};
    u64             output_dev_addr[4][1]      = {0};
    for (int i = 0; i < image_num; i++) {
        if(BM_SUCCESS !=bm_image_get_device_mem(input[i], tensor_input[i])) {
            BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");
            for (int free_idx = 0; free_idx < output_image_num; free_idx ++) {
                if (output_alloc_flag[free_idx]) {
                       bm_device_mem_t dmem;
                       bm_image_get_device_mem(output[free_idx], &dmem);
                       bm_free_device(handle, dmem);
                   }
            }

            return BM_ERR_FAILURE;
        }
        for (int channel_idx = 0;
             channel_idx < bm_image_get_plane_num(input[i]);
             channel_idx++) {
            input_dev_addr[i][channel_idx] =
                bm_mem_get_device_addr(tensor_input[i][channel_idx]);
        }
    }

    for (int i = 0; i < output_image_num; i++) {
        if(BM_SUCCESS !=bm_image_get_device_mem(output[i], tensor_output[i])) {
            BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");
            for (int free_idx = 0; free_idx < output_image_num; free_idx ++) {
                if (output_alloc_flag[free_idx]) {
                       bm_device_mem_t dmem;
                       bm_image_get_device_mem(output[free_idx], &dmem);
                       bm_free_device(handle, dmem);
                   }
            }

            return BM_ERR_FAILURE;
        }
        output_dev_addr[i][0] = bm_mem_get_device_addr(tensor_output[i][0]);
    }

    bm_api_cv_yuv2rgb_t api = {input_dev_addr[0][0],
                               input_dev_addr[0][1],
                               input_dev_addr[0][2],
                               input_dev_addr[1][0],
                               input_dev_addr[1][1],
                               input_dev_addr[1][2],
                               input_dev_addr[2][0],
                               input_dev_addr[2][1],
                               input_dev_addr[2][2],
                               input_dev_addr[3][0],
                               input_dev_addr[3][1],
                               input_dev_addr[3][2],

                               output_dev_addr[0][0],
                               output_dev_addr[1][0],
                               output_dev_addr[2][0],
                               output_dev_addr[3][0],

                               (int)(src_data_format),
                               src_image_format,
                               image_num,
                               height,
                               width,

                               (int)(dst_data_format),
                               dst_image_format};

    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_YUV2RGB, (uint8_t *)&api, sizeof(api))) {
        BMCV_ERR_LOG("yuv2rgb send api error\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("yuv2rgb sync api error\r\n");
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

// in order to support input_num > 4
bm_status_t bm1684_bmcv_image_yuv2bgr(bm_handle_t  handle,
                                   int          image_num,
                                   bm_image *   input,
                                   bm_image *   output) {
    bm_status_t ret = BM_SUCCESS;
    int loop = (image_num + 3) / 4;
    for (int i = 0; i < loop; i++) {
        int num = (i == loop - 1) ? (image_num - (loop - 1) * 4) : 4;
        ret = bmcv_image_yuv2bgr_ext_(handle, num, input + i * 4, output + i * 4);
        if (ret != BM_SUCCESS) return ret;
    }
    return ret;
}

bm_status_t bmcv_image_yuv2bgr_ext(
    bm_handle_t  handle,
    int          image_num,
    bm_image *   input,
    bm_image *   output)
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bm1684_bmcv_image_yuv2bgr(handle, image_num, input, output);
        break;


      case BM1684X:
        ret = bm1684x_vpp_yuv2bgr_ext(handle, image_num, input, output);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

bm_status_t bm_img_yuv2bgr(bm_handle_t handle,
                           // input
                           bmcv_image input,
                           // output
                           bmcv_image output) {
    bm_image_format_ext src_image_format = FORMAT_YUV420P;
    bm_image_data_format_ext  src_data_format = DATA_TYPE_EXT_1N_BYTE;
    bm_image_format_ext dst_image_format = FORMAT_YUV420P;
    bm_image_data_format_ext  dst_data_format = DATA_TYPE_EXT_1N_BYTE;

    ASSERT(input.color_space == COLOR_YCbCr && output.color_space == COLOR_RGB);
    ASSERT(input.image_format == YUV420P || input.image_format == NV12 ||
           input.image_format == NV21);
    ASSERT(output.image_format == BGR || output.image_format == RGB ||
           output.image_format == BGR4N);
    ASSERT(input.data_format == DATA_TYPE_BYTE);
    ASSERT(!(output.image_format == BGR4N &&
             output.data_format == DATA_TYPE_FLOAT));

    switch (input.image_format) {
        case YUV420P:
            src_image_format = FORMAT_YUV420P;
            break;
        case NV12:
            src_image_format = FORMAT_NV12;
            break;
        case NV21:
            src_image_format = FORMAT_NV21;
            break;
        default:
            break;
    }
    src_data_format = DATA_TYPE_EXT_1N_BYTE;

    switch (output.data_format) {
        case DATA_TYPE_FLOAT:
            dst_data_format = DATA_TYPE_EXT_FLOAT32;
            break;
        case DATA_TYPE_BYTE:
            dst_data_format = DATA_TYPE_EXT_1N_BYTE;
            break;
        default:
            break;
    }

    switch (output.image_format) {
        case RGB:
            dst_image_format = FORMAT_BGR_PLANAR;
            break;
        case BGR:
            dst_image_format = FORMAT_RGB_PLANAR;
            break;
        case BGR4N:
            dst_image_format = FORMAT_BGR_PLANAR;
            dst_data_format  = DATA_TYPE_EXT_4N_BYTE;
            break;
        case RGB4N:
            dst_image_format = FORMAT_RGB_PLANAR;
            dst_data_format  = DATA_TYPE_EXT_4N_BYTE;
            break;
        default:
            break;
    }

    bm_image image_input, image_output;
    bm_image_create(handle,
                      input.image_height,
                      input.image_width,
                      src_image_format,
                      src_data_format,
                      &image_input);
    bm_image_create(handle,
                      output.image_height,
                      output.image_width,
                      dst_image_format,
                      dst_data_format,
                      &image_output);

    bm_image_attach(image_input, input.data);
    bm_image_attach(image_output, output.data);

    bm_status_t status    = bmcv_image_yuv2bgr_ext(
        handle, 1, &image_input, &image_output);

    bm_image_destroy(image_input);
    bm_image_destroy(image_output);

    return status;
}

