bmcv_image_jpeg_enc
===================

This API can be used for JPEG encoding of multiple bm_image.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_enc(
                    bm_handle_t handle,
                    int image_num,
                    bm_image* src,
                    void* p_jpeg_data[],
                    size_t* out_size,
                    int quality_factor = 85);


**Input parameter description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* int  image_num

  Input parameter. The number of input image, up to 4.

* bm_image\* src

  Input parameter. Input bm_image pointer. Each bm_image requires an external call to bmcv_image_create to create the image memory. Users can call bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* void \*  p_jpeg_data,

  Output parameter. The data pointer of the encoded images. Since the interface supports the encoding of multiple images, it is a pointer array, and the size of the array is image_num. Users can choose not to apply for space (that is, each element of the array is NULL). Space will be automatically allocated according to the size of encoded data within the API, but when it is no longer used, users need to release the space manually. Of course, users can also choose to apply for enough space.

* size_t \*out_size,

  Output parameter. After encoding, the size of each image (in bytes) is stored in the pointer.

* int quality_factor = 85

  Input parameter. The quality factor of the encoded image. The value is between 0 and 100. The larger the value, the higher the image quality, but the greater the amount of data. On the contrary, the smaller the value, the lower the image quality, and the less the amount of data. This parameter is optional and the default value is 85.


**Return value Description:**

* BM_SUCCESS: success

* Other: failed


.. note::

    Currently, the image formats which support encoding include the following:
     | FORMAT_YUV420P
     | FORMAT_YUV422P
     | FORMAT_YUV444P
     | FORMAT_NV12
     | FORMAT_NV21
     | FORMAT_NV16
     | FORMAT_NV61
     | FORMAT_GRAY

    The interface supports the following data_type:
     | DATA_TYPE_EXT_1N_BYTE


**Sample code**

    .. code-block:: c

        #include <stdio.h>
        #include <stdint.h>
        #include <stdlib.h>
        #include <memory.h>
        #include "bmcv_api_ext.h"
        #include <assert.h>
        #include <math.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        int main()
        {
            int format = FORMAT_YUV420P;
            int image_h = 1080;
            int image_w = 1920;
            bm_image src;
            bm_handle_t handle;
            size_t byte_size = image_w * image_h * 3 / 2;
            unsigned char* input_data = (unsigned char*)malloc(byte_size);
            unsigned char* in_ptr[3] = {input_data, input_data + image_h * image_w, input_data + 2 * image_h * image_w};
            void* jpeg_data[4] = {NULL, NULL, NULL, NULL};
            const char *src_name = "path/to/src";

            readBin(src_name, input_data, byte_size);
            bm_dev_request(&handle, 0);
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_alloc_dev_mem(src, BMCV_HEAP1_ID);
            bm_image_copy_host_to_device(src, (void**)in_ptr);
            bmcv_image_jpeg_enc(handle, 1, &src, jpeg_data, &byte_size, 95);

            bm_image_destroy(src);
            free(input_data);
            bm_dev_free(handle);
            return 0;
        }