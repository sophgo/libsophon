#include <stdio.h>
#include <vector>
#include <memory>
#include <functional>
#include "bm1684/bmcv_1684_vpp_internal.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"
#include "bmlib_runtime.h"
#include "bmcv_internal.h"

static bm_status_t bm1684_image_csc_convert_to_check(
  bm_handle_t             handle,
  int                     img_num,
  int                     crop_num,
  bm_image*               input,
  bm_image*               output,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_convert_to_attr*   convert_to_attr)
{
    bm_status_t ret = BM_SUCCESS;
    if (handle == NULL)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "handle is nullptr");
        return BM_ERR_FAILURE;
    }
    if (input == NULL || output == NULL)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "input or output is nullptr");
        return BM_ERR_FAILURE;
    }
    for(int i = 0; i < img_num; i++){
        if(input[i].data_type != DATA_TYPE_EXT_1N_BYTE)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp only support DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
        if(input[i].image_format != input[0].image_format)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "input image list format must be the same %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
    }
    for(int i = 0; i < crop_num; i++){
        if(output[i].data_type != DATA_TYPE_EXT_1N_BYTE)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp only support DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
        if(output[i].image_format < FORMAT_RGB_PLANAR || output[i].image_format > FORMAT_BGR_PLANAR)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "output image format not supposted %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
        if(output[i].image_format != output[0].image_format)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "output image list format must be the same %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
    }
    if (padding_attr == NULL){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "padding_attr should not be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    if (convert_to_attr == NULL){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "convert_to_attr should not be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    return ret;
}

static bm_status_t bm1684_image_csc_convert_to(
  bm_handle_t             handle,
  int                     img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec_,
  bmcv_rect_t*            crop_rect_,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr)
{
    int crop_idx = 0, crop_num = 0;
    bm_status_t ret = BM_SUCCESS;
    int* crop_num_vec = NULL;
    bmcv_rect_t *crop_rect = NULL;
    if(crop_rect_ == NULL && crop_num_vec_ == NULL){
        crop_num = img_num;
    } else if (crop_rect_ != NULL && crop_num_vec_ != NULL){
        for(int i=0; i<img_num; i++){
            crop_num = crop_num + crop_num_vec_[i];
        }
        crop_num_vec = crop_num_vec_;
        crop_rect = crop_rect_;
    } else {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "crop_num_vec or crop_rect should not be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    ret = bm1684_image_csc_convert_to_check(handle, img_num, crop_num, input, output, padding_attr, convert_to_attr);
    if(ret != BM_SUCCESS)
        return ret;

    if(crop_rect_ == NULL){
        crop_num_vec = new int [img_num];
        crop_rect = new bmcv_rect_t [img_num];
        for(int i=0; i<img_num; i++){
            crop_num_vec[i] = 1;
        }
    }

    bm_image* output_crop = new bm_image [crop_num];
    bm_image* output_convertto = new bm_image [crop_num];

    for(int i = 0; i < crop_num; i++){
        int stride = VPPALIGN(padding_attr[i].dst_crop_w, 64);
        bm_image_create(handle, padding_attr[i].dst_crop_h, padding_attr[i].dst_crop_w, output->image_format, output->data_type, &output_crop[i], &stride);
        ret = bm_image_alloc_dev_mem(output_crop[i], BMCV_HEAP1_ID);
        if(ret != BM_SUCCESS){
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "bm_image_alloc_dev_mem failed %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            goto fail2;
        }
    }
    for(int i = 0; i < crop_num; i++){
        bm_image_create(handle, padding_attr[i].dst_crop_h, padding_attr[i].dst_crop_w, output->image_format, output->data_type, &output_convertto[i]);
        ret = bm_image_alloc_dev_mem(output_convertto[i], BMCV_HEAP1_ID);
        if(ret != BM_SUCCESS){
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "bm_image_alloc_dev_mem failed %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            goto fail1;
        }
    }

    csc_matrix_t black_matrix;
    bmcv_copy_to_atrr_t copy_to_attr;
    memset(&black_matrix, 0, sizeof(csc_matrix_t));
    ret = bm1684_vpp_csc_matrix_convert(handle, crop_num, input[0], output, CSC_USER_DEFINED_MATRIX, &black_matrix);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "get black background failed %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        goto fail1;
    }
    crop_idx = 0;
    for(int i = 0; i < img_num; i++){
        ret = bm1684_vpp_csc_matrix_convert(handle, crop_num_vec[i], input[i], &output_crop[crop_idx], csc_type, matrix, algorithm, crop_rect + crop_idx);
        if(ret != BM_SUCCESS){
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "get crop bm_image failed %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            goto fail1;
        }
        for(int j = 0; j < crop_num_vec[i]; j++){
            ret = bmcv_image_convert_to(handle, 1, convert_to_attr[0], &output_crop[crop_idx], &output_convertto[crop_idx]);
            if(ret != BM_SUCCESS){
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "bmcv_image_convert_to failed %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
                goto fail1;
            }
            copy_to_attr = {(int)padding_attr[crop_idx].dst_crop_stx, (int)padding_attr[crop_idx].dst_crop_sty, 0, 0, 0, 0};
            ret = bmcv_image_copy_to(handle, copy_to_attr, output_convertto[crop_idx], output[crop_idx]);
            if(ret != BM_SUCCESS){
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "bmcv_image_copy_to failed %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
                goto fail1;
            }
            crop_idx += 1;
        }
    }
fail1:
    for (int i = 0; i < crop_num; i++){
        bm_image_destroy(output_convertto[i]);
    }
fail2:
    for (int i = 0; i < crop_num; i++){
        bm_image_destroy(output_crop[i]);
    }
    delete [] output_crop;
    delete [] output_convertto;
    if (crop_rect_ == NULL){
        delete [] crop_num_vec;
        delete [] crop_rect;
    }
    return ret;
}

bm_status_t bmcv_image_vpp_basic_v2(
  bm_handle_t             handle,
  int                     img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr)
{
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid = BM1684X;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;
    switch(chipid)
    {
        case BM1684X:
            ret = bm1684x_vpp_basic_v2(handle, img_num, input,
                output, crop_num_vec, crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr);
            break;

        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    return ret;
}

bm_status_t bmcv_image_csc_convert_to(
  bm_handle_t             handle,
  int                     img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr)
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
            ret = bm1684_image_csc_convert_to(handle, img_num, input,
                output, crop_num_vec, crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr);
            break;
    #endif

        case BM1684X:
            ret = bmcv_image_vpp_basic_v2(handle, img_num, input,
                output, crop_num_vec, crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr);
            break;

        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }

    return ret;
}
