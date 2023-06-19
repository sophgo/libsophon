bmcv_image_copy_to
==================


The interface copies an image to the corresponding memory area of the target image.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_copy_to(
                bm_handle_t handle,
                bmcv_copy_to_atrr_t copy_to_attr,
                bm_image            input,
                bm_image            output
        );


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bmcv_copy_to_atrr_t copy_to_attr

  Input parameter. The attribute configuration corresponding to the API.

* bm_image input

  Input parameter. Input bm_image. The creation of bm_image requires an external call to bmcv_image_create. Users can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Users can use bm_image_alloc_dev_mem to create new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.


**Return value Description:**

* BM_SUCCESS: success

* Other: failed


**Data Type Description:**


    .. code-block:: c

        typedef struct bmcv_copy_to_atrr_s {
            int           start_x;
            int           start_y;
            unsigned char padding_r;
            unsigned char padding_g;
            unsigned char padding_b;
            int if_padding;
        } bmcv_copy_to_atrr_t;


* padding_b represents the value filled in the b channel of the extra image when the input image is smaller than the output image.

* padding_r represents the value filled in the r channel of the extra image when the input image is smaller than the output image.

* padding_g represents the value filled in the g channel of the extra image when the input image is smaller than the output image.

* start_x describes the starting abscissa of the output image of copy_to.

* start_y describes the starting ordinate of the output image of copy_to.

* if_padding indicates whether the extra image area needs to be filled with a specific color when the input image is smaller than the output image. 0 indicates no and 1 indicates yes. When the value is filled with 0, the setting of padding_r, padding_g and padding_b will be invalid.


**Format support**

bm1684 supports the following combination of image_formats and data_type:

+-----+-------------------+-----------------------+
| num | image_format      | data_type             |
+=====+===================+=======================+
| 1   | FORMAT_BGR_PACKED | DATA_TYPE_EXT_FLOAT32 |
+-----+-------------------+-----------------------+
| 2   | FORMAT_BGR_PLANAR | DATA_TYPE_EXT_FLOAT32 |
+-----+-------------------+-----------------------+
| 3   | FORMAT_BGR_PACKED | DATA_TYPE_EXT_1N_BYTE |
+-----+-------------------+-----------------------+
| 4   | FORMAT_BGR_PLANAR | DATA_TYPE_EXT_1N_BYTE |
+-----+-------------------+-----------------------+
| 5   | FORMAT_BGR_PLANAR | DATA_TYPE_EXT_4N_BYTE |
+-----+-------------------+-----------------------+
| 6   | FORMAT_RGB_PACKED | DATA_TYPE_EXT_FLOAT32 |
+-----+-------------------+-----------------------+
| 7   | FORMAT_RGB_PLANAR | DATA_TYPE_EXT_FLOAT32 |
+-----+-------------------+-----------------------+
| 8   | FORMAT_RGB_PACKED | DATA_TYPE_EXT_1N_BYTE |
+-----+-------------------+-----------------------+
| 9   | FORMAT_RGB_PLANAR | DATA_TYPE_EXT_1N_BYTE |
+-----+-------------------+-----------------------+
| 10  | FORMAT_RGB_PLANAR | DATA_TYPE_EXT_4N_BYTE |
+-----+-------------------+-----------------------+
| 11  | FORMAT_GRAY       | DATA_TYPE_EXT_1N_BYTE |
+-----+-------------------+-----------------------+

bm1684x supports the following data_type of bm_image:

+-----+------------------------+-------------------------------+
| num | input data type        | output data type              |
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
|  6  | DATA_TYPE_EXT_FLOAT32  | DATA_TYPE_EXT_FLOAT32         |
+-----+------------------------+-------------------------------+

The color formats supported for input and output are:

+-----+-------------------------------+
| num | image_format                  |
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


**Notes**

1. Before calling bmcv_image_copy_to(), users must ensure that the input image memory has been applied for.

2. The data_type and image_format of input and output must be the same.

3. To avoid memory overrun, the width + start_x of input image must be less than or equal to the width stride of output image.


**Code example:**

    .. code-block:: c


        int channel   = 3;
        int in_w      = 400;
        int in_h      = 400;
        int out_w     = 800;
        int out_h     = 800;
        int    dev_id = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src_ptr(
                new unsigned char[channel * in_w * in_h],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> res_ptr(
                new unsigned char[channel * out_w * out_h],
                std::default_delete<unsigned char[]>());
        unsigned char * src_data = src_ptr.get();
        unsigned char * res_data = res_ptr.get();
        for (int i = 0; i < channel * in_w * in_h; i++) {
            src_data[i] = rand() % 255;
        }
        // calculate res
        bmcv_copy_to_atrr_t copy_to_attr;
        copy_to_attr.start_x   = 0;
        copy_to_attr.start_y   = 0;
        copy_to_attr.padding_r = 0;
        copy_to_attr.padding_g = 0;
        copy_to_attr.padding_b = 0;
        bm_image input, output;
        bm_image_create(handle,
                in_h,
                in_w,
                FORMAT_RGB_PLANAR,
                DATA_TYPE_EXT_1N_BYTE,
                &input);
        bm_image_alloc_dev_mem(input);
        bm_image_copy_host_to_device(input, (void **)&src_data);
        bm_image_create(handle,
                out_h,
                out_w,
                FORMAT_RGB_PLANAR,
                DATA_TYPE_EXT_1N_BYTE,
                &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_copy_to(handle, copy_to_attr, input, output)) {
            std::cout << "bmcv_copy_to error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);

            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle)
