bmcv_image_canny
================

Canny operator for edge detection.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_canny(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    float threshold1,
                    float threshold2,
                    int aperture_size = 3,
                    bool l2gradient = false);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to open up new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The output bm_image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.

* float threshold1

  The first threshold in the lag process.

* float threshold2

  The second threshold in the lag process.

* int aperture_size = 3

  The size of Sobel core, which currently supports only 3.

* bool l2gradient = false

  Whether to use L2 norm to calculate image gradient. The default value is false.


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

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type and image_format of input and output must be the same.

3. The currently supported maximum image width is 2048.

4. The stride and width of the input image must be the same.


**Code example:**

    .. code-block:: c

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
            int channel = 1;
            int width = 1920;
            int height = 1080;
            int dev_id = 0;
            bm_handle_t handle;
            bm_image input, output;
            float low_thresh = 100;
            float high_thresh = 200;
            unsigned char * src_data = (unsigned char*)malloc(channel * width * height * sizeof(unsigned char));
            unsigned char * res_data = (unsigned char*)malloc(channel * width * height * sizeof(unsigned char));
            const char* src_name = "path/to/src";
            const char* dst_name = "path/to/dst";

            readBin(src_name, src_data, channel * width * height);
            bm_dev_request(&handle, dev_id);
            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void **)&src_data);
            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_canny(handle, input, output, low_thresh, high_thresh);
            bm_image_copy_device_to_host(output, (void **)&res_data);
            writeBin(dst_name, res_data, channel * width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            return 0;
        }