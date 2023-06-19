#include "global_layer_data_split.h"

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
    int pad_h,
    int pad_h_after,
    int pad_w,
    int pad_w_after,
    int using_bias,
    conv_secs_info_t* p_secs_info) {
  UNUSED(dw);
  UNUSED(stride_w);
  int kh_ext     = dh * (kh - 1) + 1;
  int kw_ext     = dw * (kw - 1) + 1;
  int ic         = input_c / groups;
  int oc         = output_c / groups;
  int ic_per_NPU = ceiling_func_shift(ic, NPU_SHIFT);
  int oc_per_NPU = ceiling_func_shift(oc, NPU_SHIFT);
  int h_max_pad = pad_h + pad_h_after + stride_h - 1;
  int w_max_pad = pad_w + pad_w_after + stride_w - 1;

  bool having_pad = (pad_h || pad_h_after || pad_w || pad_w_after);

  int bias_tensor_size;
  bias_tensor_size = using_bias ? oc_per_NPU * FLOAT_SIZE : 0;

  int bank_size = LOCAL_MEM_SIZE / LOCAL_MEM_BANKS;
  int weight_tensor_size;
  weight_tensor_size = ALIGN(ic, 2) * oc_per_NPU * kh * kw * FLOAT_SIZE;

  int if_csize_local          = get_neuron_csize_local(input_h, input_w);
  int of_csize_local          = get_neuron_csize_local(output_h, output_w);
  int ifmap_total_tensor_size = input_n * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = input_n * oc_per_NPU * of_csize_local;

  int weight_capacity         = addr_EU_align(weight_tensor_size * 2 + bias_tensor_size * 2);
  ifmap_total_tensor_size *= 2;
  int ofmap_offset = ALIGN(weight_capacity + ifmap_total_tensor_size, bank_size);
  ofmap_total_tensor_size *= 2;
  int totalneed_local_size = ofmap_offset + ofmap_total_tensor_size;

  const int ih_pad = input_h + h_max_pad;
  const int iw_pad = input_w + w_max_pad;
  const int ow = ((iw_pad > 2047 ? 2047 : iw_pad) - kw_ext) / stride_w + 1;
  const int oh = ((ih_pad > 2047 ? 2047 : ih_pad) - kh_ext) / stride_h + 1;
  if (ih_pad > 2047 || iw_pad > 2047) {
      int tmp_offset = ALIGN(totalneed_local_size, EU_NUM * FLOAT_SIZE);
      int tmp_size = input_n * oc_per_NPU * oh * ow * FLOAT_SIZE;
      totalneed_local_size = tmp_offset + tmp_size;
  }
  if (dh > 15 || dw > 15) {
      if (having_pad) {
        int pad_offset = totalneed_local_size;
        int pad_csize = get_neuron_csize_local(ih_pad > 2047 ? 2047 : ih_pad, iw_pad > 2047 ? 2047 : iw_pad);
        int pad_size = input_n * ic_per_NPU * pad_csize;
        totalneed_local_size = pad_offset + pad_size;
      }
      int dilation_offset = totalneed_local_size;
      int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
      int dilation_size = input_n * ic_per_NPU * dilation_csize;
      totalneed_local_size = dilation_offset + dilation_size;
  }

  int nsecs = 1, ocsecs = 1, icsecs = 1, hsecs = 1, wsecs = 1;
  int nslice = input_n, ocslice_per_NPU = oc_per_NPU, icslice = ic;
  if (totalneed_local_size > LOCAL_MEM_SIZE || ic >= (1<<12) || oc >= (1<<12) || ((dh > 15 || dw > 15) && (oh * kh > 2047 || ow * kw > 2047))) {
    // if weight_capacity > 2 * bank_size then split oc and ic
    if (weight_capacity > (bank_size << 1) || ic >= (1<<12) || oc >= (1<<12)) {
      icsecs         = bm_max(weight_capacity / bank_size + 1, ceiling_func(ic, (1<<12)-1));
      int max_icsecs = ic_per_NPU;
      if (icsecs > max_icsecs) {
        icsecs = max_icsecs;
      }
      icslice         = (ic + icsecs - 1) / icsecs;
      weight_capacity = icslice * oc_per_NPU * kh * kw * FLOAT_SIZE;
      weight_capacity = addr_EU_align(weight_capacity + 2 * bias_tensor_size);
      int max_ocsecs  = oc_per_NPU;
      while ((weight_capacity > bank_size) || (ocslice_per_NPU * output_w > bank_size) || ocslice_per_NPU * NPU_NUM >= (1<<12)) {
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
    if (ih_pad > 2047 || iw_pad > 2047) {
        int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
        int tmp_size = input_n * ocslice_per_NPU * oh * ow * FLOAT_SIZE;
        local_mem_end = tmp_offset + tmp_size;
    }
    if (dh > 15 || dw > 15) {
        if (having_pad) {
          int pad_offset = local_mem_end;
          int pad_csize = get_neuron_csize_local(ih_pad > 2047 ? 2047 : ih_pad, iw_pad > 2047 ? 2047 : iw_pad);
          int pad_size = input_n * ic_per_NPU * pad_csize;
          local_mem_end = pad_offset + pad_size;
        }
        int dilation_offset = local_mem_end;
        int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
        int dilation_size = input_n * ic_per_NPU * dilation_csize;
        local_mem_end = dilation_offset + dilation_size;
    }

    if (local_mem_end > LOCAL_MEM_SIZE || ((dh > 15 || dw > 15) && (oh * kh > 2047 || ow * kw > 2047))) {
      if (input_n > 1) {
        int bottom_size_per_n = ic_per_NPU * if_csize_local;
        int top_size_per_n    = ocslice_per_NPU * of_csize_local;
        local_mem_end         = ALIGN(bottom_local_start + 2 * bottom_size_per_n, bank_size) + 2 * top_size_per_n;
        if (ih_pad > 2047 || iw_pad > 2047) {
            int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
            int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
            local_mem_end = tmp_offset + tmp_size;
        }
        if (dh > 15 || dw > 15) {
            if (having_pad) {
              int pad_offset = local_mem_end;
              int pad_csize = get_neuron_csize_local(ih_pad > 2047 ? 2047 : ih_pad, iw_pad > 2047 ? 2047 : iw_pad);
              int pad_size = ic_per_NPU * pad_csize;
              local_mem_end = pad_offset + pad_size;
            }
            int dilation_offset = local_mem_end;
            int dilation_size = ic_per_NPU * get_neuron_csize_local(oh * kh, ow * kw);
            local_mem_end = dilation_offset + dilation_size;
        }
        if (local_mem_end > LOCAL_MEM_SIZE || ((dh > 1 || dw > 1) && (oh * kh > 2047 || ow * kw > 2047))) {
          nsecs            = input_n;
          hsecs            = (2 * (bottom_size_per_n + top_size_per_n)) / (LOCAL_MEM_SIZE - bottom_local_start);
          hsecs            = (hsecs == 0) ? 1 : hsecs;
          int oh_slice     = (output_h + hsecs - 1) / hsecs;
          int ih_slice     = oh_slice * stride_h + kh_ext;
          int bottom_size  = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w) + bottom_local_end;
          int ih_slice_pad = ih_slice + h_max_pad;
          if (ih_slice_pad > 2047 || iw_pad > 2047) {
              int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
              int oh = ((ih_slice_pad > 2047 ? 2047 : ih_slice_pad) - kh_ext) / stride_h + 1;
              int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
              local_mem_end = tmp_offset + tmp_size;
              if (dh > 15 || dw > 15) {
                if (having_pad) {
                  int pad_offset = local_mem_end;
                  int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                  int pad_size = ic_per_NPU * pad_csize;
                  local_mem_end = pad_offset + pad_size;
                }
                int dilation_offset = local_mem_end;
                int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                int dilation_size = ic_per_NPU * dilation_csize;
                local_mem_end = dilation_offset + dilation_size;
              }
          } else {
              if (dh > 15 || dw > 15) {
                if (having_pad) {
                  int pad_offset = local_mem_end;
                  int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                  int pad_size = ic_per_NPU * pad_csize;
                  local_mem_end = pad_offset + pad_size;
                }
                int dilation_offset = local_mem_end;
                int dilation_csize = get_neuron_csize_local(oh_slice * kh, ow * kw);
                int dilation_size = ic_per_NPU * dilation_csize;
                local_mem_end = dilation_offset + dilation_size;
              }
          }

          int d_h = 1;
          if (dh > 15 || dw > 15) {
            int oh_slice_tmp         = (output_h + hsecs - 1) / hsecs;
            int ih_slice_tmp         = oh_slice_tmp * stride_h + kh_ext;
            int ih_slice_pad_tmp = ih_slice_tmp + h_max_pad;
            d_h = (ih_slice_pad_tmp > 2047 || iw_pad > 2047) ?
                        ((ih_slice_pad_tmp > 2047 ? 2047 : ih_slice_pad_tmp) - kh_ext) / stride_h + 1 :
                        oh_slice_tmp;
          }
          while (local_mem_end > LOCAL_MEM_SIZE || ((dh > 15 || dw > 15) && (d_h * kh > 2047 || ow * kw > 2047))) {
            hsecs++;
            oh_slice         = (output_h + hsecs - 1) / hsecs;
            ih_slice         = oh_slice * stride_h + kh_ext;
            bottom_size      = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
            bottom_local_end = ALIGN(bottom_local_start + bottom_size * 2, bank_size);
            local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);
            ih_slice_pad = ih_slice + h_max_pad;
            if (ih_slice_pad > 2047 || iw_pad > 2047) {
                int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
                int oh = ((ih_slice_pad > 2047 ? 2047 : ih_slice_pad) - kh_ext) / stride_h + 1;
                int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
                local_mem_end = tmp_offset + tmp_size;
                if (dh > 15 || dw > 15) {
                  if (oh * kh > 2047 || ow * kw > 2047) continue;
                  if (having_pad) {
                    int pad_offset = local_mem_end;
                    int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                    int pad_size = ic_per_NPU * pad_csize;
                    local_mem_end = pad_offset + pad_size;
                  }
                  int dilation_offset = local_mem_end;
                  int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                  int dilation_size = ic_per_NPU * dilation_csize;
                  local_mem_end = dilation_offset + dilation_size;
                }
            } else {
                if (dh > 15 || dw > 15) {
                  if (oh_slice * kh > 2047 || ow * kw > 2047) continue;
                  if (having_pad) {
                    int pad_offset = local_mem_end;
                    int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                    int pad_size = ic_per_NPU * pad_csize;
                    local_mem_end = pad_offset + pad_size;
                  }
                  int dilation_offset = local_mem_end;
                  int dilation_csize = get_neuron_csize_local(oh_slice * kh, ow * kw);
                  int dilation_size = ic_per_NPU * dilation_csize;
                  local_mem_end = dilation_offset + dilation_size;
                }
            }
          }
        } else {
          nsecs = bm_min(
              2, 2 * (ifmap_total_tensor_size + ofmap_total_tensor_size) / (LOCAL_MEM_SIZE - bottom_local_start));
          if (nsecs == 0) nsecs = 1;
          nslice                = (input_n + nsecs - 1) / nsecs;
          local_mem_end =
              ALIGN(bottom_local_start + 2 * bottom_size_per_n * nslice, bank_size) + 2 * top_size_per_n * nslice;
          if (ih_pad > 2047 || iw_pad > 2047) {
              int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
              int tmp_size = nslice * ocslice_per_NPU * oh * ow * FLOAT_SIZE;
              local_mem_end = tmp_offset + tmp_size;
          }
          if (dh > 15 || dw > 15) {
              if (having_pad) {
                  int pad_offset = local_mem_end;
                  int pad_csize = get_neuron_csize_local(ih_pad > 2047 ? 2047 : ih_pad, iw_pad > 2047 ? 2047 : iw_pad);
                  int pad_size = nslice * ic_per_NPU * pad_csize;
                  local_mem_end = pad_offset + pad_size;
              }
              int dilation_offset = local_mem_end;
              int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
              int dilation_size = nslice * ic_per_NPU * dilation_csize;
              local_mem_end = dilation_offset + dilation_size;
          }

          while (local_mem_end > LOCAL_MEM_SIZE) {
            nsecs++;
            nslice = (input_n + nsecs - 1) / nsecs;
            local_mem_end =
                ALIGN(bottom_local_start + 2 * bottom_size_per_n * nslice, bank_size) + 2 * top_size_per_n * nslice;
            if (ih_pad > 2047 || iw_pad > 2047) {
                int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
                int tmp_size = nslice * ocslice_per_NPU * oh * ow * FLOAT_SIZE;
                local_mem_end = tmp_offset + tmp_size;
            }
            if (dh > 15 || dw > 15) {
                if (having_pad) {
                  int pad_offset = local_mem_end;
                  int pad_csize = get_neuron_csize_local(ih_pad > 2047 ? 2047 : ih_pad, iw_pad > 2047 ? 2047 : iw_pad);
                  int pad_size = nslice * ic_per_NPU * pad_csize;
                  local_mem_end = pad_offset + pad_size;
                }
                int dilation_offset = local_mem_end;
                int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                int dilation_size = nslice * ic_per_NPU * dilation_csize;
                local_mem_end = dilation_offset + dilation_size;
            }
          }
        }
      } else { // input_n = 1
        hsecs = (2 * (ifmap_total_tensor_size + ofmap_total_tensor_size)) / (LOCAL_MEM_SIZE - bottom_local_start);
        hsecs = (hsecs == 0) ? 1 : hsecs;
        int oh_slice     = (output_h + hsecs - 1) / hsecs;
        int ih_slice     = oh_slice * stride_h + kh_ext;
        int bottom_size  = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
        bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
        local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);
        int ih_slice_pad = ih_slice + h_max_pad;

        bool consider_dilation_k = false;

        if (ih_slice_pad > 2047 || iw_pad > 2047) {
            int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
            int oh = ((ih_slice_pad > 2047 ? 2047 : ih_slice_pad) - kh_ext) / stride_h + 1;
            int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
            local_mem_end = tmp_offset + tmp_size;
            if (dh > 15 || dw > 15) {
                if (oh * kh > 2047 || ow * kw > 2047) consider_dilation_k = true;
                if (having_pad) {
                  int pad_offset = local_mem_end;
                  int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                  int pad_size = ic_per_NPU * pad_csize;
                  local_mem_end = pad_offset + pad_size;
                }
                int dilation_offset = local_mem_end;
                int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                int dilation_size = ic_per_NPU * dilation_csize;
                local_mem_end = dilation_offset + dilation_size;
            }
        } else {
            if (dh > 15 || dw > 15) {
               if (oh_slice * kh > 2047 || ow * kw > 2047) consider_dilation_k = true;
               if (having_pad) {
                  int pad_offset = local_mem_end;
                  int pad_csize = get_neuron_csize_local(ih_slice_pad, iw_pad);
                  int pad_size = ic_per_NPU * pad_csize;
                  local_mem_end = pad_offset + pad_size;
                }
                int dilation_offset = local_mem_end;
                int dilation_csize = get_neuron_csize_local(oh_slice * kh, ow * kw);
                int dilation_size = ic_per_NPU * dilation_csize;
                local_mem_end = dilation_offset + dilation_size;
            }
        }

        while (local_mem_end > LOCAL_MEM_SIZE || consider_dilation_k) {
          consider_dilation_k = false;
          bool consider_dilation_h = dh>15;
          if (hsecs >= output_h && !consider_dilation_h) {
              bool consider_dilation_w = false;
              ++wsecs;
              hsecs = output_h;
              oh_slice         = (output_h + hsecs - 1) / hsecs;
              ih_slice         = oh_slice * stride_h + kh_ext;
              int ow_slice     = (output_w + wsecs - 1) / wsecs;
              int iw_slice     = ow_slice * stride_w + kw_ext;
              bottom_size      = ic_per_NPU * get_neuron_csize_local(ih_slice, iw_slice);
              bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
              local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, ow_slice);
              ih_slice_pad = ih_slice + h_max_pad;
              int iw_slice_pad = iw_slice + w_max_pad;
              if (ih_slice_pad > 2047 || iw_slice_pad > 2047) {
                  int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
                  int oh = ((ih_slice_pad > 2047 ? 2047 : ih_slice_pad) - kh_ext) / stride_h + 1;
                  int ow = ((iw_slice_pad > 2047 ? 2047 : iw_slice_pad) - kw_ext) / stride_w + 1;
                  int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
                  local_mem_end = tmp_offset + tmp_size;
                  if (dh > 15 || dw > 15) {
                      if (ow * kw > 2047) {
                        consider_dilation_k = true;
                        consider_dilation_w = true;
                      }
                      if (having_pad) {
                        int pad_offset = local_mem_end;
                        int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                        int pad_size = ic_per_NPU * pad_csize;
                        local_mem_end = pad_offset + pad_size;
                      }
                      int dilation_offset = local_mem_end;
                      int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                      int dilation_size = ic_per_NPU * dilation_csize;
                      local_mem_end = dilation_offset + dilation_size;
                  }
              } else {
                  if (dh > 15 || dw > 15) {
                      if (ow_slice * kw > 2047) {
                         consider_dilation_k = true;
                         consider_dilation_w = true;
                      }
                      if (having_pad) {
                        int pad_offset = local_mem_end;
                        int pad_csize = get_neuron_csize_local(ih_slice_pad, iw_pad);
                        int pad_size = ic_per_NPU * pad_csize;
                        local_mem_end = pad_offset + pad_size;
                      }
                      int dilation_offset = local_mem_end;
                      int dilation_csize = get_neuron_csize_local(oh_slice * kh, ow_slice * kw);
                      int dilation_size = ic_per_NPU * dilation_csize;
                      local_mem_end = dilation_offset + dilation_size;
                  }
              }

              while (local_mem_end > LOCAL_MEM_SIZE || consider_dilation_w) {
                  consider_dilation_w = false;
                  if (wsecs >= output_w)
                      return 0;
                  ++wsecs;
                  ow_slice     = (output_w + wsecs - 1) / wsecs;
                  iw_slice     = ow_slice * stride_w + kw_ext;
                  bottom_size      = ic_per_NPU * get_neuron_csize_local(ih_slice, iw_slice);
                  bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
                  local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, ow_slice);
                  iw_slice_pad = iw_slice + w_max_pad;
                  if (ih_slice_pad > 2047 || iw_slice_pad > 2047) {
                      int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
                      int oh = ((ih_slice_pad > 2047 ? 2047 : ih_slice_pad) - kh_ext) / stride_h + 1;
                      int ow = ((iw_slice_pad > 2047 ? 2047 : iw_slice_pad) - kw_ext) / stride_w + 1;
                      int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
                      local_mem_end = tmp_offset + tmp_size;
                      if (dh > 15 || dw > 15) {
                          if (ow * kw > 2047) continue;
                          if (having_pad) {
                            int pad_offset = local_mem_end;
                            int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_slice_pad > 2047 ? 2047 : iw_slice_pad);
                            int pad_size = ic_per_NPU * pad_csize;
                            local_mem_end = pad_offset + pad_size;
                          }
                          int dilation_offset = local_mem_end;
                          int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                          int dilation_size = ic_per_NPU * dilation_csize;
                          local_mem_end = dilation_offset + dilation_size;
                      }
                  } else {
                      if (dh > 15 || dw > 15) {
                          if (ow_slice * kw > 2047) continue;
                          if (having_pad) {
                            int pad_offset = local_mem_end;
                            int pad_csize = get_neuron_csize_local(ih_slice_pad, iw_slice_pad);
                            int pad_size = ic_per_NPU * pad_csize;
                            local_mem_end = pad_offset + pad_size;
                          }
                          int dilation_offset = local_mem_end;
                          int dilation_csize = get_neuron_csize_local(oh_slice * kh, ow_slice * kw);
                          int dilation_size = ic_per_NPU * dilation_csize;
                          local_mem_end = dilation_offset + dilation_size;
                      }
                  }
              }
          } else {
              consider_dilation_h = false;
              if (hsecs >= output_h) return 0;
              hsecs++;
              oh_slice         = (output_h + hsecs - 1) / hsecs;
              ih_slice         = oh_slice * stride_h + kh_ext;
              bottom_size      = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
              bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
              local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);
              ih_slice_pad = ih_slice + h_max_pad;
              if (ih_slice_pad > 2047 || iw_pad > 2047) {
                  int tmp_offset = ALIGN(local_mem_end, EU_NUM * FLOAT_SIZE);
                  int oh = ((ih_slice_pad > 2047 ? 2047 : ih_slice_pad) - kh_ext) / stride_h + 1;
                  int tmp_size = ocslice_per_NPU * oh * ow * FLOAT_SIZE;
                  local_mem_end = tmp_offset + tmp_size;
                  if (dh > 15 || dw > 15) {
                      if (oh * kh > 2047) {
                         consider_dilation_k = true;
                         consider_dilation_h = true;
                         }
                      if (having_pad) {
                        int pad_offset = local_mem_end;
                        int pad_csize = get_neuron_csize_local(ih_slice_pad > 2047 ? 2047 : ih_slice_pad, iw_pad > 2047 ? 2047 : iw_pad);
                        int pad_size = ic_per_NPU * pad_csize;
                        local_mem_end = pad_offset + pad_size;
                      }
                      int dilation_offset = local_mem_end;
                      int dilation_csize = get_neuron_csize_local(oh * kh, ow * kw);
                      int dilation_size = ic_per_NPU * dilation_csize;
                      local_mem_end = dilation_offset + dilation_size;
                  }
              } else {
                  if (dh > 15 || dw > 15) {
                      if (oh_slice * kh > 2047) {
                         consider_dilation_k = true;
                         consider_dilation_h = true;
                         }
                      if (having_pad) {
                        int pad_offset = local_mem_end;
                        int pad_csize = get_neuron_csize_local(ih_slice_pad, iw_pad);
                        int pad_size = ic_per_NPU * pad_csize;
                        local_mem_end = pad_offset + pad_size;
                      }
                      int dilation_offset = local_mem_end;
                      int dilation_csize = get_neuron_csize_local(oh_slice * kh, ow * kw);
                      int dilation_size = ic_per_NPU * dilation_csize;
                      local_mem_end = dilation_offset + dilation_size;
                  }
              }
          }
        }
      }
    }
  }

  int max_oc_per_NPU = (oc_per_NPU + ocsecs - 1) / ocsecs;
  while (max_oc_per_NPU * NPU_NUM >= (1 << 12)) {
      ++ocsecs;
      max_oc_per_NPU = (oc_per_NPU + ocsecs - 1) / ocsecs;
  }

  p_secs_info->nsecs  = nsecs;
  p_secs_info->hsecs  = hsecs;
  p_secs_info->owsecs  = wsecs;
  p_secs_info->icsecs = icsecs;
  p_secs_info->ocsecs = ocsecs;

  return 1;
}
int conv16_ofmap_csize_local(int oh) {
  return oh * 256;
}

int global_conv_data_split_fix16b(
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
  UNUSED(input_h);
  UNUSED(dw);
  UNUSED(stride_w);
  UNUSED(ins_w);
  // insert 0 manually
  ins_h = 0;

  p_secs_info->owsecs = (output_w + 16 - 1)/ 16;
  int kh_ext     = dh * (kh - 1) + 1;
  int ic         = input_c / groups;
  int oc         = output_c / groups;
  int ic_per_NPU = ceiling_func_shift(ic, NPU_SHIFT);
  int ocsecs = ceiling_func_shift(oc, NPU_SHIFT);
  int oc_per_NPU = 1;

  int bias_tensor_size        = using_bias ? oc_per_NPU * 2 : 0;
  int bank_size               = LOCAL_MEM_SIZE / LOCAL_MEM_BANKS;
  int weight_tensor_size      = ALIGN(ic, 4) * oc_per_NPU * kh * kw;
  int nsecs                   = (input_n + 4 - 1) / 4;
  int nNumIn4N = 1;
  int ih_slice = ((output_h * stride_h + kh_ext - 1) + ins_h)/(ins_h + 1);
  int if_csize_local          = get_neuron_csize_local(ih_slice, input_w);
  int of_csize_local          = conv16_ofmap_csize_local(output_h);
  int ifmap_total_tensor_size = nNumIn4N * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = nNumIn4N * oc_per_NPU * of_csize_local;
  int weight_capacity         = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
  ifmap_total_tensor_size     = 2 * ALIGN(ifmap_total_tensor_size, bank_size);
  //int ofmap_offset            = weight_capacity + ifmap_total_tensor_size;
  //ofmap_total_tensor_size     = 2 * ALIGN(ofmap_total_tensor_size, bank_size);
  //int totalneed_local_size    = ofmap_offset + ofmap_total_tensor_size;

  int icsecs = 1, hsecs = 1;
  int ocslice_per_NPU = oc_per_NPU, icslice = ic;

  ocslice_per_NPU        = (oc_per_NPU + ocsecs - 1) / ocsecs;
  ic_per_NPU             = ceiling_func_shift(icslice, NPU_SHIFT);
  weight_tensor_size     = ALIGN(icslice, 4) * ocslice_per_NPU * kh * kw;
  weight_capacity        = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
  int bottom_local_start = weight_capacity;

  if (bottom_local_start >= LOCAL_MEM_SIZE) return 0;
  ifmap_total_tensor_size = nNumIn4N * ic_per_NPU * if_csize_local;
  ofmap_total_tensor_size = nNumIn4N * ocslice_per_NPU * of_csize_local;

  int bottom_local_end_A  = ALIGN(bottom_local_start + ifmap_total_tensor_size, bank_size);
  int bottom_local_end_B  = ALIGN(bottom_local_end_A + ifmap_total_tensor_size, bank_size);
  int local_mem_end_A     = ALIGN(bottom_local_end_B + ofmap_total_tensor_size, bank_size);
  int local_mem_end_B     = ALIGN(local_mem_end_A + ofmap_total_tensor_size, bank_size);

  if (local_mem_end_B > LOCAL_MEM_SIZE) {
    {  // only split h
      hsecs        = (2 * (ifmap_total_tensor_size + ofmap_total_tensor_size)) / (LOCAL_MEM_SIZE - bottom_local_start);
      hsecs        = (hsecs == 0) ? 1 : hsecs;
      int oh_slice = (output_h + hsecs - 1) / hsecs;
      int ih_slice = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1) + kh; // here add kh, need pad h use atomic operation
      int bottom_size = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
      int top_size    = ocslice_per_NPU  * conv16_ofmap_csize_local(oh_slice);

      bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
      bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
      local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
      local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
      while (local_mem_end_B > LOCAL_MEM_SIZE) {
        hsecs++;
        oh_slice    = (output_h + hsecs - 1) / hsecs;
        ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1) + kh;
        bottom_size = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
        top_size    = ocslice_per_NPU * conv16_ofmap_csize_local(oh_slice);

        bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
        bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
        local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
        local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
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
  UNUSED(input_h);
  UNUSED(ins_w);
  int kh_ext     = dh * (kh - 1) + 1;
  int kw_ext     = dw * (kw - 1) + 1;
  int ic         = input_c / groups;
  int oc         = output_c / groups;
  int ic_per_NPU = ceiling_func_shift(ic, NPU_SHIFT);
  int oc_per_NPU = ceiling_func_shift(oc, NPU_SHIFT);

  int bias_tensor_size;
  bias_tensor_size            = using_bias ? oc_per_NPU * 2 : 0;
  int bank_size               = LOCAL_MEM_SIZE / LOCAL_MEM_BANKS;
  int weight_tensor_size      = ALIGN(ic, 4) * oc_per_NPU * kh * kw;
  int nNumIn4N                = (input_n + 4 - 1) / 4;
  int ih_slice = ((output_h * stride_h + kh_ext - 1) + ins_h)/(ins_h + 1);
  int if_csize_local          = get_neuron_csize_local(ih_slice, input_w);
  int of_csize_local          = get_neuron_csize_local(output_h, output_w);
  int ifmap_total_tensor_size = nNumIn4N * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = nNumIn4N * oc_per_NPU * of_csize_local;
  int weight_capacity         = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
  ifmap_total_tensor_size     = 2 * ALIGN(ifmap_total_tensor_size, bank_size);
  int ofmap_offset            = weight_capacity + ifmap_total_tensor_size;
  ofmap_total_tensor_size     = 2 * ALIGN(ofmap_total_tensor_size, bank_size);
  int totalneed_local_size    = ofmap_offset + ofmap_total_tensor_size;

  int nsecs = 1, ocsecs = 1, icsecs = 1, hsecs = 1, wsecs = 1;
  int nslice = nNumIn4N, ocslice_per_NPU = oc_per_NPU, icslice = ic;
  int ocslice = oc;  // int min_ocsecs = 1;
  while ((1<<12) <= ocslice) {
     ocsecs++;
     ocslice = (oc + ocsecs -1)/ocsecs;
     // min_ocsecs = ocsecs;
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
        weight_capacity = ALIGN(icslice, 4) * ocslice_per_NPU * kh * kw;
        weight_capacity = ALIGN(weight_capacity + 2 * bias_tensor_size, bank_size);
      }
    }
  }
  ocslice_per_NPU        = (oc_per_NPU + ocsecs - 1) / ocsecs;
  ic_per_NPU             = ceiling_func_shift(icslice, NPU_SHIFT);
  weight_tensor_size     = ALIGN(icslice, 4) * ocslice_per_NPU * kh * kw;
  weight_capacity        = ALIGN(2 * weight_tensor_size + 2 * bias_tensor_size, bank_size);
  int bottom_local_start = weight_capacity;
  if (bottom_local_start >= LOCAL_MEM_SIZE) return 0;
  ifmap_total_tensor_size = nNumIn4N * ic_per_NPU * if_csize_local;
  ofmap_total_tensor_size = nNumIn4N * ocslice_per_NPU * of_csize_local;
  int bottom_local_end_A  = ALIGN(bottom_local_start + ifmap_total_tensor_size, bank_size);
  int bottom_local_end_B  = ALIGN(bottom_local_end_A + ifmap_total_tensor_size, bank_size);
  int local_mem_end_A     = ALIGN(bottom_local_end_B + ofmap_total_tensor_size, bank_size);
  int local_mem_end_B     = ALIGN(local_mem_end_A + ofmap_total_tensor_size, bank_size);
  if (local_mem_end_B > LOCAL_MEM_SIZE) {
    if (nNumIn4N > 1) {
      int bottom_size_per_n = ic_per_NPU * if_csize_local;
      int top_size_per_n    = ocslice_per_NPU * of_csize_local;

      bottom_local_end_A = ALIGN(bottom_local_start + bottom_size_per_n, bank_size);
      bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size_per_n, bank_size);
      local_mem_end_A    = ALIGN(bottom_local_end_B + top_size_per_n, bank_size);
      local_mem_end_B    = ALIGN(local_mem_end_A + top_size_per_n, bank_size);
      // split n and h
      if (local_mem_end_B > LOCAL_MEM_SIZE) {
        nsecs           = nNumIn4N;
        hsecs           = (2 * (bottom_size_per_n + top_size_per_n)) / (LOCAL_MEM_SIZE - bottom_local_start);
        hsecs           = (hsecs == 0) ? 1 : hsecs;
        int oh_slice    = (output_h + hsecs - 1) / hsecs;
        int ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
        int bottom_size = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
        int top_size    = ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);

        bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
        bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
        local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
        local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        while (local_mem_end_B > LOCAL_MEM_SIZE) {
          if (hsecs >= output_h) {
            ++wsecs;
            hsecs    = output_h;
            oh_slice = (output_h + hsecs - 1) / hsecs;
            ih_slice = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
            int ow_slice = (output_w + wsecs - 1) / wsecs;
            int iw_slice = (ow_slice * stride_w + kw_ext - 1 + ins_w) / (ins_w + 1);
            bottom_size  = ic_per_NPU * get_neuron_csize_local(ih_slice, iw_slice);
            top_size     = ocslice_per_NPU * get_neuron_csize_local(oh_slice, ow_slice);
            bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
            bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
            local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
            local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
          } else {
            hsecs++;
            oh_slice    = (output_h + hsecs - 1) / hsecs;
            ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
            bottom_size = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
            top_size    = ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);

            bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
            bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
            local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
            local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
          }
        }
      } else {  // only split n
        nsecs =
            bm_min(2, 2 * (ifmap_total_tensor_size + ofmap_total_tensor_size) / (LOCAL_MEM_SIZE - bottom_local_start));
        if (nsecs == 0) nsecs = 1;
        nslice                = (nNumIn4N + nsecs - 1) / nsecs;
        int bottom_size       = bottom_size_per_n * nslice;
        int top_size          = top_size_per_n * nslice;

        bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
        bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
        local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
        local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        while (local_mem_end_B > LOCAL_MEM_SIZE) {
          nsecs++;
          nslice             = (nNumIn4N + nsecs - 1) / nsecs;
          bottom_size        = bottom_size_per_n * nslice;
          top_size           = top_size_per_n * nslice;
          bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
          bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
          local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
          local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        }
      }
    } else {  // only split h/w
      hsecs        = (2 * (ifmap_total_tensor_size + ofmap_total_tensor_size)) / (LOCAL_MEM_SIZE - bottom_local_start);
      hsecs        = (hsecs == 0) ? 1 : hsecs;
      int oh_slice = (output_h + hsecs - 1) / hsecs;
      int ih_slice = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
      int bottom_size = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
      int top_size    = ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);

      bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
      bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
      local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
      local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
      while (local_mem_end_B > LOCAL_MEM_SIZE) {
        if (hsecs >= output_h) {
          ++wsecs;
          hsecs    = output_h;
          oh_slice = (output_h + hsecs - 1) / hsecs;
          ih_slice = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
          int ow_slice = (output_w + wsecs - 1) / wsecs;
          int iw_slice = (ow_slice* stride_w + kw_ext - 1 + ins_w) / (ins_w + 1);
          bottom_size  = ic_per_NPU * get_neuron_csize_local(ih_slice, iw_slice);
          top_size     = ocslice_per_NPU * get_neuron_csize_local(oh_slice, ow_slice);
          bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
          bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
          local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
          local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        } else {
          ASSERT(oh_slice > 1);  // cannot split H any more. need split W instead.
          hsecs++;
          oh_slice    = (output_h + hsecs - 1) / hsecs;
          ih_slice    = (oh_slice * stride_h + kh_ext - 1 + ins_h) / (ins_h + 1);
          bottom_size = ic_per_NPU * get_neuron_csize_local(ih_slice, input_w);
          top_size    = ocslice_per_NPU * get_neuron_csize_local(oh_slice, output_w);

          bottom_local_end_A = ALIGN(bottom_local_start + bottom_size, bank_size);
          bottom_local_end_B = ALIGN(bottom_local_end_A + bottom_size, bank_size);
          local_mem_end_A    = ALIGN(bottom_local_end_B + top_size, bank_size);
          local_mem_end_B    = ALIGN(local_mem_end_A + top_size, bank_size);
        }
      }
    }
  }

  p_secs_info->nsecs  = nsecs;
  p_secs_info->hsecs  = hsecs;
  p_secs_info->owsecs = wsecs;
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
  const int if_csize_local    = get_neuron_csize_local(bottom_h, bottom_w);
  const int of_csize_local    = get_neuron_csize_local(top_h, top_w);
  int ifmap_total_tensor_size = n * ic_per_NPU * if_csize_local;
  int ofmap_total_tensor_size = n * oc_per_NPU * of_csize_local;
  int weight_capacity         = addr_EU_align(weight_tensor_size * 2);
  ifmap_total_tensor_size *= 2;
  ifmap_total_tensor_size =
      ALIGN(ifmap_total_tensor_size + weight_capacity + addr_EU_align(bias_tensor_size * 2), bank_size);
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
      weight_capacity = addr_EU_align(weight_capacity + 2 * bias_tensor_size);
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
          int bottom_size  = ic_per_NPU * get_neuron_csize_local(ih_slice, bottom_w);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, top_w) + bottom_local_end;
          while (local_mem_end > LOCAL_MEM_SIZE) {
            hsecs++;
            oh_slice         = (top_h + hsecs - 1) / hsecs;
            ih_slice         = (oh_slice + 2 * pad_h - kh_ext) / stride_h + 2;
            bottom_size      = ic_per_NPU * get_neuron_csize_local(ih_slice, bottom_w);
            bottom_local_end = ALIGN(bottom_local_start + bottom_size * 2, bank_size);
            local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, top_w);
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
        int bottom_size  = ic_per_NPU * get_neuron_csize_local(ih_slice, bottom_w);
        bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
        local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, top_w);
        while (local_mem_end > LOCAL_MEM_SIZE) {
          hsecs++;
          oh_slice         = (top_h + hsecs - 1) / hsecs;
          ih_slice         = (oh_slice + 2 * pad_h - kh_ext) / stride_h + 2;
          bottom_size      = ic_per_NPU * get_neuron_csize_local(ih_slice, bottom_w);
          bottom_local_end = ALIGN(bottom_local_start + 2 * bottom_size, bank_size);
          local_mem_end    = bottom_local_end + 2 * ocslice_per_NPU * get_neuron_csize_local(oh_slice, top_w);
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
