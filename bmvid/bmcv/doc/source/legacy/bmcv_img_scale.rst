bmcv_img_scale
================

此接口用于BM1682对图像进行缩放，并支持从原图中抠图缩放, 以及对输出图像进行归一化操作


    .. code-block:: c

        bm_status_t bmcv_img_scale(
            bm_handle_t handle,
            bmcv_image input,
            int n, int do_crop, int top, int left, int height, int width,
            unsigned char stretch, unsigned char padding_b, unsigned char padding_g, unsigned char padding_r,
            int pixel_weight_bias,
            float weight_b, float bias_b,
            float weight_g, float bias_g,
            float weight_r, float bias_r,
            bmcv_image output
        );



**传入参数说明:**

* bm_handle_t handle

 bmcv初始化获取的handle

* bmcv_image input

 输入图像数据地址, 只支持BGR planar或者RGB planar格式数据，支持float或者byte类型数据

* int n

 处理图像的数量，目前只支持1

* int do_crop

 是否进行抠图缩放。0：不抠图，下面四个参数无用。1：抠图，下面四个参数用于确定抠图的尺寸

* int top

 抠图开始点纵坐标，图片左上角为原点，单位为像素

* int left

 抠图开始点横坐标，图片左上角为原点，单位为像素

* int height

 抠图高度，单位为像素

* int width

 抠图宽度，单位为像素

* unsigned char stretch

 scale模式。0：fit模式，等比例缩放，按照下面的padding参数填充颜色；1：拉伸模式，填满目标大小，下面的padding参数无用

* unsigned char padding_b	

fit模式下蓝色填充色值，为0-255之byte类型，如果输出类型为byte或float但未指定归一化计算，则此值为填充颜色分量，如果输出为float类型且需要归一化，则此分量也相应进行归一化计算。

* unsigned char padding_g	

fit模式下绿色填充色值，为0-255之byte类型，如果输出类型为byte或float但未指定归一化计算，则此值为填充颜色分量，如果输出为float类型且需要归一化，则此分量也相应进行归一化计算。

* unsigned char padding_r	

fit模式下红色填充色值，为0-255之byte类型，如果输出类型为byte或float但未指定归一化计算，则此值为填充颜色分量，如果输出为float类型且需要归一化，则此分量也相应进行归一化计算。

* int pixel_weight_bias

 是否对输出图像进行归一化计算。0：不做归一化计算，下面weight和bias参数无用；1：进行归一化计算

* float weight_b

 b通道的系数

* float bias_b

 b通道偏移量

* float weight_g

 g通道的系数

* float bias_g

 g通道的偏移量

* float weight_r

 r通道的系数

* float bias_r

 r通道的偏移量

* bmcv_image ouput

 输出的图像的描述结构，支持BGR planar或者RGB planar格式，但是必须与输入相同。

