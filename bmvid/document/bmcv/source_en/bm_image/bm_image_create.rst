bm_image_create
===============

We do not recommend that users directly fill bm_image structure, but create/destroy bm_image structure through the following API:

**Interface form:**

    .. code-block:: c

       bm_status_t  bm_image_create(
           bm_handle_t handle,
           int img_h,
           int img_w,
           bmcv_image_format_ext image_format,
           bmcv_data_format_ext data_type,
           bm_image *image,
           int* stride);

**Description of incoming parameters:**

* bm_handle_t handle

  input parameter. HDC (handle of device’s capacity) to be obtained through bm_dev_request

* int img_h

  Input parameter. Image height

* int img_w

  Input parameter. Image width

* bmcv_image_format_ext image_format

  Input parameter. Create bm_image format as required. The supported image formats are introduced in bm_image_format_ext.

* bm_image_format_ext data_type

  Input parameter. Create bm_image data format as required. The supported data formats are introduced in bm_image_data_format_ext

* bm_image \*image

  Output parameter. Output filled bm_image structure pointer

* int* stride

  Input parameter. Stride describes the device memory layout associated with the created bm-image. The width stride value of each plane is counted in bytes.Defaults to the same width as a line of data (in BYTE count) when not filled.


**Description of returning parameters:**

The successful call of bmcv_image_create will return to BM_SUCCESS, and fill in the output image pointer structure. This structure records the size and related format of the image. But at this time, it is not associated with any device memory, and there is no device memory corresponding to the application data.


**Notice:**

1) The width and height of the following picture formats can be odd, and the interface will be adjusted to even numbers before completing the corresponding functions. However, it is recommended to use width and height with even numbers so as to maximize efficiency.

       * FORMAT_YUV420P
       * FORMAT_NV12
       * FORMAT_NV21
       * FORMAT_NV16
       * FORMAT_NV61


2)  The width or stride of images with the format of FORMAT_COMPRESSED must be 64 aligned, otherwise it will return failure.


3) The default value of the stride parameter is NULL. At this time, the data of each plane is packed in compact, not in stride by default.


4) If the stride is not NULL, it will check whether the width stride value in the stride is legal. The so-called legality means the stride of all planes corresponding to image_format is greater than the default stride. The default stride value is calculated as follows:

     .. code-block:: c

        int data_size = 1;
        switch (data_type) {
            case DATA_TYPE_EXT_FLOAT32:
                data_size = 4;
                break;
            case DATA_TYPE_EXT_4N_BYTE:
            case DATA_TYPE_EXT_4N_BYTE_SIGNED:
                data_size = 4;
                break;
            default:
                data_size = 1;
                break;
        }
        int default_stride[3] = {0};
        switch (image_format) {
            case FORMAT_YUV420P: {
                image_private->plane_num = 3;
                default_stride[0] = width * data_size;
                default_stride[1] = (ALIGN(width, 2) >> 1) * data_size;
                default_stride[2] = default_stride[1];
                break;
            }
            case FORMAT_YUV422P: {
                default_stride[0] = res->width * data_size;
                default_stride[1] = (ALIGN(res->width, 2) >> 1) * data_size;
                default_stride[2] = default_stride[1];
                break;
            }
            case FORMAT_YUV444P: {
                default_stride[0] = res->width * data_size;
                default_stride[1] = res->width * data_size;
                default_stride[2] = default_stride[1];
                break;
            }
            case FORMAT_NV12:
            case FORMAT_NV21: {
                image_private->plane_num = 2;
                default_stride[0] = width * data_size;
                default_stride[1] = ALIGN(res->width, 2) * data_size;
                break;
            }
            case FORMAT_NV16:
            case FORMAT_NV61: {
                image_private->plane_num = 2;
                default_stride[0] = res->width * data_size;
                default_stride[1] = ALIGN(res->width, 2) * data_size;
                break;
            }
            case FORMAT_GRAY: {
                image_private->plane_num = 1;
                default_stride[0] = res->width * data_size;
                break;
            }
            case FORMAT_COMPRESSED: {
                image_private->plane_num = 4;
                break;
            }
            case FORMAT_BGR_PACKED:
            case FORMAT_RGB_PACKED: {
                image_private->plane_num = 1;
                default_stride[0] = res->width * 3 * data_size;
                break;
            }
            case FORMAT_BGR_PLANAR:
            case FORMAT_RGB_PLANAR: {
                image_private->plane_num = 1;
                default_stride[0] = res->width * data_size;
                break;
            }
            case FORMAT_BGRP_SEPARATE:
            case FORMAT_RGBP_SEPARATE: {
                image_private->plane_num = 3;
                default_stride[0] = res->width * data_size;
                default_stride[1] = res->width * data_size;
                default_stride[2] = res->width * data_size;
                break;
            }
        }
