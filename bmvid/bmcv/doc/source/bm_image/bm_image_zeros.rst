bm_image_zeros
=====================

该接口用于将申请的 bm_image 空间进行内存清零，避免内存中原本的数据造成影响。

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_zeros(bm_image image);


**输入参数说明：**

* bm_image image

输入参数。 bm_image 结构体。



**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败

