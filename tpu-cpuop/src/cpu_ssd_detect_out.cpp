#include "cpu_ssd_detect_out.h"
#include <algorithm>

#define Dtype float
//#define TIME_PROFILE

#ifdef TIME_PROFILE
#include <sys/time.h>

static struct timeval t0, t1;
#define TIMER_START gettimeofday(&t0, NULL);
#define TIMER_END(str) \
{ \
  gettimeofday(&t1, NULL); \
  double time_spent =  1000.0f * (t1. tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1000.0f; \
  printf("%s : %.2f ms\n", str, time_spent); \
}
#else
#define TIMER_START
#define TIMER_END(str)
#endif

namespace bmcpu {

int cpu_ssd_detect_outlayer::process(void *param, int param_size)
{
#ifdef TIME_PROFILE
  struct timeval tpstart;
  gettimeofday(&tpstart,NULL);
#endif

  setParam(param, param_size);

  /* code from CAFFE */
  const Dtype* loc_data   = input_tensors_[0];
  const Dtype* conf_data  = input_tensors_[1];
  const Dtype* prior_data = input_tensors_[2];
  const int num = input_shapes_[0][0];

  // Retrieve all location predictions.
  TIMER_START;
  NormalizedBBoxOpt* all_loc_preds = NULL;
  GetLocPredictions_opt(loc_data, num, num_priors_, num_loc_classes_,
                    share_location_, all_loc_preds);
  TIMER_END("GetLocPredictions");

  // Retrieve all prior bboxes. It is same within a batch since we assume all
  // images in a batch are of same dimension.
  TIMER_START;
  float* prior_bboxes = NULL;
  float* prior_variances = NULL;
  GetPriorBBoxes_opt(prior_data, num_priors_, prior_bboxes, prior_variances);
  TIMER_END("GetPriorBBoxes");

  // Decode all loc predictions to bboxes.
  TIMER_START;
  NormalizedBBoxOpt* all_decode_bboxes = NULL;
  float* all_decode_bboxes_area = NULL;
  const bool clip_bbox = false;
  DecodeBBoxesAll_opt(all_loc_preds, prior_bboxes, prior_variances, num,
      share_location_, num_priors_, num_loc_classes_,
      background_label_id_, code_type_, variance_encoded_in_target_,
      clip_bbox, all_decode_bboxes, all_decode_bboxes_area);
  TIMER_END("DecodeBBoxesAll");

  TIMER_START;
  int num_kept = 0;
  vector<map<int, vector<pair<float,int>> > > all_indices;
  for (int i = 0; i < num; ++i) {
    map<int, vector<pair<float,int>> > indices;
    int num_det = 0;
    for (int c = 0; c < num_classes_; ++c) {
      if (c == background_label_id_) continue;
      if (c >= num_classes_) {
        // Something bad happened if there are no predictions for current label.
        BM_LOG(FATAL) << "Could not find confidence predictions for label " << c;
      }
      int label = share_location_ ? 0 : c;
      if (label >= num_loc_classes_) {
        // Something bad happened if there are no predictions for current label.
        BM_LOG(FATAL) << "Could not find location predictions for label " << label;
        continue;
      }

      const float* scores = (float*)conf_data + i * num_priors_ * num_classes_ + c;
      const NormalizedBBoxOpt* bboxes = all_decode_bboxes + label * num_priors_ +
                                        i * num_priors_ * num_loc_classes_;
      const float* bboxes_area = all_decode_bboxes_area + label * num_priors_ +
                                 i * num_priors_ * num_loc_classes_;
      ApplyNMSFast_opt(bboxes, num_priors_, bboxes_area, scores, confidence_threshold_,
          nms_threshold_, eta_, top_k_, num_classes_, &(indices[c]));
      num_det += indices[c].size();
    }
    if (keep_top_k_ > -1 && num_det > keep_top_k_) {
      vector<pair<float, pair<int, int> > > score_index_pairs;
      for (auto it = indices.begin(); it != indices.end(); ++it) {
        int label = it->first;
        const vector<pair<float,int>>& label_indices = it->second;
        if (label >= num_classes_) {
          // Something bad happened for current label.
          BM_LOG(FATAL) << "Could not find location predictions for " << label;
          continue;
        }
        for (int j = 0; j < label_indices.size(); ++j) {
          int idx = label_indices[j].second;
          BM_CHECK_LT(idx, num_priors_);
          score_index_pairs.push_back(std::make_pair(
                  label_indices[j].first, std::make_pair(label, idx)));
        }
      }
      // Keep top k results per image.
      // insert_sort(score_index_pairs.begin(), score_index_pairs.end());
      std::sort(score_index_pairs.begin(), score_index_pairs.end(),
                SortScorePairDescend<pair<int, int> >);
      score_index_pairs.resize(keep_top_k_);
      // Store the new indices.
      map<int, vector<pair<float,int>> > new_indices;
      for (int j = 0; j < score_index_pairs.size(); ++j) {
        int label = score_index_pairs[j].second.first;
        int idx = score_index_pairs[j].second.second;
        float s = score_index_pairs[j].first;
        new_indices[label].push_back(make_pair(s , idx));
      }
      all_indices.push_back(new_indices);
      num_kept += keep_top_k_;
    } else {
      all_indices.push_back(indices);
      num_kept += num_det;
    }
  }
  TIMER_END("NMS");

  TIMER_START;
  Dtype* top_data = output_tensors_[0];
  if (num_kept == 0) {
    (*output_shapes_)[0].clear();
    (*output_shapes_)[0].push_back(1);
    (*output_shapes_)[0].push_back(1);
    (*output_shapes_)[0].push_back(num);
    (*output_shapes_)[0].push_back(7);

    // Generate fake results per image.
    for (int i = 0; i < num; ++i) {
      top_data[0] = i;
      for(int j = 1; j < 7; ++j) {
        top_data[j] = -1;
      }
      top_data += 7;
    }
  } else {
    (*output_shapes_)[0].clear();
    (*output_shapes_)[0].push_back(1);
    (*output_shapes_)[0].push_back(1);
    (*output_shapes_)[0].push_back(num_kept);
    (*output_shapes_)[0].push_back(7);
  }

  top_data = output_tensors_[0];
  for (int i = 0; i < num; ++i) {
    for (auto it = all_indices[i].begin(); it != all_indices[i].end(); ++it) {
      int label = it->first;
      if (label >= num_classes_) {
        // Something bad happened if there are no predictions for current label.
        BM_LOG(FATAL) << "Could not find confidence predictions for " << label;
        continue;
      }
      int loc_label = share_location_ ? 0 : label;
      if (loc_label >= num_loc_classes_) {
        // Something bad happened if there are no predictions for current label.
        BM_LOG(FATAL) << "Could not find location predictions for " << loc_label;
        continue;
      }
      const NormalizedBBoxOpt* bboxes = all_decode_bboxes + loc_label * num_priors_ +
                                        i * num_priors_ * num_loc_classes_;
      vector<pair<float,int>>& indices = it->second;

      for (int j = 0; j < indices.size(); ++j) {
        int idx = indices[j].second;
        top_data[0] = i;
        top_data[1] = label;
        top_data[2] = indices[j].first;
        const NormalizedBBoxOpt& bbox = bboxes[idx];
        top_data[3] = bbox.xmin;
        top_data[4] = bbox.ymin;
        top_data[5] = bbox.xmax;
        top_data[6] = bbox.ymax;
        top_data += 7;
      }
    }
  }
  TIMER_END("output box");

  TIMER_START;
  if(num_loc_classes_ != 1 && all_loc_preds != NULL) delete[] all_loc_preds;
  if(all_decode_bboxes != NULL)                      delete[] all_decode_bboxes;
  if(all_decode_bboxes_area != NULL)                 delete[] all_decode_bboxes_area;
  TIMER_END("Release memory");

#ifdef TIME_PROFILE
  struct timeval tpend;
  gettimeofday(&tpend,NULL);
  double time_spent =  1000.0f * (tpend. tv_sec - tpstart.tv_sec) + (tpend.tv_usec - tpstart.tv_usec) / 1000.0f;
  printf("cpu time: %.2f ms\n",time_spent);
#endif

  return 0;
}

void cpu_ssd_detect_outlayer::setParam(void *param, int param_size)
{
  layer_param_ = param;
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_ssd_detect_out_param_t, ssd_detect_out_param, layer_param_, param_size);

  /* input loc/conf shape could be varified, such as [N, BOX_NUM*4], [N, BOX_NUM*4, CLASS_NUM]*/
  num_priors_                  =  (bmcpu_shape_count(input_shapes_[0]) / input_shapes_[0][0]) / 4;
  num_classes_                 =  (bmcpu_shape_count(input_shapes_[1]) / input_shapes_[1][0]) / num_priors_;

  share_location_              = ssd_detect_out_param->share_location_;
  background_label_id_         = ssd_detect_out_param->background_label_id_;
  nms_threshold_               = ssd_detect_out_param->nms_threshold_;
  top_k_                       = ssd_detect_out_param->top_k_;
  code_type_                   = (CodeType)(ssd_detect_out_param->code_type_);
  keep_top_k_                  = ssd_detect_out_param->keep_top_k_;
  confidence_threshold_        = ssd_detect_out_param->confidence_threshold_;
  num_loc_classes_             = ssd_detect_out_param->num_loc_classes_;
  variance_encoded_in_target_  = ssd_detect_out_param->variance_encoded_in_target_;
  eta_                         = ssd_detect_out_param->eta_;
}


int cpu_ssd_detect_outlayer::reshape(
    void* param, int param_size,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes)
{
  /* Since the number of detect bboxes to be kept is unknown before nms, using
   * max shape [1, 1, batch * keep_top_k, 7]
   */
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_ssd_detect_out_param_t, ssd_detect_out_param, param, param_size);
  output_shapes[0].clear();
  output_shapes[0].push_back(1);
  output_shapes[0].push_back(1);
  output_shapes[0].push_back(input_shapes[0][0] * ssd_detect_out_param->keep_top_k_);
  output_shapes[0].push_back(7);
  cout << " ssd detect_output reshape "<< endl;
  return 0;
}

int cpu_ssd_detect_outlayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
        vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(0); // DTYPE_FP32
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_SSD_DETECTION_OUTPUT, cpu_ssd_detect_out)

} /* namespace bmcpu */
