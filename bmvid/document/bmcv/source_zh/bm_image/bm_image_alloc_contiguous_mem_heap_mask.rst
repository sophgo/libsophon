bm_image_alloc_contiguous_mem_heap_mask
=======================================

为多个 image 在指定的 heap 上分配连续的内存。

**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_alloc_contiguous_mem_heap_mask(
                int           image_num,
                bm_image      *images,
                int           heap_mask
        );


**传入参数说明:**

* int image_num

  输入参数。待分配内存的 image 个数

* bm_image \*images

  输入参数。待分配内存的 image 的指针

* int heap_mask

  输入参数。选择一个或多个 heap id 的掩码，每一个 bit 表示一个 heap id 的是否有效, 1 表示可以在该 heap 上分配，0 则表示不可以在该 heap 上分配，最低位表示 heap0，以此类推。比如 heap_mask=2 则表示指定在 heap1 上分配空间，heap_mask=5 则表示指定在 heap0 或者 heap2 上分配空间


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1、image_num 应该大于 0,否则将返回错误。

2、如传入的 image 已分配或者 attach 过内存，应先 detach 已有内存，否则将返回失败。

3、所有待分配的 image 应该尺寸相同，否则将返回错误。

4、当希望 destory 的 image 是通过调用本 api 所分配的内存时，应先调用 bm_image_free_contiguous_mem 将分配内存释放，再用 bm_image_destroy 来实现 destory image

5、bm_image_alloc_contiguous_mem 与 bm_image_free_contiguous_mem 应成对使用。


**Heap属性说明:**

+------------------+------------------+------------------+------------------+
|    heap id       |   bm1684 VPP     |   bm1684x VPP    |      对应关系    |
+==================+==================+==================+==================+
|    heap0         |      W           |     R/W          |         TPU      |
+------------------+------------------+------------------+------------------+
|    heap1         |     R/W          |     R/W          |        JPU/VPP   |
+------------------+------------------+------------------+------------------+
|    heap2         |     R/W          |     R/W          |         VPU      |
+------------------+------------------+------------------+------------------+