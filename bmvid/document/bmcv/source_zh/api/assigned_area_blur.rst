bmcv_image_assigned_area_blur
==================

对图像指定区域进行模糊处理。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_assigned_area_blur(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int ksize,
                    int assigned_area_num,
                    float center_weight_scale,
                    bmcv_rect_t *assigned_area);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* int ksize

  输入参数。区域模糊卷积核的宽高尺寸。ksize的有效值为3，5，7，9，11。

* int assigned_area_num

  输入参数。模糊区域的个数。assigned_area_num的有效值为1-200。

* float center_weight_scale

  输入参数。区域模糊卷积核中心权重缩放系数。缩放系数越小，卷积时中心像素权重越低，边缘像素权重约高。center_weight_scale有效值为0.0-1.0。

* bmcv_rect_t *assigned_area

    指定区域的详细信息。其中包含指定区域的起始横坐标和纵坐标，还有指定区域的宽和高大小。其中bmcv_rect_t结构体定义如下。

        typedef struct bmcv_rect {
            unsigned int start_x;
            unsigned int start_y;
            unsigned int crop_w;
            unsigned int crop_h;
        } bmcv_rect_t;

**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_YUV420P         |
+-----+------------------------+
| 2   | FORMAT_YUV444P         |
+-----+------------------------+
| 3   | FORMAT_RGB_PLANNAR     |
+-----+------------------------+
| 4   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 5   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 6   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 7   | FORMAT_GRAY            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用 bmcv_image_assigned_area_blur()之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type，image_format必须相同。

3、需要处理的底图尺寸有效值为8*8-4096-4096,指定区域起始横坐标有效值为0～底图宽，纵坐标有效值为0～底图高，指定区域宽的有效值为1～（底图宽 - 起始横坐标），高的有效值为1～（底图高 - 起始纵坐标）。

4、如果输入图像格式为yuv420p和yuv444p，可在调用接口前将bm_image创建为FORMAT_GRAY格式，进入算子后仅处理Y通道，处理效果和三通道处理近似，性能较三通道处理有较大优势，具体处理请参考代码示例。


**代码示例：**

    .. code-block:: c

        #include <cerrno>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <sys/time.h>
        #include <pthread.h>
        #include <math.h>
        #include "bmcv_api_ext_c.h"
        #define ASSIGNED_AREA_MAX_NUM 200

        static int read_bin_yuv420p(const char *input_path, unsigned char *input_data, int width, int height, float channel) {
            FILE *fp_src = fopen(input_path, "rb");
            if (fp_src == NULL) {
                printf("无法打开输入文件 %s: %s\n", input_path, strerror(errno));
                return -1;
            }
            if(fread(input_data, sizeof(char), width * height * channel, fp_src) != 0) {
                printf("read image success\n");
            }
            fclose(fp_src);
            return 0;
        }

        static int write_bin_yuv420p(const char *output_path, unsigned char *output_data, int width, int height, float channel) {
            FILE *fp_dst = fopen(output_path, "wb");
            if (fp_dst == NULL) {
                printf("无法打开输出文件 %s\n", output_path);
                return -1;
            }
            fwrite(output_data, sizeof(char), width * height * channel, fp_dst);
            fclose(fp_dst);
            return 0;
        }

        //yuv420p——api input yuv420p
        int main() {
            int base_width = 1920;
            int base_height = 1088;
            int ksize = 5;
            int assigned_area_num = 5;
            int format = FORMAT_YUV420P;
            int dev_id = 0;
            float center_weight_scale = 0.01;
            const char* input_path = "image/420_1920x1088_input.bin";
            const char* output_path = "image/2105output_assigned_area.bin";
            bm_handle_t handle;
            bm_dev_request(&handle, dev_id);
            bmcv_rect assigned_area[ASSIGNED_AREA_MAX_NUM];
            bm_image img_i, img_o;
            int assigned_width[ASSIGNED_AREA_MAX_NUM];
            int assigned_height[ASSIGNED_AREA_MAX_NUM];
            int dis_x_max[ASSIGNED_AREA_MAX_NUM];
            int dis_y_max[ASSIGNED_AREA_MAX_NUM];
            int start_x[ASSIGNED_AREA_MAX_NUM];
            int start_y[ASSIGNED_AREA_MAX_NUM];

            for (int i = 0; i < assigned_area_num; i++) {
                int w = 1 + rand() % (500);
                int h = 1 + rand() % (500);
                assigned_width[i] = w > base_width ? base_width : w;
                assigned_height[i] = h > base_height ? base_height : h;
                dis_x_max[i] = base_width - assigned_width[i];
                dis_y_max[i] = base_height - assigned_height[i];
                start_x[i] = rand() % (dis_x_max[i] + 1);
                start_y[i] = rand() % (dis_y_max[i] + 1);
            }

            for (int i = 0; i < assigned_area_num; i++) {
                assigned_area[i].start_x = start_x[i];
                assigned_area[i].start_y = start_y[i];
                assigned_area[i].crop_w = assigned_width[i];
                assigned_area[i].crop_h = assigned_height[i];
            }
            unsigned char *input_data = (unsigned char*)malloc(base_width * base_height * 3 / 2);
            unsigned char *output_tpu = (unsigned char*)malloc(base_width * base_height * 3 / 2);
            read_bin_yuv420p(input_path, input_data, base_width, base_height, 1.5);
            bm_image_create(handle, base_height, base_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_i, NULL);
            bm_image_create(handle, base_height, base_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_o, NULL);
            bm_image_alloc_dev_mem(img_i, 2);
            bm_image_alloc_dev_mem(img_o, 2);
            unsigned char *input_addr[3] = {input_data, input_data + base_height * base_width, input_data + base_height * base_width * 5 / 4};
            bm_image_copy_host_to_device(img_i, (void **)(input_addr));
            bmcv_image_assigned_area_blur(handle, img_i, img_o, ksize, assigned_area_num, center_weight_scale, assigned_area);
            unsigned char *output_addr[3] = {output_tpu, output_tpu + base_height * base_width, output_tpu + base_height * base_width * 5 / 4};
            bm_image_copy_device_to_host(img_o, (void **)output_addr);
            write_bin_yuv420p(output_path, output_tpu, base_width, base_height, 1.5);
            bm_image_destroy(img_i);
            bm_image_destroy(img_o);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }

        //yuv420p——api input gray
        int main() {
            int base_width = 1920;
            int base_height = 1088;
            int ksize = 5;
            int assigned_area_num = 5;
            int dev_id = 0;
            float center_weight_scale = 0.01;
            const char* input_path = "image/420_1920x1088_input.bin";
            const char* output_path = "image/2105output_assigned_area.bin";
            bm_handle_t handle;
            bm_dev_request(&handle, dev_id);
            bmcv_rect assigned_area[ASSIGNED_AREA_MAX_NUM];
            bm_image img_i, img_o;
            int assigned_width[ASSIGNED_AREA_MAX_NUM];
            int assigned_height[ASSIGNED_AREA_MAX_NUM];
            int dis_x_max[ASSIGNED_AREA_MAX_NUM];
            int dis_y_max[ASSIGNED_AREA_MAX_NUM];
            int start_x[ASSIGNED_AREA_MAX_NUM];
            int start_y[ASSIGNED_AREA_MAX_NUM];

            for (int i = 0; i < assigned_area_num; i++) {
                int w = 1 + rand() % (500);
                int h = 1 + rand() % (500);
                assigned_width[i] = w > base_width ? base_width : w;
                assigned_height[i] = h > base_height ? base_height : h;
                dis_x_max[i] = base_width - assigned_width[i];
                dis_y_max[i] = base_height - assigned_height[i];
                start_x[i] = rand() % (dis_x_max[i] + 1);
                start_y[i] = rand() % (dis_y_max[i] + 1);
            }

            for (int i = 0; i < assigned_area_num; i++) {
                assigned_area[i].start_x = start_x[i];
                assigned_area[i].start_y = start_y[i];
                assigned_area[i].crop_w = assigned_width[i];
                assigned_area[i].crop_h = assigned_height[i];
            }
            unsigned char *input_data = (unsigned char*)malloc(base_width * base_height * 3 / 2);
            unsigned char *output_tpu = (unsigned char*)malloc(base_width * base_height * 3 / 2);
            read_bin_yuv420p(input_path, input_data, base_width, base_height, 1.5);
            bm_image_create(handle, base_height, base_width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img_i, NULL);
            bm_image_create(handle, base_height, base_width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img_o, NULL);
            bm_image_alloc_dev_mem(img_i, 2);
            bm_image_alloc_dev_mem(img_o, 2);
            unsigned char *input_addr[1] = {input_data};
            bm_image_copy_host_to_device(img_i, (void **)(input_addr));
            bm_image_copy_host_to_device(img_o, (void **)(input_addr));
            bmcv_image_assigned_area_blur(handle, img_i, img_o, ksize, assigned_area_num, center_weight_scale, assigned_area);
            memcpy(output_tpu, input_data, base_height * base_width * 3 / 2);
            unsigned char *output_addr[1] = {output_tpu};
            bm_image_copy_device_to_host(img_o, (void **)output_addr);
            write_bin_yuv420p(output_path, output_tpu, base_width, base_height, 1.5);
            bm_image_destroy(img_i);
            bm_image_destroy(img_o);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }
