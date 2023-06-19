#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"

#define ALL_MASK_IN_L2_MAX_SIZE (1400)
#define ALL_MASK_IN_L2_SOFT_NMS_MAX_SIZE (350)
#define SG_API_ID_NMS 49
#define DEVICE_MEM_DEL_OUTPUT(handle, raw_mem, new_mem)\
do{\
        if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){\
        BM_CHECK_RET(bm_sync_api(handle)); \
        BM_CHECK_RET(bm_memcpy_d2s(handle, bm_mem_get_system_addr(raw_mem), new_mem)); \
        bm_free_device(handle, new_mem); \
        }\
} while(0)

#define DEVICE_MEM_ASSIGN_OR_COPY(handle, raw_mem, need_copy, len, new_mem)\
  do{\
      if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){\
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff_byte(\
            handle, &new_mem, raw_mem, need_copy, \
            (len))); \
      }else{\
          new_mem = raw_mem; \
      }\
    }while(0)

#define DEVICE_MEM_NEW_BUFFER(handle, buffer_mem, len)\
  BM_CHECK_RET(bm_malloc_device_byte(handle, &buffer_mem, len));

#define DEVICE_MEM_DEL_BUFFER(handle, buffer_mem)\
  bm_free_device(handle, buffer_mem)

#define DEVICE_MEM_DEL_INPUT(handle, raw_mem, new_mem)\
    do{\
      if (bm_mem_get_type(raw_mem) == BM_MEM_TYPE_SYSTEM){\
        bm_free_device(handle, new_mem); \
      }\
    } while(0)

#define DEVICE_MEM_NEW_INPUT(handle, src, len, dst)\
    DEVICE_MEM_ASSIGN_OR_COPY(handle, src, true, len, dst)

#define DEVICE_MEM_NEW_OUTPUT(handle, src, len, dst)\
    DEVICE_MEM_ASSIGN_OR_COPY(handle, src, false, len, dst)

struct norm_data {
    unsigned int manti : 23;
    unsigned int exp : 8;
    unsigned int sign : 1;
};

typedef union {
    int       ival;
    float     fval;
    norm_data nval;
} IF_VAL_C;

template <typename T>
class Blob {
   public:
    T * data      = NULL;
    int elem_size = 0;
    int byte_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data      = nullptr;
            byte_size = 0;
            return;
        }
        data      = new T[size];
        elem_size = size;
        byte_size = size * sizeof(T);
    }
    ~Blob() {
        if (data) {
            delete[] data;
            elem_size = 0;
            byte_size = 0;
        }
    }
};

inline bool compareBBox(const face_rect_t &a, const face_rect_t &b) {
    return a.score > b.score;
}

static IF_VAL_C microp_nr(IF_VAL_C x) {
    float c_0p5  = 0.5;
    float c_1p5  = 1.5;
    float half_x = c_0p5 * x.fval;
    x.ival       = 0x5f3759df - (x.ival >> 1);
    for (int i = 0; i < 3; i++)
        x.fval = x.fval * (c_1p5 - (half_x * x.fval * x.fval));
    return x;
}

static int microop_normb(float x) {
    IF_VAL_C work_b;
    int      exp_inc;
    work_b.fval = x;
    exp_inc     = work_b.nval.manti > 0x2aaaab && !work_b.nval.sign;
    return work_b.nval.exp + exp_inc;
}

static float div_fnc(float x, float y) {
    IF_VAL_C srca, srcb;
    srca.fval      = x;
    srcb.fval      = y;
    srca.nval.sign = srca.nval.sign ^ srcb.nval.sign;
    srcb.ival &= 0x7fffffff;
    srcb      = microp_nr(srcb);
    srcb.fval = srcb.fval * srcb.fval;
    return srca.fval * srcb.fval;
}

inline bool pair_compare_bbox(const std::pair<face_rect_t, int> &a,
                              const std::pair<face_rect_t, int> &b) {
    return a.first.score > b.first.score;
}

static int soft_nms_post_process(const std::vector<face_rect_t> &proposals,
                                 std::vector<std::vector<float>> overlap_vec,
                                 std::vector<std::vector<float>> weithting_res,
                                 float                           nms_threshold,
                                 float                     nms_score_threshold,
                                 std::vector<face_rect_t> &nmsProposals,
                                 const std::vector<float>  density_vec,
                                 int nms_type = SOFT_NMS) {
    if (proposals.empty()) {
        nmsProposals.clear();
        return BM_ERR_FAILURE;
    }
    int   num_bbox               = proposals.size();
    float adaptive_nms_threshold = nms_threshold;
    std::vector<std::pair<face_rect_t, int>> pair_vec;
    for (int i = 0; i < num_bbox; i++) {
        pair_vec.push_back(std::make_pair(proposals[i], i));
    }
    for (int select_idx = 0; select_idx < num_bbox; select_idx++) {
        std::stable_sort(pair_vec.begin(), pair_vec.end(), pair_compare_bbox);
        int select_origin_idx = pair_vec[select_idx].second;
        if (nms_type == ADAPTIVE_NMS) {
            #ifdef __linux__
            adaptive_nms_threshold =
                std::max(nms_threshold, density_vec[select_origin_idx]);
            #else
            adaptive_nms_threshold =
                (std::max)(nms_threshold, density_vec[select_origin_idx]);
            #endif
        } else {
            adaptive_nms_threshold = nms_threshold;
        }
        for (int i = select_idx + 1; i < num_bbox; ++i) {
            int origin_idx = pair_vec[i].second;
            // Union method
            float overlap = overlap_vec[select_origin_idx][origin_idx];
            if (overlap == 0) {
                continue;
            }
            if (overlap >= adaptive_nms_threshold) {
                pair_vec[i].first.score *=
                    weithting_res[select_origin_idx][origin_idx];
            }
        }
    }
    for (auto iter = pair_vec.begin(); iter != pair_vec.end(); iter++) {
        if (iter->first.score >= nms_score_threshold) {
            nmsProposals.push_back(iter->first);
        }
    }

    return BM_SUCCESS;
}

static int ssd_nms_post_process(const std::vector<face_rect_t>  proposals,
                                std::vector<std::vector<float>> overlap_vec,
                                float                           nms_threshold,
                                float                           eta,
                                std::vector<face_rect_t> &      nmsProposals) {
    if (proposals.empty()) {
        nmsProposals.clear();
        return BM_ERR_FAILURE;
    }
    int                                      num_bbox = proposals.size();
    float                                    adaptive_threshold = nms_threshold;
    std::vector<std::pair<face_rect_t, int>> pair_vec;
    std::vector<int>                         kept_indx_vec;
    for (int i = 0; i < num_bbox; i++) {
        pair_vec.push_back(std::make_pair(proposals[i], i));
    }
    kept_indx_vec.clear();
    std::stable_sort(pair_vec.begin(), pair_vec.end(), pair_compare_bbox);
    while (pair_vec.size() != 0) {
        bool keep       = true;
        int  origin_idx = pair_vec.front().second;
        for (unsigned int k = 0; k < nmsProposals.size(); ++k) {
            int select_origin_idx = kept_indx_vec[k];
            if (keep) {
                // Union method
                float overlap = overlap_vec[select_origin_idx][origin_idx];
                if (overlap == 0) {
                    continue;
                }
                keep = overlap <= adaptive_threshold;
            } else {
                break;
            }
        }
        if (keep) {
            nmsProposals.push_back(pair_vec.front().first);
            kept_indx_vec.push_back(pair_vec.front().second);
        }
        pair_vec.erase(pair_vec.begin());
        if (keep && eta < 1 && adaptive_threshold > 0.5) {
            adaptive_threshold *= eta;
        }
    }

    return BM_SUCCESS;
}


void nms_cpu(const std::vector<face_rect_t> &proposals,
             const float                     nms_threshold,
             std::vector<face_rect_t> &      nmsProposals) {
    printf("nms_cpu\n");
    if (proposals.empty()) {
        nmsProposals.clear();
        return;
    }

    std::vector<face_rect_t> bboxes = proposals;
    // std::sort(bboxes.begin(), bboxes.end(), compareBBox);
    std::stable_sort(bboxes.begin(), bboxes.end(), compareBBox);
    int              select_idx = 0;
    int              num_bbox   = bboxes.size();
    std::vector<int> mask_merged(num_bbox, 0);
    bool             all_merged = false;
    while (!all_merged) {
        while (select_idx < num_bbox && 1 == mask_merged[select_idx])
            ++select_idx;
        if (select_idx == num_bbox) {
            all_merged = true;
            continue;
        }
        nmsProposals.push_back(bboxes[select_idx]);
        mask_merged[select_idx] = 1;
        face_rect_t select_bbox = bboxes[select_idx];
        float       area1       = (select_bbox.x2 - select_bbox.x1 + 1) *
                      (select_bbox.y2 - select_bbox.y1 + 1);
        ++select_idx;
        for (int i = select_idx; i < num_bbox; ++i) {
            if (mask_merged[i] == 1) {
                continue;
            }
            face_rect_t &bbox_i = bboxes[i];
            #ifdef __linux__
            float        x      = std::max(select_bbox.x1, bbox_i.x1);
            float        y      = std::max(select_bbox.y1, bbox_i.y1);
            float        w      = std::min(select_bbox.x2, bbox_i.x2) - x + 1;
            float        h      = std::min(select_bbox.y2, bbox_i.y2) - y + 1;
            #else
            float x = (std::max)(select_bbox.x1, bbox_i.x1);
            float y = (std::max)(select_bbox.y1, bbox_i.y1);
            float w = (std::min)(select_bbox.x2, bbox_i.x2) - x + 1;
            float h = (std::min)(select_bbox.y2, bbox_i.y2) - y + 1;
            #endif
            if (w <= 0 || h <= 0)
                continue;
            float area2 =
                (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
            float area_intersect = w * h;
            // Union method
            if (div_fnc(area_intersect, (area1 + area2 - area_intersect)) >
                nms_threshold)
                mask_merged[i] = 1;
        }
    }
}

static int cpu_soft_nms(const std::vector<face_rect_t> &   proposals,
                        const float                        nms_threshold,
                        const float                        nms_score_threshold,
                        const float                        sigma,
                        std::function<float(float, float)> weighting_func,
                        std::vector<face_rect_t> &         nmsProposals,
                        std::vector<float>                 density_vec,
                        bool if_adaptive = false) {
    if (proposals.empty()) {
        nmsProposals.clear();
        return -1;
    }
    int   num_bbox               = proposals.size();
    float adaptive_nms_threshold = nms_threshold;
    // std::sort(bboxes.begin(), bboxes.end(), compareBBox);
    std::vector<std::pair<face_rect_t, float>> pair_vec;
    for (int i = 0; i < num_bbox; i++) {
        if (if_adaptive) {
            pair_vec.push_back(std::make_pair(proposals[i], density_vec[i]));
        } else {
            pair_vec.push_back(std::make_pair(proposals[i], nms_threshold));
        }
    }
    for (int select_idx = 0; select_idx < num_bbox; select_idx++) {
        // std::stable_sort(bboxes.begin(), bboxes.end(), compareBBox);
        std::stable_sort(pair_vec.begin(), pair_vec.end(), pair_compare_bbox);
        face_rect_t select_bbox = pair_vec[select_idx].first;
        float       area1       = (select_bbox.x2 - select_bbox.x1 + 1) *
                      (select_bbox.y2 - select_bbox.y1 + 1);
        #ifdef __linux__
        adaptive_nms_threshold =
            std::max(nms_threshold, pair_vec[select_idx].second);
        #else
        adaptive_nms_threshold =
            (std::max)(nms_threshold, pair_vec[select_idx].second);
        #endif
        for (int i = select_idx + 1; i < num_bbox; ++i) {
            face_rect_t &bbox_i = pair_vec[i].first;
            #ifdef __linux__
            float        x      = std::max(select_bbox.x1, bbox_i.x1);
            float        y      = std::max(select_bbox.y1, bbox_i.y1);
            float        w      = std::min(select_bbox.x2, bbox_i.x2) - x + 1;
            float        h      = std::min(select_bbox.y2, bbox_i.y2) - y + 1;
            #else
            float x = (std::max)(select_bbox.x1, bbox_i.x1);
            float y = (std::max)(select_bbox.y1, bbox_i.y1);
            float w = (std::min)(select_bbox.x2, bbox_i.x2) - x + 1;
            float h = (std::min)(select_bbox.y2, bbox_i.y2) - y + 1;
            #endif
            if (w <= 0 || h <= 0)
                continue;
            float area2 =
                (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
            float area_intersect = w * h;
            // Union method
            float overlap =
                div_fnc(area_intersect, (area1 + area2 - area_intersect));
            if (overlap >= adaptive_nms_threshold) {
                pair_vec[i].first.score *= weighting_func(overlap, sigma);
            }
        }
    }
    for (auto iter = pair_vec.begin(); iter != pair_vec.end(); iter++) {
        if (iter->first.score >= nms_score_threshold) {
            nmsProposals.push_back(iter->first);
        }
    }

    return 0;
}

static int cpu_ssd_nms(const std::vector<face_rect_t> proposals,
                       const float                    nms_threshold,
                       const float                    eta,
                       std::vector<face_rect_t> &     nmsProposals) {
    std::vector<face_rect_t> bboxes = proposals;
    // std::sort(bboxes.begin(), bboxes.end(), compareBBox);
    std::stable_sort(bboxes.begin(), bboxes.end(), compareBBox);
    float adaptive_threshold = nms_threshold;
    nmsProposals.clear();
    while (bboxes.size() != 0) {
        face_rect_t bbox_i = bboxes.front();
        bool        keep   = true;
        float area2 = (bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1);
        for (unsigned int k = 0; k < nmsProposals.size(); ++k) {
            if (keep) {
                face_rect_t kept_bbox = nmsProposals[k];
                #ifdef __linux__
                float       x         = std::max(kept_bbox.x1, bbox_i.x1);
                float       y         = std::max(kept_bbox.y1, bbox_i.y1);
                float       w = std::min(kept_bbox.x2, bbox_i.x2) - x + 1;
                float       h = std::min(kept_bbox.y2, bbox_i.y2) - y + 1;
                #else
                float x = (std::max)(kept_bbox.x1, bbox_i.x1);
                float y = (std::max)(kept_bbox.y1, bbox_i.y1);
                float w = (std::min)(kept_bbox.x2, bbox_i.x2) - x + 1;
                float h = (std::min)(kept_bbox.y2, bbox_i.y2) - y + 1;
                #endif
                if (w <= 0 || h <= 0)
                    continue;
                float area_intersect = w * h;
                float area1          = (kept_bbox.x2 - kept_bbox.x1 + 1) *
                              (kept_bbox.y2 - kept_bbox.y1 + 1);
                // Union method
                float overlap =
                    div_fnc(area_intersect, (area1 + area2 - area_intersect));
                keep = overlap <= adaptive_threshold;
            } else {
                break;
            }
        }
        if (keep) {
            nmsProposals.push_back(bboxes.front());
        }
        bboxes.erase(bboxes.begin());
        if (keep && eta < 1 && adaptive_threshold > 0.5) {
            adaptive_threshold *= eta;
        }
    }

    return 0;
}

static float linear_weighting(float overlap, float sigma) {
    UNUSED(sigma);

    return (1 - overlap);
}

static float gaussian_weighting(float overlap, float sigma) {
    return exp(-1 * ((overlap * overlap) / sigma));
}

bm_status_t bmcv_nms_1684x(bm_handle_t     handle,
                     bm_device_mem_t input_proposal_addr,
                     int             proposal_size,
                     float           nms_threshold,
                     int             nms_type,
                     bm_device_mem_t output_proposal_addr) {

    sg_api_nms_t arg;
    bm_device_mem_t input_proposal_buf_device;
    bm_device_mem_t score_buf_device;
    bm_device_mem_t output_buf_device;
    bm_device_mem_t all_mask_buf_device;
    bm_device_mem_t iou_buf_device;
    bm_device_mem_t score_addr = bm_mem_from_system(nullptr);
    int hard_nms_version = 0;
    if (hard_nms_version == 1 && nms_type == 0){
        DEVICE_MEM_NEW_INPUT(handle, input_proposal_addr, sizeof(float) * 4 * proposal_size, input_proposal_buf_device);
        DEVICE_MEM_NEW_INPUT(handle, score_addr, sizeof(float) * 1 * proposal_size, score_buf_device);
    }
    if (handle == NULL) {
        BMCV_ERR_LOG("Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (proposal_size < MIN_PROPOSAL_NUM) {
        BMCV_ERR_LOG("proposal_size not support:%d\r\n", proposal_size);

        return BM_NOT_SUPPORTED;
    }
    if (proposal_size > MAX_PROPOSAL_NUM) {
        face_rect_t *input_proposals = NULL;
        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_DEVICE) {
            input_proposals = new face_rect_t[proposal_size];
            if (BM_SUCCESS !=
                bm_memcpy_d2s(handle, input_proposals, input_proposal_addr)) {
                BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
                delete[] input_proposals;
                return BM_ERR_FAILURE;
            }
        } else {
            input_proposals =
                (face_rect_t *)bm_mem_get_system_addr(input_proposal_addr);
        }

        nms_proposal_t *output_proposals = NULL;
        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_DEVICE) {
            output_proposals = new nms_proposal_t;
        } else {
            output_proposals =
                (nms_proposal_t *)bm_mem_get_system_addr(output_proposal_addr);
        }
        std::vector<face_rect_t> proposals;
        for (int i = 0; i < proposal_size; i++) {
            face_rect_t rect;
            rect.x1    = input_proposals[i].x1;
            rect.y1    = input_proposals[i].y1;
            rect.x2    = input_proposals[i].x2;
            rect.y2    = input_proposals[i].y2;
            rect.score = input_proposals[i].score;
            proposals.push_back(rect);
        }

        std::vector<face_rect_t> nms_proposal;
        nms_cpu(proposals, nms_threshold, nms_proposal);

        output_proposals->size = nms_proposal.size();
        memcpy(output_proposals->face_rect,
               nms_proposal.data(),
               output_proposals->size * sizeof(face_rect_t));
        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_DEVICE) {
            if (BM_SUCCESS !=
                bm_memcpy_s2d(handle, output_proposal_addr, output_proposals)) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

                return BM_ERR_FAILURE;
            }
            delete output_proposals;
        }

        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_DEVICE) {
            delete[] input_proposals;
        }

        return BM_SUCCESS;
    }

    if (proposal_size <= MAX_PROPOSAL_NUM) {
        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS !=
                bm_malloc_device_byte(handle,
                                   &input_proposal_buf_device,
                                    sizeof(face_rect_t) * proposal_size)) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
                goto err0;
            }
            if (BM_SUCCESS !=
                bm_memcpy_s2d(handle,
                            input_proposal_buf_device,
                            bm_mem_get_system_addr(input_proposal_addr))) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
                goto err1;
            }
        } else {
            input_proposal_buf_device = input_proposal_addr;
        }
        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                    &output_buf_device,
                                                    sizeof(nms_proposal_t))) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

                goto err1;
            }
        } else {
            output_buf_device = output_proposal_addr;
        }
        arg.input_proposal_addr = bm_mem_get_device_addr(input_proposal_buf_device);
        arg.output_proposal_addr = bm_mem_get_device_addr(output_buf_device);
        arg.proposal_size = proposal_size;
        arg.nms_threshold = nms_threshold;
        arg.nms_type = nms_type;
        arg.score_threshold = 0;
        arg.iou_addr = 0;
        arg.sigma = 0;
        arg.weighting_method = 0;
        arg.eta = 0;
        arg.hard_nms_version = 1;
        arg.keep_top_k = proposal_size;
        if (hard_nms_version == 0 && proposal_size > ALL_MASK_IN_L2_MAX_SIZE) {
            if (nms_type != HARD_NMS) {
                if (BM_SUCCESS !=
                    bm_malloc_device_byte(handle, &all_mask_buf_device,
                                        (((proposal_size + EU_NUM - 1) / EU_NUM)) * EU_NUM * 4
                                        * (((proposal_size + EU_NUM - 1) / EU_NUM)) * EU_NUM * 4)) {
                    BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
                    goto err2;
                }
            } else {
                if (BM_SUCCESS !=
                    bm_malloc_device_byte(handle, &all_mask_buf_device,
                                        (((proposal_size + EU_NUM - 1) / EU_NUM)) * EU_NUM
                                        * (((proposal_size + EU_NUM - 1) / EU_NUM)) * EU_NUM)) {
                    BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
                    goto err2;
                }
            }
            arg.all_mask_addr = bm_mem_get_device_addr(all_mask_buf_device);
        }
        if (nms_type == 0 && hard_nms_version == 1){
            arg.hard_nms_version = 1;
            arg.keep_top_k = arg.keep_top_k;
            arg.score_threshold = 0;
            DEVICE_MEM_NEW_BUFFER(handle, iou_buf_device, proposal_size * sizeof(float));
            arg.iou_addr = bm_mem_get_device_addr(iou_buf_device);
            arg.all_mask_addr = bm_mem_get_device_addr(score_buf_device);
        } else {
            arg.hard_nms_version = 0;
        }
        if(BM_SUCCESS != bm_kernel_main_launch(handle, SG_API_ID_NMS, (unsigned char *)&arg, sizeof(sg_api_nms_t))){
            BMCV_ERR_LOG("nms sync api error\r\n");
            goto err3;
        }
        // if (BM_SUCCESS !=
        //     bm_send_api(handle, SG_API_ID_NMS, (unsigned char *)&arg, sizeof(sg_api_nms_t))) {
        //         BMCV_ERR_LOG("nms send apt error\r\n");
        //         goto err3;
        //     }

        // if (BM_SUCCESS != bm_sync_api(handle)) {
        //     BMCV_ERR_LOG("nms sync api error\r\n");
        //     goto err3;
        // }

        if (hard_nms_version == 0 && proposal_size > ALL_MASK_IN_L2_MAX_SIZE) {
            bm_free_device(handle, all_mask_buf_device);
        }

        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS !=
                bm_memcpy_d2s(handle,
                            bm_mem_get_system_addr(output_proposal_addr),
                            output_buf_device)) {
                BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

                goto err3;
            }
            bm_free_device(handle, output_buf_device);
        }
        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_proposal_buf_device);
        }
        if (nms_type == 0 && hard_nms_version == 1){
            DEVICE_MEM_DEL_BUFFER(handle, iou_buf_device);
            DEVICE_MEM_DEL_INPUT(handle, score_addr, score_buf_device);
        }

        return BM_SUCCESS;

    err3:
        if (proposal_size > ALL_MASK_IN_L2_MAX_SIZE) {
            bm_free_device(handle, all_mask_buf_device);
        }

    err2:
        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, output_buf_device);
        }

    err1:
        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_proposal_buf_device);
        }

    err0:
        return BM_ERR_FAILURE;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_nms(bm_handle_t     handle,
                     bm_device_mem_t input_proposal_addr,
                     int             proposal_size,
                     float           nms_threshold,
                     bm_device_mem_t output_proposal_addr) {
    if (handle == NULL) {
        BMCV_ERR_LOG("Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    unsigned int chipid;
    if (BM_SUCCESS != bm_get_chipid(handle, &chipid)) {
        BMCV_ERR_LOG("bmcv_nms_send_api get chipid error\r\n");
        return BM_ERR_FAILURE;
    }
    if (chipid == BM1684X){
        return bmcv_nms_1684x(
                handle,
                input_proposal_addr,
                proposal_size,
                nms_threshold,
                HARD_NMS,
                output_proposal_addr);

    }else{
        if (proposal_size < MIN_PROPOSAL_NUM) {
            BMCV_ERR_LOG("proposal_size not support:%d\r\n", proposal_size);

            return BM_NOT_SUPPORTED;
        }
        if (proposal_size > MAX_PROPOSAL_NUM) {
            face_rect_t *input_proposals = NULL;
            if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_DEVICE) {
                input_proposals = new face_rect_t[proposal_size];
                if (BM_SUCCESS !=
                    bm_memcpy_d2s(handle, input_proposals, input_proposal_addr)) {
                    BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
                    delete[] input_proposals;
                    return BM_ERR_FAILURE;
                }
            } else {
                input_proposals =
                    (face_rect_t *)bm_mem_get_system_addr(input_proposal_addr);
            }

            nms_proposal_t *output_proposals = NULL;
            if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_DEVICE) {
                output_proposals = new nms_proposal_t;
            } else {
                output_proposals =
                    (nms_proposal_t *)bm_mem_get_system_addr(output_proposal_addr);
            }

            std::vector<face_rect_t> proposals;
            for (int i = 0; i < proposal_size; i++) {
                face_rect_t rect;
                rect.x1    = input_proposals[i].x1;
                rect.y1    = input_proposals[i].y1;
                rect.x2    = input_proposals[i].x2;
                rect.y2    = input_proposals[i].y2;
                rect.score = input_proposals[i].score;
                proposals.push_back(rect);
            }

            std::vector<face_rect_t> nms_proposal;
            nms_cpu(proposals, nms_threshold, nms_proposal);

            output_proposals->size = nms_proposal.size();
            memcpy(output_proposals->face_rect,
                nms_proposal.data(),
                output_proposals->size * sizeof(face_rect_t));

            if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_DEVICE) {
                if (BM_SUCCESS !=
                    bm_memcpy_s2d(handle, output_proposal_addr, output_proposals)) {
                    BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

                    return BM_ERR_FAILURE;
                }
                delete output_proposals;
            }

            if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_DEVICE) {
                delete[] input_proposals;
            }

            return BM_SUCCESS;
        }

        bm_api_cv_nms_t arg;
        bm_device_mem_t input_proposal_buf_device;
        bm_device_mem_t output_buf_device;

        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS !=
                bm_malloc_device_byte(handle,
                                    &input_proposal_buf_device,
                                    sizeof(face_rect_t) * proposal_size)) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

                goto err0;
            }
            if (BM_SUCCESS !=
                bm_memcpy_s2d(handle,
                            input_proposal_buf_device,
                            bm_mem_get_system_addr(input_proposal_addr))) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

                goto err1;
            }
        } else {
            input_proposal_buf_device = input_proposal_addr;
        }

        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                    &output_buf_device,
                                                    sizeof(nms_proposal_t))) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

                goto err1;
            }
        } else {
            output_buf_device = output_proposal_addr;
        }

        arg.input_proposal_addr = bm_mem_get_device_addr(input_proposal_buf_device);
        arg.proposal_size       = proposal_size;
        arg.nms_threshold       = nms_threshold;
        arg.output_proposal_addr = bm_mem_get_device_addr(output_buf_device);
        arg.all_mask_addr = 0;
        arg.score_threshold = 0;
        arg.iou_addr = 0;
        arg.sigma = 0;
        arg.nms_type = 0;
        arg.weighting_method = 0;
        arg.eta = 0;
        arg.hard_nms_version = 0;
        arg.keep_top_k = 0;

        if (BM_SUCCESS !=
            bm_send_api(handle,  BM_API_ID_CV_NMS, (uint8_t *)&arg, sizeof(arg))) {
            BMCV_ERR_LOG("nms send api error\r\n");
            goto err2;
        }
        if (BM_SUCCESS != bm_sync_api(handle)) {
            BMCV_ERR_LOG("nms sync api error\r\n");
            goto err2;
        }

        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS !=
                bm_memcpy_d2s(handle,
                            bm_mem_get_system_addr(output_proposal_addr),
                            output_buf_device)) {
                BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

                goto err2;
            }
            bm_free_device(handle, output_buf_device);
        }
        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_proposal_buf_device);
        }

        return BM_SUCCESS;

    err2:
        if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, output_buf_device);
        }
    err1:
        if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_proposal_buf_device);
        }
    err0:
        return BM_ERR_FAILURE;
    }
}

bm_status_t bmcv_soft_nms(bm_handle_t     handle,
                          bm_device_mem_t input_proposal_addr,
                          int             proposal_size,
                          float           nms_threshold,
                          float           nms_score_threshold,
                          float           sigma,
                          int             weighting_method,
                          bm_device_mem_t output_proposal_addr,
                          float *         densities = NULL,
                          int             nms_type  = SOFT_NMS,
                          float           eta       = 0) {
    if (handle == NULL) {
        BMCV_ERR_LOG("Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (proposal_size < MIN_PROPOSAL_NUM) {
        BMCV_ERR_LOG("proposal_size not support:%d\r\n", proposal_size);

        return BM_NOT_SUPPORTED;
    }
    bm_api_cv_soft_nms_t               arg;
    bm_device_mem_t                    input_proposal_buf_device;
    bm_device_mem_t                    overlap_output_device;
    bm_device_mem_t                    weighting_result_device;
    face_rect_t *                      input_proposals  = nullptr;
    nms_proposal_t *                   output_proposals = nullptr;
    std::shared_ptr<Blob<face_rect_t>> input_proposals_shared;
    std::shared_ptr<nms_proposal_t>    output_proposals_shared;
    std::vector<face_rect_t>           input_proposals_vec;
    std::shared_ptr<Blob<float>>       overlap_outputs =
        std::make_shared<Blob<float>>(ALIGN(proposal_size, 32) *
                                      ALIGN(proposal_size, 32));
    std::shared_ptr<Blob<float>> weighting_results =
        std::make_shared<Blob<float>>(ALIGN(proposal_size, 32) *
                                      ALIGN(proposal_size, 32));
    std::vector<std::vector<float>> overlap_output_vec(proposal_size);
    std::vector<std::vector<float>> weighting_res_vec(proposal_size);
    std::vector<face_rect_t>        nms_proposals;
    int                             overlap_sz =
        sizeof(float) * ALIGN(proposal_size, 32) * ALIGN(proposal_size, 32);
    std::function<float(float, float)> weighting_func;
    std::vector<float>                 density_vec;

    if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
        input_proposals =
            (face_rect_t *)bm_mem_get_system_addr(input_proposal_addr);
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &input_proposal_buf_device,
                                  sizeof(face_rect_t) * proposal_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          input_proposal_buf_device,
                          bm_mem_get_system_addr(input_proposal_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        input_proposals_shared =
            std::make_shared<Blob<face_rect_t>>(proposal_size);
        input_proposals = input_proposals_shared.get()->data;
        if (BM_SUCCESS !=
            bm_memcpy_d2s(handle, input_proposals, input_proposal_addr)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
            return BM_ERR_FAILURE;
        }
        input_proposal_buf_device = input_proposal_addr;
    }
    if (BM_SUCCESS !=
        bm_malloc_device_byte(handle, &overlap_output_device, overlap_sz)) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

        goto err1;
    }
    if (BM_SUCCESS !=
        bm_malloc_device_byte(handle, &weighting_result_device, overlap_sz)) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

        goto err2;
    }
    if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
        output_proposals =
            (nms_proposal_t *)bm_mem_get_system_addr(output_proposal_addr);
    } else {
        output_proposals_shared = std::make_shared<nms_proposal_t>();
        output_proposals        = output_proposals_shared.get();
    }
    for (int i = 0; i < proposal_size; i++) {
        input_proposals_vec.push_back(*(input_proposals + i));
    }
    if (nms_type == ADAPTIVE_NMS) {
        for (int i = 0; i < proposal_size; i++) {
            density_vec.push_back(densities[i]);
        }
    } else {
        for (int i = 0; i < proposal_size; i++) {
            density_vec.push_back(0);
        }
    }
    if (proposal_size > MAX_SOFT_SUPPORT_PROPOSAL_NUM) {
        if (weighting_method == LINEAR_WEIGHTING) {
            weighting_func = linear_weighting;
        } else {
            weighting_func = gaussian_weighting;
        }
        if (nms_type == SSD_NMS) {
            cpu_ssd_nms(input_proposals_vec, nms_threshold, eta, nms_proposals);
        } else {
            cpu_soft_nms(input_proposals_vec,
                         nms_threshold,
                         nms_score_threshold,
                         sigma,
                         weighting_func,
                         nms_proposals,
                         density_vec,
                         nms_type);
        }
    } else {
        arg.input_proposal_addr =
            bm_mem_get_device_addr(input_proposal_buf_device);
        arg.proposal_size       = proposal_size;
        arg.weighting_method    = weighting_method;
        arg.sigma               = sigma;
        arg.overlap_output_addr = bm_mem_get_device_addr(overlap_output_device);
        arg.weighting_res_output_addr =
            bm_mem_get_device_addr(weighting_result_device);

        if (BM_SUCCESS !=
            bm_send_api(
                handle,  BM_API_ID_CV_SOFT_NMS, (uint8_t *)&arg, sizeof(arg))) {
            BMCV_ERR_LOG("nms send api error\r\n");
            goto err3;
        }
        if (BM_SUCCESS != bm_sync_api(handle)) {
            BMCV_ERR_LOG("nms sync api error\r\n");
            goto err3;
        }
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        overlap_outputs.get()->data,
                                        overlap_output_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err3;
        }
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        weighting_results.get()->data,
                                        weighting_result_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err3;
        }
        for (int i = 0; i < proposal_size; i++) {
            for (int j = 0; j < proposal_size; j++) {
                overlap_output_vec[i].push_back(
                    *(overlap_outputs.get()->data + i * proposal_size + j));
                weighting_res_vec[i].push_back(
                    *(weighting_results.get()->data + i * proposal_size + j));
            }
        }
        if (nms_type == SSD_NMS) {
            if (ssd_nms_post_process(input_proposals_vec,
                                     overlap_output_vec,
                                     nms_threshold,
                                     eta,
                                     nms_proposals) != BM_SUCCESS) {
                BMCV_ERR_LOG("ssd_nms_post_process error\r\n");

                goto err3;
            }
        } else {
            if (soft_nms_post_process(input_proposals_vec,
                                      overlap_output_vec,
                                      weighting_res_vec,
                                      nms_threshold,
                                      nms_score_threshold,
                                      nms_proposals,
                                      density_vec,
                                      nms_type) != BM_SUCCESS) {
                BMCV_ERR_LOG("soft_nms_post_process error\r\n");

                goto err3;
            }
        }
    }
    output_proposals->size = nms_proposals.size();
    memcpy(output_proposals->face_rect,
           nms_proposals.data(),
           output_proposals->size * sizeof(face_rect_t));
    if (bm_mem_get_type(output_proposal_addr) == BM_MEM_TYPE_DEVICE) {
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle, output_proposal_addr, output_proposals)) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err3;
        }
    }
    bm_free_device(handle, overlap_output_device);
    bm_free_device(handle, weighting_result_device);
    if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_proposal_buf_device);
    }
    return BM_SUCCESS;

err3:
    bm_free_device(handle, weighting_result_device);
err2:
    bm_free_device(handle, overlap_output_device);
err1:
    if (bm_mem_get_type(input_proposal_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_proposal_buf_device);
    }
err0:
    return BM_ERR_FAILURE;
}

static bm_status_t bmcv_nms_send_api(bm_handle_t     handle,
                              bm_device_mem_t input_proposal_addr,
                              bm_device_mem_t output_proposal_addr,
                              int             nms_type,
                              int             proposal_size,
                              float           nms_threshold,
                              float           score_threshold,
                              float           sigma,
                              int weighting_method,
                              float * densities,
                              float eta) {
    unsigned int chipid;
    bm_device_mem_t input_proposal_buf_device;
    bm_device_mem_t output_buf_device;
    bm_device_mem_t all_mask_buf_device;
    std::vector<float> density_vec;

    if (proposal_size < MIN_PROPOSAL_NUM || proposal_size > MAX_SOFT_SUPPORT_PROPOSAL_NUM) {
        BMCV_ERR_LOG("proposal_size not support:%d\r\n", proposal_size);
        return BM_NOT_SUPPORTED;
    }

    if (BM_SUCCESS != bm_get_chipid(handle, &chipid)) {
        BMCV_ERR_LOG("bmcv_nms_send_api get chipid error\r\n");
        return BM_ERR_FAILURE;
    }

    if (chipid == BM1684X) {
        DEVICE_MEM_NEW_INPUT(handle, input_proposal_addr, sizeof(face_rect_t) * proposal_size, input_proposal_buf_device);
        DEVICE_MEM_NEW_OUTPUT(handle, output_proposal_addr, sizeof(nms_proposal_t), output_buf_device);
        if (nms_type == ADAPTIVE_NMS) {
            for (int i = 0; i < proposal_size; i++) {
                density_vec.push_back(densities[i]);
            }
        } else {
            for (int i = 0; i < proposal_size; i++) {
                density_vec.push_back(0);
            }
        }

        sg_api_nms_t arg;
        arg.input_proposal_addr = bm_mem_get_device_addr(input_proposal_buf_device);
        arg.proposal_size       = proposal_size;
        arg.nms_threshold       = nms_threshold;
        arg.nms_type            = nms_type;
        arg.output_proposal_addr = bm_mem_get_device_addr(output_buf_device);
        arg.score_threshold     = score_threshold;
        arg.weighting_method    = weighting_method;
        arg.sigma               = sigma;

        if (proposal_size > ALL_MASK_IN_L2_SOFT_NMS_MAX_SIZE){
            //alloc memory at ddr for all_mask
            DEVICE_MEM_NEW_BUFFER(handle, all_mask_buf_device, (((proposal_size + EU_NUM - 1) / EU_NUM))
                                                                * EU_NUM * 4
                                                                * (((proposal_size + EU_NUM - 1) / EU_NUM))
                                                                * EU_NUM * 4);

            arg.all_mask_addr = bm_mem_get_device_addr(all_mask_buf_device);
        }
        BM_CHECK_RET(bm_kernel_main_launch(handle, SG_API_ID_NMS, (unsigned char *)&arg, sizeof(sg_api_nms_t)));

        // BM_CHECK_RET(bm_send_api(handle, SG_API_ID_NMS, (unsigned char *)&arg, sizeof(sg_api_nms_t)));
        DEVICE_MEM_DEL_OUTPUT(handle, output_proposal_addr, output_buf_device);
        DEVICE_MEM_DEL_INPUT(handle, input_proposal_addr, input_proposal_buf_device);
        if (proposal_size > ALL_MASK_IN_L2_SOFT_NMS_MAX_SIZE) {
            DEVICE_MEM_DEL_BUFFER(handle, all_mask_buf_device);
        }

        return BM_SUCCESS;

    }else if (chipid == 0x1684){
        if (nms_type == HARD_NMS) {
            return bmcv_nms(handle,
                            input_proposal_addr,
                            proposal_size,
                            nms_threshold,
                            output_proposal_addr);
        } else if (nms_type == SOFT_NMS) {
            return bmcv_soft_nms(handle,
                        input_proposal_addr,
                        proposal_size,
                        nms_threshold,
                        score_threshold,
                        sigma,
                        weighting_method,
                        output_proposal_addr);
        } else if (nms_type == ADAPTIVE_NMS) {
            return bmcv_soft_nms(handle,
                        input_proposal_addr,
                        proposal_size,
                        nms_threshold,
                        score_threshold,
                        sigma,
                        weighting_method,
                        output_proposal_addr,
                        densities,
                        nms_type);
        } else if (nms_type == SSD_NMS) {
            return bmcv_soft_nms(handle,
                        input_proposal_addr,
                        proposal_size,
                        nms_threshold,
                        score_threshold,
                        sigma,
                        weighting_method,
                        output_proposal_addr,
                        densities,
                        nms_type,
                        eta);
        } else {
            printf("nms_type error\r\n");
            return BM_ERR_FAILURE;
        }
            return BM_ERR_FAILURE;
    }else{
            std::cout << "Not support!\n" << std::endl;
            return BM_ERR_FAILURE;
        }
}

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
                         float           eta) {
    UNUSED(topk);
    return bmcv_nms_send_api(handle,
                            input_proposal_addr,
                            output_proposal_addr,
                            nms_alg,
                            proposal_size,
                            nms_threshold,
                            score_threshold,
                            sigma,
                            weighting_method,
                            densities,
                            eta);
}
