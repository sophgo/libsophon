bm_image_alloc_dev_mem_heap_mask
================================

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_alloc_dev_mem_heap_mask(
                bm_image      image,
                int           heap_mask
        );

The API applies for internal management memory for the bm_image object. The applied device memory size is the sum of the device memory sizes required by each plane. The calculation method of plane_byte_size is introduced in bm_image_copy_host_to_device. It can also be confirmed by calling bm_image_get_byte_size API.


**Description of incoming parameters:**

* bm_image image

  Input parameter. The bm_image object to apply for device memory.

* int heap_mask

  Input parameter. Select the mask of one or more heap IDs. Each bit indicates whether a heap ID is valid, 1 indicates that it can be allocated on the heap, 0 indicates that it cannot be allocated on the heap, the lowest bit indicates heap0, and so on. For example, heap_mask=2 indicates specific allocation of space on heap1, heap_mask=5 indicates allocation of space on heap0 or heap2.



**Note:**

1. If the bm_image object is not created, a failure will be returned.

2. If the image format is FORMAT_COMPRESSES, a failure will be returned.

3. If the bm_image object is associated with device memory, a direct success will be returned.

4. The requested device memory is managed internally which will not be released again when destroyed or no longer in use.


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
