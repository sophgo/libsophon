#include "cpu_paddle_multiclass_nms.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>

namespace bmcpu {
template <class T>
static bool SortScorePairDescend(
    const std::pair<float,  T>& pair1,
    const std::pair<float,  T>& pair2) {
    return pair1.first > pair2.first;
}

void cpu_paddle_multiclass_nmslayer::GetMaxScoreIndex(
    const float* scores,
    std::vector<std::pair<float,  int>>* sorted_indices) {
    int num_boxes = boxes_dims_[1];
    for (size_t i = 0; i < num_boxes; ++i) {
        if (scores[i] > score_threshold_) {
            sorted_indices->push_back(std::make_pair(scores[i],  i));
        }
    }
    // Sort the score pair according to the scores in descending order
    std::sort(sorted_indices->begin(),  sorted_indices->end(),
              SortScorePairDescend<int>);
    // Keep top_k scores if needed.
    if (nms_top_k_ > -1 &&
            nms_top_k_ < static_cast<int>(sorted_indices->size())) {
        sorted_indices->resize(nms_top_k_);
    }
}

float cpu_paddle_multiclass_nmslayer::BBoxArea(const float* box) {
    if (box[2] < box[0] || box[3] < box[1]) {
        return 0.f;
    } else {
        const float w = box[2] - box[0];
        const float h = box[3] - box[1];
        if (normalized_) {
            return w * h;
        } else {
            return (w + 1) * (h + 1);
        }
    }
}

float cpu_paddle_multiclass_nmslayer::JaccardOverlap(
    const float* box1,
    const float* box2) {
    if (box2[0] > box1[2] || box2[2] < box1[0] || box2[1] > box1[3] ||
            box2[3] < box1[1]) {
        return 0.f;
    } else {
        const float inter_xmin = std::max(box1[0],  box2[0]);
        const float inter_ymin = std::max(box1[1],  box2[1]);
        const float inter_xmax = std::min(box1[2],  box2[2]);
        const float inter_ymax = std::min(box1[3],  box2[3]);
        float norm = normalized_ ? 0.f : 1.f;
        float inter_w = inter_xmax - inter_xmin + norm;
        float inter_h = inter_ymax - inter_ymin + norm;
        const float inter_area = inter_w * inter_h;
        const float bbox1_area = BBoxArea(box1);
        const float bbox2_area = BBoxArea(box2);
        return inter_area / (bbox1_area + bbox2_area - inter_area);
    }
}

void cpu_paddle_multiclass_nmslayer::NMSFast(float* box,
        float* score,
        std::vector<int>* selected_indices) {
    int box_size = boxes_dims_[2];
    std::vector<std::pair<float,  int>> sorted_indices;
    GetMaxScoreIndex(score,  &sorted_indices);
    selected_indices->clear();
    float adaptive_threshold = nms_threshold_;
    while (sorted_indices.size() != 0) {
        const int idx = sorted_indices.front().second;
        bool keep = true;
        for (size_t k = 0; k < selected_indices->size(); ++k) {
            if (keep) {
                const int kept_idx = (*selected_indices)[k];
                float overlap = 0.f;
                if (box_size == 4) {
                    overlap = JaccardOverlap(box + idx * box_size,
                                             box + kept_idx * box_size);
                }
                keep = overlap <= adaptive_threshold;
            } else {
                break;
            }
        }
        if (keep) {
            selected_indices->push_back(idx);
        }
        sorted_indices.erase(sorted_indices.begin());
        if (keep && nms_eta_ < 1 && adaptive_threshold > 0.5) {
            adaptive_threshold *= nms_eta_;
        }
    }
}

void cpu_paddle_multiclass_nmslayer::MultiClassOutput(float* box,
        float* score,
        float* output,
        int* output_index,
        const std::map<int,  std::vector<int>>& selected_indices,
        const int scores_size,
        const int offset,
        int &count) {
    int predict_dim = score_dims_[2];
    int box_size = boxes_dims_[2];
    int out_dim = box_size + 2;
    const float* sdata;
    for (const auto& it : selected_indices) {
        int label = it.first;
        const std::vector<int>& indices = it.second;
        sdata = score + label * predict_dim;
        for (size_t j = 0; j < indices.size(); ++j) {
            int idx = indices[j];
            output[count * out_dim] = label;
            const float* bdata;
            bdata = box + idx * box_size;
            output[count * out_dim + 1] = sdata[idx];
            memcpy(output + count * out_dim + 2,  bdata,  box_size * sizeof(float));
            if (return_index_) {
                output_index[count] = offset + idx;
            }
            count++;
        }
    }
}

void cpu_paddle_multiclass_nmslayer::MultiClassNMS(float* box,
        float* score,
        std::map<int,  std::vector<int>>* indices,
        int* num_nmsed_out) {
    int num_det = 0;
    int class_num = score_dims_[1];
    for (int c = 0; c < class_num; ++c) {
        if (c == background_label_) continue;
        float* c_score = score + c * score_dims_[2];
        NMSFast(box,  c_score,  &((*indices)[c]));
        num_det += (*indices)[c].size();
    }
    *num_nmsed_out = num_det;
    if (keep_top_k_ > -1 && num_det > keep_top_k_) {
        const float* sdata;
        std::vector<std::pair<float,  std::pair<int,  int>>> score_index_pairs;
        for (const auto& it : *indices) {
            int label = it.first;
            sdata = score + label * score_dims_[2];
            const std::vector<int>& label_indices = it.second;
            for (size_t j = 0; j < label_indices.size(); ++j) {
                int idx = label_indices[j];
                score_index_pairs.push_back(
                    std::make_pair(sdata[idx],  std::make_pair(label,  idx)));
            }
        }
        std::sort(score_index_pairs.begin(),  score_index_pairs.end(),
                  SortScorePairDescend<std::pair<int,  int>>);
        score_index_pairs.resize(keep_top_k_);
        std::map<int,  std::vector<int>> new_indices;
        for (size_t j = 0; j < score_index_pairs.size(); ++j) {
            int label = score_index_pairs[j].second.first;
            int idx = score_index_pairs[j].second.second;
            new_indices[label].push_back(idx);
        }
        new_indices.swap(*indices);
        *num_nmsed_out = keep_top_k_;
    }
}

int cpu_paddle_multiclass_nmslayer::process(void *param, int psize) {
#ifdef CALC_TIME
    struct timeval start,  stop;
    gettimeofday(&start, 0);
#endif
    setParam(param, psize);
    float* boxes = input_tensors_[0];
    float* scores = input_tensors_[1];

    boxes_dims_ = input_shapes_[0];
    score_dims_ = input_shapes_[1];
    int n = boxes_dims_[0];
    int num_nmsed_out = 0;
    std::vector<std::map<int,  std::vector<int>>> all_indices;
    std::vector<int> batch_starts = {0};

    for (int i = 0; i < n; ++i) {
        std::map<int,  std::vector<int>> indices;
        float* box = boxes + i * boxes_dims_[1] * boxes_dims_[2];
        float* score = scores + i * score_dims_[1] * score_dims_[2];
        MultiClassNMS(box,  score,  &indices,  &num_nmsed_out);
        all_indices.push_back(indices);
        batch_starts.push_back(batch_starts.back() + num_nmsed_out);
    }

    float* output = output_tensors_[0];
    int out_size = 1;
    for (auto& o:(*output_shapes_)[0]) {
        out_size *= o;
    }
    memset(output,  0,  sizeof(float) * out_size);
    int* output_index = nullptr;
    int index_size = 1;
    if (return_index_) {
        output_index = reinterpret_cast<int *>(output_tensors_[1]);
        for (auto& o:(*output_shapes_)[1]) {
            index_size *= o;
        }
        memset(output_index,  0,  sizeof(int) * index_size);
    }
    int num_kept = batch_starts.back();
    int count = 0;
    if (num_kept == 0) {
        if(!return_index_) {
            output[0] = -1;
            batch_starts = {0,  1};
        }
        ++count;
    } else {
        int offset = 0;
        for (int i = 0; i < n; ++i) {
            float* box = boxes + i * boxes_dims_[1] * boxes_dims_[2];
            float* score = scores + i * score_dims_[1] * score_dims_[2];
            int s = static_cast<int>(batch_starts[i]);
            int e = static_cast<int>(batch_starts[i + 1]);
            if (return_index_) {
                offset = i * score_dims_[2];
            }
            if (e > s) {
                MultiClassOutput(
                    box,  score,  output, output_index,
                    all_indices[i],  score_dims_.size(),  offset, count);
            }
        }
    }

    if(has_output_nms_num_) {
        int batch_size = score_dims_[0];
#if 0
        int score_size = score_dims_.size();
        int n = score_size==3?batch_size:
#endif
                n = batch_size;
        int* output_roi = reinterpret_cast<int *>(output_tensors_[2]);
        out_size = 1;
        for (auto& o:(*output_shapes_)[2]) {
            out_size *= o;
        }
        for(int i=0; i<out_size; i++) {
            output_roi[i] = n;
        }
        for(int i=1; i<=n; i++) {
            output_roi[i-1] = batch_starts[i]-batch_starts[i-1];
        }

#if 0
        std::cout<<"roi result: "<<std::endl;
        for(int i=0; i<out_size; i++) {
            std::cout<<" "<<output_roi[i];
        }
        std::cout<<std::endl;
        for(auto batch:batch_starts) {
            std::cout<<" batch "<<batch;
        }
        std::cout<<std::endl;
#endif
    }

    int out_dim = boxes_dims_[2]+2;

    (*output_shapes_)[0][0] = count;
    if(return_index_) {
        if(num_kept == 0) {
            (*output_shapes_)[0][0] = 0;
            (*output_shapes_)[0][1] = out_dim;
        }
        (*output_shapes_)[1][0] = count;
    }

    auto o_size = output_tensors_.size();
    const bool v260 = !has_batch_;
    if(!v260) {
        int* lod_info = reinterpret_cast<int *>(output_tensors_[o_size-1]);
        (*output_shapes_)[o_size-1][0] = batch_starts.size();
        for (int i=0; i<batch_starts.size(); i++) {
            lod_info[i] = batch_starts[i];
        }
    }
#ifdef CALC_TIME
    gettimeofday(&stop,  0);
    float timeuse = 1000000 * (stop.tv_sec  start.tv_sec) + stop.tv_usec - start.tv_usec;
    timeuse /= 1000;
    printf("**************cost time is %f ms\n",  timeuse);
#endif
    return 0;
}

void cpu_paddle_multiclass_nmslayer::setParam(void *param, int psize) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_multiclass_nms_param_t, multiclass_nms_param, layer_param_, psize);
    score_threshold_ = multiclass_nms_param->score_threshold;
    nms_threshold_ = multiclass_nms_param->nms_threshold;
    nms_eta_ = multiclass_nms_param->nms_eta;
    keep_top_k_ = multiclass_nms_param->keep_top_k;
    nms_top_k_ = multiclass_nms_param->nms_top_k;
    normalized_ = multiclass_nms_param->normalized;
    background_label_ = multiclass_nms_param->background_label;
    has_output_nms_num_ = multiclass_nms_param->has_output_nms_num;
    has_batch_ = multiclass_nms_param->has_batch;

    const bool v260 = !multiclass_nms_param->has_batch;
    const bool cond = v260?(output_tensors_.size()>1):(output_tensors_.size()>2);

    return_index_ = false;
    if (cond) {
        return_index_ = true;
    }
}

int cpu_paddle_multiclass_nmslayer::reshape(
    void* param, int psize,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_multiclass_nms_param_t, multiclass_nms_param, param, psize);

    output_shapes[0] = {multiclass_nms_param->keep_top_k * input_shapes[0][0], input_shapes[0][2]+2};

    auto out_size = output_shapes.size();
    const bool v260 = !multiclass_nms_param->has_batch;

    const bool cond_1 = v260?(output_shapes.size()>1):(output_shapes.size()>2);
    const bool cond_2 = v260?(output_shapes.size()==3):(output_shapes.size()==4);

    if (cond_1) {
        output_shapes[1] = {multiclass_nms_param->keep_top_k * input_shapes[0][0] , 1};
    }
    if (cond_2) {
        output_shapes[2] = {input_shapes[0][0]};
    }
    if(!v260) output_shapes[out_size-1] = {input_shapes[0][0]+1};
    return 0;
}

int cpu_paddle_multiclass_nmslayer::dtype(
    const void *param, size_t psize,
    const vector<int>& input_dtypes,
    vector<int>& output_dtypes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_multiclass_nms_param_t, p, param, psize);
    CPU_ASSERT(input_dtypes.size() > 0);
    output_dtypes[0] = CPU_DTYPE_FP32;
    output_dtypes[1] = CPU_DTYPE_INT32;

    if(p->has_output_nms_num) {
        output_dtypes[2] = CPU_DTYPE_INT32;
    }
    auto out_size = output_dtypes.size();
    if(p->has_batch) output_dtypes[out_size-1] = CPU_DTYPE_INT32;
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_PADDLE_MULTICLASS_NMS, cpu_paddle_multiclass_nms);

}/* namespace bmcpu*/
