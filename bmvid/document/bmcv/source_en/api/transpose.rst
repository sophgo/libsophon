bmcv_image_transpose
====================

The interface can transpose  image width and height.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form::**

    .. code-block:: c

        bm_status_t bmcv_image_transpose(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output);


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
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            bm_handle_t handle;
            int image_h = 1080;
            int image_w = 1920;
            int channel = 3;
            bm_image src, dst;
            unsigned char* src_data = new unsigned char[image_h * image_w * channel];
            unsigned char* res_data = new unsigned char[image_h * image_w * channel];
            const char *src_name = "/path/to/src";
            const char *dst_name = "path/to/dst";

            bm_dev_request(&handle, 0);
            readBin(src_name, src_data, image_h * image_w * channel);
            bm_image_create(handle, image_h, image_w, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_create(handle, image_w, image_h, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &dst);
            bm_image_copy_host_to_device(src, (void **)&src_data);
            bmcv_image_transpose(handle, src, dst);
            bm_image_copy_device_to_host(dst, (void**)&res_data);
            writeBin(dst_name, res_data, image_h * image_w * channel);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_dev_free(handle);
            delete[] src_data;
            delete[] res_data;
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