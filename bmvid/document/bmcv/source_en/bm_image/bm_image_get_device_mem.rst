bm_image_get_device_mem
=======================

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_get_device_mem(
                bm_image image,
                bm_device_mem_t* mem
        );


**Description of incoming parameters:**

* bm_image image

  Input parameter. The bm_image object to apply for device memory.

* bm_device_mem_t* mem

  Output parameter. The bm_device_mem_t structure of each returned plane.


**Note:**

1. When the function is returned successfully, the device will fill in the bm_device_mem_t structure associated with each plane of the bm_image object in the mem pointer.

2. It will return fail if the bm_image object is not associated with device memory.

3. It will return fail if the bm_image object is not created.

4. If the device memory structure is applied internally, please do not release it in case of double free.
