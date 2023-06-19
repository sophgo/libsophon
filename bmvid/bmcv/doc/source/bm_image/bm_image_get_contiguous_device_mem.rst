bm_image_get_contiguous_device_mem
==================================


从多个内存连续的 image 中得到连续的内存的 device memory 信息。


**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_get_contiguous_device_mem(
                int image_num,
                bm_image *images,
                bm_device_mem_t * mem
        );


**传入参数说明:**

* int image_num

输入参数。待获取信息的 image 个数。

* bm_image \*images

输入参数。待获取信息的 image 指针。

* bm_device_mem_t \* mem

输出参数。得到的连续内存的 device memory 信息。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1、image_num 应该大于 0，否则将返回错误。

2、所填入的 image 应该尺寸相同，否则将返回错误。

3、所填入的 image 必须是内存连续的，否则返回错误。

4、所填入的 image 内存必须是通过 bm_image_alloc_contiguous_mem 或者 bm_image_attach_contiguous_mem 获得。
