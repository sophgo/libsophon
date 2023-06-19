bmcv_image
=================

以下数据结构以及接口仅为了兼容上一代产品，并且其功能在前边章节中均可实现，在 BM1684 系列产品中不建议使用。

BM1682的BMCV库对常用的图像相关的数据结构进行了封装，主要定义了image相关的数据结构。

定义以下:

    .. code-block:: c

        #define MAX_BMCV_IMAGE_CHANNEL 4
        typedef enum bmcv_data_format_{
            DATA_TYPE_FLOAT = 0,  // data type is 32 bit float
            DATA_TYPE_BYTE = 4  // data type is 8 bit
        }bmcv_data_format;
        
        typedef enum bmcv_color_space_{
            COLOR_YUV,  // color space is yuv
            COLOR_YCbCr, // color space is YCbCr
            COLOR_RGB  // color space is rgb
        }bmcv_color_space;
        
        typedef enum bmcv_image_format_{
            YUV420P,  // data is in YUV420 packed format
            NV12,  // data is in NV12 format
            NV21,  // data is in NV21 format
            RGB,  // data is in RGB planar format
            BGR,  // data is in BGR planar format
            RGB_PACKED, // data is in RGB packed format
            BGR_PACKED, // data is in BGR packed format
            BGR4N  // data is in BGR 4N mode, for future use
        }bmcv_image_format;
        
        typedef struct bmcv_image_t{
            bmcv_color_space color_space;
            bmcv_data_format data_format;
            bmcv_image_format image_format;
            int              image_width;
            int              image_height;
            bm_device_mem_t  data[MAX_BMCV_IMAGE_CHANNEL];
            int              stride[MAX_BMCV_IMAGE_CHANNEL];
        }bmcv_image;



**各个参数说明（bm_image中有类似的参数定义）:** 

* bmcv_color_space color_space

表示图像颜色空间

* bmcv_data_format data_format

表示图像数据类型

* bmcv_image_format image_format

表示图像格式

* int image_width

表示图像宽

* int image_height

表示图像高

* bm_device_mem_t data[MAX_BMCV_IMAGE_CHANNEL]

表示图像各通道数据地址

* int stride[MAX_BMCV_IMAGE_CHANNEL]

表示图像各通道stride


