#include "global_layer_data_split.h"
#include "common.h"

#define max(a, b) (((a) > (b)) ? (a) : (b))
// return 0: fail 1: success
int global_conv_data_split(
    int input_n,
    int input_c,
    int input_h,
    int input_w,
    int output_c,
    int output_h,
    int output_w,
    int groups,
    int kh,
    int kw,
    int dh,
    int dw,
    int stride_h,
    int stride_w,
    int using_bias,
    conv_secs_info_t* p_secs_info) {
  UNUSED(dw);
  UNUSED(stride_w);
  int kh_ext     = dh * (kh - 1) + 1;
  int ic         = input_c / groups;
  int oc         = output_c / groups;
  int ic_per_NPU = ceiling_func_shift(ic, NPU_SHIFT);
  int oc_per_NPU = ceiling_func_shift(oc, NPU_SHIFT);

  int bias_tensor_size;
  bias_tensor_size = using_bias ? oc_per_NPU * FLOAT_SIZE : 0;

  int bank_size = LOCAL_MEM_SIZE / LOCAL_MEM_BANKS;
  int weight_tensor_size;
  weight_tensor_size = ic * oc_per_NPU * kh * kw * FLOAT_SIZE;

  int if_csize_local          = get_neuron_local_csize(input_h, input_w, OP_32BIT);
  int of_csize_local          = get_neuron_local_csize(output_h, output_w, OP_32BIT);
  int ifmap_total_tensor_size = input_n * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = input_n * oc_per_NPU * of_csize_local;
  int weight_capacity         = get_neuron_align_addr(weight_tensor_size * 2 + bias_tensor_size * 2, OP_32BIT);
  ifmap_total_tensor_size *= 2;
  int ofmap_offset = ALIGN(weight_capacity + ifmap_total_tensor_size, bank_size);
  ofmap_total_tensor_size *= 2;
  int totalneed_local_size = ofmap_offset + ofmap_total_tensor_size;

  int nsecs = 1, ocsecs = 1, icsecs = 1, hsecs = 1;
  int nslice = input_n, ocslice_per_NPU = oc_per_NPU, icslice = ic;
  if (totalneed_local_size > LOCAL_MEM_SIZE) {
    // if weight_capacity > 2 * bank_size then split oc and ic
    if (weight_capacity > (bank_size << 1)) {
      icsecs         = weight_capacity / bank_size + 1;
      int max_icsecs = ic_per_NPU;
      if (icsecs > max_icsecs) {
        icsecs = max_icsecs;
      }
      icslice         = (ic + icsecs - 1) / icsecs;
      weight_capacity = icslice * oc_per_NPU * kh * kw * FLOAT_SIZE;
      weight_capacity = get_neuron_align_addr(weight_capacity + 2 * bias_tensor_size, OP_32BIT);
      int max_ocsecs  = oc_per_NPU;
      while ((weight_capacity > bank_size) || (ocslice_per_NPU * output_w > bank_size)) {
        if (ocsecs == 1) {
          ocsecs = (weight_capacity / bank_size + 1) > (oc_per_NPU * output_w / bank_size + 1)
                       ? (weight_capacity / bank_size + 1)
                       : (oc_per_NPU * output_w / bank_size + 1);
        }
        if (ocsecs > max_ocsecs) {
          ocsecs = max_ocsecs;
          break;
        } else {
          ocsecs++;
        }
        ocslice_per_NPU = (oc_per_NPU + ocsecs - 1) / ocsecs;
        weight_capacity = icslice * ocslice_per_NPU * kh * kw * FLOAT_SIZE;
        weight_capacity = ALIGN(weight_capacity + 2 * bias_tensor_size, bank_size);
      }
    }
    ocslice_per_NPU        = (oc_per_NPU + ocsecs - 1) / ocsecs;
    ic_per_NPU             = ceiling_func_shift(icslice, NPU_SHIFT);
    weight_tensor_size     = icslice * ocslice_per_NPU * kh * kw * FLOAT_SIZE;
    weight_capacity        = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
    int bottom_local_start = weight_capacity;
    if (bottom_local_start >= LOCAL_MEM_SIZE) return 0;
    ifmap_total_tensor_size = input_n * ic_per_NPU * if_csize_local;
    ofmap_total_tensor_size = input_n * ocslice_per_NPU * of_csize_local;
    int bottom_local_end    = ALIGN(bottom_local_start + 2 * ifmap_total_tensor_size, bank_size);
    int local_mem_end       = bottom_local_end + 2 * ofmap_total_tensor_size;
    if (local_mem_end > LOCAL_MEM_SIZE) {
      if (input_n > 1) {
        int bottom_size_per_n = ic_per_NPU * if_csize_local;
        int top_size_per_n    = ocslice_per_NPU * of_csize_local;
        local_mem_end         = ALIGN(bottom_local_start + 2 * bottom_size_per_n, bank_size) + 2 * top_size_per_n;
        if (local_mem_end > LOCAL_MEM_SIZE) {
          nsecs            = input_n;
          hsecs            = (2 * (bottom_size_per_n + top_size_per_n)) / (LOCAL_MEM_SIZE - bottom_local_start);
          hsecs            = (hsecs == 0) ? 1 : hsecs;
          int oh_slice     = (output_h + hsecs - 1) / hsecs;
          int ih_slice     = oh_slice * stride_h + kh_ext;
          int bottom_size  = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_32BIT);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_32BIT) + bottom_local_end;
          while (local_mem_end > LOCAL_MEM_SIZE) {
            hsecs++;
            oh_slice         = (output_h + hsecs - 1) / hsecs;
            ih_slice         = oh_slice * stride_h + kh_ext;
            bottom_size      = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_32BIT);
            bottom_local_end = ALIGN(bottom_local_start + bottom_size * 2, bank_size);
            local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_32BIT);
          }
        } else {
          nsecs = bm_min(
              2, 2 * (ifmap_total_tensor_size + ofmap_total_tensor_size) / (LOCAL_MEM_SIZE - bottom_local_start));
          if (nsecs == 0) nsecs = 1;
          nslice                = (input_n + nsecs - 1) / nsecs;
          local_mem_end =
              ALIGN(bottom_local_start + 2 * bottom_size_per_n * nslice, bank_size) + 2 * top_size_per_n * nslice;
          while (local_mem_end > LOCAL_MEM_SIZE) {
            nsecs++;
            nslice = (input_n + nsecs - 1) / nsecs;
            local_mem_end =
                ALIGN(bottom_local_start + 2 * bottom_size_per_n * nslice, bank_size) + 2 * top_size_per_n * nslice;
          }
        }
      } else {
        hsecs = (2 * (ifmap_total_tensor_size + ofmap_total_tensor_size)) / (LOCAL_MEM_SIZE - bottom_local_start);
        hsecs = (hsecs == 0) ? 1 : hsecs;
        int oh_slice     = (output_h + hsecs - 1) / hsecs;
        int ih_slice     = oh_slice * stride_h + kh_ext;
        int bottom_size  = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_32BIT);
        bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
        local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_32BIT);
        while (local_mem_end > LOCAL_MEM_SIZE) {
          hsecs++;
          oh_slice         = (output_h + hsecs - 1) / hsecs;
          ih_slice         = oh_slice * stride_h + kh_ext;
          bottom_size      = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_32BIT);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_32BIT);
        }
      }
    }
  }

  p_secs_info->nsecs  = nsecs;
  p_secs_info->hsecs  = hsecs;
  p_secs_info->icsecs = icsecs;
  p_secs_info->ocsecs = ocsecs;
  return 1;
}
int global_conv_data_split_fix8b(
    int input_n,
    int input_c,
    int input_h,
    int input_w,
    int output_c,
    int output_h,
    int output_w,
    int groups,
    int kh,
    int kw,
    int dh,
    int dw,
    int stride_h,
    int stride_w,
    int using_bias,
    int ins_h,
    int ins_w,
    conv_secs_info_t* p_secs_info) {
  UNUSED(dw);
  UNUSED(stride_w);
  UNUSED(ins_w);
  int kh_ext     = dh * (kh - 1) + 1;
  int ic         = input_c / groups;
  int oc         = output_c / groups;
  int ic_per_NPU = ceiling_func_shift(ic, NPU_SHIFT);
  int oc_per_NPU = ceiling_func_shift(oc, NPU_SHIFT);

  int bias_tensor_size;
  bias_tensor_size            = using_bias ? oc_per_NPU * 2 : 0;
  int bank_size               = LOCAL_MEM_SIZE / LOCAL_MEM_BANKS;
  int weight_tensor_size      = ic * oc_per_NPU * kh * kw;
  int if_csize_local          = get_neuron_local_csize(input_h, input_w, OP_8BIT);
  int of_csize_local          = get_neuron_local_csize(output_h, output_w, OP_8BIT);
  int ifmap_total_tensor_size = input_n * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = input_n * oc_per_NPU * of_csize_local;
  int weight_capacity         = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
  ifmap_total_tensor_size     = 2 * ALIGN(ifmap_total_tensor_size, bank_size);
  int ofmap_offset            = weight_capacity + ifmap_total_tensor_size;
  ofmap_total_tensor_size     = 2 * ALIGN(ofmap_total_tensor_size, bank_size);
  int totalneed_local_size    = ofmap_offset + ofmap_total_tensor_size;

  int nsecs = 1, ocsecs = 1, icsecs = 1, hsecs = 1;
  int nslice = input_n, ocslice_per_NPU = oc_per_NPU, icslice = ic;
  int ocslice = oc;//int min_ocsecs = 1;
  while ((1<<12) <= ocslice)
  {
     ocsecs++;
     ocslice = (oc + ocsecs -1)/ocsecs;
     //min_ocsecs = ocsecs;
  }
  if (totalneed_local_size > LOCAL_MEM_SIZE) {
    // if weight_capacity > 2 * bank_size then split oc and ic
    if (weight_capacity > (bank_size << 1)) {
      int max_ocsecs = oc_per_NPU;
      while ((weight_capacity > bank_size) || (ocslice_per_NPU * output_w > bank_size)) {
        if (ocsecs == 1) {
          ocsecs = (weight_capacity / bank_size + 1) > (oc_per_NPU * output_w / bank_size + 1)
                       ? (weight_capacity / bank_size + 1)
                       : (oc_per_NPU * output_w / bank_size + 1);
        }
        /*ASSERT_FS(ocsecs >= min_ocsecs); otherwise it will cause contradiction, because ocsecs woulde be re-assigned,
                                     only when ocsecs == 1, which means min_ocsecs ==1*/

        if (ocsecs > max_ocsecs) {
          ocsecs = max_ocsecs;
          break; 
        } else {
          ocsecs++;
        }
        ocslice_per_NPU = (oc_per_NPU + ocsecs - 1) / ocsecs;
        weight_capacity = icslice * ocslice_per_NPU * kh * kw;
        weight_capacity = ALIGN(weight_capacity + 2 * bias_tensor_size, bank_size);
      }
    }
  }
  ocslice_per_NPU        = (oc_per_NPU + ocsecs - 1) / ocsecs;
  ic_per_NPU             = ceiling_func_shift(icslice, NPU_SHIFT);
  weight_tensor_size     = icslice * ocslice_per_NPU * kh * kw;
  weight_capacity        = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
  int bottom_local_start = weight_capacity;
  if (bottom_local_start >= LOCAL_MEM_SIZE) return 0;
  ifmap_total_tensor_size = input_n * ic_per_NPU * if_csize_local;
  ofmap_total_tensor_size = input_n * ocslice_per_NPU * of_csize_local;
  int bottom_local_end_A  = ALIGN(bottom_local_start + ifmap_total_tensor_size, bank_size);
  int bottom_local_end_B  = ALIGN(bottom_local_end_A + ifmap_total_tensor_size, bank_size);
  int local_mem_end_A     = ALIGN(bottom_local_end_B + ofmap_total_tensor_size, bank_size);
  int local_men_end_B     = ALIGN(local_mem_end_A + ofmap_total_tensor_size, bank_size);
  if (local_men_end_B > LOCAL_MEM_SIZE) {
    if (input_n > 1) {
      int bottom_size_per_n = ic_per_NPU * if_csize_local;
      int top_size_per_n    = ocslice_per_NPU * of_csize_local;

      bottom_local_end_A = ALIGN(bottom_local_start + bottom_size_per_n, bank_size);
      bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size_per_n, bank_size);
      local_mem_end_A    = ALIGN(bottom_local_end_B + top_size_per_n, bank_size);
      local_men_end_B    = ALIGN(local_mem_end_A + top_size_per_n, bank_size);
      // split n and h
      if (local_men_end_B > LOCAL_MEM_SIZE) {
        nsecs           = input_n;
        hsecs           = (2 * (bottom_size_per_n + top_size_per_n)) / (LOCAL_MEM_SIZE - bottom_local_start);
        hsecs           = (hsecs == 0) ? 1 : hsecs;
        int oh_slice    = (output_h + hsecs - 1) / hsecs;
        int ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
        int bottom_size = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_8BIT);
        int top_size    = ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_8BIT);

        bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
        bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
        local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
        local_men_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        while (local_men_end_B > LOCAL_MEM_SIZE) {
          hsecs++;
          oh_slice    = (output_h + hsecs - 1) / hsecs;
          ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
          bottom_size = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_8BIT);
          top_size    = ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_8BIT);

          bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
          bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
          local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
          local_men_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        }
      } else {  // only split n
        nsecs =
            bm_min(2, 2 * (ifmap_total_tensor_size + ofmap_total_tensor_size) / (LOCAL_MEM_SIZE - bottom_local_start));
        if (nsecs == 0) nsecs = 1;
        nslice                = (input_n + nsecs - 1) / nsecs;
        int bottom_size       = bottom_size_per_n * nslice;
        int top_size          = top_size_per_n * nslice;

        bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
        bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
        local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
        local_men_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        while (local_men_end_B > LOCAL_MEM_SIZE) {
          nsecs++;
          nslice             = (input_n + nsecs - 1) / nsecs;
          bottom_size        = bottom_size_per_n * nslice;
          top_size           = top_size_per_n * nslice;
          bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
          bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
          local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
          local_men_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        }
      }
    } else {  // only split h
      hsecs        = (2 * (ifmap_total_tensor_size + ofmap_total_tensor_size)) / (LOCAL_MEM_SIZE - bottom_local_start);
      hsecs        = (hsecs == 0) ? 1 : hsecs;
      int oh_slice = (output_h + hsecs - 1) / hsecs;
      int ih_slice = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
      int bottom_size = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_8BIT);
      int top_size    = ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_8BIT);

      bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
      bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
      local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
      local_men_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
      while (local_men_end_B > LOCAL_MEM_SIZE) {
        hsecs++;
        oh_slice    = (output_h + hsecs - 1) / hsecs;
        ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
        bottom_size = ic_per_NPU * get_neuron_local_csize(ih_slice, input_w, OP_8BIT);
        top_size    = ocslice_per_NPU * get_neuron_local_csize(oh_slice, output_w, OP_8BIT);

        bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
        bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
        local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
        local_men_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
      }
    }
  }

  p_secs_info->nsecs  = nsecs;
  p_secs_info->hsecs  = hsecs;
  p_secs_info->icsecs = icsecs;
  p_secs_info->ocsecs = ocsecs;
  return 1;
}

int get_deconv_secs_info(
    bm_tensor_4d_t input_shape,
    int bottom_c,
    int top_c,
    int kh,
    int kw,
    bm_tensor_4d_t output_shape,
    int using_bias,
    int stride_h,
    int dh,
    int pad_h,
    int groups,
    conv_secs_info_t* secs_info) {
  UNUSED(using_bias);
  int bottom_h = input_shape.h;
  int bottom_w = input_shape.w;
  int top_h    = output_shape.h;
  int top_w    = output_shape.w;
  int kh_ext   = (kh - 1) * dh + 1;

  int n          = input_shape.n;
  int ic         = bottom_c / groups;
  int oc         = top_c / groups;
  int ic_per_NPU = ceiling_func_shift(ic, NPU_SHIFT);
  // const int oc_per_NPU = ceiling_func_shift(oc, NPU_SHIFT);
  int oc_per_NPU;

  oc_per_NPU           = ceiling_func_shift(oc, NPU_SHIFT);
  int bias_tensor_size = oc_per_NPU * FLOAT_SIZE;
  int bank_size        = LOCAL_MEM_SIZE / LOCAL_MEM_BANKS;

  int weight_tensor_size      = ic * oc_per_NPU * kh * kw * FLOAT_SIZE;
  const int if_csize_local    = get_neuron_local_csize(bottom_h, bottom_w, OP_32BIT);
  const int of_csize_local    = get_neuron_local_csize(top_h, top_w, OP_32BIT);
  int ifmap_total_tensor_size = n * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = n * oc_per_NPU * of_csize_local;
  int weight_capacity         = get_neuron_align_addr(weight_tensor_size * 2, OP_32BIT);
  ifmap_total_tensor_size *= 2;
  ifmap_total_tensor_size =
      ALIGN(ifmap_total_tensor_size + weight_capacity + get_neuron_align_addr(bias_tensor_size * 2, OP_32BIT), bank_size);
  ofmap_total_tensor_size *= 2;
  int totalneed_local_size = ifmap_total_tensor_size + ofmap_total_tensor_size;

  int nsecs = 1, ocsecs = 1, icsecs = 1, hsecs = 1;
  int nslice = n, ocslice_per_NPU = oc_per_NPU, icslice = ic;
  if (totalneed_local_size > LOCAL_MEM_SIZE) {
    // if weight_capacity > 2 * bank_size then split oc and ic
    if (weight_capacity > (bank_size << 1)) {
      icsecs         = weight_capacity / bank_size + 1;
      int max_icsecs = ic_per_NPU;
      if (icsecs > max_icsecs) {
        icsecs = max_icsecs;
      }
      icslice         = (ic + icsecs - 1) / icsecs;
      weight_capacity = icslice * oc_per_NPU * kh * kw * FLOAT_SIZE;
      weight_capacity = get_neuron_align_addr(weight_capacity + 2 * bias_tensor_size, OP_32BIT);
      int max_ocsecs  = oc_per_NPU;
      while ((weight_capacity > bank_size) || (ocslice_per_NPU * top_w > bank_size)) {
        if (ocsecs == 1) {
          ocsecs = max(weight_capacity / bank_size + 1, oc_per_NPU * top_w / bank_size + 1);
        }
        if (ocsecs > max_ocsecs) {
          ocsecs = max_ocsecs;
          break;
        } else {
          ocsecs++;
        }
        ocslice_per_NPU = (oc_per_NPU + ocsecs - 1) / ocsecs;
        weight_capacity = icslice * ocslice_per_NPU * kh * kw * FLOAT_SIZE;
        weight_capacity = ALIGN(weight_capacity + 2 * bias_tensor_size, bank_size);
      }
    }
    ocslice_per_NPU = (oc_per_NPU + ocsecs - 1) / ocsecs;
    ic_per_NPU      = ceiling_func_shift(icslice, NPU_SHIFT);

    weight_tensor_size = icslice * ocslice_per_NPU * kh * kw * FLOAT_SIZE;
    weight_capacity    = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);

    int bottom_local_start = weight_capacity;

    ASSERT(bottom_local_start < LOCAL_MEM_SIZE);
    ifmap_total_tensor_size = n * ic_per_NPU * if_csize_local;
    ofmap_total_tensor_size = n * ocslice_per_NPU * of_csize_local;
    int bottom_local_end    = ALIGN(bottom_local_start + 2 * ifmap_total_tensor_size, bank_size);
    int local_mem_end       = bottom_local_end + 2 * ofmap_total_tensor_size;
    if (local_mem_end > LOCAL_MEM_SIZE) {
      if (n > 1) {
        int bottom_size_per_n = ic_per_NPU * if_csize_local;
        int top_size_per_n    = ocslice_per_NPU * of_csize_local;
        local_mem_end         = ALIGN(bottom_local_start + 2 * bottom_size_per_n, bank_size) + 2 * top_size_per_n;
        if (local_mem_end > LOCAL_MEM_SIZE) {
          nsecs            = n;
          hsecs            = max(1, (2 * (bottom_size_per_n + top_size_per_n)) / (LOCAL_MEM_SIZE - bottom_local_start));
          int oh_slice     = (top_h + hsecs - 1) / hsecs;
          int ih_slice     = (oh_slice + 2 * pad_h - kh_ext) / stride_h + 2;
          int bottom_size  = ic_per_NPU * get_neuron_local_csize(ih_slice, bottom_w, OP_32BIT);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, top_w, OP_32BIT) + bottom_local_end;
          while (local_mem_end > LOCAL_MEM_SIZE) {
            hsecs++;
            oh_slice         = (top_h + hsecs - 1) / hsecs;
            ih_slice         = (oh_slice + 2 * pad_h - kh_ext) / stride_h + 2;
            bottom_size      = ic_per_NPU * get_neuron_local_csize(ih_slice, bottom_w, OP_32BIT);
            bottom_local_end = ALIGN(bottom_local_start + bottom_size * 2, bank_size);
            local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, top_w, OP_32BIT);
          }

        } else {
          nsecs =
              max(2, 2 * (ifmap_total_tensor_size + ofmap_total_tensor_size) / (LOCAL_MEM_SIZE - bottom_local_start));
          nslice = (n + nsecs - 1) / nsecs;
          local_mem_end =
              ALIGN(bottom_local_start + 2 * bottom_size_per_n * nslice, bank_size) + 2 * top_size_per_n * nslice;
          while (local_mem_end > LOCAL_MEM_SIZE) {
            nsecs++;
            nslice = (n + nsecs - 1) / nsecs;
            local_mem_end =
                ALIGN(bottom_local_start + 2 * bottom_size_per_n * nslice, bank_size) + 2 * top_size_per_n * nslice;
          }
        }
      } else {
        hsecs =
            max(1, (2 * (ifmap_total_tensor_size + ofmap_total_tensor_size)) / (LOCAL_MEM_SIZE - bottom_local_start));
        int oh_slice     = (top_h + hsecs - 1) / hsecs;
        int ih_slice     = (oh_slice + 2 * pad_h - kh_ext) / stride_h + 2;
        int bottom_size  = ic_per_NPU * get_neuron_local_csize(ih_slice, bottom_w, OP_32BIT);
        bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
        local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, top_w, OP_32BIT);
        while (local_mem_end > LOCAL_MEM_SIZE) {
          hsecs++;
          oh_slice         = (top_h + hsecs - 1) / hsecs;
          ih_slice         = (oh_slice + 2 * pad_h - kh_ext) / stride_h + 2;
          bottom_size      = ic_per_NPU * get_neuron_local_csize(ih_slice, bottom_w, OP_32BIT);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_local_csize(oh_slice, top_w, OP_32BIT);
        }
      }
    }
  }
  secs_info->hsecs  = hsecs;
  secs_info->nsecs  = nsecs;
  secs_info->icsecs = icsecs;
  secs_info->ocsecs = ocsecs;
  return 1;
}
