bm_image_get_format_info
========================

该接口用于获取 bm_image 的一些信息。

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_get_format_info(
                bm_image *              src,
                bm_image_format_info_t *info
        );


**输入参数说明：**

* bm_image\*  src

  输入参数。所要获取信息的目标 bm_image。

* bm_image_foramt_info_t \*info

  输出参数。保存所需信息的数据结构，返回给用户， 具体内容见下面的数据结构说明。



**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**数据结构说明：**

    .. code-block:: c

        typedef struct bm_image_format_info {
                int                      plane_nb;
                bm_device_mem_t          plane_data[8];
                int                      stride[8];
                int                      width;  
                int                      height; 
                bm_image_format_ext      image_format;
                bm_image_data_format_ext data_type;
                bool                     default_stride;                  
        } bm_image_format_info_t; 

* int plane_nb

  该 image 的 plane 数量

* bm_device_mem_t plane_data[8]

  各个 plane 的 device memory

* int stride[8];

  各个 plane 的 stride 值

* int width;  

  图片的宽度

* int height; 

  图片的高度

* bm_image_format_ext image_format;

  图片的格式

* bm_image_data_format_ext data_type;

  图片的存储数据类型

* bool default_stride;                  

  是否使用了默认的 stride



