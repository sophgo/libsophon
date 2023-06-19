bmcv_image_vpp_basic
=========================


bm1684和bm1684x 上有专门的视频后处理模块VPP，在满足一定条件下可以一次实现 crop、color-space-convert、resize 以及 padding 功能，速度比 TPU 更快。
该 API 可以实现对多张图片的 crop、color-space-convert、resize、padding 及其任意若干个功能的组合。

    .. code-block:: c

        bm_status_t bmcv_image_vpp_basic(
            bm_handle_t           handle,
            int                   in_img_num,
            bm_image*             input,
            bm_image*             output,
            int*                  crop_num_vec = NULL,
            bmcv_rect_t*          crop_rect = NULL,
            bmcv_padding_atrr_t*  padding_attr = NULL,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR,
            csc_type_t            csc_type = CSC_MAX_ENUM,
            csc_matrix_t*         matrix = NULL);


**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* int in_img_num

  输入参数。输入 bm_image 数量。

* bm_image* input

  输入参数。输入 bm_image 对象指针，其指向空间的长度由 in_img_num 决定。

* bm_image* output

  输出参数。输出 bm_image 对象指针，其指向空间的长度由 in_img_num 和 crop_num_vec 共同决定，即所有输入图片 crop 数量之和。

* int* crop_num_vec = NULL

  输入参数。该指针指向对每张输入图片进行 crop 的数量，其指向空间的长度由 in_img_num 决定，如果不使用 crop 功能可填 NULL。

* bmcv_rect_t * crop_rect = NULL

  输入参数。具体格式定义如下：

    .. code-block:: c

        typedef struct bmcv_rect {  
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;             
        } bmcv_rect_t;

  每个输出 bm_image 对象所对应的在输入图像上 crop 的参数，包括起始点x坐标、起始点y坐标、crop图像的宽度以及crop图像的高度。图像左上顶点作为坐标原点。如果不使用 crop 功能可填 NULL。

* bmcv_padding_atrr_t*  padding_attr = NULL

  输入参数。所有 crop 的目标小图在 dst image 中的位置信息以及要 padding 的各通道像素值，若不使用 padding 功能则设置为 NULL。 
 
    .. code-block:: c 
 
        typedef struct bmcv_padding_atrr_s { 
            unsigned int  dst_crop_stx; 
            unsigned int  dst_crop_sty; 
            unsigned int  dst_crop_w; 
            unsigned int  dst_crop_h; 
            unsigned char padding_r; 
            unsigned char padding_g; 
            unsigned char padding_b; 
            int           if_memset; 
        } bmcv_padding_atrr_t; 
 
 

    1. 目标小图的左上角顶点相对于 dst image 原点（左上角）的offset信息：dst_crop_stx 和 dst_crop_sty；
    #. 目标小图经resize后的宽高：dst_crop_w 和 dst_crop_h；
    #. dst image 如果是RGB格式，各通道需要padding的像素值信息：padding_r、padding_g、padding_b，当if_memset=1时有效，如果是GRAY图像可以将三个值均设置为同一个值；
    #. if_memset表示要不要在该api内部对dst image 按照各个通道的padding值做memset，仅支持RGB和GRAY格式的图像。如果设置为0则用户需要在调用该api前，根据需要 padding 的像素值信息，调用 bmlib 中的 api 直接对 device memory 进行 memset 操作，如果用户对padding的值不关心，可以设置为0忽略该步骤。

* bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR

  输入参数。resize 算法选择，包括 BMCV_INTER_NEAREST、BMCV_INTER_LINEAR 和 BMCV_INTER_BICUBIC三种，默认情况下是双线性差值。

  - bm1684 支持 : 
        BMCV_INTER_NEAREST，BMCV_INTER_LINEAR，BMCV_INTER_BICUBIC。

  - bm1684x 支持:
        BMCV_INTER_NEAREST， BMCV_INTER_LINEAR。

* csc_type_t csc_type = CSC_MAX_ENUM

  输入参数。color space convert 参数类型选择，填 CSC_MAX_ENUM 则使用默认值，默认为 CSC_YCbCr2RGB_BT601 或者 CSC_RGB2YCbCr_BT601，支持的类型包括：

+----------------------------+
| CSC_YCbCr2RGB_BT601        |
+----------------------------+
| CSC_YPbPr2RGB_BT601        |
+----------------------------+
| CSC_RGB2YCbCr_BT601        |
+----------------------------+
| CSC_YCbCr2RGB_BT709        |
+----------------------------+
| CSC_RGB2YCbCr_BT709        |
+----------------------------+
| CSC_RGB2YPbPr_BT601        |
+----------------------------+
| CSC_YPbPr2RGB_BT709        |
+----------------------------+
| CSC_RGB2YPbPr_BT709        |
+----------------------------+ 
| CSC_USER_DEFINED_MATRIX    |
+----------------------------+  
| CSC_MAX_ENUM               |
+----------------------------+  

* csc_matrix_t* matrix = NULL

输入参数。如果 csc_type 选择 CSC_USER_DEFINED_MATRIX，则需要传入系数矩阵，格式如下：

    .. code-block:: c 

          typedef struct {
              int csc_coe00;
              int csc_coe01;
              int csc_coe02;
              int csc_add0;
              int csc_coe10;
              int csc_coe11;
              int csc_coe12;
              int csc_add1;
              int csc_coe20;
              int csc_coe21;
              int csc_coe22;
              int csc_add2;
          } __attribute__((packed)) csc_matrix_t;



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项:**

bm1684x支持的要求如下：

1. 支持数据类型为：

+-----+------------------------+-------------------------------+
| num | input data_type        | output data_type              |
+=====+========================+===============================+
|  1  |                        | DATA_TYPE_EXT_FLOAT32         |
+-----+                        +-------------------------------+
|  2  |                        | DATA_TYPE_EXT_1N_BYTE         |
+-----+                        +-------------------------------+
|  3  | DATA_TYPE_EXT_1N_BYTE  | DATA_TYPE_EXT_1N_BYTE_SIGNED  |
+-----+                        +-------------------------------+
|  4  |                        | DATA_TYPE_EXT_FP16            |
+-----+                        +-------------------------------+
|  5  |                        | DATA_TYPE_EXT_BF16            |
+-----+------------------------+-------------------------------+


2. 输入支持色彩格式为：

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_YUV422P                |
+-----+-------------------------------+
|  3  | FORMAT_YUV444P                |
+-----+-------------------------------+
|  4  | FORMAT_NV12                   |
+-----+-------------------------------+
|  5  | FORMAT_NV21                   |
+-----+-------------------------------+
|  6  | FORMAT_NV16                   |
+-----+-------------------------------+
|  7  | FORMAT_NV61                   |
+-----+-------------------------------+
|  8  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  9  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  10 | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  11 | FORMAT_BGR_PACKED             |
+-----+-------------------------------+
|  12 | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  13 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  14 | FORMAT_GRAY                   |
+-----+-------------------------------+
|  15 | FORMAT_COMPRESSED             |
+-----+-------------------------------+
|  16 | FORMAT_YUV444_PACKED          |
+-----+-------------------------------+
|  17 | FORMAT_YVU444_PACKED          |
+-----+-------------------------------+
|  18 | FORMAT_YUV422_YUYV            |
+-----+-------------------------------+
|  19 | FORMAT_YUV422_YVYU            |
+-----+-------------------------------+
|  20 | FORMAT_YUV422_UYVY            |
+-----+-------------------------------+
|  21 | FORMAT_YUV422_VYUY            |
+-----+-------------------------------+


3. 输出支持色彩格式为：

+-----+-------------------------------+
| num | output image_format           |
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
|  12 | FORMAT_RGBYP_PLANAR           |
+-----+-------------------------------+
|  13 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  14 | FORMAT_HSV180_PACKED          |
+-----+-------------------------------+
|  15 | FORMAT_HSV256_PACKED          |
+-----+-------------------------------+

4.1684x vpp 不支持从FORMAT_COMPRESSED 转为 FORMAT_HSV180_PACKED 或 FORMAT_HSV256_PACKED。

5.图片缩放倍数（（crop.width / output.width) 以及 (crop.height / output.height））限制在 1/128 ～ 128 之间。

6.输入输出的宽高（src.width, src.height, dst.widht, dst.height）限制在 8 ～ 8192 之间。

7.输入必须关联 device memory，否则返回失败。

8.FORMAT_COMPRESSED 格式的使用方法见bm1684部分介绍。

bm1684支持的要求如下：

1. 该 API 所需要满足的格式以及部分要求,如下表格所示：

+------------------+---------------------+----------+
| src format       | dst format          | 其他限制 |
+==================+=====================+==========+
|                  | RGB_PACKED          |  条件1   |
|                  +---------------------+----------+
| RGB_PACKED       | RGB_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件1   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件1   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件1   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件1   |
|                  +---------------------+----------+
| BGR_PACKED       | RGB_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件1   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件1   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件1   |
|                  +---------------------+----------+
| RGB_PLANAR       | RGB_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件1   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件1   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件1   |
|                  +---------------------+----------+
| BGR_PLANAR       | RGB_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件1   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件1   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件1   |
|                  +---------------------+----------+
| RGBP_SEPARATE    | RGB_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件1   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件1   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件1   |
|                  +---------------------+----------+
| BGRP_SEPARATE    | RGB_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件1   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件1   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件1   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件1   |
+------------------+---------------------+----------+
| GRAY             | GRAY                |  条件1   |
+------------------+---------------------+----------+
| YUV420P          | YUV420P             |  条件2   |
+------------------+---------------------+----------+
| COMPRESSED       | YUV420P             |  条件2   |
+------------------+---------------------+----------+
| RGB_PACKED       | YUV420P             |  条件3   |
+------------------+                     +----------+
| RGB_PLANAR       |                     |  条件3   |
+------------------+                     +----------+
| BGR_PACKED       |                     |  条件3   |
+------------------+                     +----------+
| BGR_PLANAR       |                     |  条件3   |
+------------------+                     +----------+
| RGBP_SEPARATE    |                     |  条件3   |
+------------------+                     +----------+
| BGRP_SEPARATE    |                     |  条件3   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件4   |
|                  +---------------------+----------+
| YUV420P          | RGB_PLANAR          |  条件4   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件4   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件4   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件4   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件4   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件4   |
|                  +---------------------+----------+
| NV12             | RGB_PLANAR          |  条件4   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件4   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件4   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件4   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件4   |
+------------------+---------------------+----------+
|                  | RGB_PACKED          |  条件4   |
|                  +---------------------+----------+
| COMPRESSED       | RGB_PLANAR          |  条件4   |
|                  +---------------------+----------+
|                  | BGR_PACKED          |  条件4   |
|                  +---------------------+----------+
|                  | BGR_PLANAR          |  条件4   |
|                  +---------------------+----------+
|                  | RGBP_SEPARATE       |  条件4   |
|                  +---------------------+----------+
|                  | BGRP_SEPARATE       |  条件4   |
+------------------+---------------------+----------+

其中：

     - 条件1： src.width >= crop.x + crop.width，src.height >= crop.y + crop.height
     - 条件2： src.width, src.height, dst.widht，dst.height 必须是2的整数倍，src.width >= crop.x + crop.width，src.height >= crop.y + crop.height
     - 条件3： dst.widht，dst.height 必须是2的整数倍，src.width == dst.width，src.height == dst.height，crop.x == 0，crop.y == 0,src.width >= crop.x + crop.width，src.height >= crop.y + crop.height
     - 条件4： src.width，src.height 必须是2的整数倍，src.width >= crop.x + crop.width，src.height >= crop.y + crop.height

2. 输入 bm_image 的 device mem 不能在 heap0 上。

3. 所有输入输出 image 的 stride 必须 64 对齐。

4. 所有输入输出 image 的地址必须 32 byte 对齐。

5. 图片缩放倍数（（crop.width / output.width) 以及 (crop.height / output.height））限制在 1/32 ～ 32 之间。

6. 输入输出的宽高（src.width, src.height, dst.widht, dst.height）限制在 16 ～ 4096 之间。

7. 输入必须关联 device memory，否则返回失败。

8. FORMAT_COMPRESSED 是 VPU 解码后内置的一种压缩格式，它包括4个部分：Y compressed table、Y compressed data、CbCr compressed table 以及 CbCr compressed data。请注意 bm_image 中这四部分存储的顺序与 FFMPEG 中 AVFrame 稍有不同，如果需要 attach AVFrame 中 device memory 数据到 bm_image 中时，对应关系如下，关于 AVFrame 详细内容请参考 VPU 的用户手册。

    .. code-block:: c

        bm_device_mem_t src_plane_device[4];
        src_plane_device[0] = bm_mem_from_device((u64)avframe->data[6],
                avframe->linesize[6]);
        src_plane_device[1] = bm_mem_from_device((u64)avframe->data[4],
                avframe->linesize[4] * avframe->h);
        src_plane_device[2] = bm_mem_from_device((u64)avframe->data[7],
                avframe->linesize[7]);
        src_plane_device[3] = bm_mem_from_device((u64)avframe->data[5],
                avframe->linesize[4] * avframe->h / 2);

        bm_image_attach(*compressed_image, src_plane_device);


   
