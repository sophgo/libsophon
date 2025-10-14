bmcv_image_median_blur
========================

该接口用于对图像进行中值滤波。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

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


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t input_data_global_addr

  输入参数。输入图像的 device 空间。

* bm_device_mem_t padded_input_data_global_addr

  输入参数。输入图像pad0后的 device 空间。

* unsigned char* output

  输出参数。中值滤波后的输出图像。

* int width

  输入参数。表示输入图像的宽。

* int height

  输入参数。表示输入图像的高。

* int format

  输入参数。表示输入图像的格式。

* int ksize

  输入参数。表示中值滤波核尺寸。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

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

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用该接口之前需确保输入的 input_data_global_addr， padded_input_data_global_addr 和 output 内存已经申请。

2、支持的图像宽高范围为8*8～4096*3000。

3、支持的中值滤波核尺寸为3*3， 5*5, 7*7, 9*9。


**代码示例：**

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
                printf("无法打开输出文件 %s\n", input_path);
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
                printf("无法打开输出文件 %s\n", output_path);
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