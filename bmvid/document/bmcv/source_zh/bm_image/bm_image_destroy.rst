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

**注意事项:**

bm_image_destroy(bm_image image) 接口设计时，采用了结构体做形参，内部释放了image.image_private指向的内存，但是对指针image.image_private的修改无法传到函数外，导致第二次调用时出现了野指针问题。

为了使客户代码对于sdk的兼容性达到最好，目前不对接口做修改。

建议使用bm_image_destroy（image）后将 image.image_private = NULL，避免多线程时引发野指针问题。
