bm_image_get_stride
====================

该接口用于获取目标 bm_image 的 stride 信息。

**接口形式:**

     .. code-block:: c

         bm_status_t bm_image_get_stride(
                 bm_image image,
                 int *stride
         );


**输入参数说明：**

* bm_image image

  输入参数。所要获取 stride 信息的目标 bm_image

* int \*stride

  输出参数。存放各个 plane 的 stride 的指针



**返回值说明**

* BM_SUCCESS: 成功

* 其他:失败

