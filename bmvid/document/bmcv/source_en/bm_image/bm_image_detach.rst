bm_image_detach
===============

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_detach(
                bm_image
        );



**Description of incoming parameters:**

* bm_image image

  Input parameter. The bm_image object to be disassociated.


**Note:**

1. It will return fail if the incoming bm_image object is not created.

2. The bm_device will dissociate with the bm_image object when the function returns successfully. The bm_image object will no longer be associated with device_memory.

3. If the disassociated device_memory is automatically applied internally, this device memory will be released.

4. If the object is not associated with any device memory, a direct success will be returned.
