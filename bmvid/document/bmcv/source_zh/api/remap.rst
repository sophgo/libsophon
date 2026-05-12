bmcv_image_remap
==================

使用指定的x轴与y轴映射表数据，对源图像进行重映射操作，边缘填充方式为填充常数0。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_remap(
            bm_handle_t handle,
            bm_image input,
            bm_image output,
            bm_device_mem_t mapx_data_global_addr,
            bm_device_mem_t mapy_data_global_addr,
            int interpolation_mode);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* bm_device_mem_t mapx_data_global_addr

  输入参数。x轴方向映射表数据的设备内存地址。

* bm_device_mem_t mapy_data_global_addr

  输入参数。y轴方向映射表数据的设备内存地址。

* int interpolation_mode

  输入参数。插值方式，0将启用Nearest近邻插值算法，1将启用Bilinear双线性插值算法。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_YUV444P         |
+-----+------------------------+
| 2   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 3   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 4   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 5   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 6   | FORMAT_GRAY            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用 bmcv_image_remap()之前必须确保输入的 image 内存已经申请。

2、输入输出图像的data_type，image_format必须相同。

3、输入图像的宽高尺寸支持范围为8*8-8192*8192，当插值方式为Nearest时，输出图像尺寸支持范围为8*8-8192*8192，当插值方式为Bilinear时，输出图像尺寸支持范围为8*8-4096*4096。


**代码示例：**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext_c.h"
        #include "stdlib.h"

        static void read_bin_float(const char *input_path, float *input_data, int width, int height, int channel) {
            FILE *fp_src = fopen(input_path, "rb");
            if (fp_src == NULL) {
                printf("无法打开输出文件 %s\n", input_path);
                return;
            }
            if(fread(input_data, sizeof(float), width * height * channel, fp_src) != 0) {
                printf("read map success\n");
            }
            fclose(fp_src);
        }

        static void read_bin(const char *input_path, unsigned char *input_data, int input_width, int input_height, int channel) {
            FILE *fp_src = fopen(input_path, "rb");
            if (fp_src == NULL) {
                printf("无法打开输入文件 %s\n", input_path);
                return;
            }
            if(fread(input_data, sizeof(char), input_width * input_height * channel, fp_src) != 0) {
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
            printf("write image success\n");
        }

        int main() {
            int input_width = 1920;
            int input_height = 1080;
            int output_width = 1920;
            int output_height = 1080;
            int format = 8;
            int interpolation_mode = 1; //0-nearest 1-bilinear
            const char *mapx_path = "mapx.bin";
            const char *mapy_path = "mapy.bin";
            const char *input_path = "rgbplanar_1920_1080.bin";
            const char *output_path = "remap_output.bin";
            int ret = 0;
            bm_handle_t handle;
            ret = bm_dev_request(&handle, 0);
            if (ret != BM_SUCCESS) {
                printf("bm_dev_request failed. ret = %d\n", ret);
                return -1;
            }
            unsigned char *input_data, *output_tpu;
            float *mapx_data = (float*)malloc(output_width * output_height * 3 * sizeof(float));
            float *mapy_data = (float*)malloc(output_width * output_height * 3 * sizeof(float));
            input_data = (unsigned char*)malloc(input_width * input_height * 3);
            output_tpu = (unsigned char*)malloc(output_width * output_height * 3);
            read_bin(input_path, input_data, input_width, input_height, 3);
            read_bin_float(mapx_path, mapx_data, output_width, output_height, 3);
            read_bin_float(mapy_path, mapy_data, output_width, output_height, 3);
            bm_status_t bm_ret = BM_SUCCESS;
            bm_image input_image, output_image;
            bm_device_mem_t mapx_data_global_addr, mapy_data_global_addr;
            bm_ret = bm_image_create(handle, input_height, input_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_image, NULL);
            if (bm_ret != BM_SUCCESS) {
                printf("bm_image_create input_image error\n");
                return bm_ret;
            }
            bm_ret = bm_image_create(handle, output_height, output_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_image, NULL);
            if (bm_ret != BM_SUCCESS) {
                printf("bm_image_create output_image error\n");
                return bm_ret;
            }
            bm_ret = bm_image_alloc_dev_mem(input_image, BMCV_HEAP_ANY);
            if (bm_ret != BM_SUCCESS) {
                printf("bm_image_alloc_dev_mem input_image error\n");
                return bm_ret;
            }
            bm_ret = bm_image_alloc_dev_mem(output_image, BMCV_HEAP_ANY);
            if (bm_ret != BM_SUCCESS) {
                printf("bm_image_alloc_dev_mem output_image error\n");
                return bm_ret;
            }
            unsigned char *input_addr[3] = {input_data, input_data + input_height * input_width, input_data + 2 * input_height * input_width};
            bm_ret = bm_image_copy_host_to_device(input_image, (void **)(input_addr));
            if (bm_ret != BM_SUCCESS) {
                printf("bm_image_copy_host_to_device input_image error\n");
                return bm_ret;
            }
            bm_ret = bm_malloc_device_byte(handle, &mapx_data_global_addr, output_width * output_height * sizeof(float));
            if (BM_SUCCESS != bm_ret) {
                printf("bm_malloc_device_byte mapx_data_global_addr error\n");
                return bm_ret;
            }
            bm_ret = bm_malloc_device_byte(handle, &mapy_data_global_addr, output_width * output_height * sizeof(float));
            if (BM_SUCCESS != bm_ret) {
                printf("bm_malloc_device_byte mapy_data_global_addr error\n");
                return bm_ret;
            }
            bm_ret = bm_memcpy_s2d(handle, mapx_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(mapx_data)));
            if (bm_ret != BM_SUCCESS) {
                printf("bm_memcpy_s2d mapx_data error\n");
                return bm_ret;
            }
            bm_ret = bm_memcpy_s2d(handle, mapy_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(mapy_data)));
            if (bm_ret != BM_SUCCESS) {
                printf("bm_memcpy_s2d mapy_data error\n");
                return bm_ret;
            }
            bm_ret = bmcv_image_remap(handle, input_image, output_image, mapx_data_global_addr, mapy_data_global_addr, interpolation_mode);
            if(bm_ret != BM_SUCCESS){
                printf("bmcv_image_remap error\n");
                return bm_ret;
            }
            unsigned char *output_addr[3] = {output_tpu, output_tpu + output_height * output_width, output_tpu + 2 * output_height * output_width};
            bm_ret = bm_image_copy_device_to_host(output_image, (void **)output_addr);
            if (bm_ret != BM_SUCCESS) {
                printf("bm_image_copy_device_to_host output_image error\n");
            }
            write_bin(output_path, output_tpu, output_width, output_height, 3);
            free(input_data);
            free(output_tpu);
            free(mapx_data);
            free(mapy_data);
            bm_image_destroy(input_image);
            bm_image_destroy(output_image);
            bm_free_device(handle, mapx_data_global_addr);
            bm_free_device(handle, mapy_data_global_addr);
            bm_dev_free(handle);
            return ret;
        }
