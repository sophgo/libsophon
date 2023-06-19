bm_image_is_attached
=====================

This interface is used to judge whether the target has attached storage space.

**Interface form:**

    .. code-block:: c

        bool bm_image_is_attached(bm_image image);


**Input parameter description:**

* bm_image image

  Input parameters. To determine whether attach to the target bm_image of the storage space.



**Return parameter description:**

If the target bm_image has attached the storage space, it will return true; otherwise, it will return false.


