
#ifndef BMCV_1684_VPP_INTERANL_H
#define BMCV_1684_VPP_INTERANL_H

#ifdef _WIN32
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define DECL_EXPORT
#define DECL_IMPORT
#endif
#include "bmlib_runtime.h"
#include "bmcv_api_ext.h"

DECL_EXPORT bm_status_t bm1684_vpp_basic(
    bm_handle_t             handle,
    int                     in_img_num,
    bm_image*               input,
    bm_image*               output,
    int*                    crop_num_vec = NULL,
    bmcv_rect_t*            crop_rect = NULL,
    bmcv_padding_atrr_t*    padding_attr = NULL,
    bmcv_resize_algorithm   algorithm = BMCV_INTER_LINEAR,
    csc_type_t              csc_type = CSC_MAX_ENUM,
    csc_matrix_t*           matrix = NULL);

DECL_EXPORT bm_status_t bm1684_vpp_convert_internal(
    bm_handle_t             handle,
    int                     output_num,
    bm_image                input,
    bm_image*               output,
    bmcv_rect_t*            crop_rect_,
    bmcv_resize_algorithm   algorithm,
    csc_matrix_t *          matrix );

DECL_EXPORT bm_status_t bm1684_vpp_cvt_padding(
    bm_handle_t             handle,
    int                     output_num,
    bm_image                input,
    bm_image*               output,
    bmcv_padding_atrr_t*    padding_attr,
    bmcv_rect_t*            crop_rect,
    bmcv_resize_algorithm   algorithm,
    csc_matrix_t*           matrix = NULL);

DECL_EXPORT bm_status_t bm1684_vpp_stitch(
    bm_handle_t             handle,
    int                     input_num,
    bm_image*               input,
    bm_image                output,
    bmcv_rect_t*            dst_crop_rect,
    bmcv_rect_t*            src_crop_rect = NULL,
    bmcv_resize_algorithm   algorithm = BMCV_INTER_LINEAR);

DECL_EXPORT bm_status_t bm1684_vpp_csc_matrix_convert(
    bm_handle_t           handle,
    int                   output_num,
    bm_image              input,
    bm_image *            output,
    csc_type_t            csc,
    csc_matrix_t *        matrix = nullptr,
    bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR,
    bmcv_rect_t *         crop_rect = NULL);


#endif
