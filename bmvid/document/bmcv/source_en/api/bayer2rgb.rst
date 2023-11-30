bmcv_image_bayer2rgb
==================

Converts bayerBG8 format images to RGB Plannar format.

**接口形式：**


    .. code-block:: c

         bm_status_t bmcv_image_bayer2rgb(
                 bm_handle_t handle,
                 unsigned char* convd_kernel,
                 bm_image input
                 bm_image output);


**参数说明：**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* unsigned char* convd_kernel

  Input parameter. Convolutional kernel for convolutional computation.

* bm_image input

  Input parameter. The bm_image of the input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to create new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be allocated automatically within the API.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following input format:

+-----+--------------------------------+
| num | image_format                   |
+=====+================================+
| 1   | FORMAT_BAYER                   |
+-----+--------------------------------+

The interface currently supports the following output format:

+-----+--------------------------------+
| num | image_format                   |
+=====+================================+
| 1   | FORMAT_RGB_PLANAR              |
+-----+--------------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1、The format of input is bayerBG, the format of output is rgb plannar, and the data_type is uint8.
2、The interface currently supports bm1684x.
3、The interface supports the size range of 8*8 ~ 8096*8096, and the width and height of the image need to be even.


**Code example:**

    .. code-block:: c


        #define KERNEL_SIZE 3 * 3 * 3 * 4 * 64
        #define CONVD_MATRIX 12 * 9

        const unsigned char convd_kernel[CONVD_MATRIX] = {1, 0, 1, 0, 0, 0, 1, 0, 1,
                                                        0, 0, 2, 0, 0, 0, 0, 0, 2,
                                                        0, 0, 0, 0, 0, 0, 2, 0, 2,
                                                        0, 0, 0, 0, 0, 0, 0, 0, 4, // r R
                                                        4, 0, 0, 0, 0, 0, 0, 0, 0, // b B
                                                        2, 0, 2, 0, 0, 0, 0, 0, 0,
                                                        2, 0, 0, 0, 0, 0, 2, 0, 0,
                                                        1, 0, 1, 0, 0, 0, 1, 0, 1,
                                                        0, 1, 0, 1, 0, 1, 0, 1, 0,
                                                        0, 0, 0, 0, 0, 4, 0, 0, 0, // g1 G1
                                                        0, 0, 0, 0, 0, 0, 0, 4, 0, // g2 G2
                                                        0, 1, 0, 1, 0, 1, 0, 1, 0};
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src1_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        bm_image input_img;
        bm_image output_img;
        bm_image_create(handle, height, width, FORMAT_BAYER, DATA_TYPE_EXT_1N_BYTE, &input_img);
        bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_img);
        bm_image_alloc_dev_mem(output_img, BMCV_HEAP_ANY);

        unsigned char kernel_data[KERNEL_SIZE];
        memset(kernel_data, 0, KERNEL_SIZE);
        // constructing convd_kernel_data
        for (int i = 0;i < 12;i++) {
            for (int j = 0;j < 9;j++) {
                kernel_data[i * 9 * 64 + 64 * j] = convd_kernel[i * 9 + j];
            }
        }
        unsigned char* input_data[3] = {srcImage.data, srcImage.data + height * width, srcImage.data + 2 * height * width};
        bm_image_copy_host_to_device(input_img, (void **)input_data);
        bmcv_image_bayer2rgb(handle, kernel_data, input_img, output_img);
        bm_image_copy_device_to_host(output_img, (void **)(&output));
        bm_image_destroy(input_img);
        bm_image_destroy(output_img);
        bm_dev_free(handle);