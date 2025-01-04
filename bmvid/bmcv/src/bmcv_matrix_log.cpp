#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <stdio.h>

bm_status_t check_matrix_log_param(bm_image src, bm_image dst)
{
  bm_status_t ret = BM_SUCCESS;
  if((src.height != dst.height) || (src.width!= dst.width))
  {
    bmlib_log("cv_matrix_log", BMLIB_LOG_ERROR, "matrix_log ,src.height != dst.height || src.width!= dst.width\n");
    ret = BM_ERR_PARAM;
    goto exit;
  }
  if((src.data_type != DATA_TYPE_EXT_FLOAT32 ) || (dst.data_type != DATA_TYPE_EXT_FLOAT32))
  {
    bmlib_log("cv_matrix_log", BMLIB_LOG_ERROR, "matrix_log ,src.data_type != DATA_TYPE_EXT_FLOAT32  || dst.data_type != DATA_TYPE_EXT_FLOAT32\n");
    ret = BM_ERR_PARAM;
    goto exit;
  }

  if((src.image_format != FORMAT_GRAY ) || (dst.image_format != FORMAT_GRAY))
  {
    bmlib_log("cv_matrix_log", BMLIB_LOG_ERROR, "matrix_log ,src.image_format != FORMAT_GRAY || dst.image_format != FORMAT_GRAY\n");
    ret = BM_ERR_PARAM;
    goto exit;
  }

  if((src.height > 65000) || (src.width > 65000) || (src.height < 2) || (src.width < 2))
  {
    bmlib_log("cv_matrix_log", BMLIB_LOG_ERROR, "matrix_log ,matrix width and height should be 2~65000\n");
    ret = BM_ERR_PARAM;
    goto exit;
  }

exit:
  return ret;
}

bm_status_t bmcv_matrix_log(bm_handle_t handle, bm_image src, bm_image dst)
{
  bm_status_t ret = BM_SUCCESS;
  bm_device_mem_t s_mem[4],d_mem[4];
  bm_handle_check_2(handle, src, dst);
  ret = check_matrix_log_param(src,dst);

  if (BM_SUCCESS != ret)
    return ret;

  bm_image_get_device_mem(src, s_mem);
  bm_image_get_device_mem(dst, d_mem);

  bm_matrix_log_t api;
  api.s_addr = bm_mem_get_device_addr(s_mem[0]);
  api.d_addr = bm_mem_get_device_addr(d_mem[0]);
  api.row = src.height;
  api.col = src.width;
  data_type_conversion(src.data_type,&api.s_dtype);
  data_type_conversion(dst.data_type,&api.d_dtype);

  unsigned int chipid;
  bm_get_chipid(handle, &chipid);

  switch(chipid)
  {
    case BM1684X:
      ret = bm_tpu_kernel_launch(handle, "cv_matrix_log", (u8 *)&api, sizeof(api));
      if(BM_SUCCESS != ret){
        bmlib_log("cv_matrix_log", BMLIB_LOG_ERROR, "bmcv_matrix_log error\n");
      }
      break;

    default:
      bmlib_log("cv_matrix_log", BMLIB_LOG_ERROR, "chipid %x,bmcv_matrix_log NOT_SUPPORTED\n",chipid);
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}

