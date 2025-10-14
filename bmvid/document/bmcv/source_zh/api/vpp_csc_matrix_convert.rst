bmcv_image_vpp_csc_matrix_convert
=================================

默认情况下，bmcv_image_vpp_convert使用的是BT_601标准进行色域转换。有些情况下需要使用其他标准，或者用户自定义csc参数。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_vpp_csc_matrix_convert(
                    bm_handle_t handle,
                    int output_num,
                    bm_image input,
                    bm_image* output,
                    csc_type_t csc,
                    csc_matrix_t* matrix = nullptr,
                    bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);


**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄，通过调用 bm_dev_request 获取

* int image_num

  输入参数。输入 bm_image 数量

* bm_image input

  输入参数。输入 bm_image 对象

* bm_image* output

  输出参数。输出 bm_image 对象指针

* csc_type_t csc

  输入参数。色域转换枚举类型，目前可选：

    .. code-block:: c

        typedef enum csc_type {
            CSC_YCbCr2RGB_BT601 = 0,
            CSC_YPbPr2RGB_BT601,
            CSC_RGB2YCbCr_BT601,
            CSC_YCbCr2RGB_BT709,
            CSC_RGB2YCbCr_BT709,
            CSC_RGB2YPbPr_BT601,
            CSC_YPbPr2RGB_BT709,
            CSC_RGB2YPbPr_BT709,
            CSC_USER_DEFINED_MATRIX = 1000,
            CSC_MAX_ENUM
        } csc_type_t;

* csc_matrix_t * matrix

  输入参数。色域转换自定义矩阵，当且仅当csc为CSC_USER_DEFINED_MATRIX时这个值才生效。

  具体格式定义如下：

    .. code-block:: c

       typedef struct {
           int csc_coe00;
           int csc_coe01;
           int csc_coe02;
           int csc_add0;
           int csc_coe10;
           int csc_coe11;
           int csc_coe12;
           int csc_add1;
           int csc_coe20;
           int csc_coe21;
           int csc_coe22;
           int csc_add2;
       } __attribute__((packed)) csc_matrix_t;

    bm1684：

    .. math::

        \left\{
        \begin{array}{c}
        dst_0=(csc\_coe_{00} * src_0+csc\_coe_{01} * src_1+csc\_coe_{02} * src_2 + csc\_add_0) >> 10 \\
        dst_1=(csc\_coe_{10} * src_0+csc\_coe_{11} * src_1+csc\_coe_{12} * src_2 + csc\_add_1) >> 10 \\
        dst_2=(csc\_coe_{20} * src_0+csc\_coe_{21} * src_1+csc\_coe_{22} * src_2 + csc\_add_2) >> 10 \\
        \end{array}
        \right.

    bm1684x：

    .. math::

        \left\{
        \begin{array}{c}
        dst_0=csc\_coe_{00} * src_0+csc\_coe_{01} * src_1+csc\_coe_{02} * src_2 + csc\_add_0 \\
        dst_1=csc\_coe_{10} * src_0+csc\_coe_{11} * src_1+csc\_coe_{12} * src_2 + csc\_add_1 \\
        dst_2=csc\_coe_{20} * src_0+csc\_coe_{21} * src_1+csc\_coe_{22} * src_2 + csc\_add_2 \\
        \end{array}
        \right.


* bmcv_resize_algorithm algorithm

  输入参数。resize 算法选择，包括 BMCV_INTER_NEAREST 、BMCV_INTER_LINEAR 和 BMCV_INTER_BICUBIC 三种，默认情况下是双线性插值。

  bm1684 支持 BMCV_INTER_NEAREST，BMCV_INTER_LINEAR，BMCV_INTER_BICUBIC。

  bm1684x 支持BMCV_INTER_NEAREST， BMCV_INTER_LINEAR。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1. 该 API 所需要满足的格式以及部分要求与vpp_convert一致

2. 如果色域转换枚举类型与input和output格式不对应，如csc == CSC_YCbCr2RGB_BT601,而input image_format为RGB格式，则返回失败。

3. 如果csc == CSC_USER_DEFINED_MATRIX而matrix为nullptr，则返回失败。


**代码示例：**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include <memory>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>

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
            bm_image src, dst[4];
            unsigned char* src_data = new unsigned char[image_h * image_w * 3 / 2];
            unsigned char* dst_data = new unsigned char[image_h / 2 * image_w / 2 * 3];
            unsigned char* in_ptr[3] = {src_data, src_data + image_h * image_w, src_data + 2 * image_h * image_w};
            unsigned char* out_ptr[3] = {dst_data, dst_data + image_h * image_w, dst_data + 2 * image_h * image_w};
            const char *src_name = "/path/to/src";
            const char *dst_names = {"path/to/dst0", "path/to/dst1", "path/to/dst2", "path/to/dst3"};

            bm_dev_request(&handle, 0);
            readBin(src_name, src_data, image_h * image_w * 3 / 2);
            bm_image_create(handle, image_h, image_w, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_alloc_dev_mem(src, 1);
            for (int i = 0; i < 4; i++) {
                bm_image_create(handle, image_h / 2, image_w / 2, FORMAT_BGR_PACKED, DATA_TYPE_EXT_1N_BYTE, dst + i);
                bm_image_alloc_dev_mem(dst[i]);
            }
            memset(src_data, 148, image_h * image_w * 3 / 2);
            bm_image_copy_host_to_device(src, (void**)in_ptr);
            bmcv_image_vpp_csc_matrix_convert(handle, 4, src, dst, CSC_YCbCr2RGB_BT601);

            for(int i = 0; i < 4; ++i) {
                bm_image_copy_device_to_host(dst[i], (void**)out_ptr);
                writeBin(dst_names[i], dst_data, image_h / 2 * image_w / 2 * 3);
            }

            for (int i = 0; i < 4; i++) {
                bm_image_destroy(dst[i]);
            }
            bm_image_destroy(src);
            bm_dev_free(handle);
            delete[] src_data;
            return 0;
        }