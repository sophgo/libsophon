bmcv_image_convert_to
=====================

 该接口用于实现图像像素线性变化，具体数据关系可用如下公式表示：

.. math::
    \begin{array}{c}
    y=kx+b
    \end{array}


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_convert_to (
                    bm_handle_t handle,
                    int input_num,
                    bmcv_convert_to_attr convert_to_attr,
                    bm_image* input,
                    bm_image* output
        );


**输入参数说明:**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* int input_num

  输入参数。输入图片数，如果 input_num > 1, 那么多个输入图像必须是连续存储的（可以使用 bm_image_alloc_contiguous_mem 给多张图申请连续空间）。

* bmcv_convert_to_attr convert_to_attr

  输入参数。每张图片对应的配置参数。

* bm_image\* input

  输入参数。输入 bm_image。每个 bm_image 外部需要调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存,或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image\* output

  输出参数。输出 bm_image。每个 bm_image 外部需要调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型说明:**

    .. code-block:: c

        typedef struct bmcv_convert_to_attr_s{
                float alpha_0;
                float beta_0;
                float alpha_1;
                float beta_1;
                float alpha_2;
                float beta_2;
        } bmcv_convert_to_attr;

* alpha_0 描述了第 0 个 channel 进行线性变换的系数

* beta_0 描述了第 0 个 channel 进行线性变换的偏移

* alpha_1 描述了第 1 个 channel 进行线性变换的系数

* beta_1 描述了第 1 个 channel 进行线性变换的偏移

* alpha_2 描述了第 2 个 channel 进行线性变换的系数

* beta_2 描述了第 2 个 channel 进行线性变换的偏移


**代码示例:**

    .. code-block:: c

        #include <stdlib.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <string.h>
        #include <math.h>
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
            int image_num = 4, image_channel = 3;
            int image_w = 1920, image_h = 1080;
            bm_handle_t handle;
            bm_image input_images[4], output_images[4];
            bmcv_convert_to_attr convert_to_attr;
            convert_to_attr.alpha_0 = 1;
            convert_to_attr.beta_0 = 0;
            convert_to_attr.alpha_1 = 1;
            convert_to_attr.beta_1 = 0;
            convert_to_attr.alpha_2 = 1;
            convert_to_attr.beta_2 = 0;
            int img_size = image_w * image_h * image_channel;
            int image_len = image_num * image_channel * image_w * image_h;
            unsigned char* img_data = (unsigned char*)malloc(image_len * sizeof(unsigned char));
            unsigned char *res_data = (unsigned char*)malloc(image_len * sizeof(unsigned char));
            const char *src_names[4] = {"path/to/src0", "path/to/src1", "path/to/src2", "path/to/src3"};
            const char *dst_names[4] = {"path/to/dst0", "path/to/dst1", "path/to/dst2", "path/to/dst3"};

            for(int i = 0; i < image_num; i++){
                readBin(src_names[i], img_data + i * img_size, img_size);
            }

            bm_dev_request(&handle, 0);
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                bm_image_create(handle, image_h, image_w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &input_images[img_idx]);
            }

            bm_image_alloc_contiguous_mem(image_num, input_images, BMCV_IMAGE_FOR_IN);
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                unsigned char *input_img_data = img_data + img_size * img_idx;
                bm_image_copy_host_to_device(input_images[img_idx], (void **)&input_img_data);
            }

            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                bm_image_create(handle, image_h, image_w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_images[img_idx]);
            }
            bm_image_alloc_contiguous_mem(image_num, output_images, BMCV_IMAGE_FOR_OUT);
            bmcv_image_convert_to(handle, image_num, convert_to_attr, input_images, output_images);
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                unsigned char *res_img_data = res_data + img_size * img_idx;
                bm_image_copy_device_to_host(output_images[img_idx], (void **)&res_img_data);
                writeBin(dst_names[img_idx], res_img_data, img_size);
            }

            bm_image_free_contiguous_mem(image_num, input_images);
            bm_image_free_contiguous_mem(image_num, output_images);
            for(int i = 0; i < image_num; i++) {
                bm_image_destroy(input_images[i]);
                bm_image_destroy(output_images[i]);
            }
            bm_dev_free(handle);
            free(img_data);
            free(res_data);
            return 0;
        }


**格式支持:**

1. 该接口支持下列 image_format 的转化：

* FORMAT_BGR_PLANAR ——> FORMAT_BGR_PLANAR

* FORMAT_RGB_PLANAR ——> FORMAT_RGB_PLANAR

* FORMAT_GRAY ——> FORMAT_GRAY

2. 该接口支持下列情形data type之间的转换：

bm1684支持：

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_FLOAT32

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE

* DATA_TYPE_EXT_1N_BYTE_SIGNED ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_FLOAT32 ——> DATA_TYPE_EXT_FLOAT32

* DATA_TYPE_EXT_4N_BYTE ——> DATA_TYPE_EXT_FLOAT32

bm1684x支持：

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_FLOAT32

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE

* DATA_TYPE_EXT_1N_BYTE_SIGNED ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_1N_BYTE ——> DATA_TYPE_EXT_1N_BYTE_SIGNED

* DATA_TYPE_EXT_FLOAT32 ——> DATA_TYPE_EXT_FLOAT32


**注意事项:**

1. 在调用 bmcv_image_convert_to()之前必须确保输入的 image 内存已经申请。

2. 输入的各个 image 的宽、高以及 data_type、image_format 必须相同。

3. 输出的各个 image 的宽、高以及 data_type、image_format 必须相同。

4. 输入 image 宽、高必须等于输出 image 宽高。

5. image_num 必须大于 0。

6. 输出 image 的 stride 必须等于 width。

7. 输入 image 的 stride 必须大于等于 width。

8. bm1684支持最大尺寸为2048*2048，最小尺寸为16*16，当 image format 为 DATA_TYPE_EXT_4N_BYTE 时，w * h 不应大于 1024 * 1024。

9. bm1684x支持最小尺寸为16*16, 当input data_type 为 DATA_TYPE_EXT_1N_BYTE_SIGNED 或 DATA_TYPE_EXT_FLOAT32 时 ，支持最大尺寸为4096*4096，当input data_type 为 DATA_TYPE_EXT_1N_BYTE 时，支持最大尺寸为8192*8192。