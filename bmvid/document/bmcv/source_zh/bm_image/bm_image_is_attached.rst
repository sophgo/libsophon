bm_image_is_attached
=====================

该接口用于判断目标  是否已经 attach 存储空间。

**接口形式:**

    .. code-block:: c

        bool bm_image_is_attached(bm_image image);


**输入参数说明：**

* bm_image image

  输入参数。所要判断是否 attach 存储空间的目标 bm_image。



**返回值说明：**

若目标 bm_image 已经 attach 存储空间则返回 true，否则返回 false。


