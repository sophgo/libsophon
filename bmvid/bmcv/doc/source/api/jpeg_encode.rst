bmcv_image_jpeg_enc
===================

该接口可以实现对多张 bm_image 的 JPEG 编码过程。

**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_enc(
                bm_handle_t handle,
                int         image_num,
                bm_image *  src,      
                void *      p_jpeg_data[],
                size_t *    out_size,
                int         quality_factor = 85
        );


**输入参数说明：**

* bm_handle_t handle

输入参数。bm_handle 句柄。

* int  image_num

输入参数。输入图片数量，最多支持 4。

* bm_image\* src

输入参数。输入 bm_image的指针。每个 bm_image 需要外部调用 bmcv_image_create 创建，image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存,或者使用 bmcv_image_attach 来 attach 已有的内存。

* void \*  p_jpeg_data,

输出参数。编码后图片的数据指针，由于该接口支持对多张图片的编码，因此为指针数组，数组的大小即为 image_num。用户可以选择不为其申请空间（即数组每个元素均为NULL），在 api 内部会根据编码后数据的大小自动分配空间，但当不再使用时需要用户手动释放该空间。当然用户也可以选择自己申请足够的空间。

* size_t \*out_size,

输出参数。完成编码后各张图片的大小（以 byte 为单位）存放在该指针中。

* int quality_factor = 85

输入参数。编码后图片的质量因子。取值 0～100 之间，值越大表示图片质量越高，但数据量也就越大，反之值越小图片质量越低，数据量也就越少。该参数为可选参数，默认值为85。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


.. note::

    目前编码支持的图片格式包括以下几种：
     | FORMAT_YUV420P
     | FORMAT_YUV422P
     | FORMAT_YUV444P
     | FORMAT_NV12
     | FORMAT_NV21
     | FORMAT_NV16
     | FORMAT_NV61
     | FORMAT_GRAY



**示例代码**


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



