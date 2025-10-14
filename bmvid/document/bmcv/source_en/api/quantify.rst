bmcv_image_quantify
====================

Convert float type data into int type (the rounding mode is truncation directly after the decimal point), and change the number less than 0 to 0, and the number greater than 255 to 255.


**Processor model support:**

This interface only support BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_quantify(
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
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 2   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+


Input data currently supports the following data_types:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

Output data currently supports the following data_types:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note:**

1. Before calling this interface, you must ensure that the input image memory has been allocated.

2. If the program calling this interface is a multi-threaded program, thread locks need to be added before creating bm_image and after destroying bm_image.

3. This interface supports image width and height ranging from 1x1 to 8192x8192.


**Code example:**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include <stdlib.h>
        #include <string.h>
        #include <assert.h>
        #include <float.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 4, size, fp_src) < (unsigned int)size) {
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
            int width = 1920;
            int height = 1080;
            bm_handle_t handle;
            bm_image input_img;
            bm_image output_img;
            float* input = (float*)malloc(width * height * 3 * sizeof(float));
            unsigned char* output = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";

            readBin(input_path, (unsigned char*)input, width * height * 3);

            bm_dev_request(&handle, 0);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, &input_img, NULL);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_alloc_dev_mem(input_img, 2);
            bm_image_alloc_dev_mem(output_img, 2);
            bm_image_copy_host_to_device(input_img, (void **)&input);
            bmcv_image_quantify(handle, input_img, output_img);
            bm_image_copy_device_to_host(output_img, (void **)&output);
            writeBin(output_path, output, (width * height * 3));

            bm_image_destroy(input_img);
            bm_image_destroy(output_img);
            free(input);
            free(output);
            bm_dev_free(handle);
            return 0;
        }