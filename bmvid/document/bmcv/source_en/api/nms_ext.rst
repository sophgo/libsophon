bmcv_nms_ext
==============

This interface is the generalized form of bmcv_nms.It supports Hard_NMS/Soft_NMS/Adaptive_NMS/SSD_NMS which is used to eliminate excessive object frames obtained by network calculation and find the best object frame.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_nms_ext(bm_handle_t     handle,
                         bm_device_mem_t input_proposal_addr,
                         int             proposal_size,
                         float           nms_threshold,
                         bm_device_mem_t output_proposal_addr,
                         int             topk,
                         float           score_threshold,
                         int             nms_alg,
                         float           sigma,
                         int             weighting_method,
                         float *         densities,
                         float           eta)

**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input_proposal_addr

  Input parameter. Input the address where the object box data is located, and input the object box data structure as face_rect_t. Please refer to the following data structure description for details. Users need to call bm_mem_from_system() to convert the data address to structure corresponding to bm_device_mem_t.

* int proposal_size

  Input parameter. The number of object frames.

* float nms_threshold

  Input parameter. The threshold value of the filtered object frame. The object frame whose score is less than the threshold value will be filtered out.

* bm_device_mem_t output_proposal_addr

  Output parameter. The address where the output object frame data is located, and the output object frame data structure is nms_proposal_t. Please refer to the following data structure description for details. Users need to call bm_mem_from_system() to convert the data address to structure corresponding to bm_device_mem_t.

* int topk

  Input parameter. It is not currently in use and is reserved for possible subsequent extensions.

* float score_threshold

  Input parameter. The minimum score threshold when using Soft_NMS or Adaptive_NMS. When the score is lower than this value, the box corresponding to the score will be filtered out.

* int nms_alg

  Input parameter. Selection of different NMS algorithms, including Hard_NMS/Soft_NMS/Adaptive_NMS/SSD_NMS.

* float sigma

  Input parameter. When using Soft_NMS or Adaptive_NMS, the parameters of Gaussian re-score function.

* int weighting_method

  Input parameter. When using Soft_NMS or Adaptive_NMS, re-score function options include linear weight and Gaussian weight. Optional parameters:

    .. code-block:: c

        typedef enum {
            LINEAR_WEIGHTING = 0,
            GAUSSIAN_WEIGHTING,
            MAX_WEIGHTING_TYPE
        } weighting_method_e;

The linear weight expression is as follows:

.. math::

     s_i =
     \begin{cases}
     s_i,  & {iou(\mathcal{M}, b_i)<N_t} \\
     s_i \times (1-iou(\mathcal{M},b_i)), & {iou(\mathcal{M}, b_i) \geq N_t}
     \end{cases}

Gaussian weight expression is as follows:

.. math::

     s_i = s_i \times e^{-iou(\mathcal{M}, \  b_i)^2/\sigma}

In the above two expressions, :math:`\mathcal{M}` represents the object frame with the largest current score, :math:`b_i`  represents the object frame with score lower than :math:`\mathcal{M}` , :math:`s_i`  represents the score value of the object frame with score lower than :math:`\mathcal{M}` and :math:`N_t`  represents the NMS threshold, :math:`\sigma`  corresponds to the parameter float sigma of this interface.


* float\* densities

  Input parameters. Adaptive-NMS density value.

* float eta

  Input parameter. SSD-NMS coefficient, used to adjust iou threshold.

**Return value:**

* BM_SUCCESS: success

* Other: failed

**Code example:**


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

        #define MAX_PROPOSAL_NUM (65535)
        typedef float bm_nms_data_type_t;

        typedef struct {
            float x1;
            float y1;
            float x2;
            float y2;
            float score;
        } face_rect_t;

        typedef struct nms_proposal {
            int          size;
            face_rect_t  face_rect[MAX_PROPOSAL_NUM];
            int          capacity;
            face_rect_t *begin;
            face_rect_t *end;
        } nms_proposal_t;

        typedef enum {
            LINEAR_WEIGHTING = 0,
            GAUSSIAN_WEIGHTING,
            MAX_WEIGHTING_TYPE
        } weighting_method_e;

        template <typename data_type>
        static bool generate_random_buf(std::vector<data_type> &random_buffer,
                                        int                     random_min,
                                        int                     random_max,
                                        int                     scale) {
            for (int i = 0; i < scale; i++) {
                data_type data_val = (data_type)(
                    random_min + (((float)((random_max - random_min) * i)) / scale));
                random_buffer.push_back(data_val);
            }
            std::random_shuffle(random_buffer.begin(), random_buffer.end());

            return false;
        }

         int main(int argc, char *argv[]) {
             unsigned int seed1 = 100;
            bm_nms_data_type_t  nms_threshold = 0.22;
            bm_nms_data_type_t  nms_score_threshold = 0.22;
            bm_nms_data_type_t  sigma               = 0.4;
            int  proposal_size       = 500;
            int  rand_loop_num       = 10;
            int  weighting_method    = GAUSSIAN_WEIGHTING;
            std::function<float(float, float)> weighting_func;
            int  nms_type = SOFT_NMS;  // ADAPTIVE NMS / HARD NMS / SOFT NMS
            const int soft_nms_total_types = MAX_NMS_TYPE - HARD_NMS - 1;

            for (int rand_loop_idx = 0;rand_loop_idx < (rand_loop_num * soft_nms_total_types);rand_loop_idx++) {
                for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
                    std::shared_ptr<Blob<face_rect_t>> proposal_rand =
                        std::make_shared<Blob<face_rect_t>>(MAX_PROPOSAL_NUM);
                    std::shared_ptr<nms_proposal_t> output_proposal =
                        std::make_shared<nms_proposal_t>();

                    std::vector<face_rect_t>        proposals_ref;
                    std::vector<face_rect_t>        nms_proposal;
                    std::vector<bm_nms_data_type_t> score_random_buf;
                    std::vector<bm_nms_data_type_t> density_vec;
                    std::shared_ptr<Blob<float>>    densities =
                        std::make_shared<Blob<float>>(proposal_size);
                    generate_random_buf<bm_nms_data_type_t>(
                        score_random_buf, 0, 1, 10000);
                    face_rect_t *proposal_rand_ptr = proposal_rand.get()->data;
                    float eta = ((float)(rand() % 10)) / 10;
                    for (int32_t i = 0; i < proposal_size; i++) {
                        proposal_rand_ptr[i].x1 =
                            ((bm_nms_data_type_t)(rand() % 100)) / 10;
                        proposal_rand_ptr[i].x2 = proposal_rand_ptr[i].x1
                            + ((bm_nms_data_type_t)(rand() % 100)) / 10;
                        proposal_rand_ptr[i].y1 =
                            ((bm_nms_data_type_t)(rand() % 100)) / 10;
                        proposal_rand_ptr[i].y2 = proposal_rand_ptr[i].y1
                            + ((bm_nms_data_type_t)(rand() % 100)) / 10;
                        proposal_rand_ptr[i].score = score_random_buf[i];
                        proposals_ref.push_back(proposal_rand_ptr[i]);
                        densities.get()->data[i] = ((float)(rand() % 100)) / 100;
                    }
                    assert(proposal_size <= MAX_PROPOSAL_NUM);
                    if (weighting_method == LINEAR_WEIGHTING) {
                        weighting_func = linear_weighting;
                    } else if (weighting_method == GAUSSIAN_WEIGHTING) {
                        weighting_func = gaussian_weighting;
                    } else {
                        std::cout << "weighting_method error: " << weighting_method
                                    << std::endl;
                    }
                    bmcv_nms_ext(handle,
                                    bm_mem_from_system(proposal_rand.get()->data),
                                    proposal_size,
                                    nms_threshold,
                                    bm_mem_from_system(output_proposal.get()),
                                    1,
                                    nms_score_threshold,
                                    nms_type,
                                    sigma,
                                    weighting_method,
                                    densities.get()->data,
                                    eta);
                }
            }

            return 0;
         }



**Note:**

The maximum number of proposal that can be entered by this API is 1024.
