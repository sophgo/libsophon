#ifndef BMCV_1684X_VPP_EXT_H
#define BMCV_1684X_VPP_EXT_H

#ifdef _WIN32
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define DECL_EXPORT
#define DECL_IMPORT
#endif
#include "bmlib_runtime.h"
#include "bmcv_api_ext.h"

DECL_EXPORT bm_status_t bm1684x_vpp_basic(
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

DECL_EXPORT bm_status_t bm1684x_vpp_convert_internal(
    bm_handle_t             handle,
    int                     output_num,
    bm_image                input,
    bm_image*               output,
    bmcv_rect_t*            crop_rect_ = NULL,
    bmcv_resize_algorithm   algorithm = BMCV_INTER_LINEAR,
    csc_matrix_t *          matrix = NULL);

DECL_EXPORT bm_status_t bm1684x_vpp_cvt_padding(
    bm_handle_t             handle,
    int                     output_num,
    bm_image                input,
    bm_image*               output,
    bmcv_padding_atrr_t*    padding_attr = NULL,
    bmcv_rect_t*            crop_rect = NULL,
    bmcv_resize_algorithm   algorithm = BMCV_INTER_LINEAR,
    csc_matrix_t*           matrix = NULL);

DECL_EXPORT bm_status_t bm1684x_vpp_stitch(
    bm_handle_t             handle,
    int                     input_num,
    bm_image*               input,
    bm_image                output,
    bmcv_rect_t*            dst_crop_rect = NULL,
    bmcv_rect_t*            src_crop_rect = NULL,
    bmcv_resize_algorithm   algorithm = BMCV_INTER_LINEAR);

DECL_EXPORT bm_status_t bm1684x_vpp_csc_matrix_convert(
    bm_handle_t           handle,
    int                   output_num,
    bm_image              input,
    bm_image *            output,
    csc_type_t            csc = CSC_MAX_ENUM,
    csc_matrix_t *        matrix = nullptr,
    bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR,
    bmcv_rect_t *         crop_rect = NULL);

DECL_EXPORT bm_status_t bm1684x_vpp_convert_to(
    bm_handle_t          handle,
    int                  input_num,
    bmcv_convert_to_attr convert_to_attr,
    bm_image *           input,
    bm_image *           output);

DECL_EXPORT bm_status_t bm1684x_vpp_draw_rectangle(
    bm_handle_t   handle,
    bm_image      image,
    int           rect_num,
    bmcv_rect_t * rects,
    int           line_width,
    unsigned char r,
    unsigned char g,
    unsigned char b);

DECL_EXPORT bm_status_t bm1684x_vpp_yuv2bgr_ext(
    bm_handle_t  handle,
    int          image_num,
    bm_image *   input,
    bm_image *   output);

DECL_EXPORT bm_status_t bm1684x_vpp_crop(
    bm_handle_t  handle,
    int          crop_num,
    bmcv_rect_t* rects,
    bm_image     input,
    bm_image*    output);

DECL_EXPORT bm_status_t bm1684x_vpp_resize(
    bm_handle_t        handle,
    int                input_num,
    bmcv_resize_image* resize_attr,
    bm_image *         input,
    bm_image *         output);

bm_status_t  bm1684x_vpp_bgrsplit(
    bm_handle_t handle,
    bm_image input,
    bm_image output);

DECL_EXPORT bm_status_t bm1684x_vpp_storage_convert(
    bm_handle_t      handle,
    int              image_num,
    bm_image*        input_,
    bm_image*        output_,
    csc_type_t       csc_type);

DECL_EXPORT bm_status_t bm1684x_vpp_yuv2hsv(
    bm_handle_t     handle,
    bmcv_rect_t     rect,
    bm_image        input,
    bm_image        output);

DECL_EXPORT bm_status_t bm1684x_vpp_copy_to(
    bm_handle_t         handle,
    bmcv_copy_to_atrr_t copy_to_attr,
    bm_image            input,
    bm_image            output);

DECL_EXPORT bm_status_t bm1684x_vpp_yuv_resize(
    bm_handle_t       handle,
    int               input_num,
    bm_image *        input,
    bm_image *        output);

DECL_EXPORT bm_status_t bm1684x_vpp_mosaic(
    bm_handle_t       handle,
    int               mosaic_num,
    bm_image          image,
    bmcv_rect_t *     mosaic_rect);

DECL_EXPORT bm_status_t bm1684x_vpp_put_text(
    bm_handle_t       handle,
    bm_image *        image,
    int               font_num,
    bmcv_rect_t *     font_rects,
    int               font_pitch,
    bm_device_mem_t * font_mem,
    int               font_type,
    unsigned char     r,
    unsigned char     g,
    unsigned char     b);

DECL_EXPORT bm_status_t bm1684x_vpp_basic_v2(
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
    bmcv_convert_to_attr*   convert_to_attr);

#endif
