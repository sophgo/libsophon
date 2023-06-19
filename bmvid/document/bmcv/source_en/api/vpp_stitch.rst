bmcv_image_vpp_stitch
=====================

Use the crop function of vpp hardware to complete image stitching. The src crop , csc, resize and dst crop operation can be completed on the input image at one time. The number of small images stitiched in dst image cannot exceed 256.


    .. code-block:: c

        bm_status_t bmcv_image_vpp_stitch(
            bm_handle_t           handle,
            int                   input_num,
            bm_image*             input,
            bm_image              output,
            bmcv_rect_t*          dst_crop_rect,
            bmcv_rect_t*          src_crop_rect = NULL,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);

**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. Handle of device’s capacity (HDC) obtained by calling bm_dev_request.

* int input_num

  Input parameter. The number of input bm_image.

* bm_imagei\* input

  Input parameter. Input bm_image object pointer.

* bm_image output

  Output parameter. Output bm_image object.

* bmcv_rect_t \*   dst_crop_rect

  Input parameter. The coordinates, width and height of each target small image on dst images.

* bmcv_rect_t \*   src_crop_rect

  Input parameter. The coordinates, width and height of each target small image on src image.

  The specific format is defined as follows:

    .. code-block:: c

       typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* bmcv_resize_algorithm algorithm

  Input parameter. Resize algorithm selection, including BMCV_INTER_NEAREST, BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC. By default, it is set as bilinear difference.

  bm1684 supports BMCV_INTER_NEAREST, BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC.

  bm1684x supports BMCV_INTER_NEAREST and BMCV_INTER_LINEAR.

**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Notes:**

1. The src image of this API does not support data in compressed format.

2. The format and some requirements that the API needs to meet are consistent to bmcv_image_vpp_basic.

3. If the src image is cropped, only one target will be cropped for one src image.


