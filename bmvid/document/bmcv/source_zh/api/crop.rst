bmcv_image_crop
===============

该接口实现从一幅原图中 crop 出若干个小图。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_crop(
                    bm_handle_t handle,
                    int crop_num,
                    bmcv_rect_t* rects,
                    bm_image input,
                    bm_image* output);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* int crop_num

  输入参数。需要 crop 小图的数量，既是指针 rects 所指向内容的长度，也是输出 bm_image 的数量。

* bmcv_rect_t\* rects

  输入参数。表示crop相关的信息，包括起始坐标、crop宽高等，具体内容参考下边的数据类型说明。该指针指向了若干个 crop 框的信息，框的个数由 crop_num 决定。

* bm_image input

  输入参数。输入的 bm_image，bm_image 需要外部调用 bmcv_image _create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image\* output

  输出参数。输出 bm_image 的指针，其数量即为 crop_num。bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**数据类型说明：**


    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;

* start_x 描述了 crop 图像在原图中所在的起始横坐标。自左而右从 0 开始，取值范围 [0, width)。

* start_y 描述了 crop 图像在原图中所在的起始纵坐标。自上而下从 0 开始，取值范围 [0, height)。

* crop_w 描述的 crop 图像的宽度，也就是对应输出图像的宽度。

* crop_h 描述的 crop 图像的高度，也就是对应输出图像的高度。


**格式支持：**

crop 目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
|  1  | FORMAT_BGR_PACKED      |
+-----+------------------------+
|  2  | FORMAT_BGR_PLANAR      |
+-----+------------------------+
|  3  | FORMAT_RGB_PACKED      |
+-----+------------------------+
|  4  | FORMAT_RGB_PLANAR      |
+-----+------------------------+
|  5  | FORMAT_GRAY            |
+-----+------------------------+


bm1684 crop 目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
|  1  | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+
|  2  | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+
|  3  | DATA_TYPE_EXT_1N_BYTE_SIGNED   |
+-----+--------------------------------+

bm1684x crop 目前支持以下data_type：

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+


**注意事项：**

1、在调用 bmcv_image_crop()之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type，image_format必须相同。

3、为了避免内存越界, start_x + crop_w 必须小于等于输入图像的 width， start_y + crop_h 必须小于等于输入图像的 height。


**代码示例：**

    .. code-block:: c

        #include <cfloat>
        #include <cstdio>
        #include <cstdlib>
        #include <iostream>
        #include <math.h>
        #include <string.h>
        #include <vector>
        #include "bmcv_api.h"
        #include "test_misc.h"

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
            int channel = 3;
            int in_w = 1024;
            int in_h = 1024;
            int out_w = 64;
            int out_h = 64;
            int dev_id = 0;
            bm_handle_t handle;
            bmcv_rect_t crop_attr;
            bm_image input, output;
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";
            unsigned char* src_data = new unsigned char[channel * in_w * in_h];
            unsigned char* res_data = new unsigned char[channel * out_w * out_h];

            bm_dev_request(&handle, dev_id);
            readBin(input_path, src_data, channel * in_w * in_h);

            crop_attr.start_x = 0;
            crop_attr.start_y = 0;
            crop_attr.crop_w = 50;
            crop_attr.crop_h = 50;

            bm_image_create(handle, in_h, in_w, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void **)&src_data);
            bm_image_create(handle, out_h, out_w, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_crop(handle, 1, &crop_attr, input, &output);
            bm_image_copy_device_to_host(output, (void **)&res_data);
            writeBin(output_path, res_data, channel * out_w * out_h);

            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            delete[] src_data;
            delete[] res_data;
            return 0;
        }