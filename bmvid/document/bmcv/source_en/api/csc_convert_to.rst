bmcv_image_csc_convert_to
=========================

  The API can combine  crop, color-space-convert, resize, padding, convert_to, and any number of functions for multiple images.

    .. code-block:: c

        bm_status_t bmcv_image_csc_convert_to(
            bm_handle_t           handle,
            int                   in_img_num,
            bm_image*             input,
            bm_image*             output,
            int*                  crop_num_vec = NULL,
            bmcv_rect_t*          crop_rect = NULL,
            bmcv_padding_atrr_t*  padding_attr = NULL,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR,
            csc_type_t            csc_type = CSC_MAX_ENUM,
            csc_matrix_t*         matrix = NULL,
            bmcv_convert_to_attr* convert_to_attr);


**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* int in_img_num

  Input parameter. The number of input bm_image.

* bm_image* input

  Input parameter. Input bm_image object pointer whose length to the space is decided by in_img_num.

* bm_image* output

  Output parameter. Output bm_image image object pointer whose length to the space is jointly decided by in_img_num and crop_num_vec, that is, the sum of the number of crops of all input images.

* int* crop_num_vec = NULL

  Input parameter. The pointer points to the number of crops for each input image, and the length of the pointing space is decided by in_img_num. NULL can be filled in if the crop function is not used.

* bmcv_rect_t * crop_rect = NULL

  Input parameter. The specific format is defined as follows:

    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;

  The parameters of the crop on the input image corresponding to each output bm_image object, including the X coordinate of the starting point, the Y coordinate of the starting point, the width of the crop image and the height of the crop image. The top left vertex of the image is used as the coordinate origin. If you do not use the crop function, you can fill in NULL.

* bmcv_padding_atrr_t*  padding_attr = NULL

  Input parameter. The location information of the target thumbnails of all crops in the dst image and the pixel values of each channel to be padding. If you do not use the padding function, you can set the function to NULL.

    .. code-block:: c

        typedef struct bmcv_padding_atrr_s {
            unsigned int  dst_crop_stx;
            unsigned int  dst_crop_sty;
            unsigned int  dst_crop_w;
            unsigned int  dst_crop_h;
            unsigned char padding_r;
            unsigned char padding_g;
            unsigned char padding_b;
            int           if_memset;
        } bmcv_padding_atrr_t;



    1. Offset information of the top left corner vertex of the target thumbnail relative to the dst image origin (top left corner): dst_crop_stx and dst_crop_sty;
    2. The width and height of the target thumbnail after resize: dst_crop_w and dst_crop_h;
    3. If dst image is in RGB format, the required pixel value information of each channel: padding_r, padding_g, padding_b. When if_memset=1, it is valid. If it is a GRAY image, you can set all three values to the same value;
    4. if_memset indicates whether to memset dst image according to the padding value of each channel within the API. Only images in RGB and GRAY formats are supported. If it is set to 0, users need to call the API in bmlib according to the pixel value information of padding before calling the API to directly perform memset operation on device memory. If users are not concerned about the value of padding, it can be set to 0 to ignore this step.

* bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR

  Input parameter. Resize algorithm selection, including BMCV_INTER_NEAREST, BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC, which is the bilinear difference by default.

  - bm1684 supports : BMCV_INTER_NEAREST,

    BMCV_INTER_LINEAR, BMCV_INTER_BICUBIC.

  - bm1684x supports:

    BMCV_INTER_NEAREST, BMCV_INTER_LINEAR.

* csc_type_t csc_type = CSC_MAX_ENUM

  Input parameters. color space convert Parameter type selection, fill CSC_MAX_ENUM then use the default value. The default is CSC_YCbCr2RGB_BT601 or CSC_RGB2YCbCr_BT601. The supported types include:

  +----------------------------+
  | CSC_YCbCr2RGB_BT601        |
  +----------------------------+
  | CSC_YPbPr2RGB_BT601        |
  +----------------------------+
  | CSC_RGB2YCbCr_BT601        |
  +----------------------------+
  | CSC_YCbCr2RGB_BT709        |
  +----------------------------+
  | CSC_RGB2YCbCr_BT709        |
  +----------------------------+
  | CSC_RGB2YPbPr_BT601        |
  +----------------------------+
  | CSC_YPbPr2RGB_BT709        |
  +----------------------------+
  | CSC_RGB2YPbPr_BT709        |
  +----------------------------+
  | CSC_USER_DEFINED_MATRIX    |
  +----------------------------+
  | CSC_MAX_ENUM               |
  +----------------------------+

* csc_matrix_t* matrix = NULL

Input parameter for the selection of color space convert parameter type. Fill in CSC_MAX_ENUM to use the default value, which is by default CSC_YCbCr2RGB_BT601 or CSC_RGB2YCbCr_BT601. The supported types include:

    .. code-block:: c

          typedef struct {
              int csc_coe00;
              int csc_coe01;
              int csc_coe02;
              int csc_add0;
              int csc_coe10;
              int csc_coe11;
              int csc_coe12;
              int csc_add1;
              int csc_coe20;
              int csc_coe21;
              int csc_coe22;
              int csc_add2;
          } __attribute__((packed)) csc_matrix_t;

* bmcv_convert_to_attr* convert_to_attr

Input parameter for linear transformation coefficient.

    .. code-block:: c

        typedef struct bmcv_convert_to_attr_s{
                float alpha_0;
                float beta_0;
                float alpha_1;
                float beta_1;
                float alpha_2;
                float beta_2;
        } bmcv_convert_to_attr;


* alpha_0 describes the coefficient of the linear transformation of the 0th channel

* beta_0 describes the offset of the linear transformation of the 0th channel

* alpha_1 describes the coefficient of the linear transformation of the 1st channel

* beta_1 describes the offset of linear transformation of the 1st channel

* alpha_2 describes the coefficient of the linear transformation of the 2nd channel

* beta_2 describes the offset of linear transformation of the 2nd channel


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

- bm1684x supports the following:

1. bm1684x supports the following data_type:

+-----+------------------------+-------------------------------+
| num | input data_type        | output data_type              |
+=====+========================+===============================+
|  1  |                        | DATA_TYPE_EXT_FLOAT32         |
+-----+                        +-------------------------------+
|  2  |                        | DATA_TYPE_EXT_1N_BYTE         |
+-----+                        +-------------------------------+
|  3  | DATA_TYPE_EXT_1N_BYTE  | DATA_TYPE_EXT_1N_BYTE_SIGNED  |
+-----+                        +-------------------------------+
|  4  |                        | DATA_TYPE_EXT_FP16            |
+-----+                        +-------------------------------+
|  5  |                        | DATA_TYPE_EXT_BF16            |
+-----+------------------------+-------------------------------+


2. bm1684x supports the following color formats of input bm_image:

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_YUV422P                |
+-----+-------------------------------+
|  3  | FORMAT_YUV444P                |
+-----+-------------------------------+
|  4  | FORMAT_NV12                   |
+-----+-------------------------------+
|  5  | FORMAT_NV21                   |
+-----+-------------------------------+
|  6  | FORMAT_NV16                   |
+-----+-------------------------------+
|  7  | FORMAT_NV61                   |
+-----+-------------------------------+
|  8  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  9  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  10 | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  11 | FORMAT_BGR_PACKED             |
+-----+-------------------------------+
|  12 | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  13 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  14 | FORMAT_GRAY                   |
+-----+-------------------------------+
|  15 | FORMAT_COMPRESSED             |
+-----+-------------------------------+
|  16 | FORMAT_YUV444_PACKED          |
+-----+-------------------------------+
|  17 | FORMAT_YVU444_PACKED          |
+-----+-------------------------------+
|  18 | FORMAT_YUV422_YUYV            |
+-----+-------------------------------+
|  19 | FORMAT_YUV422_YVYU            |
+-----+-------------------------------+
|  20 | FORMAT_YUV422_UYVY            |
+-----+-------------------------------+
|  21 | FORMAT_YUV422_VYUY            |
+-----+-------------------------------+


3. bm1684x supports the following color formats of output bm_image:

+-----+-------------------------------+
| num | output image_format           |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_YUV444P                |
+-----+-------------------------------+
|  3  | FORMAT_NV12                   |
+-----+-------------------------------+
|  4  | FORMAT_NV21                   |
+-----+-------------------------------+
|  5  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  6  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  7  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  8  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+
|  9  | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  10 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  11 | FORMAT_GRAY                   |
+-----+-------------------------------+
|  12 | FORMAT_RGBYP_PLANAR           |
+-----+-------------------------------+
|  13 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  14 | FORMAT_HSV180_PACKED          |
+-----+-------------------------------+
|  15 | FORMAT_HSV256_PACKED          |
+-----+-------------------------------+

4. bm1684x vpp does not support FORMAT_COMPRESSED to FORMAT_HSV180_PACKED or FORMAT_HSV256_PACKED

5. The zoom ratio of the image ((crop.width / output.width) and (crop.height / output.height)) is limited to 1 / 128 ~ 128.

6. The width and height (src.width, src.height, dst.width, dst.height) of input and output are limited to 8 ~ 8192.

7. The input must be associated with device memory, otherwise, a failure will be returned.

8. The usage of FORMAT_COMPRESSED format is described in the bm1684 section.


- bm1684 supports the following:

1. The format and some requirements that the API needs to meet are shown in the following table:

+------------------+---------------------+-----------------+
| src format       | dst format          | Other Limitation|
+==================+=====================+=================+
| RGB_PACKED       | RGB_PLANAR          |  Condition 1    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 1    |
+------------------+---------------------+-----------------+
| BGR_PACKED       | RGB_PLANAR          |  Condition 1    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 1    |
+------------------+---------------------+-----------------+
| RGB_PLANAR       | RGB_PLANAR          |  Condition 1    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 1    |
+------------------+---------------------+-----------------+
| BGR_PLANAR       | RGB_PLANAR          |  Condition 1    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 1    |
+------------------+---------------------+-----------------+
| RGBP_SEPARATE    | RGB_PLANAR          |  Condition 1    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 1    |
+------------------+---------------------+-----------------+
| BGRP_SEPARATE    | RGB_PLANAR          |  Condition 1    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 1    |
+------------------+---------------------+-----------------+
| GRAY             | GRAY                |  Condition 1    |
+------------------+---------------------+-----------------+
| YUV420P          | RGB_PLANAR          |  Condition 4    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 4    |
+------------------+---------------------+-----------------+
| NV12             | RGB_PLANAR          |  Condition 4    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 4    |
+------------------+---------------------+-----------------+
| COMPRESSED       | RGB_PLANAR          |  Condition 4    |
|                  +---------------------+-----------------+
|                  | BGR_PLANAR          |  Condition 4    |
+------------------+---------------------+-----------------+

of which:

     - Condition 1: src.width >= crop.x + crop.width, src.height >= crop.y + crop.height
     - Condition 2: src.width, src.height, dst.width, dst.height must be an integral multiple of 2, src.width >= crop.x + crop.width, src.height >= crop.y + crop.heigh
     - Condition 3: dst.width, dst.height must be an integral multiple of 2, src.width == dst.width, src.height == dst.height, crop.x == 0, crop.y == 0, src.width >= crop.x + crop.width, src.height >= crop.y + crop.height
     - Condition 4: src.width, src.height must be an integral multiple of 2, src.width >= crop.x + crop.width, src.height >= crop.y + crop.height

2. The device mem of input bm_image cannot be on heap0.

3. The stride of all input and output images must be 64 aligned.

4. The addresses of all input and output images must be aligned with 32 byte.

5. The zoom ratio of the image ((crop.width / output.width) and (crop.height / output.height)) is limited to 1 / 32 ~ 32.

6. The width and height (src.width, src.height, dst.width, dst.height) of input and output are limited to 16 ~ 4096.

7. The input must be associated with device memory, otherwise, a failure will be returned.

8.  FORMAT_COMPRESSED is a built-in compression format after VPU decoding. It includes four parts: Y compressed table, Y compressed data, CbCr compressed table and CbCr compressed data. Please note the order of the four parts in bm_image is slightly different from that of the AVFrame in FFMPEG. If you need to attach the device memory data in AVFrame to bm_image, the corresponding relationship is as follows. For details of AVFrame, please refer to "VPU User Manual".

    .. code-block:: c

        bm_device_mem_t src_plane_device[4];
        src_plane_device[0] = bm_mem_from_device((u64)avframe->data[6],
                avframe->linesize[6]);
        src_plane_device[1] = bm_mem_from_device((u64)avframe->data[4],
                avframe->linesize[4] * avframe->h);
        src_plane_device[2] = bm_mem_from_device((u64)avframe->data[7],
                avframe->linesize[7]);
        src_plane_device[3] = bm_mem_from_device((u64)avframe->data[5],
                avframe->linesize[4] * avframe->h / 2);

        bm_image_attach(*compressed_image, src_plane_device);



