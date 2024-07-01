bmcv_image_gaussian_blur
========================

Gaussian blur of the image.

**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

         bm_status_t bmcv_image_gaussian_blur(
                 bm_handle_t handle,
                 bm_image input,
                 bm_image output,
                 int kw,
                 int kh,
                 float sigmaX,
                 float sigmaY = 0);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to open up new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.

* int kw

  The size of the kernel in the width direction.

* int kh

  The size of the kernel in the height direction.

* float sigmaX

  Gaussian kernel standard deviation in the x-direction.

* float sigmaY = 0

  Gaussian kernel standard deviation in the y-direction. If it is 0, it means that it is the same as the Gaussian kernel standard deviation in the x direction.


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

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type and image_format of input and must be the same.

3. The maximum width of the image supported by BM1684 is (2048 - kw), the maximum width supported by BM1684X is 4096, and the maximum height is 8192.

4. The maximum convolution kernel width and height supported by BM1684 is 31, and the maximum convolution kernel width and height supported by BM1684X is 3.
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
        if (BM_SUCCESS != bmcv_image_gaussian_blur(handle, input, output, 3, 3, 0.1)) {
            std::cout << "bmcv gaussian blur error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


