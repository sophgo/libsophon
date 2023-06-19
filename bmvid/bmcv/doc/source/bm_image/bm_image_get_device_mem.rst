bm_image_get_device_mem
=======================

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_get_device_mem(
                bm_image image,
                bm_device_mem_t* mem
        );


**传入参数说明:**

* bm_image image

输入参数。待获取 device memory 的 bm_image 对象。

* bm_device_mem_t* mem

输出参数。返回的各个 plane 的 bm_device_mem_t 结构。


**注意事项:**

1. 该函数将成功返回时，将在 mem 指针中填充 bm_image 对象各个 plane 关联的 bm_device_mem_t 结构。

2. 如果 bm_image 对象未关联 device memory 的话，将返回失败。

3. 如果 bm_image 对象未创建，则返回失败。

4. 如果是 bm_image 内部申请的 device memory 结构，请不要将其释放，以免发生 double free。
