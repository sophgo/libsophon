bmcv_image_warp_affine
======================


  The interface implements the affine transformation of the image, and the operations of rotation, translation and scaling. Affine transformation is a linear transformation from two-dimensional coordinates (x, y) to two-dimensional coordinates (x0, y0). The implementation of this interface is to find the corresponding coordinates in the input image for each pixel of the output image, so as to form a new image. Its mathematical expression is as follows:

.. math::

    \left\{
    \begin{array}{c}
    x_0=a_1x+b_1y+c_1 \\
    y_0=a_2x+b_2y+c_2 \\
    \end{array}
    \right.

The corresponding homogeneous coordinate matrix is expressed as:


.. math::

     \left[\begin{matrix} x_0 \\ y_0 \\ 1 \end{matrix} \right]=\left[\begin{matrix} a_1&b_1&c_1 \\ a_2&b_2&c_2 \\ 0&0&1 \end{matrix} \right]\times \left[\begin{matrix} x \\ y \\ 1 \end{matrix} \right]



The coordinate transformation matrix is a 6-point matrix, which is a coefficient matrix for deriving the input image coordinates from the output image coordinates, which can be obtained by the corresponding 3-point coordinates on the input and output image. In facial detection, the face transformation matrix can be obtained through facial detection points.


bmcv_affine_matrix defines a coordinate transformation matrix in the order of float m[6] = {a1, b1, c1, a2, b2, c2}.
bmcv_affine_image_matrix defines that there are several transformation matrices in an image. Generally speaking, when an image has multiple faces, it will correspond to multiple transformation matrices.

    .. code-block:: c

        typedef struct bmcv_affine_matrix_s{
                float m[6];
        } bmcv_warp_matrix;

        typedef struct bmcv_affine_image_matrix_s{
                bmcv_affine_matrix *matrix;
                int matrix_num;
        } bmcv_affine_image_matrix;


**Interface form 1:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_affine(
                bm_handle_t handle,
                int image_num,
                bmcv_affine_image_matrix matrix[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );

**Interface form 2:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_affine_similar_to_opencv(
                bm_handle_t handle,
                int image_num,
                bmcv_affine_image_matrix matrix[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );
This interface is an interface to align opencv affine transformations.

**Input parameter description**

* bm_handle_t handle

  Input parameter. The input bm_handle handle.

* int image_num

  Input parameter. The number of input images, up to 4.

* bmcv_affine_image_matrix matrix[4]

  Input parameter. The transformation matrix data structure corresponding to each image. Support up to 4 images.

* bm_image\* input

  Input parameter. Input bm_image. For 1N mode, up to 4 bm_image; for 4N mode, up to one bm_image.

* bm_image\* output

  Output parameter. Output bm_image. It requires calling bmcv_image_create externally. Users are recommended to call bmcv_image_attach to allocate the device memory. If users do not call attach, the device memory will be allocated internally. For output bm_image, its data type is consistent with the input, that is, if the input is 4N mode, the output is also 4N mode; if the input is 1N mode, the output is also 1N mode. The size of the required bm_image is the sum of the transformation matrix of all images. For example, input a 4N mode bm_image, and the transformation matrix of four pictures is [3,0,13,5]. The total transformation matrix is 3 + 0 + 13 + 5 = 21. Since the output is in 4N mode, it needs (21 + 4-1) / 4 = 6 bm_image output.

* int use_bilinear

  Input parameter. Whether to use bilinear interpolation. If it is 0, use nearest interpolation. If it is 1, use bilinear interpolation. The default is nearest interpolation. The performance of nearest interpolation is better than bilinear interpolation. Therefore, it is recommended to choose nearest interpolation first. Users can select bilinear interpolation unless there are requirements for accuracy.


**Return parameters description:**

* BM_SUCCESS: success

* Other: failed


**Note**

1. The API supports the following image_format:

   +-----+------------------------+
   | num | image_format           |
   +=====+========================+
   |  1  | FORMAT_BGR_PLANAR      |
   +-----+------------------------+
   |  2  | FORMAT_RGB_PLANAR      |
   +-----+------------------------+

2. The API supports the following data_type in bm1684 :

   +-----+------------------------+
   | num | data_type              |
   +=====+========================+
   |  1  | DATA_TYPE_EXT_1N_BYTE  |
   +-----+------------------------+
   |  2  | DATA_TYPE_EXT_4N_BYTE  |
   +-----+------------------------+

3. The API supports the following data_type in bm1684x :

   +-----+------------------------+
   | num | data_type              |
   +=====+========================+
   |  1  | DATA_TYPE_EXT_1N_BYTE  |
   +-----+------------------------+

4. The API’s input and output of bm_image both support stride.

5. It is required that the width, height, image_format and data_type of the input bm_image must be consistent.

6. It is required that the width, height, image_format and data_type of the output bm_image must be consistent.

7. bm1684X currently does not support bilinear interpolation.


**Code example**

    .. code-block:: c

        #inculde "common.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>
        #include <iostream>
        #include "bmcv_api_ext.h"
        #include "bmlib_utils.h"

        int main(int argc, char *argv[]) {
            bm_handle_t handle;

            int image_h = 1080;
            int image_w = 1920;

            int dst_h = 256;
            int dst_w = 256;
            bm_dev_request(&handle, 0);
            bmcv_affine_image_matrix matrix_image;
            matrix_image.matrix_num = 1;
            std::shared_ptr<bmcv_affine_matrix> matrix_data
                    = std::make_shared<bmcv_affine_matrix>();
            matrix_image.matrix = matrix_data.get();

            matrix_image.matrix->m[0] = 3.848430;
            matrix_image.matrix->m[1] = -0.02484;
            matrix_image.matrix->m[2] = 916.7;
            matrix_image.matrix->m[3] = 0.02;
            matrix_image.matrix->m[4] = 3.8484;
            matrix_image.matrix->m[5] = 56.4748;

            bm_image src, dst;
            bm_image_create(handle, image_h, image_w, FORMAT_BGR_PLANAR,
                    DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_create(handle, dst_h, dst_w, FORMAT_BGR_PLANAR,
                    DATA_TYPE_EXT_1N_BYTE, &dst);

            std::shared_ptr<u8*> src_ptr = std::make_shared<u8*>(
                    new u8[image_h * image_w * 3]);
            memset((void *)(*src_ptr.get()), 148, image_h * image_w * 3);
            u8 *host_ptr[] = {*src_ptr.get()};
            bm_image_copy_host_to_device(src, (void **)host_ptr);

            bmcv_image_warp_affine(handle, 1, &matrix_image, &src, &dst);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_dev_free(handle);

            return 0;
        }



