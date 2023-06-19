bmcv_image_transpose
====================

The interface can transpose  image width and height.

**Interface form::**

    .. code-block:: c

        bm_status_t bmcv_image_transpose(
                bm_handle_t handle,
                bm_image input,
                bm_image output
        );

**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity), which can be obtained by calling bm_dev_request.

* bm_image input

  Input parameter. The bm_image structure of the input image.

* bm_image output

  Output parameter. The bm_image structure of the output image.



**Return value description:**

* BM_SUCCESS: success

* Other: failed



**Code example**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "bmlib_utils.h"
        #include "common.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>

         int main(int argc, char *argv[]) {
             bm_handle_t handle;
             bm_dev_request(&handle, 0);

             int image_n = 1;
             int image_h = 1080;
             int image_w = 1920;
             bm_image src, dst;
             bm_image_create(handle, image_h, image_w, FORMAT_RGB_PLANAR,
                     DATA_TYPE_EXT_1N_BYTE, &src);
             bm_image_create(handle, image_w, image_h, FORMAT_RGB_PLANAR,
                     DATA_TYPE_EXT_1N_BYTE, &dst);
             std::shared_ptr<u8*> src_ptr = std::make_shared<u8*>(
                     new u8[image_h * image_w * 3]);
             memset((void *)(*src_ptr.get()), 148, image_h * image_w * 3);
             u8 *host_ptr[] = {*src_ptr.get()};
             bm_image_copy_host_to_device(src, (void **)host_ptr);
             bmcv_image_transpose(handle, src, dst);
             bm_image_destroy(src);
             bm_image_destroy(dst);
             bm_dev_free(handle);
             return 0;
         }



**Note:**

1. This API requires that the input and output bm_image have the same image format. It supports the following formats:

+-----+-------------------------------+
| num | image_format                  |
+=====+===============================+
|  1  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  2  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  3  | FORMAT_GRAY                   |
+-----+-------------------------------+

2. This API requires that the input and output bm_image have the same data type. It supports the following types:

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_FLOAT32         |
+-----+-------------------------------+
|  2  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+
|  3  | DATA_TYPE_EXT_4N_BYTE         |
+-----+-------------------------------+
|  4  | DATA_TYPE_EXT_1N_BYTE_SIGNED  |
+-----+-------------------------------+
|  5  | DATA_TYPE_EXT_4N_BYTE_SIGNED  |
+-----+-------------------------------+

3. The width of the output image must be equal to the height of the input image, and the height of the output image must be equal to the width of the input image;

4. It supports input images with stride;

5. The Input / output bm_image structure must be created in advance, or a failure will be returned.

6. The input bm_image must attach device memory, otherwise, a failure will be returned.

7. If the output object does not attach device memory, it will internally call bm_image_alloc_dev_mem to apply for internally managed device memory and fill the transposed data into device memory.

