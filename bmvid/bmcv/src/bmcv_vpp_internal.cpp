#include <stdio.h>
#include "bm1684/bmcv_1684_vpp_internal.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"
#include "bmlib_runtime.h"
#include "bmcv_internal.h"

bm_status_t bmcv_image_vpp_basic(
  bm_handle_t           handle,
  int                   in_img_num,
  bm_image*             input,
  bm_image*             output,
  int*                  crop_num_vec,
  bmcv_rect_t*          crop_rect,
  bmcv_padding_atrr_t*  padding_attr,
  bmcv_resize_algorithm algorithm,
  csc_type_t            csc_type,
  csc_matrix_t*         matrix )
{
  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;

  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
    return ret;

  switch(chipid)
  {
#ifndef USING_CMODEL
    case 0x1684:
      ret = bm1684_vpp_basic(handle, in_img_num, input,
        output, crop_num_vec, crop_rect, padding_attr, algorithm, csc_type,matrix);
      break;
#endif

    case BM1684X:
      ret = bm1684x_vpp_basic(handle, in_img_num, input,
        output, crop_num_vec, crop_rect, padding_attr, algorithm, csc_type,matrix);

      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}

bm_status_t bmcv_image_vpp_convert(
  bm_handle_t             handle,
  int                     output_num,
  bm_image                input,
  bm_image*               output,
  bmcv_rect_t*            crop_rect,
  bmcv_resize_algorithm   algorithm )
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

  switch(chipid)
  {
#ifndef USING_CMODEL
    case 0x1684:
      ret = bm1684_vpp_convert_internal(
        handle, output_num, input, output, crop_rect, algorithm, nullptr);

      break;
#endif
    case BM1684X:
      ret = bm1684x_vpp_convert_internal(
        handle, output_num, input, output, crop_rect, algorithm, nullptr);

      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}
bm_status_t bmcv_image_vpp_convert_padding(
  bm_handle_t             handle,
  int                     output_num,
  bm_image                input,
  bm_image*               output,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_rect_t*            crop_rect,
  bmcv_resize_algorithm   algorithm )
{
  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;

  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
    return ret;

  switch(chipid)
  {
#ifndef USING_CMODEL
    case 0x1684:
      ret = bm1684_vpp_cvt_padding(handle, output_num, input, output,\
        padding_attr, crop_rect, algorithm);
      break;
#endif

    case BM1684X:
      ret = bm1684x_vpp_cvt_padding(handle, output_num, input, output,\
        padding_attr, crop_rect, algorithm);
      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}

bm_status_t bmcv_image_vpp_stitch(
  bm_handle_t           handle,
  int                   input_num,
  bm_image*             input,
  bm_image              output,
  bmcv_rect_t*          dst_crop_rect,
  bmcv_rect_t*          src_crop_rect,
  bmcv_resize_algorithm algorithm )
{
  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;

  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
    return ret;

  switch(chipid)
  {
#ifndef USING_CMODEL
    case 0x1684:
      ret = bm1684_vpp_stitch(handle,
        input_num, input, output, dst_crop_rect, src_crop_rect, algorithm);
      break;
#endif

    case BM1684X:
      ret = bm1684x_vpp_stitch(handle,
        input_num, input, output, dst_crop_rect, src_crop_rect, algorithm);
      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}

bm_status_t bmcv_image_vpp_csc_matrix_convert(
  bm_handle_t           handle,
  int                   output_num,
  bm_image              input,
  bm_image*             output,
  csc_type_t            csc,
  csc_matrix_t*         matrix,
  bmcv_resize_algorithm algorithm,
  bmcv_rect_t *         crop_rect )
{
  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;

  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
    return ret;

  switch(chipid)
  {
#ifndef USING_CMODEL
    case 0x1684:
      ret = bm1684_vpp_csc_matrix_convert(
        handle, output_num, input, output, csc, matrix, algorithm, crop_rect);
      break;
#endif

    case BM1684X:
      ret = bm1684x_vpp_csc_matrix_convert(
        handle, output_num, input, output, csc, matrix, algorithm, crop_rect);
      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}

bm_status_t bmcv_image_mosaic_check(bm_handle_t         handle,
                                  int                   mosaic_num,
                                  bm_image              input,
                                  bmcv_rect_t *         crop_rect,
                                  int                   is_expand){
  if(is_expand != 0 && is_expand != 1){
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "is_expand out of range, is_expand=%d, %s: %s: %d\n",
            is_expand, filename(__FILE__), __func__, __LINE__);
    return BM_ERR_FAILURE;
  }
  if(mosaic_num > 512 || mosaic_num < 1){
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "mosaic_num out of range, mosaic_num=%d, %s: %s: %d\n",
            mosaic_num, filename(__FILE__), __func__, __LINE__);
    return BM_ERR_FAILURE;
  }
  if(handle == NULL){
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "handle is nullptr");
    return BM_ERR_FAILURE;
  }
  if(input.image_private == NULL){
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "input is nullptr");
    return BM_ERR_FAILURE;
  }
  for(int i=0; i<mosaic_num; i++){
    crop_rect[i].crop_w = crop_rect[i].crop_w + (is_expand << 4);
    crop_rect[i].crop_h = crop_rect[i].crop_h + (is_expand << 4);
    crop_rect[i].start_x = crop_rect[i].start_x - (is_expand << 3);
    crop_rect[i].start_y = crop_rect[i].start_y - (is_expand << 3);
    if(crop_rect[i].start_x < 0){
      crop_rect[i].crop_w = crop_rect[i].crop_w + crop_rect[i].start_x;
      crop_rect[i].start_x = 0;
    }
    if(crop_rect[i].start_y < 0){
      crop_rect[i].crop_h = crop_rect[i].crop_h + crop_rect[i].start_y;
      crop_rect[i].start_y = 0;
    }
    crop_rect[i].crop_w = VPPALIGN(crop_rect[i].crop_w, MOSAIC_SIZE);
    crop_rect[i].crop_h = VPPALIGN(crop_rect[i].crop_h, MOSAIC_SIZE);
    if(crop_rect[i].crop_w + crop_rect[i].start_x > input.width){
      crop_rect[i].start_x -= ((crop_rect[i].crop_w + crop_rect[i].start_x - input.width) % MOSAIC_SIZE);
      crop_rect[i].crop_w -= ((crop_rect[i].crop_w + crop_rect[i].start_x - input.width) >> 3) << 3;
    }
    if(crop_rect[i].crop_h + crop_rect[i].start_y > input.height){
      crop_rect[i].start_y -= ((crop_rect[i].crop_h + crop_rect[i].start_y - input.height) % MOSAIC_SIZE);
      crop_rect[i].crop_h -= ((crop_rect[i].crop_h + crop_rect[i].start_y - input.height) >> 3) << 3;
    }
    if(crop_rect[i].start_x < 0 || crop_rect[i].start_y < 0 || \
        crop_rect[i].crop_w < 8 || crop_rect[i].crop_h < 8 || \
        crop_rect[i].crop_w + crop_rect[i].start_x > input.width || crop_rect[i].crop_h + crop_rect[i].start_y > input.height){
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "mosaic_rect out of range, i=%d, stx=%d, sty=%d, crop_w=%d, crop_h=%d, image_w=%d, image_h=%d, %s: %s: %d\n",
            i, crop_rect[i].start_x, crop_rect[i].start_y, crop_rect[i].crop_w, crop_rect[i].crop_h, input.width, input.height, filename(__FILE__), __func__, __LINE__);
      return BM_ERR_FAILURE;
    }
  }
  return BM_SUCCESS;
}

bm_status_t bmcv_image_mosaic(bm_handle_t               handle,
                                  int                   mosaic_num,
                                  bm_image              input,
                                  bmcv_rect_t *         mosaic_rect,
                                  int                   is_expand){
  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;
  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
    return ret;
  bmcv_rect_t * crop_rect = new bmcv_rect_t [mosaic_num];
  memcpy(crop_rect, mosaic_rect, sizeof(bmcv_rect_t) * mosaic_num);
  ret = bmcv_image_mosaic_check(handle, mosaic_num, input, crop_rect, is_expand);
  if (BM_SUCCESS != ret)
    goto fail;
  switch(chipid)
  {
    case BM1684X:
      ret = bm1684x_vpp_mosaic(handle, mosaic_num, input, crop_rect);
      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }
fail:
  delete [] crop_rect;
  return ret;
}

bm_status_t bmcv_image_watermark_superpose(bm_handle_t          handle,
                                          bm_image *            image,
                                          bm_device_mem_t *     bitmap_mem,
                                          int                   bitmap_num,
                                          int                   bitmap_type,
                                          int                   pitch,
                                          bmcv_rect_t *         rects,
                                          bmcv_color_t          color)
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;
    bmcv_color_t color_inner;
    if(is_yuv_or_rgb(image->image_format) == COLOR_SPACE_YUV)
      calculate_yuv(color.r, color.g, color.b, &color_inner.r, &color_inner.g, &color_inner.b);
    else{
      color_inner.r = color.r;
      color_inner.g = color.g;
      color_inner.b = color.b;
    }
    switch(chipid)
    {
      case BM1684X:
        ret = bm1684x_vpp_put_text(handle, image, bitmap_num, rects, pitch, bitmap_mem, bitmap_type, color_inner.r, color_inner.g, color_inner.b);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    return ret;
}

bm_status_t bmcv_image_watermark_repeat_superpose(bm_handle_t         handle,
                                                bm_image              image,
                                                bm_device_mem_t       bitmap_mem,
                                                int                   bitmap_num,
                                                int                   bitmap_type,
                                                int                   pitch,
                                                bmcv_rect_t *         rects,
                                                bmcv_color_t          color){
    bm_status_t ret = BM_SUCCESS;
    bm_image *image_inner = new bm_image [bitmap_num];
    bm_device_mem_t *mem_inner = new bm_device_mem_t [bitmap_num];
    for(int i=0; i<bitmap_num; i++){
      image_inner[i] = image;
      mem_inner[i] = bitmap_mem;
    }
    ret = bmcv_image_watermark_superpose(handle, image_inner, mem_inner, bitmap_num, bitmap_type, pitch, rects, color);
    delete [] image_inner;
    delete [] mem_inner;
    return ret;
}
