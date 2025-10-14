bmcv_image_pyramid_down
=======================

This interface implements downsampling in image gaussian pyramid operations.


**Processor model support**

This interface supports BM1684/BM1684X.


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

        #include <assert.h>
        #include "bmcv_api_ext.h"
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
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
            int ow = width / 2;
            int oh = height / 2;
            int channel = 1;
            unsigned char* input = new unsigned char [width * height * channel];
            unsigned char* output = new unsigned char [ow * oh * channel];
            bm_handle_t handle;
            bm_image_format_ext fmt = FORMAT_GRAY;
            bm_image img_i;
            bm_image img_o;
            const char* src_name = "path/to/src";
            const char* dst_name = "path/to/dst";

            readBin(src_name, input, width * height * channel);
            bm_dev_request(&handle, 0);
            bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
            bm_image_create(handle, oh, ow, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
            bm_image_alloc_dev_mem(img_i);
            bm_image_alloc_dev_mem(img_o);
            bm_image_copy_host_to_device(img_i, (void**)(&input));
            bmcv_image_pyramid_down(handle, img_i, img_o);
            bm_image_copy_device_to_host(img_o, (void **)(&output));
            writeBin(dst_name, output, ow * oh * channel);

            bm_image_destroy(img_i);
            bm_image_destroy(img_o);
            bm_dev_free(handle);
            free(input);
            free(output);
            return 0;
        }