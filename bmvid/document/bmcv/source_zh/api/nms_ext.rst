bmcv_nms_ext
==============

该接口是bmcv_nms接口的广义形式，支持Hard_NMS/Soft_NMS/Adaptive_NMS/SSD_NMS，用于消除网络计算得到过多的物体框，并找到最佳物体框。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_nms_ext(
                    bm_handle_t handle,
                    bm_device_mem_t input_proposal_addr,
                    int proposal_size,
                    float nms_threshold,
                    bm_device_mem_t output_proposal_addr,
                    int topk,
                    float score_threshold,
                    int nms_alg,
                    float sigma,
                    int weighting_method,
                    float* densities,
                    float eta);


**参数说明:**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t input_proposal_addr

  输入参数。输入物体框数据所在地址，输入物体框数据结构为 face_rect_t,详见下面数据结构说明。需要调用 bm_mem_from_system()将数据地址转化成转化为 bm_device_mem_t 所对应的结构。

* int proposal_size

  输入参数。物体框个数。

* float nms_threshold

  输入参数。过滤物体框的阈值，分数小于该阈值的物体框将会被过滤掉。

* bm_device_mem_t output_proposal_addr

  输出参数。输出物体框数据所在地址，输出物体框数据结构为 nms_proposal_t，详见下面数据结构说明。需要调用 bm_mem_from_system() 将数据地址转化成转化为 bm_device_mem_t 所对应的结构。

* int topk

  输入参数。当前未使用，为后续可能的的扩展预留的接口。

* float score_threshold

  输入参数。当使用Soft_NMS或者Adaptive_NMS时，最低的score threshold。当score低于该值时，score所对应的框将被过滤掉。

* int nms_alg

  输入参数。不同的NMS算法的选择，包括 Hard_NMS/Soft_NMS/Adaptive_NMS/SSD_NMS。

* float sigma

  输入参数。当使用Soft_NMS或者Adaptive_NMS时，Gaussian re-score函数的参数。

* int weighting_method

  输入参数。当使用Soft_NMS或者Adaptive_NMS时，re-score函数选项：包括线性权值和Gaussian权值。可选参数：

    .. code-block:: c

        typedef enum {
            LINEAR_WEIGHTING = 0,
            GAUSSIAN_WEIGHTING,
            MAX_WEIGHTING_TYPE
        } weighting_method_e;

线性权值表达式如下：

.. math::

     s_i =
     \begin{cases}
     s_i,  & {iou(\mathcal{M}, b_i)<N_t} \\
     s_i \times (1-iou(\mathcal{M},b_i)), & {iou(\mathcal{M}, b_i) \geq N_t}
     \end{cases}

Gaussian权值表达式如下：

.. math::

     s_i = s_i \times e^{-iou(\mathcal{M}, \  b_i)^2/\sigma}

上面两个表达式中，:math:`\mathcal{M}` 表示当前score最大的物体框，:math:`b_i` 表示其他score比 :math:`\mathcal{M}` 低的物体框，:math:`s_i` 表示其他score比 :math:`\mathcal{M}` 低的物体框的score值，:math:`N_t` 表示NMS门限，:math:`\sigma` 对应本接口的参数 float sigma。

* float\* densities

  输入参数。Adaptive-NMS密度值。

* float eta

  输入参数。SSD-NMS系数，用于调整iou阈值。


**返回值:**

* BM_SUCCESS: 成功

* 其他: 失败


**代码示例:**

    .. code-block:: c

        #include <assert.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <algorithm>
        #include <functional>
        #include <iostream>
        #include <memory>
        #include <set>
        #include <string>
        #include <vector>
        #include <math.h>
        #include "bmcv_api.h"
        #include "bmcv_internal.h"
        #include "bmcv_common_bm1684.h"
        #include "bmcv_api_ext.h"

        int main()
        {
            float nms_threshold = 0.22;
            float nms_score_threshold = 0.22;
            float sigma = 0.4;
            int proposal_size = 500;
            int weighting_method = GAUSSIAN_WEIGHTING;
            int nms_type = SOFT_NMS; // ADAPTIVE NMS / HARD NMS / SOFT NMS
            face_rect_t* proposal_rand = (face_rect_t*)malloc(MAX_PROPOSAL_NUM * sizeof(face_rect_t));
            nms_proposal_t* output_proposal = (nms_proposal_t*)malloc(1 * sizeof(nms_proposal_t));
            float* densities = (float*)malloc(proposal_size * sizeof(float));
            float eta = ((float)(rand() % 10)) / 10;
            bm_handle_t handle;

            bm_dev_request(&handle, 0);
            for (int32_t i = 0; i < proposal_size; i++) {
                proposal_rand[i].x1 = ((float)(rand() % 100)) / 10;
                proposal_rand[i].x2 = proposal_rand[i].x1 + ((float)(rand() % 100)) / 10;
                proposal_rand[i].y1 = ((float)(rand() % 100)) / 10;
                proposal_rand[i].y2 = proposal_rand[i].y1  + ((float)(rand() % 100)) / 10;
                proposal_rand[i].score = ((float)(rand() % 100)) / 10;
                densities[i] = (float)rand() / (float)RAND_MAX;
            }

            bmcv_nms_ext(handle, bm_mem_from_system(proposal_rand), proposal_size, nms_threshold,
                        bm_mem_from_system(output_proposal), 1, nms_score_threshold,
                        nms_type, sigma, weighting_method, densities, eta);

            free(proposal_rand);
            free(output_proposal);
            free(densities);
            bm_dev_free(handle);
            return 0;
        }


**注意事项:**

该 api 可输入的最大 proposal 数为 1024。