bmcv_image_add_weighted
=======================

Fusion of two images of the same size by weighted, as follows:

.. math::
    \begin{array}{c}
    output = alpha * input1 + beta * input2 + gamma
    \end{array}


**Interface form:**

    .. code-block:: c

         bm_status_t bmcv_image_add_weighted(
                 bm_handle_t handle,
                 bm_image input1,
                 float alpha,
                 bm_image input2,
                 float beta,
                 float gamma,
                 bm_image output);


**Description of parameters:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input1

  Input parameter. The bm_image of the first input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* float alpha

  The weight of the first image.

* bm_image input2

  Input parameter. The bm_image of the second input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* float beta

  The weight of the second image.

* float gamma

  Offset after fusion.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to create new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be allocated automatically within the API.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following images_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_BGR_PACKED      |
+-----+------------------------+
| 2   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 3   | FORMAT_RGB_PACKED      |
+-----+------------------------+
| 4   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 5   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 6   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 7   | FORMAT_GRAY            |
+-----+------------------------+
| 8   | FORMAT_YUV420P         |
+-----+------------------------+
| 9   | FORMAT_YUV422P         |
+-----+------------------------+
| 10  | FORMAT_YUV444P         |
+-----+------------------------+
| 11  | FORMAT_NV12            |
+-----+------------------------+
| 12  | FORMAT_NV21            |
+-----+------------------------+
| 13  | FORMAT_NV16            |
+-----+------------------------+
| 14  | FORMAT_NV61            |
+-----+------------------------+
| 15  | FORMAT_NV24            |
+-----+------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type and image_format of input and output must be the same.



**Code example:**

    .. code-block:: c


        int channel   = 3;
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src1_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> src2_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> res_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        unsigned char * src1_data = src1_ptr.get();
        unsigned char * src2_data = src2_ptr.get();
        unsigned char * res_data = res_ptr.get();
        for (int i = 0; i < channel * width * height; i++) {
            src1_data[i] = rand() % 255;
            src2_data[i] = rand() % 255;
        }
        // calculate res
        bm_image input1, input2, output;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_RGB_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input1);
        bm_image_alloc_dev_mem(input1);
        bm_image_copy_host_to_device(input1, (void **)&src1_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_RGB_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input2);
        bm_image_alloc_dev_mem(input2);
        bm_image_copy_host_to_device(input2, (void **)&src2_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_RGB_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_add_weighted(handle, input1, 0.5, input2, 0.5, 0, output)) {
            std::cout << "bmcv add_weighted error !!!" << std::endl;
            bm_image_destroy(input1);
            bm_image_destroy(input2);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input1);
        bm_image_destroy(input2);
        bm_image_destroy(output);
        bm_dev_free(handle);


