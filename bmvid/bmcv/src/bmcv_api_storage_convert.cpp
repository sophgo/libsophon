#include <vector>
#include <memory>
#ifdef __linux__
  #include <sys/time.h>
#endif
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

#define IS_CS_YUV(a) (a == FORMAT_NV12 || a == FORMAT_NV21 || a == FORMAT_NV16 \
                   || a == FORMAT_NV61 || a == FORMAT_NV24 || a == FORMAT_YUV420P \
                   || a == FORMAT_YUV422P || a == FORMAT_YUV444P)
#define IS_CS_RGB(a) (a == FORMAT_RGB_PACKED || a == FORMAT_RGB_PLANAR \
                   || a == FORMAT_BGR_PACKED || a == FORMAT_BGR_PLANAR \
                   || a == FORMAT_RGBP_SEPARATE || a == FORMAT_BGRP_SEPARATE)

#define IS_4N(a) (a ==DATA_TYPE_EXT_4N_BYTE || a == DATA_TYPE_EXT_4N_BYTE_SIGNED)
#define IS_1N(a) (a ==DATA_TYPE_EXT_1N_BYTE || a == DATA_TYPE_EXT_1N_BYTE_SIGNED)
#define IS_FP32(a) (a == DATA_TYPE_EXT_FLOAT32)

#define IS_RGB(a) (a == FORMAT_RGB_PACKED || a == FORMAT_RGB_PLANAR)
#define IS_PACK(a) (a == FORMAT_RGB_PACKED || a == FORMAT_BGR_PACKED)

#define IS_NV12(a) (a == FORMAT_NV12)
#define IS_NV21(a) (a == FORMAT_NV21)
#define IS_NV16(a) (a == FORMAT_NV16)
#define IS_NV61(a) (a == FORMAT_NV61)
#define IS_NV24(a) (a == FORMAT_NV24)
#define IS_YUV420P(a) (a == FORMAT_YUV420P)
#define IS_YUV422P(a) (a == FORMAT_YUV422P)
#define IS_YUV444P(a) (a == FORMAT_YUV444P)

#define IS_U_FIRST(a) (IS_NV12(a) || IS_NV16(a) || IS_YUV420P(a) || IS_YUV422P(a) || IS_YUV444P(a))


static bm_status_t bmcv_convert_check(
    int                 image_n,
    bm_image*           input,
    bm_image*           output) {
  if (image_n<1 || image_n>4) {
      bmlib_log("BMCV", BMLIB_LOG_ERROR, "expected 1 <= image_n  <= 4 %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
  }
  int width = output[0].width;
  int height = output[0].height;
  if (width > 8192) {
      bmlib_log("BMCV", BMLIB_LOG_ERROR, "expected width <= 8192 %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
  }
  bm_image_format_ext expected_input_image_format, expected_output_image_format;
  bm_image_data_format_ext expected_input_data_format, expected_output_data_format;

  int input_image_num = 1;
  int output_image_num = 1;
  expected_input_image_format = input[0].image_format;
  expected_output_image_format = output[0].image_format;
  expected_input_data_format = input[0].data_type;
  expected_output_data_format = output[0].data_type;
  int stride[3] = {0};

  if ((input[0].data_type == DATA_TYPE_EXT_FP16) ||
      (output[0].data_type == DATA_TYPE_EXT_FP16)||
      (input[0].data_type == DATA_TYPE_EXT_BF16) ||
      (output[0].data_type == DATA_TYPE_EXT_BF16)){
      BMCV_ERR_LOG("data type not support\n");

      return BM_NOT_SUPPORTED;
  }

  if (!IS_4N(expected_input_data_format)) {
    input_image_num = image_n;
  }
  if (!IS_4N(expected_output_data_format)) {
    output_image_num = image_n;
  }
  for (int i = 0; i < input_image_num; i++) {
    if (expected_input_image_format != input[i].image_format \
            || expected_input_data_format != input[i].data_type || width != input[i].width \
            || height != input[i].height) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "expected consistant input image format "
                "and size %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
  }

  for (int i = 0; i < output_image_num; i++) {
    if (expected_output_image_format != output[i].image_format \
            || expected_output_data_format != output[i].data_type || width != output[i].width \
            || height != output[i].height) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "expected consistant output image format and size %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
  }

    if (expected_input_image_format == FORMAT_COMPRESSED \
            || expected_output_image_format == FORMAT_COMPRESSED) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "Not support input output image format "
                "FORMAT_COMPRESSED or output image format FORMAT_GRAY %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    switch(expected_input_image_format)
    {
      case FORMAT_YUV444_PACKED:
      case FORMAT_YVU444_PACKED:
      case FORMAT_YUV422_YUYV:
      case FORMAT_YUV422_YVYU:
      case FORMAT_YUV422_UYVY:
      case FORMAT_YUV422_VYUY:
      case FORMAT_RGBYP_PLANAR:
      case FORMAT_HSV180_PACKED:
      case FORMAT_HSV256_PACKED:
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "Not support input image format, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
      default:
        break;
    }

    switch(expected_output_image_format)
    {
      case FORMAT_YUV444_PACKED:
      case FORMAT_YVU444_PACKED:
      case FORMAT_YUV422_YUYV:
      case FORMAT_YUV422_YVYU:
      case FORMAT_YUV422_UYVY:
      case FORMAT_YUV422_VYUY:
      case FORMAT_RGBYP_PLANAR:
      case FORMAT_HSV180_PACKED:
      case FORMAT_HSV256_PACKED:
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "Not support output image format, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
      default:
        break;
    }

    if (expected_input_data_format == DATA_TYPE_EXT_1N_BYTE_SIGNED \
            || expected_input_data_format == DATA_TYPE_EXT_4N_BYTE_SIGNED) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "not expected signed data format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if (expected_output_data_format == DATA_TYPE_EXT_1N_BYTE_SIGNED \
            || expected_output_data_format == DATA_TYPE_EXT_4N_BYTE_SIGNED) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "not expected signed data format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    bm_image_get_stride(input[0], stride);
    for (int i = 1; i < input_image_num; i++) {
        int stride_[3] = {0};
        bm_image_get_stride(input[i], stride_);
        for (int k = 0; k < bm_image_get_plane_num(input[i]); k++) {
            if (stride[k] != stride_[k]) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "all input should have same stride %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
    }

    bm_image_get_stride(output[0], stride);
    for (int i = 1; i < output_image_num; i++) {
        int stride_[3] = {0};
        bm_image_get_stride(output[i], stride_);
        for (int k = 0; k < bm_image_get_plane_num(output[i]); k++) {
            if (stride[k] != stride_[k]) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR, "all output should have same stride %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
    }
    return BM_SUCCESS;
}


typedef struct ops_format {
  u32 op;
  bm_image_format_ext src_format;
  bm_image_data_format_ext src_data_type;
  bm_image_format_ext dst_format;
  bm_image_data_format_ext dst_data_type;
} ops_format_t;

static void get_op_list(
  bm_image_format_ext src_format, bm_image_data_format_ext src_data_type,
  bm_image_format_ext dst_format, bm_image_data_format_ext dst_data_type,
  std::vector<ops_format_t>& ops) {
    // 0 : rgb internal format transform
    // 1 : rgb data format transform
    // 2 : rgb planar to yuv
    // 3 : yuv to rgb planar
    if (src_format == dst_format && src_data_type == dst_data_type)
        return;
    if (IS_CS_RGB(src_format) && IS_CS_RGB(dst_format)) {
        if (src_format != dst_format) {
            ops.push_back({0, src_format, src_data_type, dst_format, src_data_type});
        }
        if (src_data_type != dst_data_type) {
            if ((IS_4N(src_data_type) && IS_FP32(dst_data_type)) || (IS_4N(dst_data_type) \
                    && IS_FP32(src_data_type))) {
                get_op_list(dst_format, src_data_type, dst_format, DATA_TYPE_EXT_1N_BYTE, ops);
                get_op_list(dst_format, DATA_TYPE_EXT_1N_BYTE, dst_format, dst_data_type, ops);
            } else {
                ops.push_back({1, src_format, src_data_type, dst_format, dst_data_type});
            }
        }
    } else if (IS_CS_RGB(src_format) && IS_CS_YUV(dst_format)) {
        get_op_list(src_format, src_data_type, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, ops);
        ops.push_back({2, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, dst_format, dst_data_type});
    } else if (IS_CS_YUV(src_format) && IS_CS_RGB(dst_format)) {
        if (IS_PACK(dst_format)) {
            if (IS_RGB(dst_format)) {
                ops.push_back({3, src_format, src_data_type, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE});
                get_op_list(FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, dst_format, dst_data_type, ops);
            } else {
                ops.push_back({3, src_format, src_data_type, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE});
                get_op_list(FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, dst_format, dst_data_type, ops);
            }
        } else {
            ops.push_back({3, src_format, src_data_type, dst_format, dst_data_type});
        }
    } else if (IS_CS_YUV(src_format) && IS_CS_YUV(dst_format) && src_format != dst_format) {
        get_op_list(src_format, src_data_type, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, ops);
        get_op_list(FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, dst_format, dst_data_type, ops);
    }
    return;
}


bm_status_t bmcv_image_storage_convert_(
    bm_handle_t      handle,
    int              image_num,
    bm_image*        input_,
    bm_image*        output_,
    csc_type_t       csc_type) {
    if (handle == NULL) {
        bmlib_log("STORAGE_CONVERT", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_status_t ret = bmcv_convert_check(image_num, input_, output_);
    if (ret != BM_SUCCESS)
        return ret;

    int input_num = IS_4N(input_[0].data_type) ? 1 : image_num;
    int output_num = IS_4N(output_[0].data_type) ? 1 : image_num;

#ifndef USING_CMODEL
    bool try_use_vpp = true;
#endif

    for (int i = 0; i < input_num; i++) {
        if (!bm_image_is_attached(input_[i])) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR, "input image not attach device memory %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

#ifndef USING_CMODEL
        bm_device_mem_t mem[3];
        bm_image_get_device_mem(input_[i], mem);
        for (int k = 0; k < bm_image_get_plane_num(input_[i]); k++) {
            if (bm_mem_get_device_addr(mem[k]) < 0x300000000) {  // DDR1_MEM_START_ADDR
                try_use_vpp = false;
            }
        }

        if (input_[i].data_type == DATA_TYPE_EXT_4N_BYTE \
                || input_[i].data_type == DATA_TYPE_EXT_FLOAT32) {
            try_use_vpp = false;
        }

        if (input_[i].image_format == FORMAT_NV16 \
                || input_[i].image_format == FORMAT_YUV422P)
            try_use_vpp = false;
#endif
    }
    #ifdef __linux__
    bool output_alloc_flag[output_num];
    #else
    std::shared_ptr<bool> output_alloc_flag_(new bool[output_num], std::default_delete<bool[]>());
    bool*                 output_alloc_flag = output_alloc_flag_.get();
    #endif
    for (int i = 0; i < output_num; i++) {
        output_alloc_flag[i] = false;
        if (!bm_image_is_attached(output_[i])) {
            if (BM_SUCCESS !=bm_image_alloc_dev_mem(output_[i], BMCV_HEAP_ANY)) {
               BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
            for (int free_idx = 0; free_idx < i; free_idx++) {
                if (output_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output_[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            return BM_ERR_FAILURE;
            }
        }
        output_alloc_flag[i] = true;
#ifndef USING_CMODEL
        if (output_[i].data_type == DATA_TYPE_EXT_4N_BYTE \
                || output_[i].data_type == DATA_TYPE_EXT_FLOAT32) {
            try_use_vpp = false;
        }
        if (output_[i].image_format == FORMAT_NV16)
            try_use_vpp = false;
#endif
    }

#ifndef USING_CMODEL
    if (try_use_vpp) {
        bool success = true;
        bmcv_rect_t rect = {0, 0, input_[0].width, input_[0].height};
        for (int i = 0; i < image_num; i++) {
            if (bmcv_image_vpp_csc_matrix_convert(handle, 1,
                                                  input_[i], output_ + i,
                                                  csc_type, nullptr,
                                                  BMCV_INTER_LINEAR, &rect) != BM_SUCCESS) {
                bmlib_log("BMCV", BMLIB_LOG_INFO, "can't use vpp,  go through tpu path\n");
                success = false;
                break;
            }
        }
        if (success) {
            return BM_SUCCESS;
        }
    }
#endif

    bm_image_format_ext in_fmt = (input_[0].image_format == FORMAT_BGRP_SEPARATE) ? FORMAT_BGR_PLANAR :
                                 ((input_[0].image_format == FORMAT_RGBP_SEPARATE) \
                                 ? FORMAT_RGB_PLANAR : input_[0].image_format);
    bm_image_format_ext out_fmt = (output_[0].image_format == FORMAT_BGRP_SEPARATE) ? FORMAT_BGR_PLANAR :
                                 ((output_[0].image_format == FORMAT_RGBP_SEPARATE) \
                                 ? FORMAT_RGB_PLANAR : output_[0].image_format);
    bool separate_to_planar = (input_[0].image_format == FORMAT_BGRP_SEPARATE) \
                                 || (input_[0].image_format == FORMAT_RGBP_SEPARATE);
    bool planar_to_separate = (output_[0].image_format == FORMAT_BGRP_SEPARATE) \
                                 || (output_[0].image_format == FORMAT_RGBP_SEPARATE);
    bm_image* in_planar;
    if (separate_to_planar) {
        in_planar = new bm_image[input_num];
        for (int i = 0; i < input_num; i++) {
            int stride[4];
            bm_image_get_stride(input_[i], stride);
            bm_image_create(handle, input_[0].height, input_[0].width, in_fmt, \
                    input_[0].data_type, in_planar + i, stride);
            bm_separate_to_planar(handle, input_[i], in_planar[i]);
        }
    } else {
       in_planar = input_;
    }

    if (in_fmt == out_fmt && input_[0].data_type == output_[0].data_type) {
        bm_image* out;
        if (planar_to_separate) {
            out = new bm_image[output_num];
            for (int i = 0; i < output_num; i++) {
                bm_image_create(handle, output_[0].height, output_[0].width, out_fmt, \
                        output_[0].data_type, out + i);
                if (bm_image_alloc_dev_mem(out[i], BMCV_HEAP_ANY) != BM_SUCCESS) {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "alloc intermediate memory failed\n",
                        filename(__FILE__), __func__, __LINE__);
                    for (int free_idx = 0; free_idx < output_num; free_idx++) {
                        if (output_alloc_flag[free_idx]) {
                            bm_device_mem_t dmem;
                            bm_image_get_device_mem(output_[free_idx], &dmem);
                            bm_free_device(handle, dmem);
                        }
                    }
                    for (int free_idx = 0; free_idx < i; free_idx++) {
                        bm_image_destroy(out[i]);
                    }
                    if (separate_to_planar) {
                        for (int i = 0; i < input_num; i++) {
                             bm_image_destroy(in_planar[i]);
                        }
                        delete [] in_planar;
                    }
                    delete [] out;
                    return BM_ERR_FAILURE;
                }
            }
        } else {
            out = output_;
        }
        for (int i = 0; i < input_num; i++) {
            int plane_num = bm_image_get_plane_num(in_planar[i]);
            for (int p = 0; p < plane_num; p++) {
                layout::update_memory_layout(
                    handle,
                    in_planar[i].image_private->data[p],
                    in_planar[i].image_private->memory_layout[p],
                    out[i].image_private->data[p],
                    out[i].image_private->memory_layout[p]);
            }
        }
        if (planar_to_separate) {
            for (int i = 0; i < output_num; i++) {
                bm_planar_to_separate(handle, out[i], output_[i]);
                bm_image_destroy(out[i]);
            }
            delete [] out;
        }
        if (separate_to_planar) {
            for (int i = 0; i < input_num; i++) {
                 bm_image_destroy(in_planar[i]);
            }
            delete [] in_planar;
        }
        return BM_SUCCESS;
    }

    auto input = std::make_shared<image_warpper>(handle, input_num, input_[0].height, \
            input_[0].width, in_fmt, input_[0].data_type);
    auto output = std::make_shared<image_warpper>(handle, output_num, output_[0].height, \
            output_[0].width, out_fmt, output_[0].data_type);

    #ifdef __linux__
    bool input_inner_alloc_flag[input_num];
    #else
    std::shared_ptr<bool> input_inner_alloc_flag_(new bool[input_num], std::default_delete<bool[]>());
    bool*                 input_inner_alloc_flag = input_inner_alloc_flag_.get();
    #endif

    for (int i = 0; i < input_num; i++) {
        int plane_num = bm_image_get_plane_num(in_planar[i]);
        bool in_need_convert = false;
        for (int p = 0; p < plane_num; p++) {
            if ((in_planar[i].image_private->memory_layout[p].pitch_stride !=
                 input->inner[i].image_private->memory_layout[p].pitch_stride) ||
                (in_planar[i].image_private->memory_layout[p].channel_stride !=
                 input->inner[i].image_private->memory_layout[p].channel_stride) ||
                (in_planar[i].image_private->memory_layout[p].batch_stride !=
                 input->inner[i].image_private->memory_layout[p].batch_stride)) {
                in_need_convert = true;
                break;
            }
        }
        input_inner_alloc_flag[i] = false;
        if (in_need_convert) {
            if (bm_image_alloc_dev_mem(input->inner[i], BMCV_HEAP_ANY) != BM_SUCCESS) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "alloc intermediate memory failed\n",
                    filename(__FILE__), __func__, __LINE__);
                for (int free_idx = 0; free_idx < output_num; free_idx++) {
                    if (output_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(output_[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }
                for (int free_idx = 0; free_idx < i; free_idx++) {
                    if (input_inner_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(input->inner[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }
                if (separate_to_planar) {
                    for (int i = 0; i < input_num; i++) {
                        bm_image_destroy(in_planar[i]);
                    }
                    delete [] in_planar;
                }
                return BM_ERR_FAILURE;
            }
            input_inner_alloc_flag[i] = true;
            for (int p = 0; p < plane_num; p++) {
                layout::update_memory_layout(
                    handle,
                    in_planar[i].image_private->data[p],
                    in_planar[i].image_private->memory_layout[p],
                    input->inner[i].image_private->data[p],
                    input->inner[i].image_private->memory_layout[p]);
            }
        } else {
            bm_image_attach(input->inner[i], in_planar[i].image_private->data);
        }
    }

    #ifdef __linux__
    bool output_inner_alloc_flag[input_num];
    bool out_need_convert[output_num];
    #else
    std::shared_ptr<bool> output_inner_alloc_flag_(new bool[input_num], std::default_delete<bool[]>());
    bool*                 output_inner_alloc_flag = output_inner_alloc_flag_.get();
    std::shared_ptr<bool> out_need_convert_(new bool[output_num], std::default_delete<bool[]>());
    bool*                 out_need_convert = out_need_convert_.get();
    #endif
    for (int i = 0; i < output_num; i++) {
        out_need_convert[i] = false;
        int plane_num = bm_image_get_plane_num(output_[i]);
        for (int p = 0; p < plane_num; p++) {
            if ((output_[i].image_private->memory_layout[p].pitch_stride !=
                 output->inner[i].image_private->memory_layout[p].pitch_stride) ||
                (output_[i].image_private->memory_layout[p].channel_stride !=
                 output->inner[i].image_private->memory_layout[p].channel_stride) ||
                (output_[i].image_private->memory_layout[p].batch_stride !=
                 output->inner[i].image_private->memory_layout[p].batch_stride)) {
                out_need_convert[i] = true;
                break;
            }
        }
        output_inner_alloc_flag[i] = false;
        if (out_need_convert[i] || planar_to_separate) {
            if (bm_image_alloc_dev_mem(output->inner[i], BMCV_HEAP_ANY) != BM_SUCCESS) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "alloc intermediate memory failed\n",
                    filename(__FILE__), __func__, __LINE__);
                for (int free_idx = 0; free_idx < output_num; free_idx++) {
                    if (output_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(output_[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }
                for (int free_idx = 0; free_idx < input_num; free_idx++) {
                    if (input_inner_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(input->inner[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }
                for (int free_idx = 0; free_idx < i; free_idx++) {
                    if (output_inner_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(output->inner[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }
                if (separate_to_planar) {
                    for (int i = 0; i < input_num; i++) {
                         bm_image_destroy(in_planar[i]);
                    }
                    delete [] in_planar;
                }
                return BM_ERR_FAILURE;
            }
            output_inner_alloc_flag[i] = true;
        } else {
            bm_image_attach(output->inner[i], output_[i].image_private->data);
        }
    }

    std::vector<ops_format_t> op;
    get_op_list(in_fmt, input_[0].data_type, out_fmt, output_[0].data_type, op);

    bm_device_mem_t input_mem[4][3];
    bm_device_mem_t output_mem[4][3];
    u64 input_addr[4] = {0};
    u64 output_addr[4] = {0};
    u64 reserved0_addr[4] = {0};
    u64 reserved1_addr[4] = {0};
    int width = input_[0].width;
    int height = input_[0].height;
    std::shared_ptr<image_warpper> inner_in, inner_out;

    for (u32 i = 0; i < op.size(); i++) {
        int expect_input_num = IS_4N(op[i].src_data_type) ? 1 : image_num;
        int expect_output_num = IS_4N(op[i].dst_data_type) ? 1 : image_num;
        if (i == 0) {
            for (int k = 0; k < expect_input_num; k++) {
                bm_image_get_device_mem(input->inner[k], input_mem[k]);
                for (int p = 0; p < bm_image_get_plane_num(input->inner[0]); p++) {
                    if (p == 0)
                        input_addr[k] = bm_mem_get_device_addr(input_mem[k][p]);
                    else if (p == 1)
                        reserved0_addr[k] = bm_mem_get_device_addr(input_mem[k][p]);
                    else if (p == 2)
                        reserved1_addr[k] = bm_mem_get_device_addr(input_mem[k][p]);
                }
            }
        } else {
            for (int k = 0; k < expect_input_num; k++) {
                bm_image_get_device_mem(inner_in->inner[k], input_mem[k]);
                for (int p = 0; p < bm_image_get_plane_num(inner_in->inner[k]); p++) {
                    if (p == 0)
                        input_addr[k] = bm_mem_get_device_addr(input_mem[k][p]);
                    else if (p == 1)
                        reserved0_addr[k] = bm_mem_get_device_addr(input_mem[k][p]);
                    else if (p == 2)
                        reserved1_addr[k] = bm_mem_get_device_addr(input_mem[k][p]);
                }
            }
        }

        if (i == op.size() - 1) {
            for (int k = 0; k < expect_output_num; k++) {
                bm_image_get_device_mem(output->inner[k], output_mem[k]);
                for (int p = 0; p < bm_image_get_plane_num(output->inner[0]); p++) {
                    if (p == 0)
                        output_addr[k] = bm_mem_get_device_addr(output_mem[k][p]);
                    else if (p == 1)
                        reserved0_addr[k] = bm_mem_get_device_addr(output_mem[k][p]);
                    else if (p == 2)
                        reserved1_addr[k] = bm_mem_get_device_addr(output_mem[k][p]);
                }
            }
        } else {
            inner_out = std::make_shared<image_warpper>(handle, expect_output_num, height, \
                    width, (bm_image_format_ext)op[i].dst_format, \
                    (bm_image_data_format_ext)op[i].dst_data_type);
            for (int i = 0; i < expect_output_num; i++) {
                if (bm_image_alloc_dev_mem(inner_out->inner[i], BMCV_HEAP_ANY) != BM_SUCCESS) {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "alloc intermediate memory failed\n",
                        filename(__FILE__), __func__, __LINE__);
                if (separate_to_planar) {
                    for (int i = 0; i < input_num; i++) {
                        bm_image_destroy(in_planar[i]);
                    }
                    delete [] in_planar;
                }
                return BM_ERR_FAILURE;
                }
            }
            for (int k = 0; k < expect_output_num; k++) {
                bm_image_get_device_mem(inner_out->inner[k], output_mem[k]);
                for (int p = 0; p < bm_image_get_plane_num(inner_out->inner[k]); p++) {
                    if (p == 0)
                        output_addr[k] = bm_mem_get_device_addr(output_mem[k][p]);
                    else if (p == 1)
                        reserved0_addr[k] = bm_mem_get_device_addr(output_mem[k][p]);
                    else if (p == 2)
                        reserved1_addr[k] = bm_mem_get_device_addr(output_mem[k][p]);
                }
            }
        }
        bm_api_cv_storage_convert_t api = {
            input_addr[0],
            input_addr[1],
            input_addr[2],
            input_addr[3],

            output_addr[0],
            output_addr[1],
            output_addr[2],
            output_addr[3],

            reserved0_addr[0],
            reserved0_addr[1],
            reserved0_addr[2],
            reserved0_addr[3],

            reserved1_addr[0],
            reserved1_addr[1],
            reserved1_addr[2],
            reserved1_addr[3],

            (u32)op[i].src_format,
            (u32)op[i].dst_format,
            (u32)op[i].src_data_type,
            (u32)op[i].dst_data_type,

            image_num,
            height,
            width,
            op[i].op,
            csc_type};
        if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_STORAGE_CONVERT, (uint8_t *)&api, sizeof(api))) {
            BMCV_ERR_LOG("storage_convert send api error\r\n");
            if (separate_to_planar) {
                for (int i = 0; i < input_num; i++) {
                     bm_image_destroy(in_planar[i]);
                }
                delete [] in_planar;
            }
            return BM_ERR_FAILURE;
        }
        if (BM_SUCCESS != bm_sync_api(handle)) {
            BMCV_ERR_LOG("storage_convert sync api error\r\n");
            if (separate_to_planar) {
                for (int i = 0; i < input_num; i++) {
                     bm_image_destroy(in_planar[i]);
                }
                delete [] in_planar;
            }
            return BM_ERR_FAILURE;
        }
        inner_in = inner_out;
    }

    bm_image* out_separate;
    if (planar_to_separate) {
        out_separate = new bm_image[output_num];
        for (int i = 0; i < output_num; i++) {
            bm_image_create(handle, output_[0].height, output_[0].width, output_[0].image_format, \
                    output_[0].data_type, out_separate + i);
            bm_planar_to_separate(handle, output->inner[i], out_separate[i]);
        }
    } else {
       out_separate = output->inner;
    }

    for (int i = 0; i < output_num; i++) {
        if (planar_to_separate || out_need_convert[i]) {
            int plane_num = bm_image_get_plane_num(out_separate[0]);
            for (int p = 0; p < plane_num; p++) {
                layout::update_memory_layout(
                    handle,
                    out_separate[i].image_private->data[p],
                    out_separate[i].image_private->memory_layout[p],
                    output_[i].image_private->data[p],
                    output_[i].image_private->memory_layout[p]);
            }
        }
    }
    if (separate_to_planar) {
        for (int i = 0; i < input_num; i++) {
             bm_image_destroy(in_planar[i]);
        }
        delete [] in_planar;
    }
    if (planar_to_separate) {
        for (int i = 0; i < output_num; i++) {
             bm_image_destroy(out_separate[i]);
        }
        delete [] out_separate;
    }

    return BM_SUCCESS;
}

// in order to support input_num > 4
bm_status_t bmcv_image_storage_convert_with_csctype(
    bm_handle_t      handle,
    int              image_num,
    bm_image*        input_,
    bm_image*        output_,
    csc_type_t       csc_type) {

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid = BM1684X;
    int loop = (image_num + 3) / 4;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        for (int i = 0; i < loop; i++) {
            int num = (i == loop - 1) ? (image_num - (loop - 1) * 4) : 4;
            ret = bmcv_image_storage_convert_(handle, num, input_ + i * 4, output_ + i * 4, csc_type);
            if (ret != BM_SUCCESS) return ret;
        }

        break;
      case BM1684X:
        ret = bm1684x_vpp_storage_convert(handle, image_num, input_, output_, csc_type);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

  return ret;

}

bm_status_t bmcv_image_storage_convert(
  bm_handle_t      handle,
  int              image_num,
  bm_image*        input_,
  bm_image*        output_)
{

  bm_status_t ret = BM_SUCCESS;
  csc_type_t csc_type = CSC_MAX_ENUM;

  ret = bmcv_image_storage_convert_with_csctype(handle, image_num, input_, output_, csc_type);
  return ret;
}
