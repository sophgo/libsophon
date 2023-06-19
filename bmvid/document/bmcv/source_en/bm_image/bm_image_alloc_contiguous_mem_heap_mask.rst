bm_image_alloc_contiguous_mem_heap_mask
=======================================

Allocate continuous memory for multiple images on the specified heap.

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_alloc_contiguous_mem_heap_mask(
                int           image_num,
                bm_image      *images,
                int           heap_mask
        );


**Description of incoming parameters:**

* int image_num

  Input parameter. The number of images to be allocated.

* bm_image \*images

  Input parameter. Pointer to the image of the memory to be allocated.

* int heap_mask

  Input parameter. Select the mask of one or more heap IDs. Each bit indicates whether a heap ID is valid, 1 indicates that it can be allocated on the heap, 0 indicates that it cannot be allocated on the heap, the lowest bit indicates heap0, and so on. For example, heap_mask=2 indicates specific allocation of space on heap1, heap_mask=5 indicates allocation of space on heap0 or heap2.


**Description of returning parameters:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1、image_num should be greater than 0, otherwise an error will be returned.

2、If the incoming image has been allocated or the memory has been attached, the existing memory should be detached first. Otherwise, a failure will be returned.

3、All images to be allocated should have the same size, otherwise, an error will be returned.

4、If the memory of the image to be destroyed is allocated by calling this API, users should first call bm_image_free_contiguous_mem to release the allocated memory, and then implement bm_image_destroy to destroy image.

5、bm_image_alloc_contiguous_mem and bm_image_free_contiguous_mem should be used in pairs.


**Heap Note:**

+------------------+------------------+------------------+------------------+
|    heap id       |   bm1684 VPP     |   bm1684x VPP    |  Correspondence  |
+==================+==================+==================+==================+
|    heap0         |      W           |     R/W          |       TPU        |
+------------------+------------------+------------------+------------------+
|    heap1         |     R/W          |     R/W          |     JPU/VPP      |
+------------------+------------------+------------------+------------------+
|    heap2         |     R/W          |     R/W          |       VPU        |
+------------------+------------------+------------------+------------------+