bm_image_destroy
================
Destroy bm_image object and bm_image_create are used in pairs. It is recommended to destroy the object where it is created to avoid unnecessary memory leakage.


**Interface form:**

     .. code-block:: c

        bm_status_t bm_image_destroy(
                bm_image image
        );


**Description of incoming parameters:**

* bm_image image

  Input parameter. The object of bm_image to be destroyed.


**Description of returning parameters:**

Successful return will destroy the bm_image object. If the device memory of this object is applied by bm_image_alloc_dev_mem, the space will be released, otherwise the device memory of the object will not be released and managed by the user.

**Note:**

The bm_image_destroy(bm_image image) interface is designed with a structure as a formal reference, which internally frees the memory pointed to by image.image_private, but changes to the pointer image.image_private cannot be passed outside the function, resulting in a wild pointer problem on the second call.

In order to achieve the best compatibility of customer code for sdk, no changes are made to the interface at this time.

It is recommended to use bm_image_destroy (image) followed by image.image_private = NULL to avoid wild pointer problems when multithreading.
