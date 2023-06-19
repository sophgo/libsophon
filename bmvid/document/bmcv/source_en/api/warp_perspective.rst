bmcv_image_warp_perspective
===========================


  The interface implments the transmission transformation of the image, also known as projection transformation or perspective transformation. The transmission transformation projects the picture to a new visual plane, which is a nonlinear transformation from two-dimensional coordinates (x0, y0) to two-dimensional coordinates (x, y). The implementation of the API is to obtain the coordinates of the corresponding input image for each pixel coordinate of the output image, and then form a new image. Its mathematical expression is as follows:

.. math::

    \left\{
    \begin{array}{c}
    x'=a_1x+b_1y+c_1 \\
    y'=a_2x+b_2y+c_2 \\
    w'=a_3x+b_3y+c_3 \\
    x_0 = x' / w'          \\
    y_0 = y' / w'          \\
    \end{array}
    \right.

The corresponding homogeneous coordinate matrix is expressed as:


.. math::

     \left[\begin{matrix} x' \\ y' \\ w' \end{matrix} \right]=\left[\begin{matrix} a_1&b_1&c_1 \\ a_2&b_2&c_2 \\ a_3&b_3&c_3 \end{matrix} \right]\times \left[\begin{matrix} x \\ y \\ 1 \end{matrix} \right]

.. math::

    \left\{
    \begin{array}{c}
    x_0 = x' / w'   \\
    y_0 = y' / w'   \\
    \end{array}
    \right.



The coordinate transformation matrix is a 9-point matrix (usually c3=1). Through the transformation matrix, the corresponding coordinates of the original input image can be derived from the coordinates of the output image. The transformation matrix can be obtained by inputting the coordinates of 4 points corresponding to the output image.

In order to complete the transmission transformation more conveniently, the library provides two forms of interfaces for users: one is that the user provides the transformation matrix to the interface as input; the other interface is to provide the coordinates of four points in the input image as input, which is suitable for transmitting an irregular quadrilateral into a rectangle with the same size as the output. As shown in the figure below, the input image A’ B’ C’ D’ can be mapped into the output image ABCD. The user only needs to provide the coordinates of four points A’ B’ C’ D’ in the input image, the interface will automatically calculate the transformation matrix according to the coordinates of these four vertices and the coordinates of the four vertices of the output image, so as to complete the function.

.. figure:: ./perspective.jpg
   :width: 1047px
   :height: 452px
   :scale: 50%
   :align: center



**Interface form 1:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_perspective(
                bm_handle_t handle,
                int image_num,
                bmcv_perspective_image_matrix matrix[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );

  Among them, bmcv_perspective_matrix defines a coordinate transformation matrix in the order of float m[9] = {a1, b1, c1, a2, b2, c2, a3, b3, c3}.
  bmcv_perspective_image_Matrix defines several transformation matrices in an image, which can implements the transmission transformation of multiple small images in an image.

    .. code-block:: c

        typedef struct bmcv_perspective_matrix_s{
                float m[9];
        } bmcv_perspective_matrix;

        typedef struct bmcv_perspective_image_matrix_s{
                bmcv_perspective_matrix *matrix;
                int matrix_num;
        } bmcv_perspective_image_matrix;



**Interface form 2:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_perspective_with_coordinate(
                bm_handle_t handle,
                int image_num,
                bmcv_perspective_image_coordinate coord[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );

  Among them, bmcv_perspective_coordinate defines the coordinates of the four vertices of the quadrilateral, which are stored in the order of top left, top right, bottom left and bottom right.
  bmcv_perspective_image_coordinate defines the coordinates of several groups of quadrangles in an image, which can complete the transmission transformation of multiple small images in an image.

    .. code-block:: c

        typedef struct bmcv_perspective_coordinate_s{
                int x[4];
                int y[4];
        } bmcv_perspective_coordinate;

        typedef struct bmcv_perspective_image_coordinate_s{
                bmcv_perspective_coordinate *coordinate;
                int coordinate_num;
        } bmcv_perspective_image_coordinate;



**Interface form 3:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_perspective_similar_to_opencv(
                bm_handle_t handle,
                int image_num,
                bmcv_perspective_image_matrix matrix[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );

  The transformation matrix defined by bmcv_perspective_image_matrix in this interface is the same as the transformation matrix required to be input by the warpPerspective interface of opencv, and is the inverse of the matrix defined by the structure of the same name in interface 1, and the other parameters are the same as interface 1.

    .. code-block:: c

        typedef struct bmcv_perspective_matrix_s{
                float m[9];
        } bmcv_perspective_matrix;

        typedef struct bmcv_perspective_image_matrix_s{
                bmcv_perspective_matrix *matrix;
                int matrix_num;
        } bmcv_perspective_image_matrix;


**输入参数说明**

* bm_handle_t handle

  Input parameter. The input bm_handle handle.

* int image_num

  Input parameter. The number of input images, up to 4.

* bmcv_perspective_image_matrix matrix[4]

  Input parameter. The transformation matrix data structure corresponding to each image. Support up to 4 images.

* bmcv_perspective_image_coordinate coord[4]

  Input parameter. The quadrilateral coordinate information corresponding to each image. Support up to 4 images.

* bm_image\* input

  Input parameter. Input bm_image. For 1N mode, up to 4 bm_image; for 4N mode, up to 1 bm_image.

* bm_image\* output

  Output parameter. Output bm_image. It requires calling bmcv_image_create externally. Users are recommended to call bmcv_image_attach to allocate the device memory. If users do not call attach, the device memory will be allocated internally. For output bm_image, its data type is consistent with the input, that is, if the input is 4N mode, the output is also 4N mode; if the input is 1N mode, the output is also 1N mode. The size of the required bm_image is the sum of the transformation matrix of all images. For example, input a 4N mode bm_image, and the transformation matrix of four pictures is [3,0,13,5]. The total transformation matrix is 3 + 0 + 13 + 5 = 21. Since the output is in 4N mode, it needs (21 + 4-1) / 4 = 6 bm_image output.

* int use_bilinear

  Input parameter. Whether to use bilinear interpolation. If it is 0, use nearest interpolation. If it is 1, use bilinear interpolation. The default is nearest interpolation. The performance of nearest interpolation is better than bilinear interpolation. Therefore, it is recommended to choose nearest interpolation first. Users can select bilinear interpolation unless there are requirements for accuracy.



**Return parameter description:**

* BM_SUCCESS: success

* Other: failed


**注意事项**

1. The interface requires that all coordinate points of the output image can find the corresponding coordinates in the original input image, which cannot exceed the size of the original image. It is recommended to give priority to interface 2, which can automatically meet this requirement.

2. The API supports the following image_format:

   +-----+------------------------+
   | num | image_format           |
   +=====+========================+
   |  1  | FORMAT_BGR_PLANAR      |
   +-----+------------------------+
   |  2  | FORMAT_RGB_PLANAR      |
   +-----+------------------------+

3. The API supports the following data_type in bm1684:

   +-----+------------------------+
   | num | data_type              |
   +=====+========================+
   |  1  | DATA_TYPE_EXT_1N_BYTE  |
   +-----+------------------------+
   |  2  | DATA_TYPE_EXT_4N_BYTE  |
   +-----+------------------------+

4.  The API supports the following data_type in bm1684x:

   +-----+------------------------+
   | num | data_type              |
   +=====+========================+
   |  1  | DATA_TYPE_EXT_1N_BYTE  |
   +-----+------------------------+

5. The API’s input and output of bm_image both support stride.

6. It is required that the width, height, image_format and data_type of the input bm_image must be consistent.

7. It is required that the width, height, image_format and data_type of the output bm_image must be consistent.

8. bm1684X currently does not support bilinear interpolation.


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

            int dst_h = 1080;
            int dst_w = 1920;
            bm_dev_request(&handle, 0);
            bmcv_perspective_image_matrix matrix_image;
            matrix_image.matrix_num = 1;
            std::shared_ptr<bmcv_perspective_matrix> matrix_data
                    = std::make_shared<bmcv_perspective_matrix>();
            matrix_image.matrix = matrix_data.get();

            matrix_image.matrix->m[0] = 0.529813;
            matrix_image.matrix->m[1] = -0.806194;
            matrix_image.matrix->m[2] = 1000.000;
            matrix_image.matrix->m[3] = 0.193966;
            matrix_image.matrix->m[4] = -0.019157;
            matrix_image.matrix->m[5] = 300.000;
            matrix_image.matrix->m[6] = 0.000180;
            matrix_image.matrix->m[7] = -0.000686;
            matrix_image.matrix->m[8] = 1.000000;

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

            bmcv_image_warp_perspective(handle, 1, &matrix_image, &src, &dst);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_dev_free(handle);

            return 0;
        }



