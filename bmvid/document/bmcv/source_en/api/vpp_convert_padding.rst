bmcv_image_vpp_convert_padding
==============================

  The effect of image padding is implemented by using VPP hardware resources and memset operation on dst image. This effect is done through the function of dst crop of vpp. Generally speaking, it is to fill a small image into a large one. Users can crop multiple target images from one src image. For each small target image, users can complete the csc and resize operation at one time, and then fill it into the large image according to its offset information in the large image. The number of crop at a time cannot exceed 256.


    .. code-block:: c

        bm_status_t bmcv_image_vpp_convert_padding(
            bm_handle_t           handle,
            int                   output_num,
            bm_image              input,
            bm_image *            output,
            bmcv_padding_atrr_t * padding_attr,
            bmcv_rect_t *         crop_rect = NULL,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);

**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* int output_num

  Output parameter. The number of output bm_image, which is equal to the number of crop of SRC image. One src crop outputs one dst bm_image.

* bm_image input

  Input parameter. Input bm_image object.

* bm_image\* output

  Output parameter. Output bm_image object pointer.

* bmcv_padding_atrr_t \*  padding_attr

  Input parameter. The location information of the target thumbnail of src crop in the dst image and the pixel values of each channel to be pdding.

    .. code-block:: c

        typedef struct bmcv_padding_atrr_s {
            unsigned int    dst_crop_stx;
            unsigned int    dst_crop_sty;
            unsigned int    dst_crop_w;
            unsigned int    dst_crop_h;
            unsigned char padding_r;
            unsigned char padding_g;
            unsigned char padding_b;
            int           if_memset;
        } bmcv_padding_atrr_t;


1. Offset information of the top left corner of the target small image relative to the dst image origin (top left corner): dst_crop_stx and dst_crop_sty;
2. Width and height of the target small image after resizing: dst_crop_w and dst_crop_h;
3. If dst image is in RGB format, each channel needs padding pixel value information: padding_r, padding_g, padding_b, which is valid when if_memset=1. If it is a GRAY image, users can set all three values to the same;
4. if_memset indicates whether to memset dst image according to the padding value of each channel within the API.

* bmcv_rect_t \*   crop_rect

  Input parameter. Coordinates, width and height of each target small image on src image.

  The specific format is defined as follows:

    .. code-block:: c

       typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* bmcv_resize_algorithm algorithm

  Input parameter. Resize algorithm selection, including BMCV_INTER_NEAREST、BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC . By default, it is set as bilinear difference.

  bm1684 supports BMCV_INTER_NEAREST,BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC.

  bm1684x supports BMCV_INTER_NEAREST and BMCV_INTER_LINEAR.

**Description of returning value:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The format of dst image of this API only supports:

+-----+-------------------------------+
| num | dst image_format              |
+=====+===============================+
|  1  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  2  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  3  | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  4  | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  5  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  6  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+

2. The format and some requirements that the API needs to meet are consistent to bmcv_image_vpp_basic.


