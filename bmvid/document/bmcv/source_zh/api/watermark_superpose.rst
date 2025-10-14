bmcv_image_watermark_superpose
===============================

该接口用于在图像上叠加一个或多个水印。该接口可搭配bmcv_gen_text_watermark接口实现绘制中英文的功能，参照代码示例2。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式一：**
    .. code-block:: c

        bm_status_t bmcv_image_watermark_superpose(
                    bm_handle_t handle,
                    bm_image* image,
                    bm_device_mem_t* bitmap_mem,
                    int bitmap_num,
                    int bitmap_type,
                    int pitch,
                    bmcv_rect_t* rects,
                    bmcv_color_t color);

  此接口可实现在不同的输入图的指定位置，叠加不同的水印。


**接口形式二：**
    .. code-block:: c

        bm_status_t bmcv_image_watermark_repeat_superpose(
                    bm_handle_t handle,
                    bm_image image,
                    bm_device_mem_t bitmap_mem,
                    int bitmap_num,
                    int bitmap_type,
                    int pitch,
                    bmcv_rect_t* rects,
                    bmcv_color_t color);

  此接口为接口一的简化版本，可在一张图中的不同位置重复叠加一种水印。


**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* bm_image\* image

  输入参数。需要打水印的 bm_image 对象指针。

* bm_device_mem_t\* bitmap_mem

  输入参数。水印的 bm_device_mem_t 对象指针。

* int bitmap_num

  输入参数。水印数量，指 rects 指针中所包含的 bmcv_rect_t 对象个数、 也是 image 指针中所包含的 bm_image 对象个数、 也是 bitmap_mem 指针中所包含的 bm_device_mem_t 对象个数。

* int bitmap_type

  输入参数。水印类型, 值0表示水印为8bit数据类型(有透明度信息), 值1表示水印为1bit数据类型(无透明度信息)。

* int pitch

  输入参数。水印文件每行的byte数, 可理解为水印的宽。

* bmcv_rect_t\* rects

  输入参数。水印位置指针，包含每个水印起始点和宽高。具体内容参考下面的数据类型说明。

* bmcv_color_t color

  输入参数。水印的颜色。具体内容参考下面的数据类型说明。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型说明：**

    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;

* start_x 描述了水印在原图中所在的起始横坐标。自左而右从 0 开始，取值范围 [0, width)。

* start_y 描述了水印在原图中所在的起始纵坐标。自上而下从 0 开始，取值范围 [0, height)。

* crop_w 描述的水印的宽度。

* crop_h 描述的水印的高度。

* r 颜色的r分量。

* g 颜色的g分量。

* b 颜色的b分量。


**注意事项:**

1. bm1684x要求如下：

- 输入和输出的数据类型必须为：

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

- 输入的色彩格式可支持：

+-----+-------------------------------+
| num | image_format                  |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_YUV444P                |
+-----+-------------------------------+
|  3  | FORMAT_NV12                   |
+-----+-------------------------------+
|  4  | FORMAT_NV21                   |
+-----+-------------------------------+
|  5  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  6  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  7  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  8  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+
|  9  | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  10 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  11 | FORMAT_GRAY                   |
+-----+-------------------------------+

如果不满足输入输出格式要求，则返回失败。

2. 输入输出所有 bm_image 结构必须提前创建，否则返回失败。

3. 水印数量最多可设置512个。

4. 如果水印区域超出原图宽高，会返回失败。


**代码示例1**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include <sstream>
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
            bm_handle_t handle = NULL;
            int src_w, src_h, water_h, water_w, font_mode, water_byte;
            bmcv_color_t color;
            bm_image src;
            int dev_id = 0;
            bm_device_mem_t water;
            unsigned char* water_data;
            int font_num;
            bmcv_rect_t* rect;
            const char *filename_src = "path/to/src";
            const char *filename_water = "path/to/water_file";
            const char *filename_dst = "path/to/dst";

            src_w = 800;
            src_h = 800;
            font_mode = 0;
            water_byte = 1024;
            water_w = 32;
            water_h = 32;
            dev_id = 0;
            color.r = 128;
            color.g = 128;
            color.b = 128;
            water_data = new unsigned char [water_byte];
            bm_dev_request(&handle, dev_id);
            font_num = 2;
            rect = new bmcv_rect_t [font_num];

            unsigned char* input_data = (unsigned char*)malloc(src_h * src_w);
            unsigned char* in_ptr[3] = {input_data, input_data + src_h * src_w, input_data + 2 * src_h * src_w};

            for(int font_idx = 0; font_idx < font_num; font_idx++) {
                rect[font_idx].start_x = font_idx * water_w;
                rect[font_idx].start_y = font_idx * water_h;
                rect[font_idx].crop_w = water_w;
                rect[font_idx].crop_h = water_h;
            }
            readBin(filename_src, input_data, src_h * src_w);
            readBin(filename_water, water_data, water_byte);

            bm_malloc_device_byte(handle, &water, water_byte);
            bm_memcpy_s2d(handle, water, (void*)water_data);
            bm_image_create(handle, src_h, src_w, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_alloc_dev_mem(src);
            bm_image_copy_host_to_device(src, (void**)in_ptr);
            bmcv_image_watermark_repeat_superpose(handle, src, water, font_num, font_mode, water_w, rect, color);
            bm_image_copy_device_to_host(src, (void **)in_ptr);
            writeBin(filename_dst, input_data, src_h * src_w);

            bm_image_destroy(src);
            bm_free_device(handle, water);
            bm_dev_free(handle);
            delete [] rect;
            delete [] water_data;
            free(input_data);
            return 0;
        }


**代码示例2**

    .. code-block:: c

        #include <stdio.h>
        #include <string.h>
        #include <math.h>
        #include <stdbool.h>
        #include <stdlib.h>
        #include <iostream>
        #include <cstring>
        #include <wchar.h>
        #include <locale.h>
        #include <bmcv_api_ext.h>

        #define BITMAP_1BIT 1
        #define BITMAP_8BIT 0

        int main(int argc, char* args[]){

            setlocale(LC_ALL, "");
            bm_status_t ret = BM_SUCCESS;
            wchar_t hexcode[256];
            unsigned char r = 255, g = 255, b = 0, fontScale = 2;
            std::string output_path = "out.bmp";
            if ((argc == 1) ||
                (argc == 2 && atoi(args[1]) == -1)) {
                printf("usage: %d\n", argc);
                printf("%s text_string r g b fontscale out_name\n", args[0]);
                printf("example:\n");
                printf("%s bitmain.go\n", args[0]);
                printf("%s bitmain.go 255 255 255 2 out.bmp\n", args[0]);
                return 0;
            }
            mbstowcs(hexcode, args[1], sizeof(hexcode) / sizeof(wchar_t)); //usigned
            printf("Received wide character string: %ls\n", hexcode);
            if (argc > 2) r = atoi(args[2]);
            if (argc > 3) g = atoi(args[3]);
            if (argc > 4) b = atoi(args[4]);
            if (argc > 5) fontScale = atoi(args[5]);
            if (argc > 6) output_path = args[6];
            printf("output path: %s\n", output_path.c_str());

            bm_image image;
            bm_handle_t handle = NULL;
            bm_dev_request(&handle, 0);
            bm_image_create(handle, 1080, 1920, FORMAT_YUV420P, DATA_TYPE_EXT_1N_BYTE, &image, NULL);
            bm_image_alloc_dev_mem(image, BMCV_HEAP1_ID);
            bm_read_bin(image,"path/to/src");
            bmcv_point_t org;
            org.x = 10;
            org.y = 10;
            bmcv_color_t color;
            color.r = r;
            color.g = g;
            color.b = b;

            bm_image watermark;
            bm_device_mem_t watermark_mem;
            bmcv_rect_t rect;
            int stride;
            ret = bmcv_gen_text_watermark(handle, hexcode, color, fontScale, FORMAT_GRAY, &watermark);
            if (ret != BM_SUCCESS) {
                printf("bmcv_gen_text_watermark fail\n");
                goto fail1;
            }

            rect.start_x = org.x;
            rect.start_y = org.y;
            rect.crop_w = watermark.width;
            rect.crop_h = watermark.height;

            bm_image_get_stride(watermark, &stride);
            bm_image_get_device_mem(watermark, &watermark_mem);
            ret = bmcv_image_watermark_superpose(handle, &image, &watermark_mem, 1, BITMAP_8BIT,
                stride, &rect, color);
            if (ret != BM_SUCCESS) {
                printf("bmcv_image_overlay fail\n");
                goto fail2;
            }
            bm_image_write_to_bmp(image, output_path.c_str());

        fail2:
            bm_image_destroy(watermark);
        fail1:
            bm_image_destroy(image);
            bm_dev_free(handle);
            return ret;
        }