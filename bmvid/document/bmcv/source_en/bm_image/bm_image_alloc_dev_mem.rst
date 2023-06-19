bm_image_alloc_dev_mem
======================

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_alloc_dev_mem(
                bm_image      image
        );

The API applies for internal memory management of the bm_image object. The size of the applied device memory is the sum of the size of the device memory required by each plane. The calculation method of plane_byte_size is introduced in bm_image_copy_host_to_device. It can also be confirmed by calling the bm_image_get_byte_size API.


**Description of incoming parameters:**

* bm_image image

  Input parameter. The bm_image object to apply for device memory.



**Note:**

1. It will return fail if the bm_image object is not created.

2.  It will return fail if the image format is FORMAT_COMPRESSED.

3. If the bm_image object is associated with device memory, a direct success will be returned.

4. The requested device memory is managed internally which will not be released again when destroyed or no longer in use.

