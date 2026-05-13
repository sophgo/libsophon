bmcv_add_mask_to_image
======================

该接口可以实现根据同等大小的掩码图(uint8)设置rgb图(float)的像素值。


**处理器型号支持：**

该接口支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_add_mask_to_image(
                    bm_handle_t handle,
                    bm_image mask,
                    bm_image image,
                    int num_mask,
                    mask_info_t *mask_color_config);


**输入参数说明：**

* bm_handle_t handle

  输入参数。输入的 bm_handle 句柄。

* bm_image mask

  输入参数。掩码图，灰度图 U8类型。

* bm_image  image

  输入及输出参数。跟掩码图同等大小的RGB图像，RGB PACKED Float类型。

* int  num_mask

  输入参数。表示需要处理的掩码值种类数量。

* mask_info_t  mask_color_config

  输入参数。表示掩码值及其对应的颜色。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败

**数据类型说明:**

    .. code-block:: c

        typedef struct _mask_info {
            int mask_val;
            float rgb[3];
        };

* mask_val 定义掩码值

* rgb 描述了在当前掩码值的情况下R、G、B 三个通道的像素值


**注意事项：**

num_mask: 不可以大于4。


**示例代码**

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