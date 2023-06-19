bm_image_alloc_contiguous_mem
=============================

Allocate contiguous memory for multiple images.

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_alloc_contiguous_mem(
                int           image_num,
                bm_image      *images
        );


**Description of incoming parameters:**

* int image_num

  Input parameter. The number of images to be allocated.

* bm_image \*images

  Input parameter. The pointer of the image whose memory is to be allocated.



**Description of returning parameters:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1、image_num should be greater than 0, otherwise an error will be returned.

2、If the incoming image has been allocated or attached the memory, the existing memory should be detached first. Otherwise, a failure will be returned.

3、All images to be allocated should have the same size, otherwise, an error will be returned.

4、If the memory of the image to be destroyed is allocated by calling this API, users should first call bm_image_free_contiguous_mem to release the allocated memory, and then implement bm_image_destroy to destroy image.

5、bm_image_alloc_contiguous_mem and bm_image_free_contiguous_mem should be used in pairs.

