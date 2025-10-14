bmcv_image_draw_point
=========================

该接口用于在图像上填充一个或者多个point。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**
    .. code-block:: c

        bm_status_t bmcv_image_draw_point(
                    bm_handle_t handle,
                    bm_image image,
                    int point_num,
                    bmcv_point_t* coord,
                    int length,
                    unsigned char r,
                    unsigned char g,
                    unsigned char b);


**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* bm_image image

  输入参数。需要在其上填充point的 bm_image 对象。

* int point_num

  输入参数。需填充point的数量，指 coord 指针中所包含的 bmcv_point_t 对象个数。

* bmcv_point_t\* rect

  输入参数。point位置指针。具体内容参考下面的数据类型说明。

* int length

  输入参数。point的边长，取值范围为[1, 510]。

* unsigned char r

  输入参数。矩形填充颜色的r分量。

* unsigned char g

  输入参数。矩形填充颜色的g分量。

* unsigned char b

  输入参数。矩形填充颜色的b分量。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型说明：**

    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;


* x 描述了 point 在原图中所在的起始横坐标。自左而右从 0 开始，取值范围 [0, width)。

* y 描述了 point 在原图中所在的起始纵坐标。自上而下从 0 开始，取值范围 [0, height)。


**注意事项:**

1. 该接口支持输入 bm_image 的图像格式为

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_NV12                   |
+-----+-------------------------------+
|  2  | FORMAT_NV21                   |
+-----+-------------------------------+
|  3  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  4  | RGB_PLANAR                    |
+-----+-------------------------------+
|  5  | RGB_PACKED                    |
+-----+-------------------------------+
|  6  | BGR_PLANAR                    |
+-----+-------------------------------+
|  7  | BGR_PACKED                    |
+-----+-------------------------------+

支持输入 bm_image 数据格式为

+-----+-------------------------------+
| num | intput data_type              |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

如果不满足输入输出格式要求，则返回失败。

3. 输入输出所有 bm_image 结构必须提前创建，否则返回失败。

4. 所有输入point对象区域必须在图像以内。

5. 当输入是FORMAT_YUV420P、FORMAT_NV12、FORMAT_NV21时，length必须为偶数。


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
            bmcv_point_t rect = {100, 100};
            int length = 10;
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
            bmcv_image_draw_point(handle, img, 1, &rect, length, 255, 255, 255);
            bm_image_copy_device_to_host(img, (void**)&data_ptr);
            writeBin(output_path, data_ptr, channel * width * height);

            bm_image_destroy(img);
            bm_dev_free(handle);
            delete[] data_ptr;
            return 0;
        }