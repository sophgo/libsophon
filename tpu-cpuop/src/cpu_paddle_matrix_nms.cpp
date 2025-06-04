#include "cpu_paddle_matrix_nms.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>

namespace bmcpu {

template <typename T,  bool gaussian>
struct decay_score;
template <typename T>
struct decay_score<T,  true> {
    T operator()(T iou,  T max_iou,  T sigma) {
        return std::exp((max_iou * max_iou - iou * iou) * sigma);
    }
};
template <typename T>
struct decay_score<T,  false> {
    T operator()(T iou,  T max_iou,  T sigma) {
        return (1. - iou) / (1. - max_iou);
    }
};

template <class T>
static inline T BBoxArea(const T* box,  const bool normalized) {
    if (box[2] < box[0] || box[3] < box[1]) {
        return static_cast<T>(0.);
    } else {
        const T w = box[2] - box[0];
        const T h = box[3] - box[1];
        if (normalized) {
            return w * h;
        } else {
            return (w + 1) * (h + 1);
        }
    }
}

template <class T>
static inline T JaccardOverlap(const T* box1,  const T* box2,
                               const bool normalized) {
    if (box2[0] > box1[2] || box2[2] < box1[0] || box2[1] > box1[3] ||
            box2[3] < box1[1]) {
        return static_cast<T>(0.);
    } else {
        const T inter_xmin = std::max(box1[0],  box2[0]);
        const T inter_ymin = std::max(box1[1],  box2[1]);
        const T inter_xmax = std::min(box1[2],  box2[2]);
        const T inter_ymax = std::min(box1[3],  box2[3]);
        T norm = normalized ? static_cast<T>(0.) : static_cast<T>(1.);
        T inter_w = inter_xmax - inter_xmin + norm;
        T inter_h = inter_ymax - inter_ymin + norm;
        const T inter_area = inter_w * inter_h;
        const T bbox1_area = BBoxArea<T>(box1,  normalized);
        const T bbox2_area = BBoxArea<T>(box2,  normalized);
        return inter_area / (bbox1_area + bbox2_area - inter_area);
    }
}

template <typename T,  bool gaussian>
void NMSMatrix(const T* bbox,const vector<int>& bbox_shape,
               const T* scores,  const vector<int>& scores_shape,
               const T score_threshold,  const T post_threshold,
               const float sigma,  const int64_t top_k,  const bool normalized,
               std::vector<int>* selected_indices,
               std::vector<T>* decayed_scores) {
    int64_t num_boxes = bbox_shape[0];
    int64_t box_size = bbox_shape[1];

    auto score_ptr = scores;
    auto bbox_ptr = bbox;

    std::vector<int32_t> perm(num_boxes);
    std::iota(perm.begin(),  perm.end(),  0);
    auto end = std::remove_if(perm.begin(),  perm.end(),
    [&score_ptr,  score_threshold](int32_t idx) {
        return score_ptr[idx] <= score_threshold;
    });

    auto sort_fn = [&score_ptr](int32_t lhs,  int32_t rhs) {
        return score_ptr[lhs] > score_ptr[rhs];
    };

    int64_t num_pre = std::distance(perm.begin(),  end);
    if (num_pre <= 0) {
        return;
    }
    if (top_k > -1 && num_pre > top_k) {
        num_pre = top_k;
    }
    std::partial_sort(perm.begin(),  perm.begin() + num_pre,  end,  sort_fn);
    std::vector<T> iou_matrix((num_pre * (num_pre - 1)) >> 1);
    std::vector<T> iou_max(num_pre);

    iou_max[0] = 0.;
    for (int64_t i = 1; i < num_pre; i++) {
        T max_iou = 0.;
        auto idx_a = perm[i];
        for (int64_t j = 0; j < i; j++) {
            auto idx_b = perm[j];
            auto iou = JaccardOverlap<T>(bbox_ptr + idx_a * box_size,
                                         bbox_ptr + idx_b * box_size,  normalized);
            max_iou = std::max(max_iou,  iou);
            iou_matrix[i * (i - 1) / 2 + j] = iou;
        }
        iou_max[i] = max_iou;
    }

    if (score_ptr[perm[0]] > post_threshold) {
        selected_indices->push_back(perm[0]);
        decayed_scores->push_back(score_ptr[perm[0]]);
    }

    decay_score<T,  gaussian> decay_fn;
    for (int64_t i = 1; i < num_pre; i++) {
        T min_decay = 1.;
        for (int64_t j = 0; j < i; j++) {
            auto max_iou = iou_max[j];
            auto iou = iou_matrix[i * (i - 1) / 2 + j];
            auto decay = decay_fn(iou,  max_iou,  sigma);
            min_decay = std::min(min_decay,  decay);
        }
        auto ds = min_decay * score_ptr[perm[i]];
        if (ds <= post_threshold) continue;
        selected_indices->push_back(perm[i]);
        decayed_scores->push_back(ds);
    }
}


template <class T>
size_t MultiClassMatrixNMS(const T* scores,  const vector<int>& scores_shape,
                           const T* bboxes,const vector<int>& bbox_shape,
                           std::vector<T>* out,  std::vector<int>* indices,
                           int start,  int64_t background_label,
                           int64_t nms_top_k,  int64_t keep_top_k,
                           bool normalized,  T score_threshold,
                           T post_threshold,  bool use_gaussian,
                           float gaussian_sigma) {
    std::vector<int> all_indices;
    std::vector<T> all_scores;
    std::vector<T> all_classes;
    int scores_count = 1;
    for(auto& score_s: scores_shape) {
        scores_count*=score_s;
    }
    all_indices.reserve(scores_count);
    all_scores.reserve(scores_count);
    all_classes.reserve(scores_count);

    size_t num_det = 0;
    auto class_num = scores_shape[0];
    for (int64_t c = 0; c < class_num; ++c) {
        if (c == background_label) continue;
        const T* score_slice = scores+c*scores_shape[1];
        //TODO CONFIRM SHAPE
        //make a new score_slice shape
        std::vector<int> score_slice_shape = {scores_shape[1]};
        if (use_gaussian) {
            NMSMatrix<T,  true>(bboxes, bbox_shape, score_slice, score_slice_shape, score_threshold,  post_threshold,
                                gaussian_sigma,  nms_top_k,  normalized,  &all_indices,
                                &all_scores);
        } else {
            NMSMatrix<T,  false>(bboxes, bbox_shape, score_slice, score_slice_shape, score_threshold,
                                 post_threshold,  gaussian_sigma,  nms_top_k,
                                 normalized,  &all_indices,  &all_scores);
        }
        for (size_t i = 0; i < all_indices.size() - num_det; i++) {
            all_classes.push_back(static_cast<T>(c));
        }
        num_det = all_indices.size();
    }

    if (num_det <= 0) {
        return num_det;
    }

    if (keep_top_k > -1) {
        auto k = static_cast<size_t>(keep_top_k);
        if (num_det > k) num_det = k;
    }

    std::vector<int32_t> perm(all_indices.size());
    std::iota(perm.begin(),  perm.end(),  0);
    std::partial_sort(perm.begin(),  perm.begin() + num_det,  perm.end(),
    [&all_scores](int lhs,  int rhs) {
        return all_scores[lhs] > all_scores[rhs];
    });

    for (size_t i = 0; i < num_det; i++) {
        auto p = perm[i];
        auto idx = all_indices[p];
        auto cls = all_classes[p];
        auto score = all_scores[p];
        auto bbox = bboxes + idx * bbox_shape[1];
        (*indices).push_back(start + idx);
        (*out).push_back(cls);
        (*out).push_back(score);
        for (int j = 0; j < bbox_shape[1]; j++) {
            (*out).push_back(bbox[j]);
        }
    }

    return num_det;
}




int cpu_paddle_matrix_nmslayer::process(void *param, int psize) {
#ifdef CALC_TIME
    struct timeval start,  stop;
    gettimeofday(&start, 0);
#endif
    setParam(param, psize);

    float* boxes = input_tensors_[0];
    float* scores = input_tensors_[1];

    float* outs = output_tensors_[0];
    int* index = reinterpret_cast<int *>(output_tensors_[1]);

    auto boxes_dims = input_shapes_[0];
    auto score_dims = input_shapes_[1];
    CPU_ASSERT(boxes_dims[1]==score_dims[2]);

    auto batch_size = score_dims[0];
    auto num_boxes = score_dims[2];
    auto box_dim = boxes_dims[2];
    auto out_dim = box_dim + 2;

    size_t num_out = 0;
    std::vector<size_t> offsets = {0};
    std::vector<float> detections;
    std::vector<int> indices;
    std::vector<int> num_per_batch;
    detections.reserve(out_dim * num_boxes * batch_size);
    indices.reserve(num_boxes * batch_size);
    num_per_batch.reserve(batch_size);
    for (int i = 0; i < batch_size; ++i) {
        float* score_slice = scores+i*score_dims[1]*score_dims[2];
        float* boxes_slice = boxes+i*boxes_dims[1]*boxes_dims[2];
        std::vector<int> score_slice_shape = {score_dims[1],  score_dims[2]};
        std::vector<int> boxes_slice_shape = {score_dims[2],  box_dim};
        int start = i * score_dims[2];
        num_out = MultiClassMatrixNMS<float>(
                      score_slice, score_slice_shape, boxes_slice, boxes_slice_shape, &detections,  &indices,  start,
                      background_label_,  nms_top_k_,  keep_top_k_,  normalized_,  score_threshold_,
                      post_threshold_,  use_gaussian_,  gaussian_sigma_);
        offsets.push_back(offsets.back() + num_out);
        num_per_batch.emplace_back(num_out);
    }

    int64_t num_kept = offsets.back();
    if (num_kept == 0) {
        (*output_shapes_)[0] = {0,  out_dim};
        (*output_shapes_)[1] = {0,  1};
    } else {
        std::copy(detections.begin(),  detections.end(),  outs);
        std::copy(indices.begin(),  indices.end(),  index);
        (*output_shapes_)[0] = {static_cast<int>(num_kept), static_cast<int>(out_dim)};
        (*output_shapes_)[1] = {static_cast<int>(num_kept),  1};
    }

    if(has_output_nms_num_) {
        int* rois_num = reinterpret_cast<int *>(output_tensors_[2]);
        std::copy(num_per_batch.begin(),  num_per_batch.end(),
                  rois_num);
        (*output_shapes_)[2] = {static_cast<int>(batch_size)};
    }

    auto out_size = output_tensors_.size();
    const bool v260 = (has_output_nms_num_)?(out_size==3):(out_size==2);
    if(!v260) {
        int* lod_info = reinterpret_cast<int *>(output_tensors_[out_size-1]);
        (*output_shapes_)[out_size-1][0] = offsets.size();
        for (int i=0; i<offsets.size(); i++) {
            lod_info[i] = offsets[i];
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

void cpu_paddle_matrix_nmslayer::setParam(void *param, int psize) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_matrix_nms_param_t, matrix_nms_param, layer_param_, psize);
    background_label_ = matrix_nms_param->background_label;
    keep_top_k_ = matrix_nms_param->keep_top_k;
    nms_top_k_ = matrix_nms_param->nms_top_k;
    normalized_ = matrix_nms_param->normalized;
    score_threshold_ = matrix_nms_param->score_threshold;
    post_threshold_ = matrix_nms_param->post_threshold;
    use_gaussian_ = matrix_nms_param->use_gaussian;
    gaussian_sigma_ = matrix_nms_param->gaussian_sigma;
    has_output_nms_num_ = matrix_nms_param->has_output_nms_num;
}

int cpu_paddle_matrix_nmslayer::reshape(
    void* param, int psize,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_matrix_nms_param_t, matrix_nms_param, layer_param_, psize);

    output_shapes[0] = {matrix_nms_param->keep_top_k * input_shapes[0][0], input_shapes[0][2]+2};
    if (output_shapes.size() > 1) {
        output_shapes[1] = {matrix_nms_param->keep_top_k * input_shapes[0][0] , 1};
    }
    auto out_size = output_shapes.size();
    const bool v260 = (matrix_nms_param->has_output_nms_num)?(out_size==3):(out_size==2);
    const bool cond = v260?(output_shapes.size()==3):(output_shapes.size()==4);
    if(cond) {
        output_shapes[2] = {input_shapes[0][0]};
    }
    if(!v260) output_shapes[out_size-1] = {input_shapes[0][0]+1};
    return 0;
}

int cpu_paddle_matrix_nmslayer::dtype(
    const void *param, size_t psize,
    const vector<int>& input_dtypes,
    vector<int>& output_dtypes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_matrix_nms_param_t, p, param, psize);
    CPU_ASSERT(input_dtypes.size() > 0);
    output_dtypes[0] = CPU_DTYPE_FP32;
    output_dtypes[1] = CPU_DTYPE_INT32;
    if(p->has_output_nms_num) {
        output_dtypes[2] = CPU_DTYPE_INT32;
    }
    auto out_size = output_dtypes.size();
    const bool v260 = (p->has_output_nms_num)?(out_size==3):(out_size==2);
    if(!v260) output_dtypes[out_size-1] = CPU_DTYPE_INT32;
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_PADDLE_MATRIX_NMS, cpu_paddle_matrix_nms);

}/* namespace bmcpu*/
