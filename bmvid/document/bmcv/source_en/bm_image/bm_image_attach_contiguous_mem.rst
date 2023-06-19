bm_image_attach_contiguous_mem
==============================

Attach a piece of contiguous memory to multiple images.


**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_attach_contiguous_mem(
                int image_num,
                bm_image * images,
                bm_device_mem_t dmem
        );


**Description of incoming parameters:**

* int image_num

  Input parameter. The number of images in the memory to be attached.

* bm_image \*images

  Input parameter. Pointer to the image of the memory to be attached.

* bm_device_mem_t dmem

  Input parameter. Allocated device memory information.


**Description of returning parameters:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1、image_num should be greater than 0, otherwise an error will be returned.

2、All images to be attached should have the same size, otherwise an error will be returned.
