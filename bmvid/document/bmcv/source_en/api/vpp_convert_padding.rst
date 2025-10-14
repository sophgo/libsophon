bmcv_image_vpp_convert_padding
==============================

The effect of image padding is implemented by using VPP hardware resources and memset operation on dst image. This effect is done through the function of dst crop of vpp. Generally speaking, it is to fill a small image into a large one. Users can crop multiple target images from one src image. For each small target image, users can complete the csc and resize operation at one time, and then fill it into the large image according to its offset information in the large image. The number of crop at a time cannot exceed 256.


**Interface form::**

    .. code-block:: c

        bm_status_t bmcv_image_vpp_convert_padding(
                    bm_handle_t handle,
                    int output_num,
                    bm_image input,
                    bm_image* output,
                    bmcv_padding_atrr_t* padding_attr,
                    bmcv_rect_t* crop_rect = NULL,
                    bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);


**Processor model support**

This interface supports BM1684/BM1684X.


**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* int output_num

  Output parameter. The number of output bm_image, which is equal to the number of crop of SRC image. One src crop outputs one dst bm_image.

* bm_image input

  Input parameter. Input bm_image object.

* bm_image\* output

  Output parameter. Output bm_image object pointer.

* bmcv_padding_atrr_t \*  padding_attr

  Input parameter. The location information of the target thumbnail of src crop in the dst image and the pixel values of each channel to be pdding.

    .. code-block:: c

        typedef struct bmcv_padding_atrr_s {
            unsigned int    dst_crop_stx;
            unsigned int    dst_crop_sty;
            unsigned int    dst_crop_w;
            unsigned int    dst_crop_h;
            unsigned char padding_r;
            unsigned char padding_g;
            unsigned char padding_b;
            int           if_memset;
        } bmcv_padding_atrr_t;


1. Offset information of the top left corner of the target small image relative to the dst image origin (top left corner): dst_crop_stx and dst_crop_sty;
2. Width and height of the target small image after resizing: dst_crop_w and dst_crop_h;
3. If dst image is in RGB format, each channel needs padding pixel value information: padding_r, padding_g, padding_b, which is valid when if_memset=1. If it is a GRAY image, users can set all three values to the same;
4. if_memset indicates whether to memset dst image according to the padding value of each channel within the API.

* bmcv_rect_t \*   crop_rect

  Input parameter. Coordinates, width and height of each target small image on src image.

  The specific format is defined as follows:

    .. code-block:: c

       typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* bmcv_resize_algorithm algorithm

  Input parameter. Resize algorithm selection, including BMCV_INTER_NEAREST、BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC . By default, it is set as bilinear difference.

  bm1684 supports BMCV_INTER_NEAREST,BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC.

  bm1684x supports BMCV_INTER_NEAREST and BMCV_INTER_LINEAR.


**Description of returning value:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The format of dst image of this API only supports:

+-----+-------------------------------+
| num | dst image_format              |
+=====+===============================+
|  1  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  2  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  3  | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  4  | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  5  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  6  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+

2. The format and some requirements that the API needs to meet are consistent to bmcv_image_vpp_basic.


**Code example**

    .. code-block:: c

        #include <limits.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include "bmcv_api_ext_c.h"

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
            const char *filename_src = "path/to/src";
            const char *filename_dst = "path/to/dst";
            int in_width = 1920;
            int in_height = 1080;
            int out_width = 1920;
            int out_height = 1080;
            bm_image_format_ext src_format = FORMAT_YUV420P0;
            bm_image_format_ext dst_format = FORMAT_YUV420P;
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR;

            bmcv_rect_t crop_rect = {
                .start_x = 100,
                .start_y = 100,
                .crop_w = 500,
                .crop_h = 500
                };

            bmcv_padding_atrr_t padding_rect = {
                .dst_crop_stx = 0,
                .dst_crop_sty = 0,
                .dst_crop_w = 1000,
                .dst_crop_h = 1000,
                .padding_r = 155,
                .padding_g = 20,
                .padding_b = 36,
                .if_memset = 1
                };

            bm_status_t ret = BM_SUCCESS;
            int src_size = in_height * in_width * 3 / 2;
            int dst_size = in_height * in_width * 3 / 2;
            unsigned char *src_data = (unsigned char *)malloc(src_size);
            unsigned char *dst_data = (unsigned char *)malloc(dst_size);

            readBin(filename_src, src_data, src_size);
            bm_handle_t handle = NULL;
            int dev_id = 0;
            bm_image src, dst;

            ret = bm_dev_request(&handle, dev_id);
            bm_image_create(handle, in_height, in_width, src_format, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_create(handle, out_height, out_width, dst_format, DATA_TYPE_EXT_1N_BYTE, &dst, NULL);
            bm_image_alloc_dev_mem(src, BMCV_HEAP1_ID);
            bm_image_alloc_dev_mem(dst, BMCV_HEAP1_ID);

            int src_image_byte_size[4] = {0};
            bm_image_get_byte_size(src, src_image_byte_size);
            void *src_in_ptr[4] = {(void *)src_data,
                                    (void *)((char *)src_data + src_image_byte_size[0]),
                                    (void *)((char *)src_data + src_image_byte_size[0] + src_image_byte_size[1]),
                                    (void *)((char *)src_data + src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2])};

            bm_image_copy_host_to_device(src, (void **)src_in_ptr);
            ret = bmcv_image_vpp_convert_padding(handle, 1, src, &dst, &padding_rect, &crop_rect, algorithm);

            int dst_image_byte_size[4] = {0};
            bm_image_get_byte_size(dst, dst_image_byte_size);
            void *dst_in_ptr[4] = {(void *)dst_data,
                                    (void *)((char *)dst_data + dst_image_byte_size[0]),
                                    (void *)((char *)dst_data + dst_image_byte_size[0] + dst_image_byte_size[1]),
                                    (void *)((char *)dst_data + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};

            bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);
            writeBin(filename_dst, dst_data, dst_size);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_dev_free(handle);
            free(src_data);
            free(dst_data);
            return ret;
        }