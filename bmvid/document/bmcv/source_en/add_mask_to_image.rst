bmcv_add_mask_to_image
======================

This interface enables setting pixel values of an RGB image (float) based on a mask image (uint8) of the same size.


**Processor model support**

This interface supports BM1684X.



**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_add_mask_to_image(
                    bm_handle_t handle,
                    bm_image mask,
                    bm_image image,
                    int num_mask,
                    mask_info_t *mask_color_config);


**Description of parameters:**

* bm_handle_t handle

  Input parameter. The input bm_handle handle.

* bm_image mask

  Input parameter. The mask image, a grayscale image of U8 type.

* bm_image  image

  Input and output parameter. An RGB image of the same size as the mask image, of RGB PACKED Float type.

* int  num_mask

  Input parameter. Indicates the number of mask value types that need to be processed.

* mask_info_t  mask_color_config

  Input parameter. Specifies the mask values and their corresponding colors.


**Return value description:**

* BM_SUCCESS: success

* Other: failed

**Data Type Description:**

    .. code-block:: c

        typedef struct _mask_info {
            int mask_val;
            float rgb[3];
        };

* mask_val defines the mask value.

* rgb describes the pixel values for the R, G, and B channels corresponding to the current mask value.


**Note**

* num_mask must not exceed 4.


**Code example:**

    .. code-block:: c

        #include <stdint.h>
        #include <stdlib.h>
        #include <stdio.h>
        #include "bmcv_api_ext.h"

        int main()
        {
            bm_image bm_mask_data, bm_image_data;
            int width = 1920, height = 1080;
            int mask_num = 2;
            mask_info_t mask_val[mask_num]; // mask_r mask_g mask_b
            memset(mask_val, 0, sizeof(mask_info_t) * mask_num);

            mask_val[0].mask_val = 127;
            mask_val[0].rgb[0] = 255; // R
            mask_val[0].rgb[1] = 255; // G
            mask_val[0].rgb[2] = 0;   // B

            mask_val[1].mask_val = 255;
            mask_val[1].rgb[0] = 0;   // R
            mask_val[1].rgb[1] = 0;   // G
            mask_val[1].rgb[2] = 255; // B
            bm_handle_t handle;

            bm_dev_request(&handle, 0);

            unsigned char *mask_data = (unsigned char*)malloc(width * height * sizeof(unsigned char));
            float *image_data = (float *) malloc (width * height * 3 * sizeof(float));

            memset(mask_data, 0, width * height * sizeof(unsigned char));
            int w = width / 4;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width / 4; x++) {
                    mask[y * width + x] = 127;
                }
            }

            for (int y = 0; y < height; y++) {
                for (int x = width / 4; x < w + w; x++) {
                    mask[y * width + x] = 255;
                }
            }

            for(int i = 0; i < width * height * 3; i++) {
                image_data[i] = (float) (rand() % 255);
            }

            ret = bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &bm_mask_data);
            if (ret) {
                printf("bm_image: bm_mask_data create failed\n");
                return -1;
            }

            ret = bm_image_create(handle, height, width, FORMAT_RGB_PACKED, DATA_TYPE_EXT_FLOAT32, &bm_image_data);
            if (ret) {
                printf("bm_image: bm_image_data create failed\n");
                return -1;
            }

            ret = bm_image_alloc_dev_mem(bm_mask_data);
            if (ret) {
                printf("bm_image:bm_mask_data alloc dev mem failed\n");
                return -1;
            }

            ret = bm_image_alloc_dev_mem(bm_image_data);
            if (ret) {
                printf("bm_image:bm_image_data alloc dev mem failed\n");
                bm_image_destroy(bm_mask_data);
                return -1;
            }

            ret = bm_image_copy_host_to_device(bm_mask_data, (void**)(&mask_data));
            if (ret) {
                printf("bm_image:bm_mask_data copy host to device failed\n");
                bm_image_destroy(bm_mask_data);
                bm_image_destroy(bm_image_data);
                return -1;
            }

            ret = bm_image_copy_host_to_device(bm_image_data, (void**)(&image_data));
            if (ret) {
                printf("bm_image:bm_image_data copy host to device failed\n");
                bm_image_destroy(bm_mask_data);
                bm_image_destroy(bm_image_data);
                return -1;
            }

            ret = bmcv_add_mask_to_image(handle, bm_mask_data, bm_image_data, mask_num, mask_val);
            if (ret) {
                printf("bmcv_add_mask_to_image failed\n");
                bm_image_destroy(bm_mask_data);
                bm_image_destroy(bm_image_data);
                return -1;
            }

            ret = bm_image_copy_device_to_host(bm_image_data, (void**)(&image_data));
            if (ret) {
                printf("bm_image:bm_image_data copy device to host failed\n");
                bm_image_destroy(bm_mask_data);
                bm_image_destroy(bm_image_data);
                return -1;
            }

            bm_image_destroy(bm_mask_data);
            bm_image_destroy(bm_image_data);
            bm_dev_free(handle);

            return 0;
        }