bm_image_alloc_dev_mem
======================

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_alloc_dev_mem(
                bm_image      image
        );

该 API 为 bm_image 对象申请内部管理内存，所申请 device memory 大小为各个 plane 所需 device memory 大小之和。plane_byte_size 计算方法在 bm_image_copy_host_to_device 中已经介绍，或者通过调用 bm_image_get_byte_size API 来确认。


**传入参数说明:**

* bm_image image

  输入参数。待申请 device memory 的 bm_image 对象。



**注意事项:**

1. 如果 bm_image 对象未创建，则返回失败。

2. 如果 image format 为 FORMAT_COMPRESSED，则返回失败。

3. 如果 bm_image 对象已关联 device memory，则会直接返回成功。

4. 所申请 device memory 由内部管理，在 destroy 或者不再使用时释放。

