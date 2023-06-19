#include "global_layer_data_split.h"

// Column of each weight matrix aligns to 8 EUs
#define OUTPUT_DIM_ALIGN_SHIFT 3

int lstm_output_dim_extend(int output_dim) {
  int output_dim_ext = ((output_dim + (1 << OUTPUT_DIM_ALIGN_SHIFT) - 1) >> OUTPUT_DIM_ALIGN_SHIFT)
                       << OUTPUT_DIM_ALIGN_SHIFT;
  return output_dim_ext;
}

/**
 * 1. return 1:
 *   All weights can be located in local memory.
 *   Data can be only splited in n dimension
 *   Lmem buffer include: Wx, Wh, Bx, Cont, (Wx_static), x, ht, x_static, x_t*4, h_t*4, c_t, (x_static*4)
 * 2. return 2:
 *   Weight that is needed in each iteration can be located in local memory.
 *   Innerproduct of x and x_static is proceeded once.
 *   Need global buffer for innerproduct output of x and x_static
 *   Lmem buffer include: Wh, cont, h_t, c_t, x_t*4, h_t*4, (x_static*4)
 * 3. return 3:
 *   All weight can not be located in local memory.
 *   Innerproduct of x and x_static is proceeded once.
 *   Need global buffer for innerproduct output of x and x_static
 *   Lmem buffer include: patial Wh, cont, h_t, c_t, x_t*4, h_t*4, (x_static*4)
 *   2 lmem backs for double buffering Wh
 * 4. return 0:
 *   All innerproduct use global layer
 */
int lstm_data_split_select(int batch_num, int input_dim, int output_dim, int with_x_static, int* batch_slice) {
  int method_sel     = 0;
  int output_dim_ext = lstm_output_dim_extend(output_dim);

  *batch_slice = 0;
  // use EU_NUM as the w of matrix default
  // compute Wx lmem size
  int wx_c         = ceiling_func_shift(output_dim_ext * 4, EU_SHIFT);
  int wx_c_per_npu = ceiling_func_shift(wx_c, NPU_SHIFT);
  int wx_lmem_size = input_dim * wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  // compute Bx lmem size
  int bx_lmem_size = wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  // compute Wh lmem size
  int wh_lmem_size = output_dim * wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  // compute Wx_static size
  int wx_static_lmem_size = 0;
  if (with_x_static == 1) {
    wx_static_lmem_size = wx_lmem_size;
  }

  // compute cont lmem size with n = 1
  int cont_lmem_size = EU_NUM * FLOAT_SIZE;
  // compute x lmem size with n = 1
  int x_c         = ceiling_func_shift(input_dim, EU_SHIFT);
  int x_c_per_npu = ceiling_func_shift(x_c, NPU_SHIFT);
  int x_lmem_size = x_c_per_npu * EU_NUM * FLOAT_SIZE;
  // compute ht lmem size with n = 1
  int ht_lmem_size = wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  // compute ct lmem size with n = 1
  int ct_lmem_size = wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  // compute x_static lmem size with n = 1
  int x_static_lmem_size  = 0;
  int x4_static_lmem_size = 0;
  if (with_x_static == 1) {
    x_static_lmem_size  = x_lmem_size;
    x4_static_lmem_size = wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  }
  // compute xt*4 and ht*4 lmem size with n =1
  int xtx4_lmem_size = wx_c_per_npu * EU_NUM * FLOAT_SIZE;
  int htx4_lmem_size = wx_c_per_npu * EU_NUM * FLOAT_SIZE;

  // compute the remaining local memory size when locate coefficient
  int remain_lmem_size = LOCAL_MEM_SIZE - (wx_lmem_size + wh_lmem_size + wx_static_lmem_size + bx_lmem_size);
  if (remain_lmem_size > 0) {
    method_sel = 1;
  } else {
    method_sel = 2;
  }

  int n_slice             = 0;
  int per_batch_data_size = 0;
  if (method_sel == 1) {
    per_batch_data_size = x_lmem_size + ht_lmem_size + ct_lmem_size + x_static_lmem_size + x4_static_lmem_size +
                          cont_lmem_size + xtx4_lmem_size + htx4_lmem_size;
    n_slice = remain_lmem_size / per_batch_data_size;
    if (n_slice > 0) {
      *batch_slice = n_slice > batch_num ? batch_num : n_slice;
    } else {
      method_sel = 2;
    }
  }

  if (method_sel == 2) {
    int wh_lmem_use_banks = ALIGN(wh_lmem_size, LOCAL_BANK_SIZE) / LOCAL_BANK_SIZE;
    remain_lmem_size      = ((LOCAL_MEM_BANKS - wh_lmem_use_banks) / 2) * LOCAL_BANK_SIZE;
    per_batch_data_size =
        cont_lmem_size + ht_lmem_size + ct_lmem_size + xtx4_lmem_size + htx4_lmem_size + x4_static_lmem_size;
    n_slice = remain_lmem_size / per_batch_data_size;
    if (n_slice < 1) {
      method_sel = 3;
    } else {
      *batch_slice = n_slice > batch_num ? batch_num : n_slice;
    }
  }

  if (method_sel == 3) {
    int wh_partial_lmem_size = ALIGN((1 << OUTPUT_DIM_ALIGN_SHIFT) * NPU_NUM * EU_NUM * FLOAT_SIZE, LOCAL_BANK_SIZE);
    // wh_partial_lmem_size for Wh double buffering
    remain_lmem_size = LOCAL_MEM_SIZE - 4 * wh_partial_lmem_size;
    per_batch_data_size =
        2 * (xtx4_lmem_size + cont_lmem_size + ht_lmem_size) + ct_lmem_size + htx4_lmem_size + x4_static_lmem_size;
    n_slice = remain_lmem_size / per_batch_data_size;
    n_slice = n_slice > batch_num ? batch_num : n_slice;
    while (n_slice > 0) {
      int req_size0      = n_slice * (xtx4_lmem_size + cont_lmem_size + ht_lmem_size);
      int req_size1      = n_slice * (ct_lmem_size + htx4_lmem_size + x4_static_lmem_size);
      int total_req_size = 2 * ALIGN(req_size0, LOCAL_BANK_SIZE) + ALIGN(req_size1, LOCAL_BANK_SIZE);
      if (total_req_size > remain_lmem_size) {
        n_slice--;
      } else {
        break;
      }
    }
    if (n_slice > 0) {
      *batch_slice = n_slice;
    } else {
      method_sel = 0;
    }
  }

  return method_sel;
}
