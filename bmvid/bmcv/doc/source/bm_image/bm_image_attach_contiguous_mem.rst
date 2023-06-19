bm_image_attach_contiguous_mem
==============================

将一块连续内存 attach 到多个 image 中。


**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_attach_contiguous_mem(
                int image_num,
                bm_image * images,
                bm_device_mem_t dmem
        );


**传入参数说明:**

* int image_num

输入参数。待 attach 内存的 image 个数。

* bm_image \*images

输入参数。待 attach 内存的 image 的指针。

* bm_device_mem_t dmem

输入参数。已分配好的 device memory 信息。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1、image_num 应该大于 0，否则将返回错误。

2、所有待 attach 的 image 应该尺寸相同，否则将返回错误。
