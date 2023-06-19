#include <algorithm>
#include <csignal>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bbox_util_opt.hpp"
#include "bbox_util.hpp"

#define USE_NEON

#if defined(__aarch64__) && defined(USE_NEON)
#include <arm_neon.h>
#define USE_NEON

#define c_exp_hi 88.3762626647949f
#define c_exp_lo -88.3762626647949f

#define c_cephes_LOG2EF 1.44269504088896341
#define c_cephes_exp_C1 0.693359375
#define c_cephes_exp_C2 -2.12194440e-4

#define c_cephes_exp_p0 1.9875691500E-4
#define c_cephes_exp_p1 1.3981999507E-3
#define c_cephes_exp_p2 8.3334519073E-3
#define c_cephes_exp_p3 4.1665795894E-2
#define c_cephes_exp_p4 1.6666665459E-1
#define c_cephes_exp_p5 5.0000001201E-1

#define NEON_EXP(in, out) \
{ \
  float32x4_t x, y, z, fx; \
  uint32x4_t mask; \
  int32x4_t  mm; \
  x = vminq_f32(in, vdupq_n_f32(c_exp_hi)); \
  x = vmaxq_f32(x, vdupq_n_f32(c_exp_lo)); \
  \
  /* express exp(x) as exp(g + n*log(2)) */ \
  fx = vmlaq_f32(vdupq_n_f32(0.5f), x, vdupq_n_f32(c_cephes_LOG2EF)); \
  \
  /* perform a floorf */ \
  z = vcvtq_f32_s32(vcvtq_s32_f32(fx)); \
  \
  /* if greater, substract 1 */ \
  mask = vcgtq_f32(z, fx); \
  mask = vandq_u32(mask, vreinterpretq_u32_f32(vdupq_n_f32(1))); \
  fx = vsubq_f32(z, vreinterpretq_f32_u32(mask)); \
  \
  z = vmulq_f32(fx, vdupq_n_f32(c_cephes_exp_C1)); \
  x = vsubq_f32(x, z); \
  x = vsubq_f32(x, vmulq_f32(fx, vdupq_n_f32(c_cephes_exp_C2))); \
  \
  y = vdupq_n_f32(c_cephes_exp_p0); \
  y = vmulq_f32(y, x); \
  y = vaddq_f32(y, vdupq_n_f32(c_cephes_exp_p1)); \
  y = vmulq_f32(y, x); \
  y = vaddq_f32(y, vdupq_n_f32(c_cephes_exp_p2)); \
  y = vmulq_f32(y, x); \
  y = vaddq_f32(y, vdupq_n_f32(c_cephes_exp_p3)); \
  y = vmulq_f32(y, x); \
  y = vaddq_f32(y, vdupq_n_f32(c_cephes_exp_p4)); \
  y = vmulq_f32(y, x); \
  y = vaddq_f32(y, vdupq_n_f32(c_cephes_exp_p5)); \
  \
  y = vmulq_f32(y, vmulq_f32(x, x)); \
  y = vaddq_f32(y, x); \
  y = vaddq_f32(y, vdupq_n_f32(1)); \
  \
  /* build 2^n */ \
  mm = vcvtq_s32_f32(fx); \
  mm = vaddq_s32(mm, vdupq_n_s32(0x7f)); \
  mm = vshlq_n_s32(mm, 23); \
  \
  out = vmulq_f32(y, vreinterpretq_f32_s32(mm)); \
}

#else
#if defined(USE_NEON)
#undef USE_NEON
#endif
#endif

namespace bmcpu {

void GetLocPredictions_opt(const float* loc_data, const int num,
    const int num_preds_per_class, const int num_loc_classes,
    const bool share_location, NormalizedBBoxOpt* &loc_preds)
{
  if (share_location) {
    BM_CHECK_EQ(num_loc_classes, 1);
  }
  if(num_loc_classes == 1) {
    loc_preds = (NormalizedBBoxOpt*)loc_data;
  } else {
    loc_preds = new NormalizedBBoxOpt[num * num_preds_per_class * num_loc_classes];
    for (int i = 0; i < num; ++i) {
      NormalizedBBoxOpt* label_bbox = loc_preds + i * num_preds_per_class * num_loc_classes;
      const float* p_data =  loc_data + i * num_preds_per_class * num_loc_classes * 4;
      for (int p = 0; p < num_preds_per_class; ++p) {
        int start_idx = p * num_loc_classes * 4;
        for (int c = 0; c < num_loc_classes; ++c) {
          NormalizedBBoxOpt& box = label_bbox[c * num_preds_per_class + p];
          box.xmin = p_data[start_idx + c * 4];
          box.ymin = p_data[start_idx + c * 4 + 1];
          box.xmax = p_data[start_idx + c * 4 + 2];
          box.ymax = p_data[start_idx + c * 4 + 3];
        }
      }
    }
  }
}

void GetConfidenceScores_opt(const float* conf_data, const int num,
    const int num_preds_per_class, const int num_classes, float*& conf_preds)
{
  conf_preds = new float[num * num_preds_per_class * num_classes];
  for (int i = 0; i < num; ++i) {
    float* label_scores = conf_preds + i * num_preds_per_class * num_classes;
    const float* conf = conf_data + i * num_preds_per_class * num_classes;
    for (int p = 0; p < num_preds_per_class; ++p) {
      for (int c = 0; c < num_classes; ++c) {
        label_scores[c * num_preds_per_class + p] = conf[p * num_classes + c];
      }
    }
  }
}

void GetMaxScoreIndex_opt(const float* scores, const int num, const float threshold,
    const int top_k, const int stride, vector<pair<float, int> >* score_index_vec)
{
  // Generate index score pairs.
  for (int i = 0; i < num; ++i) {
    float s = scores[i*stride];
    if (s > threshold) {
      score_index_vec->push_back(std::make_pair(s, i));
    }
  }

  // Sort the score pair according to the scores in descending order
  // insert_sort(score_index_vec->begin(), score_index_vec->end());
  std::stable_sort(score_index_vec->begin(), score_index_vec->end(),
                   SortScorePairDescend<int>);

  // Keep top_k scores if needed.
  if (top_k > -1 && top_k < score_index_vec->size()) {
    score_index_vec->resize(top_k);
  }
}

static void IntersectBBoxOpt(const NormalizedBBoxOpt& bbox1,
    const NormalizedBBoxOpt& bbox2, NormalizedBBoxOpt* intersect_bbox)
{
  if (bbox2.xmin > bbox1.xmax || bbox2.xmax < bbox1.xmin ||
      bbox2.ymin > bbox1.ymax || bbox2.ymax < bbox1.ymin) {
    // Return [0, 0, 0, 0] if there is no intersection.
    intersect_bbox->xmin = 0;
    intersect_bbox->ymin = 0;
    intersect_bbox->xmax = 0;
    intersect_bbox->ymax = 0;
  } else {
    intersect_bbox->xmin = std::max(bbox1.xmin, bbox2.xmin);
    intersect_bbox->ymin = std::max(bbox1.ymin, bbox2.ymin);
    intersect_bbox->xmax = std::min(bbox1.xmax, bbox2.xmax);
    intersect_bbox->ymax = std::min(bbox1.ymax, bbox2.ymax);
  }
}

static float JaccardOverlapOpt(const NormalizedBBoxOpt& bbox1,
    const float bbox1_size, const NormalizedBBoxOpt& bbox2,
    const float bbox2_size, const bool normalized = true)
{
  NormalizedBBoxOpt intersect_bbox;
  IntersectBBoxOpt(bbox1, bbox2, &intersect_bbox);
  float intersect_width, intersect_height;
  if (normalized) {
    intersect_width = intersect_bbox.xmax - intersect_bbox.xmin;
    intersect_height = intersect_bbox.ymax - intersect_bbox.ymin;
  } else {
    intersect_width = intersect_bbox.xmax - intersect_bbox.xmin + 1;
    intersect_height = intersect_bbox.ymax - intersect_bbox.ymin + 1;
  }
  if (intersect_width > 0 && intersect_height > 0) {
    float intersect_size = intersect_width * intersect_height;
    return intersect_size / (bbox1_size + bbox2_size - intersect_size);
  } else {
    return 0.;
  }
}

void ApplyNMSFast_opt(const NormalizedBBoxOpt* bboxes,
    const int bboxes_size, const float* bboxes_area,
    const float* scores, const float score_threshold,
    const float nms_threshold, const float eta, const int top_k,
    const int stride, vector<pair<float,int>>* indices)
{
  vector<pair<float, int> > score_index_vec;
  score_index_vec.reserve(bboxes_size);
  GetMaxScoreIndex_opt(scores, bboxes_size, score_threshold, top_k,
      stride,  &score_index_vec);

  float adaptive_threshold = nms_threshold;
  indices->clear();
  while (score_index_vec.size() != 0) {
    const int idx = score_index_vec.front().second;
    bool keep = true;
    for (int k = 0; k < indices->size(); ++k) {
      if (keep) {
        const int kept_idx = (*indices)[k].second;
        float overlap = JaccardOverlapOpt(bboxes[idx], bboxes_area[idx],
            bboxes[kept_idx], bboxes_area[kept_idx]);
        keep = overlap <= adaptive_threshold;
      } else {
        break;
      }
    }
    if (keep) {
      indices->push_back(score_index_vec.front());
    }
    score_index_vec.erase(score_index_vec.begin());
    if (keep && eta < 1 && adaptive_threshold > 0.5) {
      adaptive_threshold *= eta;
    }
  }
}

static void DecodeBBox_opt(const float* prior_bboxes,
    const float* prior_variances, const int num_priors,
    const CodeType code_type, const bool variance_encoded_in_target,
    const bool clip_bbox, const NormalizedBBoxOpt* bboxes,
    NormalizedBBoxOpt* decode_bboxes, float* bboxes_area)
{
  int i = 0;
#ifdef USE_NEON
  if(code_type == PriorBoxParameter_CodeType_CENTER_SIZE) {
    for(; i < (num_priors & 0xFFFFFFFC); i += 4) {
      const float* prior_bbox = prior_bboxes + 4 * i;
      const float* prior_variance = prior_variances + 4 * i;
      const NormalizedBBoxOpt* bbox = bboxes + i;
      NormalizedBBoxOpt* decode_bbox = decode_bboxes + i;

      float32x4x4_t box_xy  = vld4q_f32((const float*)prior_bbox);
      float32x4x4_t box_var = vld4q_f32((const float*)prior_variance);

      float32x4_t prior_width = vsubq_f32(box_xy.val[2], box_xy.val[0]);
      float32x4_t prior_height = vsubq_f32(box_xy.val[3], box_xy.val[1]);
      float32x4_t prior_ctx = vmulq_n_f32(vaddq_f32(box_xy.val[0], box_xy.val[2]), 0.5f);
      float32x4_t prior_cty = vmulq_n_f32(vaddq_f32(box_xy.val[1], box_xy.val[3]), 0.5f);

      box_xy = vld4q_f32((const float*)bbox);

      float32x4_t decode_ctx, decode_cty, decode_width, decode_height;
      if(variance_encoded_in_target) {
        decode_ctx = vaddq_f32(prior_ctx, vmulq_f32(box_xy.val[0], prior_width));
        decode_cty = vaddq_f32(prior_cty, vmulq_f32(box_xy.val[1], prior_height));
        NEON_EXP(box_xy.val[2], decode_width);
        decode_width = vmulq_f32(decode_width, prior_width);
        NEON_EXP(box_xy.val[3], decode_height);
        decode_height = vmulq_f32(decode_height, prior_height);
      } else {
        decode_ctx = vaddq_f32(prior_ctx, vmulq_f32(box_var.val[0], vmulq_f32(box_xy.val[0], prior_width)));
        decode_cty = vaddq_f32(prior_cty, vmulq_f32(box_var.val[1], vmulq_f32(box_xy.val[1], prior_height)));
        box_xy.val[2] = vmulq_f32(box_xy.val[2], box_var.val[2]);
        NEON_EXP(box_xy.val[2], decode_width);
        decode_width = vmulq_f32(decode_width, prior_width);

        box_xy.val[3] = vmulq_f32(box_xy.val[3], box_var.val[3]);
        NEON_EXP(box_xy.val[3], decode_height);
        decode_height = vmulq_f32(decode_height, prior_height);
      }

      decode_width  = vmulq_n_f32(decode_width, 0.5f);
      decode_height = vmulq_n_f32(decode_height, 0.5f);
      box_xy.val[0] = vsubq_f32(decode_ctx, decode_width);
      box_xy.val[1] = vsubq_f32(decode_cty, decode_height);
      box_xy.val[2] = vaddq_f32(decode_ctx, decode_width);
      box_xy.val[3] = vaddq_f32(decode_cty, decode_height);

      if(clip_bbox) {
        box_xy.val[0] = vminq_f32(vdupq_n_f32(1.0f), vmaxq_f32(vdupq_n_f32(0.0f), box_xy.val[0]));
        box_xy.val[1] = vminq_f32(vdupq_n_f32(1.0f), vmaxq_f32(vdupq_n_f32(0.0f), box_xy.val[1]));
        box_xy.val[2] = vminq_f32(vdupq_n_f32(1.0f), vmaxq_f32(vdupq_n_f32(0.0f), box_xy.val[2]));
        box_xy.val[3] = vminq_f32(vdupq_n_f32(1.0f), vmaxq_f32(vdupq_n_f32(0.0f), box_xy.val[3]));
      }

      decode_width = vmaxq_f32(vdupq_n_f32(0.0f), vsubq_f32(box_xy.val[2], box_xy.val[0]));
      decode_height = vmaxq_f32(vdupq_n_f32(0.0f), vsubq_f32(box_xy.val[3], box_xy.val[1]));

      vst4q_f32((float*)decode_bbox, box_xy);
      vst1q_f32(bboxes_area + i, vmulq_f32(decode_width, decode_height));
    }
  }
#endif
  for(; i < num_priors; ++i) {
    const float* prior_bbox = prior_bboxes + 4 * i;
    const float* prior_variance = prior_variances + 4 * i;
    const NormalizedBBoxOpt* bbox = bboxes + i;
    NormalizedBBoxOpt* decode_bbox = decode_bboxes + i;
    float decode_width, decode_height;

    if (code_type == PriorBoxParameter_CodeType_CORNER) {
      if (variance_encoded_in_target) {
        // variance is encoded in target, we simply need to add the offset
        // predictions.
        decode_bbox->xmin = prior_bbox[0] + bbox->xmin;
        decode_bbox->ymin = prior_bbox[1] + bbox->ymin;
        decode_bbox->xmax = prior_bbox[2] + bbox->xmax;
        decode_bbox->ymax = prior_bbox[3] + bbox->ymax;
      } else {
        // variance is encoded in bbox, we need to scale the offset accordingly.
        decode_bbox->xmin = prior_bbox[0] + prior_variance[0] * bbox->xmin;
        decode_bbox->ymin = prior_bbox[1] + prior_variance[1] * bbox->ymin;
        decode_bbox->xmax = prior_bbox[2] + prior_variance[2] * bbox->xmax;
        decode_bbox->ymax = prior_bbox[3] + prior_variance[3] * bbox->ymax;
      }
    } else if (code_type == PriorBoxParameter_CodeType_CENTER_SIZE) {
      float prior_width = prior_bbox[2] - prior_bbox[0];
      float prior_height = prior_bbox[3] - prior_bbox[1];
      float prior_center_x = (prior_bbox[0] + prior_bbox[2]) / 2.0f;
      float prior_center_y = (prior_bbox[1] + prior_bbox[3]) / 2.0f;

      float decode_center_x, decode_center_y;
      if (variance_encoded_in_target) {
        decode_center_x = bbox->xmin * prior_width + prior_center_x;
        decode_center_y = bbox->ymin * prior_height + prior_center_y;
        decode_width = exp(bbox->xmax) * prior_width;
        decode_height = exp(bbox->ymax) * prior_height;
      } else {
        decode_center_x = prior_variance[0] * bbox->xmin * prior_width + prior_center_x;
        decode_center_y = prior_variance[1] * bbox->ymin * prior_height + prior_center_y;
        decode_width    = exp(prior_variance[2] * bbox->xmax) * prior_width;
        decode_height   = exp(prior_variance[3] * bbox->ymax) * prior_height;
      }

      decode_bbox->xmin = decode_center_x - decode_width / 2.0f;
      decode_bbox->ymin = decode_center_y - decode_height / 2.0f;
      decode_bbox->xmax = decode_center_x + decode_width / 2.0f;
      decode_bbox->ymax = decode_center_y + decode_height / 2.0f;
    } else if (code_type == PriorBoxParameter_CodeType_CORNER_SIZE) {
      float prior_width = prior_bbox[2] - prior_bbox[0];
      float prior_height = prior_bbox[3] - prior_bbox[1];
      if (variance_encoded_in_target) {
        // variance is encoded in target, we simply need to add the offset
        // predictions.
        decode_bbox->xmin = prior_bbox[0] + bbox->xmin * prior_width;
        decode_bbox->ymin = prior_bbox[1] + bbox->ymin * prior_height;
        decode_bbox->xmax = prior_bbox[2] + bbox->xmax * prior_width;
        decode_bbox->ymax = prior_bbox[3] + bbox->ymax * prior_height;
      } else {
        // variance is encoded in bbox, we need to scale the offset accordingly.
        decode_bbox->xmin = prior_bbox[0] + prior_variance[0] * bbox->xmin * prior_width;
        decode_bbox->ymin = prior_bbox[1] + prior_variance[1] * bbox->ymin * prior_height;
        decode_bbox->xmax = prior_bbox[2] + prior_variance[2] * bbox->xmax * prior_width;
        decode_bbox->ymax = prior_bbox[3] + prior_variance[3] * bbox->ymax * prior_height;
      }
    } else {
      BM_LOG(FATAL) << "Unknown LocLossType.";
    }

    if (clip_bbox) {
      decode_bbox->xmin = std::max(std::min(decode_bbox->xmin, 1.f), 0.f);
      decode_bbox->ymin = std::max(std::min(decode_bbox->ymin, 1.f), 0.f);
      decode_bbox->xmax = std::max(std::min(decode_bbox->xmax, 1.f), 0.f);
      decode_bbox->ymax = std::max(std::min(decode_bbox->ymax, 1.f), 0.f);
    }

    decode_width = std::max(0.0f, decode_bbox->xmax - decode_bbox->xmin);
    decode_height = std::max(0.0f, decode_bbox->ymax - decode_bbox->ymin);
    bboxes_area[i] = decode_width * decode_height;
  }
}

void DecodeBBoxesAll_opt(const NormalizedBBoxOpt* all_loc_preds,
    const float* prior_bboxes, const float* prior_variances,
    const int num, const bool share_location, const int num_priors,
    const int num_loc_classes, const int background_label_id,
    const CodeType code_type, const bool variance_encoded_in_target,
    const bool clip, NormalizedBBoxOpt* &all_decode_bboxes,
    float* &all_decode_bboxes_area)
{
  //all_decode_bboxes = ( NormalizedBBoxOpt*)all_loc_preds;
  all_decode_bboxes = new NormalizedBBoxOpt[num * num_loc_classes * num_priors];
  all_decode_bboxes_area = new float[num * num_loc_classes * num_priors];
  for (int i = 0; i < num; ++i) {
    // Decode predictions into bboxes.
    for (int c = 0; c < num_loc_classes; ++c) {
      int label = share_location ? -1 : c;
      if (label == background_label_id)  continue;
      if (label >=  num_loc_classes) {
        // Something bad happened if there are no predictions for current label.
        BM_LOG(FATAL) << "Could not find location predictions for label " << label;
      }

      NormalizedBBoxOpt* decode_bboxes = all_decode_bboxes +
          i * num_loc_classes * num_priors + c * num_priors;
      float* bboxes_area = all_decode_bboxes_area +
          i * num_loc_classes * num_priors + c * num_priors;
      const NormalizedBBoxOpt* bboxes = all_loc_preds +
          i * num_priors * num_loc_classes +  c * num_priors;
      DecodeBBox_opt(prior_bboxes, prior_variances, num_priors, code_type,
          variance_encoded_in_target, clip, bboxes, decode_bboxes, bboxes_area);
    }
  }
}

void GetPriorBBoxes_opt(const float* prior_data, const int num_priors,
      float* &prior_bboxes, float* &prior_variances)
{
  prior_bboxes = (float*)prior_data;
  prior_variances = (float*)prior_data + 4 * num_priors;
}

template <class T>
void insert_sort(T begin, T end) {
  if (begin == end) return;
  for (auto it = begin + 1; it != end; it++) {
    auto val = (*it);
    auto pos = it;
    for (auto it1 = it; it1 != begin; it1--) {
      if (val.first - (*(it1 - 1)).first > 0.00001) {
        *it1 = *(it1 - 1);
        pos = it1 - 1;
      } else {
        break;
      }
    }
    if (it != pos) {
      *pos = val;
    }
  }
}

template void insert_sort(vector<pair<float, int>>::iterator begin,
                          vector<pair<float, int>>::iterator end);
template void insert_sort(vector<pair<float, pair<int, int>>>::iterator begin,
                          vector<pair<float, pair<int, int>>>::iterator end);

}
