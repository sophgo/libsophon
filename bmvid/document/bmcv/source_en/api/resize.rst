bmcv_image_resize
=================

The interface is used to change image size, such as zoom in, zoom out, matting and other functions.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_resize(
                    bm_handle_t handle,
                    int input_num,
                    bmcv_resize_image resize_attr[4],
                    bm_image* input,
                    bm_image* output);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. bm_handle handle.

* int input_num

  Input parameter. Input the number of images, up to 4. If input_num > 1, then multiple input images must be stored continuously (you can use bm_image_alloc_contiguous_mem to apply for continuous space for multiple images).

* bmcv_resize_image resize_attr [4]

  Input parameter. The resize parameter corresponding to each image. Support up to 4 images.

* bm_image\* input

  Input parameter. Input bm_image. Each bm_image requires an external call of bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to applicate memory, or use bmcv_image_attach to attach existing memory.

* bm_image\* output

  Output parameter. Output bm_image. Each bm_image requires an external call to bmcv_image_create and create the image memory through bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach the existing memory. If it is not actively allocated, it will be allocated within the API itself.


**Return parameter description:**

* BM_SUCCESS: success

* Other: failed


**Data type description:**

    .. code-block:: c

        typedef struct bmcv_resize_s{
                int start_x;
                int start_y;
                int in_width;
                int in_height;
                int out_width;
                int out_height;
        }bmcv_resize_t;

        typedef struct bmcv_resize_image_s{
                bmcv_resize_t *resize_img_attr;
                int roi_num;
                unsigned char stretch_fit;
                unsigned char padding_b;
                unsigned char padding_g;
                unsigned char padding_r;
                unsigned int interpolation;
        }bmcv_resize_image;

* bmcv_resize_image describes the resize configuration information in an image.

* roi_num describes the total number of subimages in an image that need to be resized.

* stretch_fit indicates whether the image is scaled according to the original scale. 1 indicates that it is not necessary to scale according to the original scale, and 0 indicates that it is scaled according to the original scale. When this method is adopted, the places in the resulting image that are scaled will be filled with specific values.

* padding_b means when stretch_fit is set to 0, the filled value on channel b.

* padding_r means when stretch_fit is set to 0, the filled value on channel r.

* padding_g means when stretch_fit is set to 0, the filled value on channel g.

* interpolation represents the algorithm used in the thumbnail. BMCV_INTER_NEAREST represents the nearest neighbor algorithm, BMCV_INTER_LINEAR represents the linear interpolation algorithm,BMCV_INTER_BICUBIC represents the bi-triple interpolation algorithm.

    bm1684 supports BMCV_INTER_NEAREST,BMCV_INTER_LINEAR,BMCV_INTER_BICUBIC.

    bm1684x supports BMCV_INTER_NEAREST,BMCV_INTER_LINEAR.

* start_x describes the start abscissa of resize (relative to the original image), which is commonly used for matting function.

* start_y describes the start ordinate of resize (relative to the original image), which is commonly used for matting function.

* in_width describes the width of the crop image.

* in_height describes the height of the crop image.

* out_width describes the width of the output image.

* out_height describes the height of the output image.


**Code example:**

    .. code-block:: c

        #include <memory>
        #include <assert.h>
        #include <iostream>
        #include <set>
        #include <stdint.h>
        #include <stdio.h>
        #include <string>
        #include <vector>
        #include <cmath>
        #include <cstring>
        #include "bmcv_api.h"
        #include "bmcv_api_ext.h"
        #include "test_misc.h"

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
            int image_num = 4;
            int crop_w = 711, crop_h = 400, resize_w = 711, resize_h = 400;
            int image_w = 1920, image_h = 1080;
            int img_size_i = image_w * image_h * 3;
            int img_size_o = resize_w * resize_h * 3;
            unsigned char* img_data = new unsigned char[img_size_i * image_num];
            unsigned char* res_data = new unsigned char[img_size_o * image_num];
            bmcv_resize_image resize_attr[image_num];
            bmcv_resize_t resize_img_attr[image_num];
            bm_image input[image_num];
            bm_image output[image_num];
            bm_handle_t handle;
            const char *src_names[4] = {"path/to/src0", "path/to/src1", "path/to/src2", "path/to/src3"};
            const char *dst_names[4] = {"path/to/dst0", "path/to/dst1", "path/to/dst2", "path/to/dst3"};

            bm_dev_request(&handle, 0);

            for(int i = 0; i < image_num; ++i) {
                readBin(src_names[i], img_data + i * img_size_i, img_size_i);
            }

            memset(res_data, 0, img_size_o * image_num);

            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                resize_img_attr[img_idx].start_x = 0;
                resize_img_attr[img_idx].start_y = 0;
                resize_img_attr[img_idx].in_width = crop_w;
                resize_img_attr[img_idx].in_height = crop_h;
                resize_img_attr[img_idx].out_width = resize_w;
                resize_img_attr[img_idx].out_height = resize_h;
            }

            for (int img_idx = 0; img_idx < image_num; img_idx++) {
            resize_attr[img_idx].resize_img_attr = &resize_img_attr[img_idx];
            resize_attr[img_idx].roi_num = 1;
            resize_attr[img_idx].stretch_fit = 1;
            resize_attr[img_idx].interpolation = BMCV_INTER_NEAREST;
            }

            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                bm_image_create(handle, image_h, image_w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &input[img_idx]);
            }
            bm_image_alloc_contiguous_mem(image_num, input, 1);
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                unsigned char * input_img_data = img_data + img_size_i * img_idx;
                bm_image_copy_host_to_device(input[img_idx], (void **)&input_img_data);
            }
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                bm_image_create(handle, resize_h, resize_w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output[img_idx]);
            }
            bm_image_alloc_contiguous_mem(image_num, output, 1);
            bmcv_image_resize(handle, image_num, resize_attr, input, output);
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                unsigned char *res_img_data = res_data + img_size_o * img_idx;
                bm_image_copy_device_to_host(output[img_idx], (void **)&res_img_data);
                for(int i = 0; i < image_num; ++i) {
                    writeBin(dst_names[i], res_img_data, img_size_o);
                }
            }

            bm_image_free_contiguous_mem(image_num, input);
            bm_image_free_contiguous_mem(image_num, output);
            for(int i = 0; i < image_num; i++) {
                bm_image_destroy(input[i]);
                bm_image_destroy(output[i]);
            }
            delete[] img_data;
            delete[] res_data;
            bm_dev_free(handle);
            return 0;
        }


**Supported format::**

1. resize supports the conversion of the following image_format:

+-----+-------------------------------------------+
| 1   | FORMAT_BGR_PLANAR ——> FORMAT_BGR_PLANAR |
+-----+-------------------------------------------+
| 2   | FORMAT_RGB_PLANAR ——> FORMAT_RGB_PLANAR |
+-----+-------------------------------------------+
| 3   | FORMAT_BGR_PACKED ——> FORMAT_BGR_PACKED |
+-----+-------------------------------------------+
| 4   | FORMAT_RGB_PACKED ——> FORMAT_RGB_PACKED |
+-----+-------------------------------------------+
| 3   | FORMAT_BGR_PACKED ——> FORMAT_BGR_PLANAR |
+-----+-------------------------------------------+
| 4   | FORMAT_RGB_PACKED ——> FORMAT_RGB_PLANAR |
+-----+-------------------------------------------+

2. resize supports the conversion between data types in the following cases:

bm1684 supports the following data_type:

  - 1 vs 1 : one image resizes (crop) one image
  - 1 vs N : one image resizes (crop) multiple image


+-----+----------------------------------------------------+--------+
| 1   | DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE    | 1 vs 1 |
+-----+----------------------------------------------------+--------+
| 2   | DATA_TYPE_EXT_FLOAT32 ——> DATA_TYPE_EXT_FLOAT32    | 1 vs 1 |
+-----+----------------------------------------------------+--------+
| 3   | DATA_TYPE_EXT_4N_BYTE ——> DATA_TYPE_EXT_4N_BYTE    | 1 vs 1 |
+-----+----------------------------------------------------+--------+
| 4   | DATA_TYPE_EXT_4N_BYTE ——> DATA_TYPE_EXT_1N_BYTE    | 1 vs 1 |
+-----+----------------------------------------------------+--------+
| 5   | DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE    | 1 vs N |
+-----+----------------------------------------------------+--------+
| 6   | DATA_TYPE_EXT_FLOAT32 ——> DATA_TYPE_EXT_FLOAT32    | 1 vs N |
+-----+----------------------------------------------------+--------+
| 7   | DATA_TYPE_EXT_4N_BYTE ——> DATA_TYPE_EXT_1N_BYTE    | 1 vs N |
+-----+----------------------------------------------------+--------+


bm1684x supports the following data_type:

+-----+------------------------+-------------------------------+
| num | input data type        | output data type              |
+=====+========================+===============================+
|  1  |                        | DATA_TYPE_EXT_FLOAT32         |
+-----+                        +-------------------------------+
|  2  |                        | DATA_TYPE_EXT_1N_BYTE         |
+-----+                        +-------------------------------+
|  3  | DATA_TYPE_EXT_1N_BYTE  | DATA_TYPE_EXT_1N_BYTE_SIGNED  |
+-----+                        +-------------------------------+
|  4  |                        | DATA_TYPE_EXT_FP16            |
+-----+                        +-------------------------------+
|  5  |                        | DATA_TYPE_EXT_BF16            |
+-----+------------------------+-------------------------------+

**Note:**

1. Before calling bmcv_image_resize(), users must ensure that the input image memory has been applied.

2. bm1684:the maximum size supported is 2048*2048, the minimum size is 16*16, and the maximum zoom ratio is 32.

3. bm1684x:the maximum size supported is 8192*8192, the minimum size is 8*8, and the maximum zoom ratio is 128.