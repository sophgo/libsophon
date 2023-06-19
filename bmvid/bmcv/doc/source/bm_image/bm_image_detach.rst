bm_image_detach
===============

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_detach(
                bm_image
        );



**传入参数说明:**

* bm_image image

输入参数。待解关联的 bm_image 对象。



**注意事项:**

1. 如果传入的 bm_image 对象未被创建，则返回失败。

2. 该函数成功返回时，会对 bm_image 对象关联的 device_memory 进行解关联，bm_image 对象将不再关联 device_memory。

3. 如果解关联的 device_memory 是内部自动申请的话，则会释放这块 device memory。

4. 如果对象未关联任何 device memory，则直接返回成功。
