bmcv_image_sobel
================

Sobel operator for edge detection.


**Processor model support**

This interface only supports BM1684.


**Interface form:**

    .. code-block:: c

         bm_status_t bmcv_image_sobel(
                 bm_handle_t handle,
                 bm_image input,
                 bm_image output,
                 int dx,
                 int dy,
                 int ksize = 3,
                 float scale = 1,
                 float delta = 0);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.

* int dx

  The difference order in the x direction.

* int dy

  The difference order in the y direction.

* int ksize = 3

  The size of Sobel core, which must be - 1,1,3,5 or 7. In particular, if it is - 1, use 3 × 3 Scharr filter; if it is 1, use 3 × 1 or 1 × 3 core. The default value is 3.

* float scale = 1

  Multiply the calculated difference result by the coefficient, and the default value is 1.

* float delta = 0

  Add this offset before outputting the final result. The default value is 0.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_BGR_PACKED      | FORMAT_BGR_PACKED      |
+-----+------------------------+------------------------+
| 2   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+
| 3   | FORMAT_RGB_PACKED      | FORMAT_RGB_PACKED      |
+-----+------------------------+------------------------+
| 4   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 5   | FORMAT_RGBP_SEPARATE   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+------------------------+
| 6   | FORMAT_BGRP_SEPARATE   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+------------------------+
| 7   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 8   | FORMAT_YUV420P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 9   | FORMAT_YUV422P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 10  | FORMAT_YUV444P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 11  | FORMAT_NV12            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 12  | FORMAT_NV21            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 13  | FORMAT_NV16            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 14  | FORMAT_NV61            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 15  | FORMAT_NV24            | FORMAT_GRAY            |
+-----+------------------------+------------------------+


The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type of input and output must be the same.

3. The currently supported maximum image width is (2048 - ksize).


**Code example:**

    .. code-block:: c


        int channel   = 1;
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
        bm_image input, output;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input);
        bm_image_alloc_dev_mem(input);
        bm_image_copy_host_to_device(input, (void **)&src_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_sobel(handle, input, output, 0, 1)) {
            std::cout << "bmcv sobel error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


