bm_image_free_contiguous_mem
============================


Release the contiguous memory of multiple images allocated by bm_image_alloc_contiguous_mem.


**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_free_contiguous_mem(
                int image_num,
                bm_image *images
        );


**Description of incoming parameters:**

* int image_num

  Input parameter. Number of images to be released

* bm_image \*images

  Input parameters. Pointer to the image of the memory to be released


**Description of returning parameters:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1、image_num should be greater than 0, otherwise an error will be returned.

2、All images to be released should be of the same size.

3、bm_image_alloc_contiguous_mem and bm_image_free_contiguous_mem should be used in pairs. The released memory of bm_image_free_contiguous_mem must be allocated by bm_image_alloc_contiguous_mem.

4、Users should first call bm_image_free_contiguous_mem, release the memory in the image, and then destroy image by calling bm_image_destroy.



