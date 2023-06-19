bm_image_get_stride
====================

This interface is used to get the stride information of the bm_image object.

**Interface form:**

     .. code-block:: c

         bm_status_t bm_image_get_stride(
                 bm_image image,
                 int *stride
         );


**Input parameter description:**

* bm_image image

  Input parameter. The target bm_image to obtain stride information.

* int \*stride

  Output parameter. Pointer to store the stride of each plane.



**Return parameter description:**

* BM_SUCCESS: success

* Other: failed

