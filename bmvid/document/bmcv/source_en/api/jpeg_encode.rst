bmcv_image_jpeg_enc
===================

This API can be used for JPEG encoding of multiple bm_image.

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_enc(
                bm_handle_t handle,
                int         image_num,
                bm_image *  src,
                void *      p_jpeg_data[],
                size_t *    out_size,
                int         quality_factor = 85
        );


**Input parameter description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* int  image_num

  Input parameter. The number of input image, up to 4.

* bm_image\* src

  Input parameter. Input bm_image pointer. Each bm_image requires an external call to bmcv_image_create to create the image memory. Users can call bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* void \*  p_jpeg_data,

  Output parameter. The data pointer of the encoded images. Since the interface supports the encoding of multiple images, it is a pointer array, and the size of the array is image_num. Users can choose not to apply for space (that is, each element of the array is NULL). Space will be automatically allocated according to the size of encoded data within the API, but when it is no longer used, users need to release the space manually. Of course, users can also choose to apply for enough space.

* size_t \*out_size,

  Output parameter. After encoding, the size of each image (in bytes) is stored in the pointer.

* int quality_factor = 85

  Input parameter. The quality factor of the encoded image. The value is between 0 and 100. The larger the value, the higher the image quality, but the greater the amount of data. On the contrary, the smaller the value, the lower the image quality, and the less the amount of data. This parameter is optional and the default value is 85.



**Return value Description:**

* BM_SUCCESS: success

* Other: failed


.. note::

    Currently, the image formats which support encoding include the following:
     | FORMAT_YUV420P
     | FORMAT_YUV422P
     | FORMAT_YUV444P
     | FORMAT_NV12
     | FORMAT_NV21
     | FORMAT_NV16
     | FORMAT_NV61
     | FORMAT_GRAY



**Sample code**


    .. code-block:: c

        int image_h     = 1080;
        int image_w     = 1920;
        int size        = image_h * image_w;
        int format      = FORMAT_YUV420P;
        bm_image src;
        bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format,
                DATA_TYPE_EXT_1N_BYTE, &src);
        std::unique_ptr<unsigned char[]> buf1(new unsigned char[size]);
        memset(buf1.get(), 0x11, size);

        std::unique_ptr<unsigned char[]> buf2(new unsigned char[size / 4]);
        memset(buf2.get(), 0x22, size / 4);

        std::unique_ptr<unsigned char[]> buf3(new unsigned char[size / 4]);
        memset(buf3.get(), 0x33, size / 4);

        unsigned char *buf[] = {buf1.get(), buf2.get(), buf3.get()};
        bm_image_copy_host_to_device(src, (void **)buf);

        void* jpeg_data = NULL;
        size_t out_size = 0;
        int ret = bmcv_image_jpeg_enc(handle, 1, &src, &jpeg_data, &out_size);
        if (ret == BM_SUCCESS) {
            FILE *fp = fopen("test.jpg", "wb");
            fwrite(jpeg_data, out_size, 1, fp);
            fclose(fp);
        }
        free(jpeg_data);
        bm_image_destroy(src);



