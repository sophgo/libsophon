bmcv_image_draw_lines
======================

可以实现在一张图像上画一条或多条线段，从而可以实现画多边形的功能，并支持指定线的颜色和线的宽度。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;

        bm_status_t bmcv_image_draw_lines(
                    bm_handle_t handle,
                    bm_image img,
                    const bmcv_point_t* start,
                    const bmcv_point_t* end,
                    int line_num,
                    bmcv_color_t color,
                    int thickness);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image img

  输入/输出参数。需处理图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* const bmcv_point_t* start

  输入参数。线段起始点的坐标指针，指向的数据长度由 line_num 参数决定。图像左上角为原点，向右延伸为x方向，向下延伸为y方向。

* const bmcv_point_t* end

  输入参数。线段结束点的坐标指针，指向的数据长度由 line_num 参数决定。图像左上角为原点，向右延伸为x方向，向下延伸为y方向。

* int line_num

  输入参数。需要画线的数量。

* bmcv_color_t color

  输入参数。画线的颜色，分别为RGB三个通道的值。

* int thickness

  输入参数。画线的宽度，对于YUV格式的图像建议设置为偶数。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_GRAY            |
+-----+------------------------+
| 2   | FORMAT_YUV420P         |
+-----+------------------------+
| 3   | FORMAT_YUV422P         |
+-----+------------------------+
| 4   | FORMAT_YUV444P         |
+-----+------------------------+
| 5   | FORMAT_NV12            |
+-----+------------------------+
| 6   | FORMAT_NV21            |
+-----+------------------------+
| 7   | FORMAT_NV16            |
+-----+------------------------+
| 8   | FORMAT_NV61            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**代码示例：**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>

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
            int thickness = 4;
            bmcv_point_t start = {0, 0};
            bmcv_point_t end = {100, 100};
            bmcv_color_t color = {255, 0, 0};
            bm_image img;
            bm_handle_t handle;
            unsigned char* data_ptr = new unsigned char[channel * width * height];
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";

            bm_dev_request(&handle, dev_id);
            readBin(input_path, data_ptr, channel * width * height);

            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img);
            bm_image_alloc_dev_mem(img);
            bm_image_copy_host_to_device(img, (void**)&data_ptr);
            bmcv_image_draw_lines(handle, img, &start, &end, 1, color, thickness);
            bm_image_copy_device_to_host(img, (void**)&data_ptr);
            writeBin(output_path, data_ptr, channel * width * height);

            bm_image_destroy(img);
            bm_dev_free(handle);
            delete[] data_ptr;
            return 0;
        }