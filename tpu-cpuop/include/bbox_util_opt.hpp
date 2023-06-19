#ifndef BMCPU_UTIL_BBOX_UTIL_OPT_H_
#define BMCPU_UTIL_BBOX_UTIL_OPT_H_

#include "bbox_util.hpp"
#include <algorithm>

namespace bmcpu {

typedef struct{
  float xmin;
  float ymin;
  float xmax;
  float ymax;
} NormalizedBBoxOpt;

inline float BBoxSizeOpt(const NormalizedBBoxOpt& bbox)
{
  float width  = bbox.xmax - bbox.xmin;
  float height = bbox.ymax - bbox.ymin;
  width = std::max(0.0f, bbox.xmax - bbox.xmin);
  height = std::max(0.0f, bbox.ymax - bbox.ymin);
  return width * height;
}


void GetLocPredictions_opt(const float* loc_data, const int num,
    const int num_preds_per_class, const int num_loc_classes,
    const bool share_location, NormalizedBBoxOpt* &loc_preds);

void GetConfidenceScores_opt(const float* conf_data,
    const int num, const int num_preds_per_class,
    const int num_classes, float* &conf_preds);

void ApplyNMSFast_opt(const NormalizedBBoxOpt* bboxes,
    const int bboxes_size, const float* bboxes_area,
    const float* scores, const float score_threshold,
    const float nms_threshold, const float eta, const int top_k,
    const int stride, vector<pair<float,int>>* indices);

void DecodeBBoxesAll_opt(const NormalizedBBoxOpt* all_loc_preds,
    const float* prior_bboxes, const float* prior_variances,
    const int num, const bool share_location, const int num_priors,
    const int num_loc_classes, const int background_label_id,
    const CodeType code_type, const bool variance_encoded_in_target,
    const bool clip, NormalizedBBoxOpt* &all_decode_bboxes,
    float* &all_decode_bboxes_size);

void GetPriorBBoxes_opt(const float* prior_data, const int num_priors,
    float* &prior_bboxes, float* &prior_variances);

template <class T>
void insert_sort(T begin, T end);

}  // namespace bmnetc

#endif  // bmnetc_UTIL_BBOX_UTIL_OPT_H_
