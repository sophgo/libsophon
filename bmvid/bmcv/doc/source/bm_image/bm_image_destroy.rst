bm_image_destroy
================
销毁 bm_image 对象，与 bm_image_create 成对使用，建议在哪里创建的 bm_image 对象，就在哪里销毁，避免不必要的内存泄漏。

**接口形式:**

     .. code-block:: c

        bm_status_t bm_image_destroy(
                bm_image image
        );


**传入参数说明：**

* bm_image image

输入参数。为待销毁的 bm_image 对象。


**返回参数说明：**

成功返回将销毁该 bm_image 对象，如果该对象的 device memory 是使用 bm_image_alloc_dev_mem 申请的则释放该空间，否则该对象的 device memory 不会被释放由用户自己管理。
