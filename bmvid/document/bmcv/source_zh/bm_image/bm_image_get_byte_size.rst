bm_image_get_byte_size
======================


获取 bm_image 对象各个 plane 字节大小。

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_get_byte_size(
                bm_image image,
                int* size
        );


**传入参数说明:**

* bm_image image

  输入参数。待获取 device memory 大小的 bm_image 对象。

* int* size

  输出参数。返回的各个 plane 字节数结果。


**注意事项**

1. 如果 bm_image 对象未创建,则返回失败。

2. 如果 image format 为 FORMAT_COMPRESSED 并且未关联外部 device memory，则返回失败。

3. 该函数成功调用时将在 size 指针中填充各个 plane 所需的 device memory 字节大小。size 大小的计算方法在 bm_image_copy_host_to_device 中已介绍。

4. 如果 bm_image 对象未关联 device memory，除了 FORMAT_COMPRESSED格式外，其他格式仍能够成功返回并填充 size。
