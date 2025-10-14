bmcv_image_put_text
=====================

可以实现在一张图像上写字的功能（中英文），并支持指定字的颜色、大小和宽度。


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

        bm_status_t bmcv_image_put_text(
                    bm_handle_t handle,
                    bm_image image,
                    const char* text,
                    bmcv_point_t org,
                    bmcv_color_t color,
                    float fontScale,
                    int thickness);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image image

  输入/输出参数。需处理图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* const char* text

  输入参数。待写入的文本内容，当文本内容中有中文时，参数thickness请设置为0。

* bmcv_point_t org

  输入参数。第一个字符左下角的坐标位置。图像左上角为原点，向右延伸为x方向，向下延伸为y方向。

* bmcv_color_t color

  输入参数。画线的颜色，分别为RGB三个通道的值。

* float fontScale

  输入参数。字体大小。

* int thickness

  输入参数。画线的宽度，对于YUV格式的图像建议设置为偶数。开启中文字库请将该参数设置为0。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1. 该接口目前支持以下 image_format:

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

thickness参数配置为0，即开启中文字库后，image_format 扩展支持 bmcv_image_watermark_superpose API 底图支持的格式。

2. 目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+

3. 若文字内容不变，推荐使用 bmcv_gen_text_watermark 与 bmcv_image_watermark_superpose 搭配的文字绘制方式，文字生成水印图，重复使用水印图进行osd叠加，以提高处理效率。示例参见bmcv_image_watermark_superpose接口文档。

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
        float fontScale = 4;
        char text[20] = "hello world";
        bmcv_point_t org = {100, 100};
        bmcv_color_t color = {255, 0, 0};
        bm_handle_t handle;
        bm_image img;
        const char* input_path = "path/to/input";
        const char* output_path = "path/to/output";
        unsigned char* data_ptr = new unsigned char[channel * width * height];

        readBin(input_path, data_ptr, channel * width * height);
        bm_dev_request(&handle, dev_id);
        bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img);
        bm_image_alloc_dev_mem(img);
        bm_image_copy_host_to_device(img, (void**)&data_ptr);
        bmcv_image_put_text(handle, img, text, org, color, fontScale, thickness);
        bm_image_copy_device_to_host(img, (void**)&data_ptr);
        writeBin(output_path, data_ptr, channel * width * height);

        bm_image_destroy(img);
        bm_dev_free(handle);
        free(data_ptr);
        return 0;
      }