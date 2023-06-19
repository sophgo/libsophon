bm_image_get_byte_size
======================


获取 bm_image 对象各个 plane 字节大小。

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_get_byte_size(
                bm_image image,
                int* size
        );


**Description of incoming parameters:**

* bm_image image

  Input parameter. The bm_image object to apply for device memory.

* int* size

  Output parameter. The number of bytes of each plane returned.


**Note:**

1. It will return fail if the bm_image object is not created.

2. It will return fail if the image format is FORMAT_COMPRESSED and is not associated with external device memory.

3. When the function is called successfully, the device memory byte size required for each plane will be filled in the size pointer. The calculation method of size is introduced in bm_image_copy_host_to_device.

4. If the bm_image object is not associated with external device memory, in addition to FORMAT_COMPRESSED, other formats can still successfully return and fill in size.
