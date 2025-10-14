bmcv_image_overlay
==================

This API implements overlaying a watermark with an alpha channel on an image.


**Processor model support:**

This interface is only supported by the BM1684X.


**Interface Form**

    .. code-block:: c

        bm_status_t bmcv_image_overlay(
                    bm_handle_t handle,
                    bm_image input_base_img,
                    int overlay_num,
                    bmcv_rect_t* overlay_info,
                    bm_image* input_overlay_img);


**Parameter description:**

* bm_handle_t handle

  input parameter. device environment handle.

* bm_image input_base_img

  input/output parameter. bm_image image information corresponding to the base image.

* int overlay_num

  input parameter. number of watermarks.

* bmcv_rect_t* overlay_info

  input parameter. information about the location of the watermark on the input image.

* bm_image* input_overlay_img

  input parameter. enter the bm_image object for the overlays.


**Data Type Definition**

    .. code-block:: c++

        typedef struct bmcv_rect {
            unsigned int start_x;
            unsigned int start_y;
            unsigned int crop_w;
            unsigned int crop_h;
        } bmcv_rect_t;

`start_x`, `start_y`, `crop_w`, and `crop_h` represent the position and size information of the overlay image on the input image, including the x-coordinate of the starting point, the y-coordinate of the starting point, the width of the overlay image, and the height of the overlay image. The top-left corner of the image is used as the origin.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Notes**

1. The base image and output image color formats are:

+-----+-------------------------------+-------------------------------+
| Num | Input Image Format            | Output Image Format           |
+=====+===============================+===============================+
| 1   | FORMAT_RGB_PACKED             | FORMAT_RGB_PACKED             |
+-----+-------------------------------+-------------------------------+

2. The overlay image color format is:

+-----+-------------------------------+
| Num | Input Overlay Image Format    |
+=====+===============================+
| 1   | FORMAT_ABGR_PACKED            |
+-----+-------------------------------+
| 2   | FORMAT_ARGB1555_PACKED        |
+-----+-------------------------------+
| 2   | FORMAT_ARGB4444_PACKED        |
+-----+-------------------------------+

3. The following `data_type` values are currently supported for input and output image data:

+-----+-------------------------------+
| Num | Data Type                     |
+=====+===============================+
| 1   | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

4. The background image is supported at a minimum size of 8 * 8，  a maximum size of 8192 * 8192.

5. The maximum number of images that can be stacked is 10, with the largest stackable image size being 850 * 850 (the maximum size for a single stacked image is also 850 * 850).


**Code Example**

    .. code-block:: c++

        #include "stb_image.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <random>
        #include <algorithm>
        #include <vector>
        #include <iostream>
        #include <string.h>
        #include "bmcv_api_ext.h"

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
            int overlay_num = 1;
            int base_width = 1920;
            int base_height = 1080;
            int pos_x[overlay_num] = {50};
            int pos_y[overlay_num] = {150};
            int overlay_width[overlay_num] = {400};
            int overlay_height[overlay_num] = {400};
            bm_handle_t handle;
            unsigned char* base_image = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
            unsigned char* output_tpu = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
            unsigned char* overlay_image[overlay_num];
            bm_image input_base_img;
            bm_image input_overlay_img[overlay_num];
            unsigned char** in_overlay_ptr[overlay_num];
            bmcv_rect rect_array[overlay_num];
            unsigned char* out_ptr[1] = {output_tpu};
            const char *base_path = "path/to/base";
            const char *overlay_path = "path/to/overlay";
            const char *output_path = "path/to/output";

            bm_dev_request(&handle, 0);
            for (int i = 0; i < overlay_num; i++) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 4 * sizeof(unsigned char));
                readBin(overlay_path, overlay_image[i], overlay_width[i] * overlay_height[i] * 4);
            }

            readBin(base_path, base_image, base_width * base_height * 3);
            memcpy(output_tpu, base_image, base_width * base_height * 3);

            for (int i = 0; i < overlay_num; i++) {
                bm_image_create(handle, overlay_height[i], overlay_width[i], FORMAT_ABGR_PACKED, DATA_TYPE_EXT_1N_BYTE, input_overlay_img + i, NULL);
            }
            for (int i = 0; i < overlay_num; i++) {
                bm_image_alloc_dev_mem(input_overlay_img[i], 2);
            }

            for (int i = 0; i < overlay_num; i++) {
                in_overlay_ptr[i] = new unsigned char*[1];
                in_overlay_ptr[i][0] = overlay_image[i];
            }
            for (int i = 0; i < overlay_num; i++) {
                bm_image_copy_host_to_device(input_overlay_img[i], (void **)in_overlay_ptr[i]);
            }

            bm_image_create(handle, base_height, base_width, FORMAT_RGB_PACKED, DATA_TYPE_EXT_1N_BYTE, &input_base_img, NULL);
            bm_image_alloc_dev_mem(input_base_img, 2);
            unsigned char* in_base_ptr[1] = {output_tpu};
            bm_image_copy_host_to_device(input_base_img, (void **)in_base_ptr);

            for (int i = 0; i < overlay_num; i++) {
                rect_array[i].start_x = pos_x[i];
                rect_array[i].start_y = pos_y[i];
                rect_array[i].crop_w = overlay_width[i];
                rect_array[i].crop_h = overlay_height[i];
            }

            bmcv_image_overlay(handle, input_base_img, overlay_num, rect_array, input_overlay_img);
            bm_image_copy_device_to_host(input_base_img, (void **)out_ptr);
            writeBin(output_path, output_tpu, base_width * base_height * 3);

            bm_image_destroy(input_base_img);
            for (int i = 0; i < overlay_num; i++) {
                bm_image_destroy(input_overlay_img[i]);
            }
            free(base_image);
            free(output_tpu);
            for (int i = 0; i < overlay_num; i++) {
                free(overlay_image[i]);
            }
            bm_dev_free(handle);
            return 0;
        }