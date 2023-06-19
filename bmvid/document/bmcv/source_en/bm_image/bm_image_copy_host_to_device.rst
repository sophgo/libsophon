bm_image_copy_host_to_device
============================

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_copy_host_to_device(
                bm_image image,
                void* buffers[]
        );

This API copies the host-side data to the corresponding device memory of bm_image structure.

**Description of incoming parameters:**

* bm_image image

  Input parameter. The bm_image object of the device memory data to be filled.

* void\* buffers[]

  Input parameter. Host side pointer, buffers are pointers to different plane data, and the number should be decided by the number of planes corresponding to image_format when creating bm_image. The amount of data per plane is decided by the width, height, stride, image_format, data_type when creating bm-image. The specific calculation method is as follows:

    .. code-block:: c

        switch (res->image_format) {
            case FORMAT_YUV420P: {
                width[0]  = res->width;
                width[1]  = ALIGN(res->width, 2) / 2;
                width[2]  = width[1];
                height[0] = res->height;
                height[1] = ALIGN(res->height, 2) / 2;
                height[2] = height[1];
                break;
            }
            case FORMAT_YUV422P: {
                width[0]  = res->width;
                width[1]  = ALIGN(res->width, 2) / 2;
                width[2]  = width[1];
                height[0] = res->height;
                height[1] = height[0];
                height[2] = height[1];
                break;
            }
            case FORMAT_YUV444P: {
                width[0]  = res->width;
                width[1]  = width[0];
                width[2]  = width[1];
                height[0] = res->height;
                height[1] = height[0];
                height[2] = height[1];
                break;
            }
            case FORMAT_NV12:
            case FORMAT_NV21: {
                width[0]  = res->width;
                width[1]  = ALIGN(res->width, 2);
                height[0] = res->height;
                height[1] = ALIGN(res->height, 2) / 2;
                break;
            }
            case FORMAT_NV16:
            case FORMAT_NV61: {
                width[0]  = res->width;
                width[1]  = ALIGN(res->width, 2);
                height[0] = res->height;
                height[1] = res->height;
                break;
            }
            case FORMAT_GRAY: {
                width[0]  = res->width;
                height[0] = res->height;
                break;
            }
            case FORMAT_COMPRESSED: {
                width[0]  = res->width;
                height[0] = res->height;
                break;
            }
            case FORMAT_BGR_PACKED:
            case FORMAT_RGB_PACKED: {
                width[0]  = res->width * 3;
                height[0] = res->height;
                break;
            }
            case FORMAT_BGR_PLANAR:
            case FORMAT_RGB_PLANAR: {
                width[0]  = res->width;
                height[0] = res->height * 3;
                break;
            }
            case FORMAT_RGBP_SEPARATE:
            case FORMAT_BGRP_SEPARATE: {
                width[0]  = res->width;
                width[1]  = width[0];
                width[2]  = width[1];
                height[0] = res->height;
                height[1] = height[0];
                height[2] = height[1];
                break;
            }
        }


Therefore, the amount of data corresponding to the buffers of each plane pointed to by the host pointer should be the above calculated plane_byte_size value. For example, FORMAT_BGR_PLANAR only needs the first address of one buffer, while FORMAT_RGBP_SEPARATE needs three buffers.


**Description of returning value:**

BM_SUCCESS when the function returns successfully.


.. note::

    1. If bm_image is not created by bm_image_create, a failure will be returned.

    2. If the incoming bm_image object is not associated with device memory, it will automatically apply for the device memory corresponding to each plane_private->plane_byte_size, and copy the host data to the requested device memory. If the application for device memory fails, the API call will fail.

    3. If the format of the incoming bm_image object is FORMAT_COMPRESSED, it will directly return failure. FORMAT_COMPRESSED does not support copying input by host pointer.

    4. If the copy fails, the API call fails.
