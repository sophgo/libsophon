bmcv_image_vpp_stitch
=====================

使用vpp硬件资源的 crop 功能，实现图像拼接的效果，对输入 image 可以一次完成 src crop + csc + resize + dst crop操作。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_vpp_stitch(
                    bm_handle_t handle,
                    int input_num,
                    bm_image* input,
                    bm_image output,
                    bmcv_rect_t* dst_crop_rect,
                    bmcv_rect_t* src_crop_rect = NULL,
                    bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);


**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄,通过调用 bm_dev_request 获取

* int input_num

  输入参数。输入 bm_image 数量

* bm_imagei\* input

  输入参数。输入 bm_image 对象指针

* bm_image output

  输出参数。输出 bm_image 对象

* bmcv_rect_t \*   dst_crop_rect

  输入参数。在dst images上，各个目标小图的坐标和宽高信息

* bmcv_rect_t \*   src_crop_rect

  输入参数。在src image上，各个目标小图的坐标和宽高信息

  具体格式定义如下：

    .. code-block:: c

       typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* bmcv_resize_algorithm algorithm

  输入参数。resize 算法选择，包括 BMCV_INTER_NEAREST 、BMCV_INTER_LINEAR 和 BMCV_INTER_BICUBIC 三种，默认情况下是双线性插值。

  bm1684 支持 BMCV_INTER_NEAREST，BMCV_INTER_LINEAR，BMCV_INTER_BICUBIC。

  bm1684x 支持BMCV_INTER_NEAREST， BMCV_INTER_LINEAR。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1. 该 API 的src image不支持压缩格式的数据。

2. 该 API 所需要满足的格式以及部分要求与 bmcv_image_vpp_basic 一致。

3. 如果对src image做crop操作，一张src image只crop一个目标。

4. 1684支持 input_num 最大为256，1684x支持 input_num 最大为512。


**代码示例：**

    .. code-block:: c

        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include "bmcv_api_ext_c.h"
        #include <unistd.h>

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
            bm_status_t ret;
            int src_h = 1080, src_w = 1920, dst_w = 1920, dst_h = 2160, dev_id = 0;
            bm_image_format_ext src_fmt = FORMAT_YUV420P, dst_fmt = FORMAT_YUV420P;
            const char *src_name = "path/to/src", *dst_name = "/path/to/dst";
            bmcv_rect_t dst_rect0 = {.start_x = 0, .start_y = 0, .crop_w = 1920, .crop_h = 1080};
            bmcv_rect_t dst_rect1 = {.start_x = 0, .start_y = 1080, .crop_w = 1920, .crop_h = 1080};
            bm_handle_t handle = NULL;
            bm_image src, dst;
            ret = bm_dev_request(&handle, dev_id);
            bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst, NULL);

            ret = bm_image_alloc_dev_mem(src,BMCV_HEAP1_ID);
            ret = bm_image_alloc_dev_mem(dst,BMCV_HEAP1_ID);

            int src_size = src_h * src_w * 3 / 2;
            int dst_size = src_h * src_w * 3 / 2;
            unsigned char *src_data = (unsigned char *)malloc(src_size);
            unsigned char *dst_data = (unsigned char *)malloc(dst_size);

            readBin(src_name, src_data, src_size);

            int src_image_byte_size[4] = {0};
            bm_image_get_byte_size(src, src_image_byte_size);
            void *src_in_ptr[4] = {(void *)src_data,
                                    (void *)((char *)src_data + src_image_byte_size[0]),
                                    (void *)((char *)src_data + src_image_byte_size[0] + src_image_byte_size[1]),
                                    (void *)((char *)src_data + src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2])};
            bm_image_copy_host_to_device(src, (void **)src_in_ptr);

            bmcv_rect_t rect = {.start_x = 0, .start_y = 0, .crop_w = src_w, .crop_h = src_h};
            bmcv_rect_t src_rect[2] = {rect, rect};
            bmcv_rect_t dst_rect[2] = {dst_rect0, dst_rect1};

            bm_image input[2] = {src, src};
            bmcv_image_vpp_stitch(handle, 2, input, dst, dst_rect, src_rect, BMCV_INTER_LINEAR);

            int dst_image_byte_size[4] = {0};
            bm_image_get_byte_size(dst, dst_image_byte_size);
            void *dst_in_ptr[4] = {(void *)dst_data,
                                    (void *)((char *)dst_data + dst_image_byte_size[0]),
                                    (void *)((char *)dst_data + dst_image_byte_size[0] + dst_image_byte_size[1]),
                                    (void *)((char *)dst_data + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};
            bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);
            writeBin(dst_name, dst_data, dst_size);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_dev_free(handle);
            return ret;
        }