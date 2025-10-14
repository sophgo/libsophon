bmcv_image_vpp_csc_matrix_convert
=================================

By default, bmcv_image_vpp_convert uses BT_609 standard for color gamut conversion. In some cases, users need to use other standards or customize csc parameters.


**Interface form::**

    .. code-block:: c

        bm_status_t bmcv_image_vpp_csc_matrix_convert(
                    bm_handle_t handle,
                    int output_num,
                    bm_image input,
                    bm_image* output,
                    csc_type_t csc,
                    csc_matrix_t* matrix = nullptr,
                    bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);


**Processor model support**

This interface supports BM1684/BM1684X.


**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. Handle of device’s capacity (HDC) obtained by calling bm_dev_request.

* int image_num

  Input parameter. The number of input bm_image

* bm_image input

  Input parameter. Input bm_image object

* bm_image* output

  Output parameter. Output bm_image object pointer

* csc_type_t csc

  Input parameters. Enumeration type of gamut conversion, currently optional:

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

  Input parameters. Color gamut conversion custom matrix, valid if and only if csc is CSC_USER_DEFINED_MATRIX.

  The specific format is defined as follows:

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

    bm1684:

    .. math::

        \left\{
        \begin{array}{c}
        dst_0=(csc\_coe_{00} * src_0+csc\_coe_{01} * src_1+csc\_coe_{02} * src_2 + csc\_add_0) >> 10 \\
        dst_1=(csc\_coe_{10} * src_0+csc\_coe_{11} * src_1+csc\_coe_{12} * src_2 + csc\_add_1) >> 10 \\
        dst_2=(csc\_coe_{20} * src_0+csc\_coe_{21} * src_1+csc\_coe_{22} * src_2 + csc\_add_2) >> 10 \\
        \end{array}
        \right.

    bm1684x:

    .. math::

        \left\{
        \begin{array}{c}
        dst_0=csc\_coe_{00} * src_0+csc\_coe_{01} * src_1+csc\_coe_{02} * src_2 + csc\_add_0 \\
        dst_1=csc\_coe_{10} * src_0+csc\_coe_{11} * src_1+csc\_coe_{12} * src_2 + csc\_add_1 \\
        dst_2=csc\_coe_{20} * src_0+csc\_coe_{21} * src_1+csc\_coe_{22} * src_2 + csc\_add_2 \\
        \end{array}
        \right.


* bmcv_resize_algorithm algorithm

  Input parameter. Resize algorithm selection, including BMCV_INTER_NEAREST 、 BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC.By default, it is set as bilinear difference.

  bm1684 supports BMCV_INTER_NEAREST, BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC.

  bm1684x supports BMCV_INTER_NEAREST and BMCV_INTER_LINEAR.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The format and some requirements that the API needs to meet are consistent to vpp_convert.

2. If the color gamut conversion enumeration type does not correspond to the input and output formats. For example, if csc == CSC_YCbCr2RGB_BT601, while input image_format is RGB, a failure will be returned.

3. If csc == CSC_USER_DEFINED_MATRIX while matrix is nullptr, a failure will be returned.


**Code example:**

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