bmcv_debug_savedata
====================

该接口用于将bm_image对象输出至内部定义的二进制文件方便debug，二进制文件格式以及解析方式在示例代码中给出。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_debug_savedata(
                bm_image input,
                const char *name
        );


**参数说明:**

* bm_image input

  输入参数。输入 bm_image。

* const char\* name

  输入参数。保存的二进制文件路径以及文件名称。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项:**

1. 在调用 bmcv_debug_savedata()之前必须确保输入的 image 已被正确创建并保证is_attached，否则该函数将返回失败。


**代码示例以及二进制文件解析方法:**

    .. code-block:: c

        bm_image input;
        bm_image_create(handle,
            1080,
            1920,
            FORMAT_BGR_PLANAR,
            DATA_TYPE_EXT_1N_BYTE,
            &input);
        bm_image_alloc_dev_mem(input);
        // ... your own function
        bmcv_debug_savedata(input, "input.bin");
        // now a file named "input.bin" is generated in current folder

        // the following code shows how to parse the binary file
        FILE *   fp           = fopen("input.bin", "rb");
        uint32_t data_offset  = 0;
        uint32_t width        = 0;
        uint32_t height       = 0;
        uint32_t image_format = 0;
        uint32_t data_type    = 0;
        uint32_t plane_num    = 0;

        uint32_t stride[4] = {0};
        uint64_t size[4]         = {0};

        fread(&data_offset, sizeof(uint32_t), 1, fp);
        fread(&width, sizeof(uint32_t), 1, fp);
        fread(&height, sizeof(uint32_t), 1, fp);
        fread(&image_format, sizeof(uint32_t), 1, fp);
        fread(&data_type, sizeof(uint32_t), 1, fp);
        fread(&plane_num, sizeof(uint32_t), 1, fp);

        fread(size, sizeof(size), 1, fp);
        fread(stride, sizeof(stride), 1, fp);

        uint32_t channel_stride[4] = {0};
        uint32_t batch_stride[4]   = {0};
        uint32_t meta_data_size[4] = {0};

        uint32_t N[4] = {0};
        uint32_t C[4] = {0};
        uint32_t H[4] = {0};
        uint32_t W[4] = {0};

        fread(channel_stride, sizeof(channel_stride), 1, fp);
        fread(batch_stride, sizeof(batch_stride), 1, fp);
        fread(meta_data_size, sizeof(meta_data_size), 1, fp);

        fread(N, sizeof(N), 1, fp);
        fread(C, sizeof(C), 1, fp);
        fread(H, sizeof(H), 1, fp);
        fread(W, sizeof(W), 1, fp);

        fseek(fp, data_offset, SEEK_SET);
        std::vector<std::unique_ptr<unsigned char[]>> host_ptr;
        host_ptr.resize(plane_num);
        void* void_ptr[4] = {0};
        for (uint32_t i = 0; i < plane_num; i++) {
            host_ptr[i] =
                std::unique_ptr<unsigned char[]>(new unsigned char[size[i]]);
            void_ptr[i] = host_ptr[i].get();
            fread(host_ptr[i].get(), 1, size[i], fp);
        }
        fclose(fp);
        std::cout << "image width " << width << " image height " << height
                << " image format " << image_format << " data type " << data_type
                << " plane num " << plane_num << std::endl;
        for (uint32_t i = 0; i < plane_num; i++) {
            std::cout << "plane" << i << " size " << size[i] << " C " << C[i]
                    << " H " << H[i] << " W " << W[i] << " stride "
                    << stride[i] << std::endl;
        }
        // The following shows how to recover the image
        bm_image recover;
        bm_image_create(handle,
                        height,
                        width,
                        (bm_image_format_ext)image_format,
                        (bm_image_data_format_ext)data_type,
                        &recover,
                        (int *)stride);
        bm_image_copy_host_to_device(recover, (void **)&void_ptr);
        bm_image_write_to_bmp(recover, "recover.bmp");
        bm_image_destroy(recover);
