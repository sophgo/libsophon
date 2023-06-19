bmcv_image_pyramid_down
=======================

This interface implements downsampling in image gaussian pyramid operations.

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_pyramid_down(
                 bm_handle_t handle,
                 bm_image input,
                 bm_image output);


**Description of parameters:**

* bm_handle_t handle

  Input parameters. bm_handle handle.

* bm_image input

  Input parameter. The bm_image of the input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The bm_image of the output image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

**Return value description:**

* BM_SUCCESS: success

* Other: failed

**Format support:**

The interface currently supports the following image_format and data_type:

+-----+------------------------+------------------------+
| num | image_format           | data_type              |
+=====+========================+========================+
| 1   | FORMAT_GRAY            | DATA_TYPE_EXT_1N_BYTE  |
+-----+------------------------+------------------------+


**Code example:**

    .. code-block:: c


        int height = 1080;
        int width = 1920;
        int ow = height / 2;
        int oh = width / 2;
        int channel = 1;
        unsigned char* input_data = new unsigned char [width * height * channel];
        unsigned char* output_tpu = new unsigned char [ow * oh * channel];
        unsigned char* output_ocv = new unsigned char [ow * oh * channel];

        for (int i = 0; i < height * channel; i++) {
            for (int j = 0; j < width; j++) {
                input_data[i * width + j] = rand() % 100;
            }
        }

        bm_handle_t handle;
        bm_status_t ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            return -1;
        }
        bm_image_format_ext fmt = FORMAT_GRAY;
        bm_image img_i;
        bm_image img_o;
        bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
        bm_image_create(handle, oh, ow, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
        bm_image_alloc_dev_mem(img_i);
        bm_image_alloc_dev_mem(img_o);
        bm_image_copy_host_to_device(img_i, (void **)(&input));

        struct timeval t1, t2;
        gettimeofday_(&t1);
        bmcv_image_pyramid_down(handle, img_i, img_o);
        gettimeofday_(&t2);
        cout << "pyramid down TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

        bm_image_copy_device_to_host(img_o, (void **)(&output));
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        bm_dev_free(handle);