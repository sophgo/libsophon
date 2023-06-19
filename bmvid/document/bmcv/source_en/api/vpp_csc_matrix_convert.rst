bmcv_image_vpp_csc_matrix_convert
=================================

  By default, bmcv_image_vpp_convert uses BT_609 standard for color gamut conversion. In some cases, users need to use other standards or customize csc parameters.

    .. code-block:: c

        bm_status_t bmcv_image_vpp_csc_matrix_convert(
            bm_handle_t  handle,
            int output_num,
            bm_image input,
            bm_image *output,
            csc_type_t csc,
            csc_matrix_t * matrix = nullptr,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);

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
        #include "bmlib_utils.h"
        #include "common.h"
        #include <memory>
        #include "stdio.h"
        #include "stdlib.h"
        #include <stdio.h>
        #include <stdlib.h>

        int main(int argc, char *argv[]) {
            bm_handle_t handle;
            int            image_h     = 1080;
            int            image_w     = 1920;
            bm_image       src, dst[4];
            bm_dev_request(&handle, 0);
            bm_image_create(handle, image_h, image_w, FORMAT_NV12,
                    DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_alloc_dev_mem(src, 1);
            for (int i = 0; i < 4; i++) {
                bm_image_create(handle,
                    image_h / 2,
                    image_w / 2,
                    FORMAT_BGR_PACKED,
                    DATA_TYPE_EXT_1N_BYTE,
                    dst + i);
                bm_image_alloc_dev_mem(dst[i]);
            }
            std::unique_ptr<u8 []> y_ptr(new u8[image_h * image_w]);
            std::unique_ptr<u8 []> uv_ptr(new u8[image_h * image_w / 2]);
            memset((void *)(y_ptr.get()), 148, image_h * image_w);
            memset((void *)(uv_ptr.get()), 158, image_h * image_w / 2);
            u8 *host_ptr[] = {y_ptr.get(), uv_ptr.get()};
            bm_image_copy_host_to_device(src, (void **)host_ptr);

            bmcv_rect_t rect[] = {{0, 0, image_w / 2, image_h / 2},
                    {0, image_h / 2, image_w / 2, image_h / 2},
                    {image_w / 2, 0, image_w / 2, image_h / 2},
                    {image_w / 2, image_h / 2, image_w / 2, image_h / 2}};

            bmcv_image_vpp_csc_matrix_convert(handle, 4, src, dst, CSC_YCbCr2RGB_BT601);

            for (int i = 0; i < 4; i++) {
                bm_image_destroy(dst[i]);
            }

            bm_image_destroy(src);
            bm_dev_free(handle);
            return 0;
        }


