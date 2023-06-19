bm_image_alloc_contiguous_mem
=============================

为多个 image 分配连续的内存。

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_alloc_contiguous_mem(
                int           image_num,
                bm_image      *images
        );


**传入参数说明:**

* int image_num

  输入参数。待分配内存的 image 个数

* bm_image \*images

  输入参数。待分配内存的 image 的指针



**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1、image_num 应该大于 0,否则将返回错误。

2、如传入的 image 已分配或者 attach 过内存，应先 detach 已有内存，否则将返回失败。

3、所有待分配的 image 应该尺寸相同，否则将返回错误。

4、当希望 destory 的 image 是通过调用本 api 所分配的内存时，应先调用 bm_image_free_contiguous_mem 将分配内存释放，再用 bm_image_destroy 来实现 destory image

5、bm_image_alloc_contiguous_mem 与 bm_image_free_contiguous_mem 应成对使用。

