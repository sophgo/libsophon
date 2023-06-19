bm_image_get_contiguous_device_mem
==================================


Get the device memory information of contiguous memory from multiple images with contiguous memory.


**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_get_contiguous_device_mem(
                int image_num,
                bm_image *images,
                bm_device_mem_t * mem
        );


**Description of incoming parameters:**

* int image_num

  Input parameter. The number of images to be obtained.

* bm_image \*images

  Input parameter. Image pointer to get information.

* bm_device_mem_t \* mem

  Output parameter. The obtained device memory information of contiguous memory.


**Description of returning parameters:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1、image_num should be greater than 0, otherwise an error will be returned.

2、The filled image should be the same size, otherwise, an error will be returned.

3、The memory of the filled image must be contiguous, otherwise, an error will be returned.

4、The memory of the filled image must be obtained through bm_image_alloc_contiguous_mem or bm_image_attach_contiguous_mem.
