bmcv_image_gaussian_blur
========================

Gaussian blur of the image.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_gaussian_blur(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int kw,
                    int kh,
                    float sigmaX,
                    float sigmaY = 0);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to open up new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.

* int kw

  The size of the kernel in the width direction.

* int kh

  The size of the kernel in the height direction.

* float sigmaX

  Gaussian kernel standard deviation in the x-direction.

* float sigmaY = 0

  Gaussian kernel standard deviation in the y-direction. If it is 0, it means that it is the same as the Gaussian kernel standard deviation in the x direction.


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

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type and image_format of input and must be the same.

3. The maximum width of the image supported by BM1684 is (2048 - kw), In the BM1684X chip, when the kernel size is 1 and 3, the supported width and height range is 8*8 to 8192*8192, when the kernel size is 5, the supported width and height range is 8*8 to 4096*8192, and when the kernel size is 7, the supported width and height range is 8*8 to 2048*8192.

4. The maximum convolution kernel width and height supported by BM1684 is 31, and the maximum convolution kernel width and height supported by BM1684X is 7.


**Code example:**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include "stdlib.h"
        #include "string.h"
        #include <assert.h>
        #include <float.h>
        #include <math.h>

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
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";
            unsigned char* src_data = new unsigned char[channel * width * height];
            unsigned char* res_data = new unsigned char[channel * width * height];

            readBin(input_path, src_data, channel * width * height);
            bm_dev_request(&handle, dev_id);
            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void**)&src_data);
            bm_image_create(handle, height,width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_gaussian_blur(handle, input, output, 3, 3, 0.1);
            bm_image_copy_device_to_host(output, (void**)&res_data);
            writeBin(output_path, res_data, channel * width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            free(src_data);
            free(res_data);
            bm_dev_free(handle);
            return 0;
        }