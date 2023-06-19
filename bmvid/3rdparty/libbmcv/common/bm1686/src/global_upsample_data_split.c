#include "global_layer_data_split.h"

#define ACTIVE_PARTITION_LOCAL_MEMORY_SIZE_RATIO 1 / 4

int nodechip_upsample_GetLineNum_parallel(float percentOneFeatureMem, int input_h) {
  int N_tmp = (int)(percentOneFeatureMem * input_h * 0.98);

  return N_tmp;
}

void nodechip_upsample_parallel_GetMemRequirement(
    int input_c,
    int input_h,
    int input_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int stride_h,
    int stride_w,
    int is_avg_pooling,
    float* inRatio,
    int* oneFeatureMem) {  // one input feature map mem requirement
  UNUSED(input_c);
  UNUSED(kw);
  UNUSED(pad_h);
  UNUSED(pad_w);
  UNUSED(stride_h);
  UNUSED(stride_w);
  UNUSED(is_avg_pooling);
  ASSERT(pad_h == pad_w);
  ASSERT(stride_h == stride_w);
  ASSERT(kw == kh);

  int size             = kh;
  int ifmap_align_size = get_neuron_local_csize(input_h, input_w, OP_32BIT);  // corresponding output feature map mem requirement
  int output_h         = (input_h * size);
  int output_w         = (input_w * size);

  int ofmap_align_size  = get_neuron_local_csize(output_h, output_w, OP_32BIT);  //  mem requirement
  int ifmap_single_size = ifmap_align_size;
  int ofmap_single_size = ofmap_align_size;
  oneFeatureMem[0]      = ofmap_single_size;
  if (oneFeatureMem[0] < ifmap_single_size)
    oneFeatureMem[0] = ifmap_single_size;  // inRatio[0]= (float)ifmap_single_size/(float)oneFeatureMem[0];
  inRatio[0]         = 1.0 / 2.0;
}

// return 0: fail, 1: success
int global_upsample_data_split(
    int input_n,
    int input_c,
    int input_h,
    int input_w,
    int kh,
    int kw,
    int pad_h,
    int pad_w,
    int stride_h,
    int stride_w,
    int is_avg_pooling,
    pooling_secs_info_t* pooling_secs_info) {
  int size            = kh;
  int local_mem_banks = 4;
  int output_h        = (input_h * size);
  int output_w        = (input_w * size);
  //*********** get the single feature map size ******************************/
  int ifmap_single_size = get_neuron_local_csize(input_h, input_w, OP_32BIT);
  int ofmap_single_size = get_neuron_local_csize(output_h, output_w, OP_32BIT);
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
  nodechip_upsample_parallel_GetMemRequirement(
      input_c, input_h, input_w, kh, kw, pad_h, pad_w, stride_h, stride_w, is_avg_pooling, inRatio_1, oneFeatureMem_1);
  int local_mem_inputbanks_1                              = (int)(inRatio_1[0] * LOCAL_MEM_BANKS) / 2;
  if (local_mem_inputbanks_1 == 0) local_mem_inputbanks_1 = 1;

  float percentOneFeatureMem_1 =
      (float)LOCAL_MEM_SIZE / (float)oneFeatureMem_1[0] * (float)local_mem_inputbanks_1 / LOCAL_MEM_BANKS;
  int n_step = 0;
  int h_step = 0;

  // if(percentOneFeatureMem_1 >= (ceiling_func_shift(input_c,NPU_SHIFT))*input_n) {
  if (percentOneFeatureMem_1 >= (ceiling_func_shift(input_n * input_c, NPU_SHIFT))) {
    n_step = (int)(percentOneFeatureMem_1 * ACTIVE_PARTITION_LOCAL_MEMORY_SIZE_RATIO) * NPU_NUM;
    n_step = n_step > 0 ? n_step : NPU_NUM;
    // h_step = output_h * stride_h;
    h_step = input_h;
  } else if (percentOneFeatureMem_1 >= 1) {
    n_step = (int)(percentOneFeatureMem_1)*NPU_NUM;
    // h_step = output_h * stride_h;
    h_step = input_h;
  } else {
    n_step = NPU_NUM;
    h_step = nodechip_upsample_GetLineNum_parallel(percentOneFeatureMem_1, input_h);
  }
  pooling_secs_info->nsecs = n_step;
  pooling_secs_info->hsecs = h_step;
  pooling_secs_info->Ratio = *inRatio_1;
#if DEBUG
  printf(" n_step %d , h_step %d \n", n_step, h_step);
#endif
  if (percentOneFeatureMem < 1.0) {
    int N_lines = (int)(percentOneFeatureMem * input_h * 0.98);
    ASSERT(N_lines > 0);
    return 1;
  } else {
    return 1;
  }
}
