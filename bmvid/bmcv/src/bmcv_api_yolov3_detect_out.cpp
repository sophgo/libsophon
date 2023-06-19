#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#define SG_API_ID_YOLOV3_DETECT_OUT 59

#define DEVICE_MEM_DEL_INPUT(handle, raw_mem, new_mem)\
    do{\
      if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){\
        bm_free_device(handle, new_mem); \
      }\
    } while(0)

#define DEVICE_MEM_DEL_OUTPUT(handle, raw_mem, new_mem)\
    do{\
      if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){\
        BM_CHECK_RET(bm_sync_api(handle)); \
        BM_CHECK_RET(bm_memcpy_d2s(handle, bm_mem_get_system_addr(raw_mem), new_mem)); \
        bm_free_device(handle, new_mem); \
      }\
    } while(0)

#define DEVICE_MEM_NEW_INPUT(handle, src, len, dst)\
    DEVICE_MEM_ASSIGN_OR_COPY(handle, src, true, len, dst)

#define DEVICE_MEM_NEW_OUTPUT(handle, src, len, dst)\
    DEVICE_MEM_ASSIGN_OR_COPY(handle, src, false, len, dst)

#define DEVICE_MEM_ASSIGN_OR_COPY(handle, raw_mem, need_copy, len, new_mem)\
  do{\
      if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){\
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff_byte(\
            handle, &new_mem, raw_mem, need_copy, \
            (len))); \
      }else{\
          new_mem = raw_mem; \
      }\
    }while(0)

bm_status_t bmcv_nms_yolov3_bm1684(
    bm_handle_t handle, int input_num, bm_device_mem_t bottom[3],
    int batch_num, int hw_shape[3][2], int num_classes,
    int num_boxes, int mask_group_size, float nms_threshold, float confidence_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    bm_device_mem_t output, int yolo_flag, int len_per_batch) {
  if (handle == NULL)
    return BM_ERR_FAILURE;

  if (input_num > 3)
    return BM_ERR_FAILURE;
  bm_device_mem_t b_mem[3];
  bm_device_mem_t top_mem;
  for (int i = 0; i < 3; ++i) {
    b_mem[i] = bm_mem_from_device(0, 4);
  }

  if (yolo_flag == 0){
    for (int i = 0; i < input_num; ++i) {
      if (bm_mem_get_type(bottom[i]) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron(
            handle, &b_mem[i], bottom[i], true, batch_num, (num_classes + 5) * num_boxes,
            hw_shape[i][0], hw_shape[i][1]));
      } else {
        b_mem[i] = bottom[i];
      }
    }
  } else {
    for (int i = 0; i < input_num; ++i) {
      if (bm_mem_get_type(bottom[i]) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron(
            handle, &b_mem[i], bottom[i], true, batch_num, 1,
            1, len_per_batch));
      } else {
        b_mem[i] = bottom[i];
      }
    }
  }
  int roi_alloc_size = batch_num * BMCV_YOLOV3_DETECT_OUT_MAX_NUM;
  if (keep_top_k > 0)
    roi_alloc_size = batch_num * keep_top_k;
  else if (keep_top_k == 0)
    roi_alloc_size = batch_num * 1;
  if (bm_mem_get_type(output) == BM_MEM_TYPE_SYSTEM) {
    BM_CHECK_RET(bm_mem_convert_system_to_device_neuron(
        handle, &top_mem, output, false, 1, 1, roi_alloc_size, 7));
  } else {
    top_mem = output;
  }

  bm_api_yolov3_detect_out_t api = {
    bm_mem_get_device_addr(b_mem[0]),
    bm_mem_get_device_addr(b_mem[1]),
    bm_mem_get_device_addr(b_mem[2]),
    bm_mem_get_device_addr(top_mem),
    input_num,
    batch_num,
    {0},
    num_classes,
    num_boxes,
    mask_group_size,
    keep_top_k,
    nms_threshold,
    confidence_threshold,
    {0.0f},
    {0.0f},
    {0.0f},
    yolo_flag,
    len_per_batch};

  memcpy(api.hw_shape, hw_shape, sizeof(int) * 2 * 3);
  memcpy(api.bias, bias, sizeof(float) * 18);
  memcpy(api.anchor_scale, anchor_scale, sizeof(float) * 3);
  memcpy(api.mask, mask, sizeof(float) * 9);

  // send API to backend
  bm_send_api(handle, BM_API_ID_YOLOV3_DETECT_OUT, (u8 *)&api, sizeof(api));

  DEVICE_MEM_DEL_OUTPUT(handle, output, top_mem);
  for (int i = 0; i < input_num; ++i) {
    DEVICE_MEM_DEL_INPUT(handle, bottom[i], b_mem[i]);
  }

  return BM_SUCCESS;
}

bm_status_t bmcv_nms_yolov7(
    bm_handle_t handle, int input_num, bm_device_mem_t bottom[3],
    int batch_num, int hw_shape[3][2], int num_classes,
    int num_boxes, int mask_group_size, float nms_threshold, float confidence_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    bm_device_mem_t output, int yolo_flag, int len_per_batch,
    int scale, int *orig_image_shape, int model_h, int model_w) {
  if (handle == NULL)
    return BM_ERR_FAILURE;
  if (input_num > 3)
    return BM_ERR_FAILURE;

  bm_device_mem_t b_mem[3];
  bm_device_mem_t top_mem;
  int roi_alloc_size = 1;
  if (keep_top_k > 0)
    roi_alloc_size = batch_num * keep_top_k;
  else if (keep_top_k == 0)
    roi_alloc_size = batch_num * 1;
  if (yolo_flag != 1){
    for (int i = 0; i < input_num; ++i) {
        DEVICE_MEM_NEW_INPUT(handle, bottom[i],
                          sizeof(float) * batch_num * (num_classes + 5) * num_boxes
                          * hw_shape[i][0] * hw_shape[i][1], b_mem[i]);
    }
  } else {
    for (int i = 0; i < input_num; ++i) {
        DEVICE_MEM_NEW_INPUT(handle, bottom[i],
                          sizeof(float) * batch_num * 1 * 1 * len_per_batch, b_mem[i]);
    }
  }
  DEVICE_MEM_NEW_OUTPUT(handle, output, sizeof(float) * roi_alloc_size * 7,
                        top_mem);

  sg_api_yolo_detect_out_1684x_t api = {
    bm_mem_get_device_addr(b_mem[0]),
    bm_mem_get_device_addr(b_mem[1]),
    bm_mem_get_device_addr(b_mem[2]),
    bm_mem_get_device_addr(top_mem),
    0, //for to support large boxes, need to alloc ddr,then transfer to nodechip
    input_num,
    batch_num,
    {0},
    num_classes,
    num_boxes,
    mask_group_size,
    keep_top_k,
    nms_threshold,
    confidence_threshold,
    {0.0f},
    {0.0f},
    {0.0f},
    yolo_flag,
    len_per_batch,
    scale,
    {0},
    model_h,
    model_w};

  memcpy(api.hw_shape, hw_shape, sizeof(int) * 2 * 3);
  memcpy(api.bias, bias, sizeof(float) * 18);
  memcpy(api.anchor_scale, anchor_scale, sizeof(float) * 3);
  memcpy(api.mask, mask, sizeof(float) * 9);
  memcpy(api.orig_image_shape, orig_image_shape, sizeof(float) * 2 * batch_num);
  // send API to backend
  // BM_CHECK_RET(bm_send_api(handle, SG_API_ID_YOLOV3_DETECT_OUT, (u8 *)&api, sizeof(api)));
  BM_CHECK_RET(bm_kernel_main_launch(handle, SG_API_ID_YOLOV3_DETECT_OUT, (u8 * )&api, sizeof(api)));

  DEVICE_MEM_DEL_OUTPUT(handle, output, top_mem);
  for (int i = 0; i < input_num; ++i) {
    DEVICE_MEM_DEL_INPUT(handle, bottom[i], b_mem[i]);
  }

  return BM_SUCCESS;
}

bm_status_t bmcv_nms_yolov3_bm1684X(
    bm_handle_t handle, int input_num, bm_device_mem_t bottom[3],
    int batch_num, int hw_shape[3][2], int num_classes,
    int num_boxes, int mask_group_size, float nms_threshold, float confidence_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    bm_device_mem_t output, int yolo_flag, int len_per_batch) {
  if (handle == NULL)
    return BM_ERR_FAILURE;

  if (input_num > 3)
    return BM_ERR_FAILURE;

  bm_device_mem_t b_mem[3];
  bm_device_mem_t top_mem;
  for (int i = 0; i < 3; ++i) {
    b_mem[i] = bm_mem_from_device(0, 4);
  }

  if (yolo_flag == 0){
    for (int i = 0; i < input_num; ++i) {
      if (bm_mem_get_type(bottom[i]) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron(
            handle, &b_mem[i], bottom[i], true, batch_num, (num_classes + 5) * num_boxes,
            hw_shape[i][0], hw_shape[i][1]));
      } else {
        b_mem[i] = bottom[i];
      }
    }
  } else {
    for (int i = 0; i < input_num; ++i) {
      if (bm_mem_get_type(bottom[i]) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron(
            handle, &b_mem[i], bottom[i], true, batch_num, 1,
            1, len_per_batch));
      } else {
        b_mem[i] = bottom[i];
      }
    }
  }
  int roi_alloc_size = batch_num * BMCV_YOLOV3_DETECT_OUT_MAX_NUM;
  if (keep_top_k > 0)
    roi_alloc_size = batch_num * keep_top_k;
  else if (keep_top_k == 0)
    roi_alloc_size = batch_num * 1;
  if (bm_mem_get_type(output) == BM_MEM_TYPE_SYSTEM) {
    BM_CHECK_RET(bm_mem_convert_system_to_device_neuron(
        handle, &top_mem, output, false, 1, 1, roi_alloc_size, 7));
  } else {
    top_mem = output;
  }

  sg_api_yolo_detect_out_1684x_t api = {
    bm_mem_get_device_addr(b_mem[0]),
    bm_mem_get_device_addr(b_mem[1]),
    bm_mem_get_device_addr(b_mem[2]),
    bm_mem_get_device_addr(top_mem),
    0,
    input_num,
    batch_num,
    {0},
    num_classes,
    num_boxes,
    mask_group_size,
    keep_top_k,
    nms_threshold,
    confidence_threshold,
    {0.0f},
    {0.0f},
    {0.0f},
    yolo_flag,
    len_per_batch,
    0, {0}, 0, 0};

  memcpy(api.hw_shape, hw_shape, sizeof(int) * 2 * 3);
  memcpy(api.bias, bias, sizeof(float) * 18);
  memcpy(api.anchor_scale, anchor_scale, sizeof(float) * 3);
  memcpy(api.mask, mask, sizeof(float) * 9);

  // send API to backend
  BM_CHECK_RET(bm_kernel_main_launch(handle, SG_API_ID_YOLOV3_DETECT_OUT, (u8 * )&api, sizeof(api)));
  // BM_CHECK_RET(bm_send_api(handle, SG_API_ID_YOLOV3_DETECT_OUT, (u8 *)&api, sizeof(api)));

  DEVICE_MEM_DEL_OUTPUT(handle, output, top_mem);
  for (int i = 0; i < input_num; ++i) {
    DEVICE_MEM_DEL_INPUT(handle, bottom[i], b_mem[i]);
  }

  return BM_SUCCESS;
}

bm_status_t bmcv_nms_yolo(
    bm_handle_t handle, int input_num, bm_device_mem_t bottom[3],
    int batch_num, int hw_shape[3][2], int num_classes,
    int num_boxes, int mask_group_size, float nms_threshold, float confidence_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    bm_device_mem_t output, int yolo_flag, int len_per_batch, void *ext) {
  if (handle == NULL)
    return BM_ERR_FAILURE;
  if (input_num > 3)
    return BM_ERR_FAILURE;
  // UNUSED(ext);

  if (yolo_flag == 0 || yolo_flag == 1){
    // yolov3 or yolov5
    return bmcv_nms_yolov3(
            handle, input_num, bottom,
            batch_num, hw_shape, num_classes, num_boxes,
            mask_group_size, nms_threshold, confidence_threshold,
            keep_top_k, bias, anchor_scale, mask,
            output, yolo_flag, len_per_batch);
  }else if (yolo_flag == 2){
    yolov7_info_t* temp = (yolov7_info_t*)ext;
    // yolov7
    return bmcv_nms_yolov7(
            handle, input_num, bottom,
            batch_num, hw_shape, num_classes, num_boxes,
            mask_group_size, nms_threshold, confidence_threshold,
            keep_top_k, bias, anchor_scale, mask,
            output, yolo_flag, len_per_batch,
            temp->scale, temp->orig_image_shape, temp->model_h, temp->model_w);
  }else{
    printf("Not supported !\n");
    return BM_NOT_SUPPORTED;
  }
}

bm_status_t bmcv_nms_yolov3(
    bm_handle_t handle, int input_num, bm_device_mem_t bottom[3],
    int batch_num, int hw_shape[3][2], int num_classes,
    int num_boxes, int mask_group_size, float nms_threshold, float confidence_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    bm_device_mem_t output, int yolo_flag, int len_per_batch) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {
      case 0x1684:
        ret = bmcv_nms_yolov3_bm1684(handle, input_num, bottom, batch_num,  hw_shape,  num_classes,
                                     num_boxes,  mask_group_size,  nms_threshold,  confidence_threshold,
                                     keep_top_k,  bias,  anchor_scale,  mask,
                                     output,  yolo_flag, len_per_batch);
        break;

      case BM1684X:
        ret = bmcv_nms_yolov3_bm1684X(handle, input_num, bottom, batch_num,  hw_shape,  num_classes,
                                     num_boxes,  mask_group_size,  nms_threshold,  confidence_threshold,
                                     keep_top_k,  bias,  anchor_scale,  mask,
                                     output,  yolo_flag, len_per_batch);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}