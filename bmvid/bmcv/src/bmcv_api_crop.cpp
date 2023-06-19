#include <math.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

INLINE static bm_image_data_format_ext bmcv_get_data_format_from_sc3(bmcv_data_format data_format)
{
    switch(data_format) {
    case DATA_TYPE_FLOAT:
        return DATA_TYPE_EXT_FLOAT32;
    break;
    case DATA_TYPE_BYTE:
        return DATA_TYPE_EXT_1N_BYTE;
    break;
    default:
        return DATA_TYPE_EXT_1N_BYTE;
    break;
    }
}

INLINE static bm_image_format_ext bmcv_get_image_format_from_sc3(bm_image_format img_format)
{
    switch(img_format) {
    case YUV420P:
        return FORMAT_YUV420P;
    break;
    case NV12:
        return FORMAT_NV12;
    break;
    case NV21:
        return FORMAT_NV21;
    break;
    case RGB:
        return FORMAT_RGB_PLANAR;
    break;
    case BGR: 
        return FORMAT_BGR_PLANAR;
    break;
    case RGB_PACKED:
        return FORMAT_RGB_PACKED;
    break;
    case BGR_PACKED:
        return FORMAT_BGR_PACKED;
    break;
    default:
        return FORMAT_BGR_PACKED;
    break;
    }
}

bm_status_t bmcv_crop_check(int          crop_num,
                            bmcv_rect_t* rects,
                            bm_image     input,
                            bm_image*    output) {
    for (int i = 0; i < crop_num; i++) {
        if ((input.data_type != output[i].data_type) ||
            (input.image_format != output[i].image_format)) {
            BMCV_ERR_LOG(
                "[Crop] input data_type and image_format must be same to "
                "output!\r\n");
            return BM_NOT_SUPPORTED;
        }
        if (rects[i].start_x < 0 || rects[i].start_x > input.width ||
            rects[i].start_y < 0 || rects[i].start_y > input.height) {
            BMCV_ERR_LOG("[Crop] %dth rect coordinate is illegal\r\n", i);
            return BM_ERR_FAILURE;
        }
        if (rects[i].start_x + rects[i].crop_w > input.width ||
            rects[i].start_y + rects[i].crop_h > input.height) {
            BMCV_ERR_LOG("[Crop] %dth crop box is out of input range\r\n", i);
            return BM_ERR_FAILURE;
        }
        if (rects[i].crop_w != output[i].width ||
            rects[i].crop_h != output[i].height) {
            BMCV_ERR_LOG(
                "[Crop] %dth output size should equal to crop size\r\n", i);
            return BM_ERR_FAILURE;
        }
    }
    // image format check
    if ((input.image_format != FORMAT_RGB_PLANAR) &&
        (input.image_format != FORMAT_RGB_PACKED) &&
        (input.image_format != FORMAT_BGR_PLANAR) &&
        (input.image_format != FORMAT_BGR_PACKED) &&
        (input.image_format != FORMAT_GRAY)) {
        BMCV_ERR_LOG("[Crop] image format only support RGB/BGR/GRAY\r\n");
        return BM_NOT_SUPPORTED;
    }
    if ((input.data_type != DATA_TYPE_EXT_1N_BYTE_SIGNED) &&
        (input.data_type != DATA_TYPE_EXT_1N_BYTE) &&
        (input.data_type != DATA_TYPE_EXT_FLOAT32)) {
        BMCV_ERR_LOG(
            "[Crop] image data type only support 1N int8 and float32\r\n");
        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

static bm_status_t bmcv_image_crop(bm_handle_t handle,
                                   bmcv_rect_t rect,
                                   bm_image    input,
                                   bm_image    output) {
    int  n, c, h, w;
    int  src_nstride, src_cstride, src_hstride, src_wstride;
    int  dst_nstride, dst_cstride, dst_hstride, dst_wstride;
    int  format_bytes;
    bool is_packed;
    int  stride_i[3] = {0};
    bm_image_get_stride(input, stride_i);
    int stride_o[3] = {0};
    bm_image_get_stride(output, stride_o);
    switch (input.data_type) {
        case DATA_TYPE_EXT_FLOAT32:
            format_bytes = 4;
            break;
        case DATA_TYPE_EXT_1N_BYTE:
        case DATA_TYPE_EXT_1N_BYTE_SIGNED:
            format_bytes = 1;
            break;
        default:
            BMCV_ERR_LOG("[Crop] not support this format!\r\n");
            return BM_ERR_FAILURE;
    }
    switch (input.image_format) {
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR:
            n           = 1;
            c           = 3;
            h           = rect.crop_h;
            w           = rect.crop_w;
            src_wstride = 1;
            src_hstride = stride_i[0] / format_bytes;
            src_cstride = input.height * stride_i[0] / format_bytes;
            src_nstride = 0;
            dst_wstride = 1;
            dst_hstride = stride_o[0] / format_bytes;
            dst_cstride = h * stride_o[0] / format_bytes;
            dst_nstride = 0;
            is_packed   = false;
            break;
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED:
            n           = 1;
            c           = rect.crop_h;
            h           = rect.crop_w;
            w           = 3;
            src_wstride = 1;
            src_hstride = w;
            src_cstride = stride_i[0] / format_bytes;
            src_nstride = 0;
            dst_wstride = 1;
            dst_hstride = w;
            dst_cstride = stride_o[0] / format_bytes;
            dst_nstride = 0;
            is_packed   = true;
            break;
        case FORMAT_GRAY: {
            n           = 1;
            c           = 1;
            h           = rect.crop_h;
            w           = rect.crop_w;
            src_wstride = 1;
            src_hstride = stride_i[0] / format_bytes;
            src_cstride = 0;
            src_nstride = 0;
            dst_wstride = 1;
            dst_hstride = stride_o[0] / format_bytes;
            dst_cstride = 0;
            dst_nstride = 0;
            is_packed   = false;
            break;
        }
        default:
            BMCV_ERR_LOG("[Crop] not support this format!\r\n");
            return BM_ERR_FAILURE;
    }

    if (!bm_image_is_attached(output)) {
        if (BM_SUCCESS != bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY)) {
            BMCV_ERR_LOG("[Crop] bm_image_alloc_dev_mem error!\r\n");
            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t in_dev_mem, out_dev_mem;
    bm_image_get_device_mem(input, &in_dev_mem);
    u64 in_dev_addr = bm_mem_get_device_addr(in_dev_mem);
    bm_image_get_device_mem(output, &out_dev_mem);
    u64 out_dev_addr = bm_mem_get_device_addr(out_dev_mem);
    in_dev_addr      = in_dev_addr + rect.start_y * stride_i[0] +
                  rect.start_x * (is_packed ? 3 : 1) * format_bytes;
    bm_api_memcpy_tensor_t arg;
    arg.src_global_offset = in_dev_addr;
    arg.dst_global_offset = out_dev_addr;
    arg.n                 = n;
    arg.c                 = c;
    arg.h                 = h;
    arg.w                 = w;
    arg.src_wstride       = src_wstride;
    arg.src_hstride       = src_hstride;
    arg.src_cstride       = src_cstride;
    arg.src_nstride       = src_nstride;
    arg.dst_wstride       = dst_wstride;
    arg.dst_hstride       = dst_hstride;
    arg.dst_cstride       = dst_cstride;
    arg.dst_nstride       = dst_nstride;
    arg.format_bytes      = format_bytes;

    if (BM_SUCCESS !=
        bm_send_api(
            handle,  BM_API_ID_MEMCPY_TENSOR, (uint8_t*)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("crop send api error\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("crop sync api error\r\n");
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bm1684_bmcv_image_crop(bm_handle_t  handle,
                            int          crop_num,
                            bmcv_rect_t* rects,
                            bm_image     input,
                            bm_image*    output) {
    if (handle == NULL) {
        BMCV_ERR_LOG("[Crop] Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_status_t ret = BM_SUCCESS;
    ret             = bmcv_crop_check(crop_num, rects, input, output);
    if (BM_SUCCESS != ret) {
        BMCV_ERR_LOG("[Crop] bmcv_crop_check error!\r\n");
        return ret;
    }
    if (crop_num < 3) {
        for (int i = 0; i < crop_num; i++) {
            ret = bmcv_image_crop(handle, rects[i], input, output[i]);
            if (BM_SUCCESS != ret) {
                BMCV_ERR_LOG("[Crop] bmcv_image_crop error!\r\n");
                return ret;
            }
        }
        return ret;
    }

    bool*                   output_alloc_flag = new bool[crop_num];
    bm_api_memcpy_tensor_t* copy_info = new bm_api_memcpy_tensor_t[crop_num];
    for (int i = 0; i < crop_num; i++) {
        output_alloc_flag[i] = false;
        if (!bm_image_is_attached(output[i])) {
            if (BM_SUCCESS !=
                bm_image_alloc_dev_mem(output[i], BMCV_HEAP_ANY)) {
                BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");
                for (int free_idx = 0; free_idx < i; free_idx++) {
                    if (output_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(output[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }
                delete[] output_alloc_flag;
                delete[] copy_info;
                return BM_ERR_FAILURE;
            }
            output_alloc_flag[i] = true;
        }
    }

    // fill tensor copy info
    bm_device_mem_t in_dev_mem;
    bm_image_get_device_mem(input, &in_dev_mem);
    u64 in_dev_addr = bm_mem_get_device_addr(in_dev_mem);
    for (int i = 0; i < crop_num; i++) {

        bool is_packed;
        int  stride_i[3] = {0};
        bm_image_get_stride(input, stride_i);
        int stride_o[3] = {0};
        bm_image_get_stride(output[i], stride_o);
        switch (input.data_type) {
            case DATA_TYPE_EXT_FLOAT32:
                copy_info[i].format_bytes = 4;
                break;
            case DATA_TYPE_EXT_1N_BYTE:
            case DATA_TYPE_EXT_1N_BYTE_SIGNED:
                copy_info[i].format_bytes = 1;
                break;
            default:
                BMCV_ERR_LOG("[Crop] not support this format!\r\n");
                return BM_ERR_FAILURE;
        }
        switch (input.image_format) {
            case FORMAT_BGR_PLANAR:
            case FORMAT_RGB_PLANAR:
                copy_info[i].n           = 1;
                copy_info[i].c           = 3;
                copy_info[i].h           = rects[i].crop_h;
                copy_info[i].w           = rects[i].crop_w;
                copy_info[i].src_wstride = 1;
                copy_info[i].src_hstride =
                    stride_i[0] / copy_info[i].format_bytes;
                copy_info[i].src_cstride =
                    input.height * stride_i[0] / copy_info[i].format_bytes;
                copy_info[i].src_nstride = 0;
                copy_info[i].dst_wstride = 1;
                copy_info[i].dst_hstride =
                    stride_o[0] / copy_info[i].format_bytes;
                copy_info[i].dst_cstride =
                    copy_info[i].h * stride_o[0] / copy_info[i].format_bytes;
                copy_info[i].dst_nstride = 0;
                is_packed                = false;
                break;
            case FORMAT_BGR_PACKED:
            case FORMAT_RGB_PACKED:
                copy_info[i].n           = 1;
                copy_info[i].c           = rects[i].crop_h;
                copy_info[i].h           = rects[i].crop_w;
                copy_info[i].w           = 3;
                copy_info[i].src_wstride = 1;
                copy_info[i].src_hstride = 3;
                copy_info[i].src_cstride =
                    stride_i[0] / copy_info[i].format_bytes;
                copy_info[i].src_nstride = 0;
                copy_info[i].dst_wstride = 1;
                copy_info[i].dst_hstride = 3;
                copy_info[i].dst_cstride =
                    stride_o[0] / copy_info[i].format_bytes;
                copy_info[i].dst_nstride = 0;
                is_packed                = true;
                break;
            case FORMAT_GRAY: {
                copy_info[i].n           = 1;
                copy_info[i].c           = 1;
                copy_info[i].h           = rects[i].crop_h;
                copy_info[i].w           = rects[i].crop_w;
                copy_info[i].src_wstride = 1;
                copy_info[i].src_hstride =
                    stride_i[0] / copy_info[i].format_bytes;
                copy_info[i].src_cstride = 0;
                copy_info[i].src_nstride = 0;
                copy_info[i].dst_wstride = 1;
                copy_info[i].dst_hstride =
                    stride_o[0] / copy_info[i].format_bytes;
                copy_info[i].dst_cstride = 0;
                copy_info[i].dst_nstride = 0;
                is_packed                = false;
                break;
            }
            default:
                BMCV_ERR_LOG("[Crop] not support this format!\r\n");
                return BM_ERR_FAILURE;
        }
        bm_device_mem_t dev_mem;
        bm_image_get_device_mem(output[i], &dev_mem);
        copy_info[i].dst_global_offset = bm_mem_get_device_addr(dev_mem);
        copy_info[i].src_global_offset =
            in_dev_addr + rects[i].start_y * stride_i[0] +
            rects[i].start_x * (is_packed ? 3 : 1) * copy_info[i].format_bytes;
    }

    // copy the parameter to device memory.
    int             info_size = crop_num * sizeof(bm_api_memcpy_tensor_t);
    bm_device_mem_t info_mem;
    if (BM_SUCCESS != bm_malloc_device_byte(handle, &info_mem, info_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte_heap error\r\n");
        for (int free_idx = 0; free_idx < crop_num; free_idx++) {
            if (output_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }
        delete[] output_alloc_flag;
        delete[] copy_info;
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_memcpy_s2d(handle, info_mem, copy_info)) {
        BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
        for (int free_idx = 0; free_idx < crop_num; free_idx++) {
            if (output_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }
        bm_free_device(handle, info_mem);
        delete[] output_alloc_flag;
        delete[] copy_info;
        return BM_ERR_FAILURE;
    }

    bm_api_memcpy_tensors_t arg;
    arg.copy_num           = crop_num;
    arg.info_global_offset = bm_mem_get_device_addr(info_mem);
    if (BM_SUCCESS !=
        bm_send_api(
            handle,  BM_API_ID_MEMCPY_TENSORS, (uint8_t*)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("crop send api error\r\n");
        delete[] output_alloc_flag;
        delete[] copy_info;
        bm_free_device(handle, info_mem);
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("crop sync api error\r\n");
        delete[] output_alloc_flag;
        delete[] copy_info;
        bm_free_device(handle, info_mem);
        return BM_ERR_FAILURE;
    }
    delete[] output_alloc_flag;
    delete[] copy_info;
    bm_free_device(handle, info_mem);

    return BM_SUCCESS;
}

bm_status_t bmcv_image_crop(
  bm_handle_t  handle,
  int          crop_num,
  bmcv_rect_t* rects,
  bm_image     input,
  bm_image*    output)
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bm1684_bmcv_image_crop(handle, crop_num, rects, input, output);
        break;


      case BM1684X:
        ret = bm1684x_vpp_crop(handle, crop_num, rects, input, output);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

// old api
bm_status_t bmcv_img_crop(bm_handle_t handle,
                          // input
                          bmcv_image input,
                          int        channels,
                          int        top,
                          int        left,
                          // output
                          bmcv_image output) {
    bm_image_format_ext      src_image_format;
    bm_image_data_format_ext src_data_format;
    bm_image_format_ext      dst_image_format;
    bm_image_data_format_ext dst_data_format;
    bm_status_t              ret = BM_SUCCESS;

    if (handle == NULL) {
        bmlib_log("CROP", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (channels != 1 && channels != 3) {
        bmlib_log("CROP", BMLIB_LOG_ERROR, "channels should be 1 or 3!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.color_space != COLOR_RGB || output.color_space != COLOR_RGB) {
        bmlib_log("CROP",
                  BMLIB_LOG_ERROR,
                  "color_space of input and output bmcv_image should be "
                  "COLOR_RGB!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.image_format != BGR && input.image_format != RGB) {
        bmlib_log("CROP",
                  BMLIB_LOG_ERROR,
                  "image_format of input bmcv_image should be RGB or BGR!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.data_format != DATA_TYPE_FLOAT &&
        input.data_format != DATA_TYPE_BYTE) {
        bmlib_log("CROP",
                  BMLIB_LOG_ERROR,
                  "data_format of input bmcv_image should be DATA_TYPE_FLOAT "
                  "or DATA_TYPE_BYTE!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (output.data_format != DATA_TYPE_FLOAT &&
        output.data_format != DATA_TYPE_BYTE) {
        bmlib_log("CROP",
                  BMLIB_LOG_ERROR,
                  "data_format of output bmcv_image should be DATA_TYPE_FLOAT "
                  "or DATA_TYPE_BYTE!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (output.stride[0] < output.image_width) {
        bmlib_log("CROP",
                  BMLIB_LOG_ERROR,
                  "stride of output should be greater than width!\r\n");
        return BM_NOT_SUPPORTED;
    }

    int   input_n = 1;
    void* sys_buf = NULL;

    src_image_format =
        (channels == 1) ? (FORMAT_GRAY)
                        : (bmcv_get_image_format_from_sc3(input.image_format));
    src_data_format = bmcv_get_data_format_from_sc3(input.data_format);
    dst_image_format =
        (channels == 1) ? (FORMAT_GRAY)
                        : (bmcv_get_image_format_from_sc3(output.image_format));
    dst_data_format = bmcv_get_data_format_from_sc3(output.data_format);

    bm_image          image_in, image_out;
    bmcv_resize_t     resize_param;
    bmcv_resize_image resize_attr[4];

    // create tensor.
    ret = bm_image_create(handle,
                          input.image_height,
                          input.image_width,
                          src_image_format,
                          src_data_format,
                          &image_in);
    if (ret != BM_SUCCESS) {
        goto exit;
    }
    ret = bm_image_create(handle,
                          output.image_height,
                          output.stride[0],
                          dst_image_format,
                          dst_data_format,
                          &image_out);
    if (ret != BM_SUCCESS) {
        goto exit1;
    }

    // copy or attach input to image in.
    if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
        sys_buf = bm_mem_get_system_addr(input.data[0]);
        ret     = bm_image_copy_host_to_device(image_in, &sys_buf);
        if (ret != BM_SUCCESS) {
            goto exit2;
        }
    } else {
        ret = bm_image_attach(image_in, &input.data[0]);
        if (ret != BM_SUCCESS) {
            goto exit2;
        }
    }

    // attach output image.
    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_DEVICE) {
        ret = bm_image_attach(image_out, &output.data[0]);
        if (ret != BM_SUCCESS) {
            goto exit2;
        }
    }
    resize_param.start_x           = left;
    resize_param.start_y           = top;
    resize_param.in_width          = output.stride[0];
    resize_param.in_height         = output.image_height;
    resize_param.out_width         = output.stride[0];
    resize_param.out_height        = output.image_height;
    resize_attr[0].roi_num         = 1;
    resize_attr[0].stretch_fit     = 1;
    resize_attr[0].resize_img_attr = &resize_param;
    resize_attr[0].interpolation   = BMCV_INTER_NEAREST;
    // do crop with resize.
    ret =
        bmcv_image_resize(handle, input_n, resize_attr, &image_in, &image_out);
    if (ret != BM_SUCCESS) {
        goto exit2;
    }
    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
        sys_buf = bm_mem_get_system_addr(output.data[0]);
        bm_image_copy_device_to_host(image_out, &sys_buf);
    }
exit2:
    bm_image_destroy(image_out);
exit1:
    bm_image_destroy(image_in);
exit:
    return ret;
}

