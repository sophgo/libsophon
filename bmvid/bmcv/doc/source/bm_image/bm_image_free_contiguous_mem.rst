bm_image_free_contiguous_mem
============================


释放通过 bm_image_alloc_contiguous_mem 所分配的多个 image 中的连续内存。


**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_free_contiguous_mem(
                int image_num,
                bm_image *images
        );


**传入参数说明:**

* int image_num

输入参数。待释放内存的 image 个数

* bm_image \*images

输入参数。待释放内存的 image 的指针


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1、image_num 应该大于 0，否则将返回错误。

2、所有待释放的 image 应该尺寸相同。

3、bm_image_alloc_contiguous_mem 与 bm_image_free_contiguous_mem 应成对使用。bm_image_free_contiguous_mem 所要释放的内存必须是通过 bm_image_alloc_contiguous_mem 所分配的。

4、应先调用 bm_image_free_contiguous_mem,将 image 中内存释放，再调 bm_image_destroy 去 destory image。



