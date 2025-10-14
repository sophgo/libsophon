bmcv_image_overlay
==================

该API实现了在图像上覆盖具有alpha通道的水印。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式**

    .. code-block:: c

        bm_status_t bmcv_image_overlay(
                    bm_handle_t handle,
                    bm_image input_base_img,
                    int overlay_num,
                    bmcv_rect_t* overlay_info,
                    bm_image* input_overlay_img);


**参数说明**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input_base_img

  输入/输出参数。对应底图的 bm_image 图片信息。

* int overlay_num

  输入参数。水印数量。

* bmcv_rect_t* overlay_info

  输入参数。关于输入图像上水印位置的信息。

* bm_image* input_overlay_img

  输入参数。输入的 bm_image 对象 ，用于叠加。


**数据类型支持**

    .. code-block:: c

      typedef struct bmcv_rect {
          unsigned int start_x;
          unsigned int start_y;
          unsigned int crop_w;
          unsigned int crop_h;
      } bmcv_rect_t;

`start_x`, `start_y`, `crop_w` 和 `crop_h` 表示覆盖图像在输入图像上的位置和大小信息，包括起始点的x坐标、起始点的y坐标、覆盖图像的宽度和覆盖图像的高度。图像的左上角用作原点。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项**

1. 基础图像和输出图像的颜色格式如下：

+-----+-------------------------------+-------------------------------+
| 编号| 输入图像格式                  | 输出图像格式                  |
+=====+===============================+===============================+
| 1   | FORMAT_RGB_PACKED             | FORMAT_RGB_PACKED             |
+-----+-------------------------------+-------------------------------+

2. 叠加图像的颜色格式如下：

+-----+-------------------------------+
| 编号| 输入叠加图像格式              |
+=====+===============================+
| 1   | FORMAT_ABGR_PACKED            |
+-----+-------------------------------+
| 2   | FORMAT_ARGB1555_PACKED        |
+-----+-------------------------------+
| 3   | FORMAT_ARGB4444_PACKED        |
+-----+-------------------------------+

3. 目前支持的输入和输出图像数据的 data_type 值如下：

+-----+-------------------------------+
| 编号| 数据类型                      |
+=====+===============================+
| 1   | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

4. 背景图像支持的最小尺寸为8 * 8，最大尺寸为8192 * 8192。

5. 最大可以叠加的图像数量为10, 叠图最大尺寸为850 * 850（单张叠图最大尺寸也为850 * 850）。


**代码示例**

    .. code-block:: c++

        #include "stb_image.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <random>
        #include <algorithm>
        #include <vector>
        #include <iostream>
        #include <string.h>
        #include "bmcv_api_ext.h"

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
            int overlay_num = 1;
            int base_width = 1920;
            int base_height = 1080;
            int pos_x[overlay_num] = {50};
            int pos_y[overlay_num] = {150};
            int overlay_width[overlay_num] = {400};
            int overlay_height[overlay_num] = {400};
            bm_handle_t handle;
            unsigned char* base_image = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
            unsigned char* output_tpu = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
            unsigned char* overlay_image[overlay_num];
            bm_image input_base_img;
            bm_image input_overlay_img[overlay_num];
            unsigned char** in_overlay_ptr[overlay_num];
            bmcv_rect rect_array[overlay_num];
            unsigned char* out_ptr[1] = {output_tpu};
            const char *base_path = "path/to/base";
            const char *overlay_path = "path/to/overlay";
            const char *output_path = "path/to/output";

            bm_dev_request(&handle, 0);
            for (int i = 0; i < overlay_num; i++) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 4 * sizeof(unsigned char));
                readBin(overlay_path, overlay_image[i], overlay_width[i] * overlay_height[i] * 4);
            }

            readBin(base_path, base_image, base_width * base_height * 3);
            memcpy(output_tpu, base_image, base_width * base_height * 3);

            for (int i = 0; i < overlay_num; i++) {
                bm_image_create(handle, overlay_height[i], overlay_width[i], FORMAT_ABGR_PACKED, DATA_TYPE_EXT_1N_BYTE, input_overlay_img + i, NULL);
            }
            for (int i = 0; i < overlay_num; i++) {
                bm_image_alloc_dev_mem(input_overlay_img[i], 2);
            }

            for (int i = 0; i < overlay_num; i++) {
                in_overlay_ptr[i] = new unsigned char*[1];
                in_overlay_ptr[i][0] = overlay_image[i];
            }
            for (int i = 0; i < overlay_num; i++) {
                bm_image_copy_host_to_device(input_overlay_img[i], (void **)in_overlay_ptr[i]);
            }

            bm_image_create(handle, base_height, base_width, FORMAT_RGB_PACKED, DATA_TYPE_EXT_1N_BYTE, &input_base_img, NULL);
            bm_image_alloc_dev_mem(input_base_img, 2);
            unsigned char* in_base_ptr[1] = {output_tpu};
            bm_image_copy_host_to_device(input_base_img, (void **)in_base_ptr);

            for (int i = 0; i < overlay_num; i++) {
                rect_array[i].start_x = pos_x[i];
                rect_array[i].start_y = pos_y[i];
                rect_array[i].crop_w = overlay_width[i];
                rect_array[i].crop_h = overlay_height[i];
            }

            bmcv_image_overlay(handle, input_base_img, overlay_num, rect_array, input_overlay_img);
            bm_image_copy_device_to_host(input_base_img, (void **)out_ptr);
            writeBin(output_path, output_tpu, base_width * base_height * 3);

            bm_image_destroy(input_base_img);
            for (int i = 0; i < overlay_num; i++) {
                bm_image_destroy(input_overlay_img[i]);
            }
            free(base_image);
            free(output_tpu);
            for (int i = 0; i < overlay_num; i++) {
                free(overlay_image[i]);
            }
            bm_dev_free(handle);
            return 0;
        }