bmcv_image_yuv2hsv
==================

Convert the specified area of YUV image to HSV format.


**Interface form:**

    .. code-block:: c

         bm_status_t bmcv_image_yuv2hsv(
                 bm_handle_t handle,
                 bmcv_rect_t rect,
                 bm_image    input,
                 bm_image    output);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bmcv_rect_t rect

  Describes the starting coordinates and size of the area to be converted in the original image. Refer to bmcv_image_crop for specific parameters.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to open up new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

bm1684:The interface currently supports the following image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
|  1  | FORMAT_YUV420P         | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+
|  2  | FORMAT_NV12            | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+
|  3  | FORMAT_NV21            | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+

bm1684x: The interface currently

- supports the following  input image_format

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_NV12                   |
+-----+-------------------------------+
|  3  | FORMAT_NV21                   |
+-----+-------------------------------+

- supports the following  output image_format:

+-----+-------------------------------+
| num | output image_format           |
+=====+===============================+
|  1  | FORMAT_HSV180_PACKED          |
+-----+-------------------------------+
|  2  | FORMAT_HSV256_PACKED          |
+-----+-------------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
|  1  | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.



**Code example:**

    .. code-block:: c


        int channel   = 2;
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> res_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        unsigned char * src_data = src_ptr.get();
        unsigned char * res_data = res_ptr.get();
        for (int i = 0; i < channel * width * height; i++) {
            src_data[i] = rand() % 255;
        }
        // calculate res
        bmcv_rect_t rect;
        rect.start_x   = 0;
        rect.start_y   = 0;
        rect.crop_w    = width;
        rect.crop_h    = height;
        bm_image input, output;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_NV12,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input);
        bm_image_alloc_dev_mem(input);
        bm_image_copy_host_to_device(input, (void **)&src_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_HSV_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_yuv2hsv(handle, rect, input, output)) {
            std::cout << "bmcv yuv2hsv error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


