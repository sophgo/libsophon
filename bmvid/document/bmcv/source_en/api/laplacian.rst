bmcv_image_laplacian
====================

Laplacian operator of gradient calculation.


**Interface form:**

    .. code-block:: c

        bm_status_t  bmcv_image_laplacian(
            bm_handle_t handle,
            bm_image input,
            bm_image output,
            unsigned int ksize);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The output bm_image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem to create new memory or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be allocated automatically within the API.

* int ksize = 3

  The number of Laplacian nucleus. Must be 1 or 3.




**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+


The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied.

2. The data_type of input and output must be the same.

3. Currently, the maximum supported image width is 2048.


**Code example:**

    .. code-block:: c

        int loop =1;
        int ih = 1080;
        int iw = 1920;
        unsigned int ksize = 3;
        bm_image_format_ext fmt = FORMAT_GRAY;

        fmt = argc > 1 ? (bm_image_format_ext)atoi(argv[1]) : fmt;
        ih = argc > 2 ? atoi(argv[2]) : ih;
        iw = argc > 3 ? atoi(argv[3]) : iw;
        loop = argc > 4 ? atoi(argv[4]) : loop;
        ksize = argc >5 ? atoi(argv[5]) : ksize;

        bm_status_t ret = BM_SUCCESS;
        bm_handle_t handle;
        ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS)
            throw("bm_dev_request failed");

        bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
        bm_image input;
        bm_image output;

        bm_image_create(handle, ih, iw, fmt, data_type, &input);
        bm_image_alloc_dev_mem(input);

        bm_image_create(handle,ih, iw, fmt, data_type, &output);
        bm_image_alloc_dev_mem(output);

        std::shared_ptr<unsigned char*> ch0_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
        std::shared_ptr<unsigned char*> tpu_res_ptr = std::make_shared<unsigned char *>(new unsigned char[ih * iw]);
        std::shared_ptr<unsigned char*> cpu_res_ptr = std::make_shared<unsigned char *>(new unsigned char[ih*iw]);

        for (int i = 0; i < loop; i++) {
            for (int j = 0; j < ih * iw; j++) {
                (*ch0_ptr.get())[j] = j % 256;
            }

            unsigned char *host_ptr[] = {*ch0_ptr.get()};
            bm_image_copy_host_to_device(input, (void **)host_ptr);

            ret = bmcv_image_laplacian(handle, input, output, ksize);
            if (ret) {
                cout << "test laplacian failed" << endl;
                bm_image_destroy(input);
                bm_image_destroy(output);
                bm_dev_free(handle);
                return ret;
            } else {
                host_ptr[0] = *tpu_res_ptr.get();
                bm_image_copy_device_to_host(output, (void **)host_ptr);
            }
        }

        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


