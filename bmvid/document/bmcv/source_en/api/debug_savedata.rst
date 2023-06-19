bmcv_debug_savedata
====================

This interface is used to input bm_image object to the internally defined binary file for debugging. The binary file format and parsing method are given in the example code.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_debug_savedata(
                bm_image input,
                const char *name
        );


**Parameter Description:**

* bm_image input

  Input parameter. Input bm_image.

* const char\* name

  Input parameter. The saved binary file path and name.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. Before calling bmcv_debug_savedata(), users must ensure that the input image has been created correctly and guaranteed is_attached, otherwise the function will return a failure.


**Code example and binary file parsing method::**

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
