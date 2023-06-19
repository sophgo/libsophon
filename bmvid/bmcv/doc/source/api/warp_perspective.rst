bmcv_image_warp_perspective
===========================


该接口实现图像的透射变换，又称投影变换或透视变换。透射变换将图片投影到一个新的视平面，是一种二维坐标 (:math:`x_0` , :math:`y_0`) 到二维坐标(:math:`x` , :math:`y`)的非线性变换，该接口的实现是针对输出图像的每一个像素点坐标得到对应输入图像的坐标，然后构成一幅新的图像，其数学表达式形式如下：

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

对应的齐次坐标矩阵表示形式为：


.. math::

     \left[\begin{matrix} x' \\ y' \\ w' \end{matrix} \right]=\left[\begin{matrix} a_1&b_1&c_1 \\ a_2&b_2&c_2 \\ a_3&b_3&c_3 \end{matrix} \right]\times \left[\begin{matrix} x \\ y \\ 1 \end{matrix} \right]

.. math::

    \left\{
    \begin{array}{c}
    x_0 = x' / w'   \\
    y_0 = y' / w'   \\
    \end{array}
    \right.



坐标变换矩阵是一个 9 点的矩阵（通常c3 = 1），利用该变换矩阵可以从输出图像坐标推导出对应的输入原图坐标，该变换矩阵可以通过输入输出图像对应的 4 个点的坐标来获取。

为了更方便地完成透射变换，该库提供了两种形式的接口供用户使用：一种是用户提供变换矩阵给接口作为输入; 另一种接口是提供输入图像中 4 个点的坐标作为输入，适用于将一个不规则的四边形透射为一个与输出大小相同的矩形，如下图所示，可以将输入图像A'B'C'D'映射为输出图像ABCD，用户只需要提供输入图像中A'、B'、C'、D'四个点的坐标即可，该接口内部会根据这四个的坐标和输出图像四个顶点的坐标自动计算出变换矩阵，从而完成该功能。

.. figure:: ./perspective.jpg
   :width: 1047px
   :height: 452px
   :scale: 50%
   :align: center



**接口形式一:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_perspective(
                bm_handle_t handle,
                int image_num,
                bmcv_perspective_image_matrix matrix[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );

其中，bmcv_perspective_matrix 定义了一个坐标变换矩阵，其顺序为 float m[9] = {a1, b1, c1, a2, b2, c2, a3, b3, c3}。
而 bmcv_perspective_image_matrix 定义了一张图片里面有几个变换矩阵，可以实现对一张图片里的多个小图进行透射变换。

    .. code-block:: c

        typedef struct bmcv_perspective_matrix_s{
                float m[9];
        } bmcv_perspective_matrix;

        typedef struct bmcv_perspective_image_matrix_s{
                bmcv_perspective_matrix *matrix;
                int matrix_num;
        } bmcv_perspective_image_matrix;



**接口形式二:**

    .. code-block:: c

        bm_status_t bmcv_image_warp_perspective_with_coordinate(
                bm_handle_t handle,
                int image_num,
                bmcv_perspective_image_coordinate coord[4],
                bm_image* input,
                bm_image* output,
                int use_bilinear = 0
        );

其中，bmcv_perspective_coordinate 定义了四边形四个顶点的坐标，按照左上、右上、左下、右下的顺序存储。
而 bmcv_perspective_image_coordinate 定义了一张图片里面有几组四边形的坐标，可以实现对一张图片里的多个小图进行透射变换。

    .. code-block:: c

        typedef struct bmcv_perspective_coordinate_s{
                int x[4];
                int y[4];
        } bmcv_perspective_coordinate;

        typedef struct bmcv_perspective_image_coordinate_s{
                bmcv_perspective_coordinate *coordinate;
                int coordinate_num;
        } bmcv_perspective_image_coordinate;


**输入参数说明**

* bm_handle_t handle

输入参数。输入的 bm_handle 句柄。

* int image_num

输入参数。输入图片数，最多支持 4。

* bmcv_perspective_image_matrix matrix[4]

输入参数。每张图片对应的变换矩阵数据结构，最多支持 4 张图片。

* bmcv_perspective_image_coordinate coord[4]

输入参数。每张图片对应的四边形坐标信息，最多支持 4 张图片。

* bm_image\* input

输入参数。输入 bm_image，对于 1N 模式，最多 4 个 bm_image，对于 4N 模式，最多一个 bm_image。

* bm_image\* output

输出参数。输出 bm_image，外部需要调用 bmcv_image_create 创建，建议用户调用 bmcv_image_attach 来分配 device memory。如果用户不调用 attach，则内部分配 device memory。对于输出 bm_image，其数据类型和输入一致，即输入是 4N 模式，则输出也是 4N 模式,输入 1N 模式，输出也是 1N 模式。所需要的 bm_image 大小是所有图片的变换矩阵之和。比如输入 1 个 4N 模式的 bm_image，4 张图片的变换矩阵数目为【3,0,13,5】，则共有变换矩阵 3+0+13+5=21，由于输出是 4N 模式，则需要(21+4-1)/4=6 个 bm_image 的输出。

* int use_bilinear

输入参数。是否使用 bilinear 进行插值，若为 0 则使用 nearest 插值，若为 1 则使用 bilinear 插值，默认使用 nearest 插值。选择 nearest 插值的性能会优于 bilinear，因此建议首选 nearest 插值，除非对精度有要求时可选择使用 bilinear 插值。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项**

1. 该接口要求输出图像的所有坐标点都能在输入的原图中找到对应的坐标点，不能超出原图大小，建议优先使用接口二，可以自动满足该条件。

2. 该接口所支持的 image_format 包括：

  * FORMAT_BGR_PLANAR

  * FORMAT_RGB_PLANAR

3. 该接口所支持的 data_type 包括：

  * DATA_TYPE_EXT_1N_BYTE

  * DATA_TYPE_EXT_4N_BYTE

4. 该接口的输入以及输出 bm_image 均支持带有 stride。

5. 要求该接口输入 bm_image 的 width、height、image_format 以及 data_type 必须保持一致。

6. 要求该接口输出 bm_image 的 width、height、image_format、data_type 以及 stride 必须保持一致。


**代码示例**

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



