bmcv_image_sobel
================

Sobel operator for edge detection.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_sobel(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int dx,
                    int dy,
                    int ksize = 3,
                    float scale = 1,
                    float delta = 0);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.

* int dx

  The difference order in the x direction.

* int dy

  The difference order in the y direction.

* int ksize = 3

  The size of the Sobel kernel must be 1, 3, 5, or 7. When the values ​​are 3, 5, or 7, the kernel size is 3*3, 5*5, or 7*7. If it is 1, the size of the Sobel kernel is determined by the values ​​of dx and dy. If dx=1, dy=0, the kernel size is 3×1, if dx=0, dy=1, the kernel size is 1×3, and if dx=1, dy=1, the kernel size becomes 3*3. The default value of ksize is 3.

* float scale = 1

  Multiply the calculated difference result by the coefficient, and the default value is 1.

* float delta = 0

  Add this offset before outputting the final result. The default value is 0.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+
| 2   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 3   | FORMAT_RGBP_SEPARATE   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+------------------------+
| 4   | FORMAT_BGRP_SEPARATE   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+------------------------+
| 5   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 6   | FORMAT_YUV420P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 7   | FORMAT_YUV422P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 8   | FORMAT_YUV444P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 9   | FORMAT_NV12            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 10  | FORMAT_NV21            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 11  | FORMAT_NV16            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 12  | FORMAT_NV61            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 13  | FORMAT_NV24            | FORMAT_GRAY            |
+-----+------------------------+------------------------+


The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type of input and output must be the same.

3. In the BM1684, the maximum width of the image supported by this operator chip is (2048 - ksize). In the BM1684X chip, when the Sobel kernel size is 1 and 3, the supported width and height range is 8*8 to 8192*8192, when the kernel size is 5, the supported width and height range is 8*8 to 4096*8192, and when the kernel size is 7, the supported width and height range is 8*8 to 2048*8192.


**Code example:**

    .. code-block:: c

        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <math.h>
        #include "bmcv_api_ext.h"
        #include "test_misc.h"

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
            unsigned char* src_data = new unsigned char[channel * width * height];
            unsigned char* res_data = new unsigned char[channel * width * height];
            const char *src_name = "/path/to/src";
            const char *dst_name = "path/to/dst";

            bm_dev_request(&handle, dev_id);
            readBin(src_name, src_data, channel * width * height);

            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void**)&src_data);
            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_sobel(handle, input, output, 0, 1);
            bm_image_copy_device_to_host(output, (void**)&res_data);
            writeBin(dst_name, res_data, channel * width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            delete[] src_data;
            delete[] res_data;
            return 0;
        }