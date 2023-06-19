#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"

#define SG_API_ID_TOPK 37

bm_status_t bmcv_batch_topk(
        bm_handle_t     handle,
        bm_device_mem_t src_data_addr,
        bm_device_mem_t src_index_addr,
        bm_device_mem_t dst_data_addr,
        bm_device_mem_t dst_index_addr,
        bm_device_mem_t buffer_addr,
        bool            src_index_valid,
        int             k,
        int             batch,
        int *           per_batch_cnt,
        bool            same_batch_cnt,
        int             src_batch_stride,
        bool            descending) {
  bm_device_mem_t src_data_device;
  bm_device_mem_t dst_data_device;
  bm_device_mem_t src_index_device;
  bm_device_mem_t dst_index_device;
  bm_device_mem_t buffer_device;

  std::vector<bm_device_mem_t*> internal_mem_v;

  if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
      BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                       handle, &src_data_device, src_data_addr,
                       true,  // needs copy
                       src_batch_stride * batch * sizeof(float)));
      internal_mem_v.push_back(&src_data_device);
  } else {
      src_data_device = src_data_addr;
  }

  if (src_index_valid && (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM)) {
      BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                       handle, &src_index_device, src_index_addr,
                       true,  // needs copy
                       src_batch_stride * batch * sizeof(int)));
      internal_mem_v.push_back(&src_index_device);
  } else {
      src_index_device = src_index_addr;
  }

  if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
      BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                       handle, &dst_data_device, dst_data_addr,
                       false,
                       batch * k * sizeof(float)));
      internal_mem_v.push_back(&dst_data_device);
  } else {
      dst_data_device = dst_data_addr;
  }

  if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM) {
      BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                       handle, &dst_index_device, dst_index_addr,
                       false,
                       batch * k * sizeof(int)));
      internal_mem_v.push_back(&dst_index_device);
  } else {
      dst_index_device = dst_index_addr;
  }

  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;

  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
      return ret;
  switch (chipid)
  {
  case 0x1684:{
    bm_api_cv_batch_topk_t arg;
    int max_cnt = 0;
    if (same_batch_cnt) {
      max_cnt = per_batch_cnt[0];
    } else {
    for (int i = 0; i < batch; ++i) {
      if (i == 0) max_cnt = per_batch_cnt[i];
      else {
        if (max_cnt < per_batch_cnt[i]) {
          max_cnt = per_batch_cnt[i];
          }
        }
      }
    }

    bool cdma_only = (max_cnt <= 200000 && k < 64);
    if (max_cnt <= 10000 || k == max_cnt) cdma_only = true;
    unsigned int buffer_size = (batch * k + max_cnt) * sizeof(float) * 2;
    if (!cdma_only) buffer_size = ((max_cnt+2047*32*64-1) / (2047*32*64)) * 3 * sizeof(float) * (k>10000 ? 2*k : 10000);
    if ( bm_mem_get_type(buffer_addr) == BM_MEM_TYPE_SYSTEM ||
         bm_mem_get_device_size(buffer_addr) < buffer_size) {
          BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                   handle, &buffer_device, buffer_addr,
                   false,
                   buffer_size / sizeof(float)));
          internal_mem_v.push_back(&buffer_device);
    } else {
      buffer_device = buffer_addr;
    }
    arg.bottom_global_offset = bm_mem_get_device_addr(src_data_device);
    arg.bottom_index_global_offset = bm_mem_get_device_addr(src_index_device);
    arg.top_value_global_offset = bm_mem_get_device_addr(dst_data_device);
    arg.top_index_global_offset = bm_mem_get_device_addr(dst_index_device);
    arg.buffer_global_offset = bm_mem_get_device_addr(buffer_device);
    arg.bottom_index_valid = src_index_valid;
    arg.k = k;
    arg.batch = batch;
    memset(arg.per_batch_size, 0, BMCV_MAX_TOPK_BATCH * sizeof(int));
    if (same_batch_cnt) {
      arg.per_batch_size[0] = per_batch_cnt[0];
    } else {
      memcpy(arg.per_batch_size, per_batch_cnt, batch * sizeof(int));
    }
    arg.per_batch_size_is_same = same_batch_cnt;
    arg.batch_stride = src_batch_stride;
    arg.descending = descending;
    ret = bm_send_api(handle,  BM_API_ID_CV_BATCH_TOPK,
                                   (uint8_t*)&arg, sizeof(arg));
    if (BM_SUCCESS != ret) {
      BMCV_ERR_LOG("batch topk send api error\r\n");
      goto free_devmem;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
      BMCV_ERR_LOG("batch topk sync api error\r\n");
      goto free_devmem;
    }
    break;
  }

  case 0x1686:{
    sg_api_topk_1684x_t api = {
      bm_mem_get_device_addr(src_data_device),
      bm_mem_get_device_addr(src_index_device),
      bm_mem_get_device_addr(dst_data_device),
      bm_mem_get_device_addr(dst_index_device),
      0,
      src_index_valid,
      k,
      descending,
      batch,
      per_batch_cnt[0],
      src_batch_stride,
      0
      };

    bm_status_t status = BM_SUCCESS;
    status = bm_kernel_main_launch(handle, SG_API_ID_TOPK, (uint8_t*)&api, sizeof(api));
    // bm_status_t status = bm_send_api(handle, SG_API_ID_TOPK,
    //                                (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != status) {
        BMCV_ERR_LOG("batch topk send api error\r\n");
        goto free_devmem;
    }
    // status = bm_sync_api(handle);
    // if (BM_SUCCESS != status) {
    //     BMCV_ERR_LOG("batch topk sync api error\r\n");
    //     goto free_devmem;
    // }
    break;
  }
  default:
    ret = BM_NOT_SUPPORTED;
    break;
  }

  // Copy data
  if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
    ret = bm_memcpy_d2s_partial(handle, bm_mem_get_system_addr(dst_data_addr),
                                   dst_data_device, batch * k * sizeof(float));
    if (BM_SUCCESS != ret) {
      BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
      goto free_devmem;
    }
  }
  if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM) {
    ret = bm_memcpy_d2s_partial(handle, bm_mem_get_system_addr(dst_index_addr),
                                   dst_index_device, batch * k * sizeof(int));
    if (BM_SUCCESS != ret) {
      BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
      goto free_devmem;
    }
  }

free_devmem:
  for (auto mem : internal_mem_v) {
    bm_free_device(handle, *mem);
  }

  return (ret == BM_SUCCESS) ? BM_SUCCESS : BM_ERR_FAILURE;
}




