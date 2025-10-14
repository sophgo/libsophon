bmcv_image_jpeg_dec
===================

The interface can decode multiple JPEG  images.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_dec(
                    bm_handle_t handle,
                    void* p_jpeg_data[],
                    size_t* in_size,
                    int image_num,
                    bm_image* dst);


**Input parameter description:**

* bm_handle_t handle

  Input parameters. Handle of bm_handle.

* void \*  p_jpeg_data[]

  Input parameter. The image data pointer to be decoded. It is a pointer array because the interface supports the decoding of multiple images.

* size_t \*in_size

  Input parameter. The size of each image to be decoded (in bytes) is stored in the pointer, that is, the size of the space pointed to by each dimensional pointer of p_jpeg_data.

* int  image_num

  Input parameter. The number of input image, up to 4.

* bm_image\* dst

  Output parameter. The pointer of output bm_image. Users can choose whether or not to call bm_image_create to create dst bm_image. If users only declare but do not create, it will be automatically created by the interface according to the image information to be decoded. The default format is shown in the following table. When it is no longer needed, users still needs to call bm_image_destroy to destroy it.

  +------------+---------------------+
  | Code Stream|Default Output Format|
  +============+=====================+
  |  YUV420    |  FORMAT_YUV420P     |
  +------------+---------------------+
  |  YUV422    |  FORMAT_YUV422P     |
  +------------+---------------------+
  |  YUV444    |  FORMAT_YUV444P     |
  +------------+---------------------+
  |  YUV400    |  FORMAT_GRAY        |
  +------------+---------------------+


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. If users do not use bmcv_image_create to create bm_image of dst, they need to set the space pointed by the parameter input pointer to 0.


2. At present, the image formats supported by decoding and their output formats are shown in the following table. If users need to specify one of the following output formats, they can use bmcv_image_create to create their own dst bm_image, so as to decode the picture to one of the following corresponding formats.

   +------------------+------------------------+
   | Code Stream      | Default Output Format  |
   +==================+========================+
   |                  |  FORMAT_YUV420P        |
   +  YUV420          +------------------------+
   |                  |  FORMAT_NV12           |
   +                  +------------------------+
   |                  |  FORMAT_NV21           |
   +------------------+------------------------+
   |                  |  FORMAT_YUV422P        |
   +  YUV422          +------------------------+
   |                  |  FORMAT_NV16           |
   +                  +------------------------+
   |                  |  FORMAT_NV61           |
   +------------------+------------------------+
   |  YUV444          |  FORMAT_YUV444P        |
   +------------------+------------------------+
   |  YUV400          |  FORMAT_GRAY           |
   +------------------+------------------------+


  The interface supports the following data_type:

   +------------------+------------------------+
   |       num        |     data_type          |
   +==================+========================+
   |        1         |  DATA_TYPE_EXT_1N_BYTE |
   +------------------+------------------------+


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
            int format = FORMAT_YUV420P;
            int image_h = 1080;
            int image_w = 1920;
            bm_image src;
            bm_image dst;
            bm_handle_t handle;
            size_t byte_size = image_w * image_h * 3 / 2;
            unsigned char* input_data = (unsigned char*)malloc(byte_size);
            unsigned char* output_data = (unsigned char*)malloc(byte_size);
            unsigned char* in_ptr[3] = {input_data, input_data + image_h * image_w, input_data + 2 * image_h * image_w};
            unsigned char* out_ptr[3] = {output_data, output_data + image_h * image_w, output_data + 2 * image_h * image_w};
            void* jpeg_data[4] = {NULL, NULL, NULL, NULL};
            const char *dst_name = "path/to/dst";
            const char *src_name = "path/to/src";

            readBin(src_name, input_data, byte_size);
            bm_dev_request(&handle, 0);
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_alloc_dev_mem(src, BMCV_HEAP1_ID);
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &dst, NULL);
            bm_image_alloc_dev_mem(dst, BMCV_HEAP1_ID);
            bm_image_copy_host_to_device(src, (void**)in_ptr);
            bmcv_image_jpeg_enc(handle, 1, &src, jpeg_data, &byte_size, 95);
            bmcv_image_jpeg_dec(handle, (void**)jpeg_data, &byte_size, 1, &dst);
            bm_image_copy_device_to_host(dst, (void**)out_ptr);
            writeBin(dst_name, output_data, byte_size);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            free(input_data);
            free(output_data);
            bm_dev_free(handle);
            return 0;
        }