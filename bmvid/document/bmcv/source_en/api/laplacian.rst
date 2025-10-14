bmcv_image_laplacian
====================

Laplacian operator of gradient calculation.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_laplacian(
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

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            int height = 1080;
            int width = 1920;
            unsigned int ksize = 3;
            bm_image_format_ext fmt = FORMAT_GRAY;
            bm_handle_t handle;
            bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
            bm_image input;
            bm_image output;
            unsigned char* input_data = (unsigned char*)malloc(width * height * sizeof(unsigned char));
            unsigned char* tpu_out = (unsigned char*)malloc(width * height * sizeof(unsigned char));
            const char* src_name = "path/to/src";
            const char* dst_name = "path/to/dst";

            bm_dev_request(&handle, 0);
            bm_image_create(handle, height, width, fmt, data_type, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_create(handle, height, width, fmt, data_type, &output);
            bm_image_alloc_dev_mem(output);

            readBin(src_name, input_data, width * height);
            bm_image_copy_host_to_device(input, (void**)&input_data);
            bmcv_image_laplacian(handle, input, output, ksize);
            bm_image_copy_device_to_host(output, (void **)&tpu_out);
            writeBin(dst_name, tpu_out, width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            free(input_data);
            free(tpu_out);
            return 0;
        }