bmcv_image_assigned_area_blur
==================

Blurs the specified area of the image.


**Processor model support**

This interface only supports BM1684X.


**Interface form**

    .. code-block:: c

        bm_status_t bmcv_image_assigned_area_blur(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int ksize,
                    int assigned_area_num,
                    float center_weight_scale,
                    bmcv_rect_t *assigned_area);


**Input parameter description**

* bm_handle_t handle

  Input parameter. bm_handle Handle.

* bm_image input

  Input parameters. Input image bm_image, bm_image needs to be created externally by calling bmcv_image_create. Image memory can be allocated new memory using bm_image_alloc_dev_mem or bm_image_copy_host_to_device, or existing memory can be attached using bmcv_image_attach.

* bm_image output

  Output parameter. Output bm_image. bm_image needs to be created by externally calling bmcv_image_create. Image memory can be allocated through bm_image_alloc_dev_mem, or bmcv_image_attach can be used to attach existing memory. If not actively allocated, it will be allocated automatically within the API.

* int ksize

  Input parameters. The width and height of the region blur convolution kernel. The valid values of ksize are 3, 5, 7, 9, 11.

* int assigned_area_num

  Input parameter. The number of blurred areas. The valid value of assigned_area_num is 1-200.

* float center_weight_scale

  Input parameter. The center weight scaling factor of the area blur convolution kernel. The smaller the scaling factor, the lower the weight of the center pixel during convolution and the higher the weight of the edge pixel. The valid value of center_weight_scale is 0.0-1.0.

* bmcv_rect_t *assigned_area

    Detailed information of the specified area. It includes the starting horizontal and vertical coordinates of the specified area, as well as the width and height of the specified area. The bmcv_rect_t structure is defined as follows.

        typedef struct bmcv_rect {
            unsigned int start_x;
            unsigned int start_y;
            unsigned int crop_w;
            unsigned int crop_h;
        } bmcv_rect_t;

**Return value description**

* BM_SUCCESS: success

* Other: failed


**Format support**

This interface currently supports the following image_format:

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

Currently the following data_type are supported:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling bmcv_image_assigned_area_blur(), you must ensure that the input image memory has been applied.

2. The data_type and image_format of input and output must be the same.

3. The valid value of the base map size to be processed is 8*8-4096-4096, the valid value of the starting horizontal coordinate of the specified area is 0 to the base map width, the valid value of the vertical coordinate is 0 to the base map height, the valid value of the specified area width is 1 to (base map width - starting horizontal coordinate), and the valid value of the height is 1 to (base map height - starting vertical coordinate).

4. If the input image format is yuv420p and yuv444p, you can create bm_image as FORMAT_GRAY format before calling the interface. After entering the operator, only the Y channel is processed. The processing effect is similar to the three-channel processing, and the performance is much better than the three-channel processing. For specific processing, please refer to the code example.


**Code example**

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
                printf("unable to open input file%s: %s\n", input_path, strerror(errno));
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
                printf("unable to open output file %s\n", output_path);
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
