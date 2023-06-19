bm_image_dettach_contiguous_mem
===============================

Detach a piece of contiguous memory from multiple images.


**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_dettach_contiguous_mem(
                int image_num,
                bm_image * images
        );


**Description of incoming parameters:**

* int image_num

  Input parameter. The number of images in the memory to be detached.

* bm_image \*images

  Input parameter. Pointer to the image of the memory to be detached.


**Description of returning parameters:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1、image_num should be greater than 0, otherwise an error will be returned.

2、All images to be detached should have the same size, otherwise an error will be returned.

3、bm_image_attach_contiguous_mem and bm_image_detach_contiguous_mem should be used in pairs. The detached device memory of bm_image_detach_contiguous must be attached to the image through bm_image_attach_contiguous_mem.
