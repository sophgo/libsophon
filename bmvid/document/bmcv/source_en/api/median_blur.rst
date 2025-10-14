bmcv_image_median_blur
========================

This interface is used to perform median filtering on an image.


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_median_blur(
            bm_handle_t handle,
            bm_device_mem_t input_data_global_addr,
            bm_device_mem_t padded_input_data_global_addr,
            unsigned char *output,
            int width,
            int height,
            int format,
            int ksize);


**Parameter description:**

* bm_handle_t handle

  Input parameters. bm_handle handle.

* bm_device_mem_t input_data_global_addr

  Input parameters. The device space of the input image.

* bm_device_mem_t padded_input_data_global_addr

  Input parameters. Input image pad0 after the device space.

* unsigned char* output

  Output parameter. The output image after median filtering.

* int width

  Input parameter. Indicates the width of the input image.

* int height

  Input parameter. Indicates the height of the input image.

* int format

  Input parameter. Indicates the format of the input image.

* int ksize

  Input parameter. Indicates the median filter kernel size.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_YUV444P         | FORMAT_YUV444P         |
+-----+------------------------+------------------------+
| 2   | FORMAT_RGBP_SEPARATE   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+------------------------+
| 3   | FORMAT_BGRP_SEPARATE   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+------------------------+
| 4   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note:**

1、Before calling this interface, make sure that the input_data_global_addr, padded_input_data_global_addr and output memory have been applied.

2、The supported image width and height range is 8*8 to 4096*3000.

3、The supported median filter kernel sizes are 3*3, 5*5, 7*7 and 9*9.


**Code example:**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include "stdlib.h"
        #include "string.h"
        #include <sys/time.h>
        #include <pthread.h>

        #define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

        static void read_bin(const char *input_path, unsigned char *input_data, int width, int height, int channel) {
            FILE *fp_src = fopen(input_path, "rb");
            if (fp_src == NULL) {
                printf("Unable to open output file %s\n", input_path);
                return;
            }
            if(fread(input_data, sizeof(char), width * height * channel, fp_src) != 0) {
                printf("read image success\n");
            }
            fclose(fp_src);
        }

        static void write_bin(const char *output_path, unsigned char *output_data, int width, int height, int channel) {
            FILE *fp_dst = fopen(output_path, "wb");
            if (fp_dst == NULL) {
                printf("Unable to open output file %s\n", output_path);
                return;
            }
            fwrite(output_data, sizeof(unsigned char), width * height * channel, fp_dst);
            fclose(fp_dst);
        }

        int main(int argc, char *args[]) {
            char *input_path = NULL;
            char *output_path = NULL;
            if (argc > 1) input_path = args[1];
            if (argc > 2) output_path = args[2];

            int width = 1920;
            int height = 1080;
            int ksize = 9;
            int format = FORMAT_RGBP_SEPARATE;
            int channel = 3;
            int dev_id = 0;
            bm_handle_t handle;
            bm_dev_request(&handle, dev_id);
            unsigned char *input_data = (unsigned char*)malloc(width * height * 3);
            unsigned char *output_tpu = (unsigned char*)malloc(width * height * 3);
            read_bin(input_path, input_data, width, height, channel);
            int padd_width = ksize - 1 + width;
            int padd_height = ksize - 1 + height;
            bm_device_mem_t input_data_global_addr, padded_input_data_global_addr;
            bm_malloc_device_byte(handle, &input_data_global_addr, channel * width * height);
            bm_malloc_device_byte(handle, &padded_input_data_global_addr, channel * padd_width * padd_height);
            struct timeval t1, t2;
            bm_memcpy_s2d(handle, input_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
            gettimeofday(&t1, NULL);
            if(BM_SUCCESS != bmcv_image_median_blur(handle, input_data_global_addr, padded_input_data_global_addr, output_tpu, width, height, format, ksize)){
                printf("bmcv_image_median_blur error\n");
                return -1;
            }
            gettimeofday(&t2, NULL);
            printf("median_blur TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
            write_bin(output_path, output_tpu, width, height, channel);
            bm_free_device(handle, input_data_global_addr);
            bm_free_device(handle, padded_input_data_global_addr);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }