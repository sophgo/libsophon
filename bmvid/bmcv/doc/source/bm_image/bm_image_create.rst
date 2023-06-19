bm_image_create
===============

我们不建议用户直接填充 bm_image 结构，而是通过以下 API 来创建/销毁一个 bm_image 结构。

**接口形式:**

    .. code-block:: c

       bm_status_t  bm_image_create(
           bm_handle_t handle,
           int img_h,
           int img_w,
           bmcv_image_format_ext image_format,
           bmcv_data_format_ext data_type,
           bm_image *image,
           int* stride);

**传入参数说明:**

* bm_handle_t handle

输入参数。设备环境句柄，通过调用 bm_dev_request 获取

* int img_h

输入参数。图片高度

* int img_w

输入参数。图片宽度

* bmcv_image_format_ext image_format

输入参数。所需创建 bm_image 图片格式，所支持图片格式在 bm_image_format_ext 中介绍

* bm_image_format_ext data_type

输入参数。所需创建 bm_image 数据格式，所支持数据格式在 bm_image_data_format_ext 中介绍

* bm_image \*image

输出参数。输出填充的 bm_image 结构指针

* int* stride

输入参数。stride 描述了所创建 bm_image 将要关联的 device memory 内存布局。在每个plane 的 width stride 值,以 byte 计数。


**返回值说明：**

bmcv_image_create 成功调用将返回 BM_SUCCESS，并填充输出的 image 指针结构。这个结构中记录了图片的大小，以及相关格式。但此时并没有与任何 device memory 关联，也没有申请数据对应的 device memory。


**注意事项:**

1) 以下几种图片格式仅支持 DATA_TYPE_EXT_1N_BYTE

       * FORMAT_YUV420P
       * FORMAT_YUV422P
       * FORMAT_YUV444P
       * FORMAT_NV12
       * FORMAT_NV21
       * FORMAT_NV16
       * FORMAT_NV61
       * FORMAT_GRAY
       * FORMAT_COMPRESSED

如果试图以上图片格式的其他数据格式 bm_image，则返回失败。

对于如下 data_type 为 4N 的情形：

       * DATA_TYPE_EXT_4N_BYTE
       * DATA_TYPE_EXT_4N_BYTE_SIGNED

每调用一次 bm_image_create()，实际是同时为 4 张 image 配置同样的属性。


2) 以下图片格式的宽和高可以是奇数，接口内部会调整到偶数再完成相应功能。但建议尽量使用偶数的宽和高，这样可以发挥最大的效率。

       * FORMAT_YUV420P
       * FORMAT_NV12
       * FORMAT_NV21
       * FORMAT_NV16
       * FORMAT_NV61


3) FORMAT_COMPRESSED 图片格式的图片宽度或者 stride 必须 64 对齐，否则返回失败。


4) stride 参数默认值为 NULL，此时默认各个 plane 的数据是 compact 排列，没有 stride。


5) 如果 stride 非 NULL，则会检测 stride 中的 width stride 值是否合法。所谓的合法，即 image_format 对应的所有 plane 的 stride 大于默认 stride。默认 stride 值的计算方法如下：

     .. code-block:: c

        int data_size = 1;
        switch (data_type) {
            case DATA_TYPE_EXT_FLOAT32:
                data_size = 4;
                break;
            case DATA_TYPE_EXT_4N_BYTE:
            case DATA_TYPE_EXT_4N_BYTE_SIGNED:
                data_size = 4;
                break;
            default:
                data_size = 1;
                break;
        }
        int default_stride[3] = {0};
        switch (image_format) {
            case FORMAT_YUV420P: {
                image_private->plane_num = 3;
                default_stride[0] = width * data_size;
                default_stride[1] = (ALIGN(width, 2) >> 1) * data_size;
                default_stride[2] = default_stride[1];
                break;
            }
            case FORMAT_YUV422P: {
                default_stride[0] = res->width * data_size;
                default_stride[1] = (ALIGN(res->width, 2) >> 1) * data_size;
                default_stride[2] = default_stride[1];
                break;
            }
            case FORMAT_YUV444P: {
                default_stride[0] = res->width * data_size;
                default_stride[1] = res->width * data_size;
                default_stride[2] = default_stride[1];
                break;
            }
            case FORMAT_NV12:
            case FORMAT_NV21: {
                image_private->plane_num = 2;
                default_stride[0] = width * data_size;
                default_stride[1] = ALIGN(res->width, 2) * data_size;
                break;
            }
            case FORMAT_NV16:
            case FORMAT_NV61: {
                image_private->plane_num = 2;
                default_stride[0] = res->width * data_size;
                default_stride[1] = ALIGN(res->width, 2) * data_size;
                break;
            }
            case FORMAT_GRAY: {
                image_private->plane_num = 1;
                default_stride[0] = res->width * data_size;
                break;
            }
            case FORMAT_COMPRESSED: {
                image_private->plane_num = 4;
                break;
            }
            case FORMAT_BGR_PACKED:
            case FORMAT_RGB_PACKED: {
                image_private->plane_num = 1;
                default_stride[0] = res->width * 3 * data_size;
                break;
            }
            case FORMAT_BGR_PLANAR:
            case FORMAT_RGB_PLANAR: {
                image_private->plane_num = 1;
                default_stride[0] = res->width * data_size;
                break;
            }
            case FORMAT_BGRP_SEPARATE:
            case FORMAT_RGBP_SEPARATE: {
                image_private->plane_num = 3;
                default_stride[0] = res->width * data_size;
                default_stride[1] = res->width * data_size;
                default_stride[2] = res->width * data_size;
                break;
            }
        }
