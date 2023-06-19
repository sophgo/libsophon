bmcv_image_threshold
====================

Image thresholding operation.

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_threshold(
                bm_handle_t handle,
                bm_image input,
                bm_image output,
                unsigned char thresh,
                unsigned char max_value,
                bm_thresh_type_t type);

    The thresh types are as follows:

    .. code-block:: c

        typedef enum {
            BM_THRESH_BINARY = 0,
            BM_THRESH_BINARY_INV,
            BM_THRESH_TRUNC,
            BM_THRESH_TOZERO,
            BM_THRESH_TOZERO_INV,
            BM_THRESH_TYPE_MAX
        } bm_thresh_type_t;



    The specific formula of each type is as follows:

      .. figure:: ./thresh_type.jpg
         :width: 899px
         :height: 454px
         :scale: 50%
         :align: center


**Description of parameters:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of the input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to create new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be allocated automatically within the API.

* unsigned char thresh

  Threshold.

* max_value

  Maximum value.

* bm_thresh_type_t type

  Thresholding type.


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
| 8   | FORMAT_YUV420P         | FORMAT_YUV420P         |
+-----+------------------------+------------------------+
| 9   | FORMAT_YUV422P         | FORMAT_YUV422P         |
+-----+------------------------+------------------------+
| 10  | FORMAT_YUV444P         | FORMAT_YUV444P         |
+-----+------------------------+------------------------+
| 11  | FORMAT_NV12            | FORMAT_NV12            |
+-----+------------------------+------------------------+
| 12  | FORMAT_NV21            | FORMAT_NV21            |
+-----+------------------------+------------------------+
| 13  | FORMAT_NV16            | FORMAT_NV16            |
+-----+------------------------+------------------------+
| 14  | FORMAT_NV61            | FORMAT_NV61            |
+-----+------------------------+------------------------+
| 15  | FORMAT_NV24            | FORMAT_NV24            |
+-----+------------------------+------------------------+


The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The image_format and data_type of input and output must be the same.



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
        if (BM_SUCCESS != bmcv_image_threshold(handle, input, output, 200, 200, BM_THRESH_BINARY)) {
            std::cout << "bmcv thresh error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


