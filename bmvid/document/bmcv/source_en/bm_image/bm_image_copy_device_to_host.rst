bm_image_copy_device_to_host
============================

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_copy_device_to_host(
                bm_image image,
                void* buffers[]
        );


**Description of incoming parameters::**

* bm_image image

  Input parameter. The bm_image object whose data is to be transmitted.

* void\* buffers[]

  Output parameter. Host-side pointer, buffers are pointers to data of different planes. The amount of data to be transmitted by each plane can be obtained through bm_image_get_byte_size API.


.. note::

    1. If bm_image is not created by bm_image_create, a failure will be returned.

    2. If bm_image is not associated with device memory, a failure will be returned.

    3. If the data transmission fails, the API call will fail.

    4. If the function returns successfully, the data in the associated device memory will be copied to the host-side buffers.
