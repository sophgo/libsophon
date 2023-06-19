#include "global_layer_data_split.h"

#define ACTIVE_PARTITION_LOCAL_MEMORY_SIZE_RATIO 1 / 4
#define TRAIN_ACTIVE_PARTITION_LOCAL_MEMORY_SIZE_RATIO 1 / 8 /* Need to be used for optimization later */
#define MAX_C_NUM 4096
static int nodechip_pooling_GetLineNum_parallel(
    float percentOneFeatureMem,
    int input_c,
    int input_h,
    int input_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int pad_h_after,
    int pad_w_after,
    int stride_h,
    int stride_w,
    int is_avg_pooling) {
  UNUSED(input_c);
  UNUSED(input_w);
  UNUSED(kw);
  UNUSED(pad_h);
  UNUSED(pad_w);
  UNUSED(pad_h_after);
  UNUSED(pad_w_after);
  UNUSED(stride_w);
  UNUSED(is_avg_pooling);
  int N_tmp     = (int)(percentOneFeatureMem * input_h * 0.98);
  int output_h  = (N_tmp - kh) / stride_h + 1;
  int N_initial = (output_h - 1) * stride_h + kh;
  ASSERT(N_initial >= kh);

  return N_initial;
}

static void nodechip_pooling_parallel_GetMemRequirement(
    int input_c,
    int input_h,
    int input_w,
    int output_h,
    int output_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int pad_h_after,
    int pad_w_after,
    int stride_h,
    int stride_w,
    int is_avg_pooling,
    float *inRatio,
    int *oneFeatureMem) {  // one input feature map mem requirement
  UNUSED(input_c);
  UNUSED(kh);
  UNUSED(kw);
  UNUSED(pad_h);
  UNUSED(pad_w);
  UNUSED(pad_h_after);
  UNUSED(pad_w_after);
  UNUSED(stride_h);
  UNUSED(stride_w);
  UNUSED(is_avg_pooling);
  int ifmap_align_size  = get_neuron_csize_local(input_h, input_w);  // corresponding output feature map mem requirement
  int ofmap_align_size  = get_neuron_csize_local(output_h, output_w);  //  mem requirement
  int ifmap_single_size = ifmap_align_size;
  int ofmap_single_size = ofmap_align_size;
  oneFeatureMem[0]      = ofmap_single_size;
  if (oneFeatureMem[0] < ifmap_single_size)
    oneFeatureMem[0] = ifmap_single_size;  // inRatio[0]= (float)ifmap_single_size/(float)oneFeatureMem[0];
  inRatio[0]         = 1.0 / 2.0;
}

// return 0: fail, 1: success
int global_pooling_data_split(
    int input_n,
    int input_c,
    int input_h,
    int input_w,
    int output_h,
    int output_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int pad_h_after,
    int pad_w_after,
    int stride_h,
    int stride_w,
    int is_avg_pooling,
    pooling_secs_info_t *pooling_secs_info) {
  if (pad_h > kh || pad_w > kw) return 0;
  int local_mem_banks = 4;
  //*********** get the single feature map size ******************************/
  int ifmap_single_size = get_neuron_csize_local(input_h, input_w);
  int ofmap_single_size = get_neuron_csize_local(output_h, output_w);
  int oneFeatureMem     = 0;
  oneFeatureMem         = ofmap_single_size;
  //********* for forward pooling, ordinary the ofmap size is smaller than ifmap
  if (oneFeatureMem < ifmap_single_size) oneFeatureMem = ifmap_single_size;
  float inRatio                                        = 1.0 / 2.0;
  int local_mem_inputbanks                             = (int)(inRatio * local_mem_banks) / 2;
  if (local_mem_inputbanks == 0) local_mem_inputbanks  = 1;
  float percentOneFeatureMem =
      (float)LOCAL_MEM_SIZE / (float)oneFeatureMem * (float)local_mem_inputbanks / local_mem_banks;

  int oneFeatureMem_1[1] = {0};
  float inRatio_1[1]     = {0.0f};
  nodechip_pooling_parallel_GetMemRequirement(
      input_c, input_h, input_w, output_h, output_w, kh, kw, pad_h, pad_w, pad_h_after, pad_w_after, stride_h, stride_w,
      is_avg_pooling, inRatio_1, oneFeatureMem_1);
  int local_mem_inputbanks_1                              = (int)(inRatio_1[0] * LOCAL_MEM_BANKS) / 2;
  if (local_mem_inputbanks_1 == 0) local_mem_inputbanks_1 = 1;

  float percentOneFeatureMem_1 =
      (float)LOCAL_MEM_SIZE / (float)oneFeatureMem_1[0] * (float)local_mem_inputbanks_1 / LOCAL_MEM_BANKS;
  int n_step = 0;
  int h_step = 0;

  // if(percentOneFeatureMem_1 >= (ceiling_func_shift(input_c,NPU_SHIFT))*input_n) {
  if (percentOneFeatureMem_1 >= (ceiling_func_shift(input_n * input_c, NPU_SHIFT))) {
    n_step = (int)(percentOneFeatureMem_1 * TRAIN_ACTIVE_PARTITION_LOCAL_MEMORY_SIZE_RATIO) * NPU_NUM;
    n_step = n_step > 0 ? n_step : NPU_NUM;
    h_step = output_h * stride_h;
  } else if (percentOneFeatureMem_1 >= 1) {
    n_step = (int)(percentOneFeatureMem_1)*NPU_NUM;
    h_step = output_h * stride_h;
  } else {
    n_step = NPU_NUM;
    h_step = nodechip_pooling_GetLineNum_parallel(
        percentOneFeatureMem_1,
        // index_width,
        input_c, input_h, input_w, kh, kw, pad_h, pad_w, pad_h_after, pad_w_after, stride_h, stride_w, is_avg_pooling);
    h_step = ((h_step - kh) / stride_h + 1) * stride_h;
  }
  pooling_secs_info->nsecs = n_step;
  pooling_secs_info->hsecs = h_step;
  pooling_secs_info->Ratio = *inRatio_1;

  if (percentOneFeatureMem < 1.0) {
    int N_lines        = (int)(percentOneFeatureMem * input_h * 0.98);
    int output_h_lines = (N_lines - kh) / stride_h + 1;
    int N_initial      = (output_h_lines - 1) * stride_h + kh;
    if (N_initial < kh)
      return 0;
    else
      return 1;
  } else {
    return 1;
  }
}

// return 0: fail, 1: success
int global_pooling_train_data_split(
    int input_n,
    int input_c,
    int input_h,
    int input_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int pad_h_after,
    int pad_w_after,
    int stride_h,
    int stride_w,
    int is_avg_pooling,
    pooling_secs_info_t *pooling_secs_info) {
  UNUSED(is_avg_pooling);
  if (pad_h > kh || pad_w > kw) return 0;
  int local_mem_banks = 8;

  int top_pad_h    = pad_h;
  int bottom_pad_h = pad_h_after;
  int left_pad_w   = pad_w;
  int right_pad_w  = pad_w_after;

  int output_h = (input_h + pad_h + pad_h_after - kh) / stride_h + 1;
  int output_w = (input_w + pad_w + pad_w_after - kw) / stride_w + 1;
  // check if have one more output h and w
  if ((output_h * stride_h < input_h + pad_h) && ((input_h + pad_h + pad_h_after - kh) % stride_h != 0)) {
    output_h++;
    bottom_pad_h += (stride_h - (input_h + pad_h + pad_h_after - kh) % stride_h);
  }
  if ((output_w * stride_w < input_w + pad_w) && ((input_w + pad_w + pad_w_after - kw) % stride_w != 0)) {
    output_w++;
    right_pad_w += (stride_w - (input_w + pad_w + pad_w_after - kw) % stride_w);
  }

  int inputh = input_h + top_pad_h + bottom_pad_h;
  int inputw = input_w + left_pad_w + right_pad_w;
  //*********** get the single feature map size ******************************/
  int ifmap_single_size = get_neuron_csize_local(inputh, inputw);
  int ofmap_single_size = get_neuron_csize_local(output_h, output_w);
  int oneFeatureMem     = 0;
  oneFeatureMem         = ofmap_single_size;
  //********* for forward pooling, ordinary the ofmap size is smaller than ifmap
  if (oneFeatureMem < ifmap_single_size) oneFeatureMem = ifmap_single_size;
  float inRatio                                        = 1.0 / 4.0;
  int need_h;
  int local_mem_inputbanks                            = (int)(inRatio * local_mem_banks) / 2;
  if (local_mem_inputbanks == 0) local_mem_inputbanks = 1;
  float percentOneFeatureMem =
      (float)LOCAL_MEM_SIZE / (float)oneFeatureMem * (float)local_mem_inputbanks / local_mem_banks;
  float inputbank_memreq = (float)LOCAL_MEM_SIZE * (float)local_mem_inputbanks / LOCAL_MEM_BANKS;
  int n_step             = 0;
  int h_step             = 0;
  int val                = 0;
  if ((percentOneFeatureMem >= (ceiling_func_shift(input_n * input_c, NPU_SHIFT))) && (input_n * input_c < MAX_C_NUM)) {
    n_step = ceiling_func_shift(input_n * input_c, NPU_SHIFT) * NPU_NUM;
    h_step = 0; /* No need to slice h */
    val    = 1;
  } else if (percentOneFeatureMem >= 1) {
    if (input_n * input_c < MAX_C_NUM)
      n_step = (int)(percentOneFeatureMem)*NPU_NUM;
    else if ((percentOneFeatureMem >= (ceiling_func_shift(input_n * input_c, NPU_SHIFT))))
      n_step = MAX_C_NUM - 1;
    else
      n_step = bm_min((int)(percentOneFeatureMem)*NPU_NUM, MAX_C_NUM - 1);
    h_step   = 0; /* No need to slice h */
    val      = 2;
  } else {
    n_step = NPU_NUM;
    h_step = output_h;
    while (h_step > 0) {
      need_h = (h_step - 1) * stride_h + kh;
      if (get_neuron_csize_local(need_h, inputw) > inputbank_memreq)
        h_step--;
      else
        break;
    }
    if (h_step == 0) {
      printf("Not support through slice H!\n");
      return 0;
    }
    val = 3;
  }
  pooling_secs_info->nsecs = n_step;
  pooling_secs_info->hsecs = h_step;
  pooling_secs_info->Ratio = inRatio;

  return val;
}

/* Return 0: fail 1: no slice 2: slice n(c) 3: slice h*/
int global_pooling_bw_data_split(
    int input_n,
    int input_c,
    int input_h,
    int input_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int pad_h_after,
    int pad_w_after,
    int stride_h,
    int stride_w,
    int is_avg_pooling,
    pooling_secs_info_t *pooling_secs_info) {
  if (pad_h > kh || pad_w > kw) return 0;
  int output_h = (input_h + 2 * pad_h - kh) / stride_h + 1;
  int output_w = (input_w + 2 * pad_w - kw) / stride_w + 1;

  int top_pad_h    = pad_h;
  int bottom_pad_h = pad_h_after;
  int left_pad_w   = pad_w;
  int right_pad_w  = pad_w_after;

  if (!is_avg_pooling) {
    if ((output_h * stride_h < input_h + pad_h) && ((input_h + pad_h + pad_h_after - kh) % stride_h != 0)) {
      output_h++;
      bottom_pad_h += (stride_h - (input_h + pad_h + pad_h_after - kh) % stride_h);
    }
    if ((output_w * stride_w < input_w + pad_w) && ((input_w + pad_w + pad_w_after - kw) % stride_w != 0)) {
      output_w++;
      right_pad_w += (stride_w - (input_w + pad_w + pad_w_after - kw) % stride_w);
    }
  }

  int inputh = input_h + top_pad_h + bottom_pad_h;
  int inputw = input_w + left_pad_w + right_pad_w;

  int ifmap_single_size = get_neuron_csize_local(inputh, inputw);
  int ofmap_single_size = get_neuron_csize_local(output_h, output_w);
  int oneFeatureMem     = 0;
  oneFeatureMem         = ofmap_single_size;
  /* or forward pooling, ordinary the ofmap size is smaller than ifmap*/
  if (oneFeatureMem < ifmap_single_size) oneFeatureMem = ifmap_single_size;

  float inRatio      = 1.0 / 4.0;
  int pipe_stage_num = 2;
  int need_h;
  /* use 0.25 as pooling bw need 4 buffer, input_diff(result) output_diff, index_buffer, index_temp_buffer */
  int local_mem_inputbanks                            = (int)(inRatio * LOCAL_MEM_BANKS) / pipe_stage_num;
  if (local_mem_inputbanks == 0) local_mem_inputbanks = 1;
  float percentOneFeatureMem =
      (float)LOCAL_MEM_SIZE / (float)oneFeatureMem * (float)local_mem_inputbanks / LOCAL_MEM_BANKS;
  float inputbank_memreq = (float)LOCAL_MEM_SIZE * (float)local_mem_inputbanks / LOCAL_MEM_BANKS;

  int n_step = 0;
  int h_step = 0;
  int val    = 0;
  if ((percentOneFeatureMem >= (ceiling_func_shift(input_n * input_c, NPU_SHIFT))) && (input_n * input_c < MAX_C_NUM)) {
    n_step = ceiling_func_shift(input_n * input_c, NPU_SHIFT) * NPU_NUM;
    h_step = 0; /* No need to slice h */
    val    = 1;
  } else if (percentOneFeatureMem >= 1) {
    if (input_n * input_c < MAX_C_NUM)
      n_step = (int)(percentOneFeatureMem)*NPU_NUM;
    else if ((percentOneFeatureMem >= (ceiling_func_shift(input_n * input_c, NPU_SHIFT))))
      n_step = MAX_C_NUM - 1;
    else
      n_step = bm_min((MAX_C_NUM - 1), (int)(percentOneFeatureMem)*NPU_NUM);
    h_step   = 0; /* No need to slice h */
    val      = 2;
  } else { /* slice h */
    n_step = NPU_NUM;
    h_step = inputh;
    while (h_step > 0) {
      need_h = h_step + 2 * (kh - 1);
      if (get_neuron_csize_local(need_h, inputw) > inputbank_memreq)
        h_step--;
      else
        break;
    }
    if (h_step == 0) {
      printf("Not support through slice H!\n");
      return 0;
      // ASSERT(0);
    }
    val = 3;
  }

  pooling_secs_info->nsecs = n_step;
  pooling_secs_info->hsecs = h_step;
  pooling_secs_info->Ratio = inRatio;

  return val;
}
