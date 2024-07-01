bm_image_write_to_bmp
=====================

This interface is used to output bm_image objects as bitmaps (.bmp).


**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_write_to_bmp(
                bm_image    input,
                const char* filename);

**Parameter description:**

* bm_image input

Input parameter. Input bm_image.

* const char\* filename

Input parameter. The path and file name of the saved bitmap file.


**Return value description:**

* BM_SUCCESS: success

* Others: failed

**Note:**

1. Before calling bm_image_write_to_bmp(), you must ensure that the input image has been created correctly and is_attached, otherwise the function will return failure.
