#include "cpu_yolov3_detect_out.h"
#include <algorithm>
#include <cmath>
#include <map>

#define KEEP_TOP_K    200
#define Dtype float
// #define TIME_PROFILE

#ifdef TIME_PROFILE
#include <sys/time.h>

static struct timeval t_total, t0, t1;
#define TIMER_START(t_s) gettimeofday(&t_s, NULL);
#define TIMER_END(t_e, t_s, str) \
{ \
  gettimeofday(&t_e, NULL); \
  double time_spent =  1000000.0f * (t_e.tv_sec - t_s.tv_sec) + (t_e.tv_usec - t_s.tv_usec); \
  printf("%s : %.2f us\n", str, time_spent); \
}
#else
#define TIMER_START(t_s)
#define TIMER_END(t_e, t_s, str)
#endif

namespace bmcpu {

#if defined(__aarch64__)
#include <arm_neon.h>

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

void sigmoid_batch(Dtype *arr, const int n) {
    int i, j = 0;
    float *p0 = arr;
    float *p1 = arr;
    for (i = 0; i < (n/4); i++) {
        float32x4_t x = vld1q_f32(p0);
        float32x4_t tmp, fx;
        float32x4_t one = vdupq_n_f32(1);
        x = vminq_f32(x, vdupq_n_f32(c_exp_hi));
        x = vmaxq_f32(x, vdupq_n_f32(c_exp_lo));
        /* express exp(x) as exp(g + n*log(2)) */
        fx = vmlaq_f32(vdupq_n_f32(0.5f), x, vdupq_n_f32(c_cephes_LOG2EF));
        /* perform a floorf */
        tmp = vcvtq_f32_s32(vcvtq_s32_f32(fx));
        /* if greater, substract 1 */
        uint32x4_t mask = vcgtq_f32(tmp, fx);
        mask = vandq_u32(mask, vreinterpretq_u32_f32(one));
        fx = vsubq_f32(tmp, vreinterpretq_f32_u32(mask));
        tmp = vmulq_f32(fx, vdupq_n_f32(c_cephes_exp_C1));
        float32x4_t z = vmulq_f32(fx, vdupq_n_f32(c_cephes_exp_C2));
        x = vsubq_f32(x, tmp);
        x = vsubq_f32(x, z);
        static const float cephes_exp_p[6] = { c_cephes_exp_p0, c_cephes_exp_p1, \
                                               c_cephes_exp_p2, c_cephes_exp_p3, \
                                               c_cephes_exp_p4, c_cephes_exp_p5
                                             };
        float32x4_t y = vld1q_dup_f32(cephes_exp_p+0);
        float32x4_t c1 = vld1q_dup_f32(cephes_exp_p+1);
        float32x4_t c2 = vld1q_dup_f32(cephes_exp_p+2);
        float32x4_t c3 = vld1q_dup_f32(cephes_exp_p+3);
        float32x4_t c4 = vld1q_dup_f32(cephes_exp_p+4);
        float32x4_t c5 = vld1q_dup_f32(cephes_exp_p+5);
        y = vmulq_f32(y, x);
        z = vmulq_f32(x, x);
        y = vaddq_f32(y, c1);
        y = vmulq_f32(y, x);
        y = vaddq_f32(y, c2);
        y = vmulq_f32(y, x);
        y = vaddq_f32(y, c3);
        y = vmulq_f32(y, x);
        y = vaddq_f32(y, c4);
        y = vmulq_f32(y, x);
        y = vaddq_f32(y, c5);
        y = vmulq_f32(y, z);
        y = vaddq_f32(y, x);
        y = vaddq_f32(y, one);
        /* build 2^n */
        int32x4_t mm;
        mm = vcvtq_s32_f32(fx);
        mm = vaddq_s32(mm, vdupq_n_s32(0x7f));
        mm = vshlq_n_s32(mm, 23);
        float32x4_t pow2n = vreinterpretq_f32_s32(mm);
        y = vmulq_f32(y, pow2n);
        y = vaddq_f32(one , y);
        float32x4_t b = vrecpeq_f32(y);
        b = vmulq_f32(vrecpsq_f32(y, b), b);
        b = vmulq_f32(vrecpsq_f32(y, b), b);
        b = vmulq_f32(vrecpsq_f32(y, b), b);
        b = vsubq_f32(one, b);
        vst1q_f32(p1, b);
        p0 = p0 + 4;
        p1 = p1 + 4;
        j += 4;
    }
    for (i = j; i < n; i++)
        *p1++ = 1.0f / (1.0f + exp(-(*p0++)));
}
#else
inline void sigmoid_batch(Dtype *x, const int n) {
    for (int i = 0; i < n; ++i)
        x[i] = 1.0f / (1.0f + exp(-x[i]));
}
#endif

struct PredictionResult {
    Dtype x;
    Dtype y;
    Dtype w;
    Dtype h;
    Dtype idx;
    Dtype confidence;
    int classType;
};

inline Dtype overlap(Dtype x1, Dtype w1, Dtype x2, Dtype w2) {
    float w1d2 = w1 * 0.5f;
    float w2d2 = w2 * 0.5f;
    float l1 = x1 - w1d2;
    float l2 = x2 - w2d2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1d2;
    float r2 = x2 + w2d2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

inline Dtype box_intersection(const PredictionResult &a,
                              const PredictionResult &b) {
    float w = overlap(a.x, a.w, b.x, b.w);
    if (w < 0)
        return 0;
    float h = overlap(a.y, a.h, b.y, b.h);
    if (h < 0)
        return 0;
    return w * h;
}

inline Dtype box_iou(const PredictionResult &a, const PredictionResult &b) {
    float i = box_intersection(a, b);
    float u = a.w * a.h + b.w * b.h - i;
    return i / u;
}

void ApplyNms(vector<PredictionResult> &boxes, vector<int> &idxes,
              Dtype threshold) {
    int bbox_cnt = (int)boxes.size();
    // init the map
    #ifdef __linux__
    unsigned int map[(bbox_cnt >> 5) + 1];
    #else
    std::shared_ptr<unsigned int> map_(new unsigned int[(bbox_cnt >> 5) + 1], std::default_delete<unsigned int[]>());
    unsigned int *map = map_.get();
    #endif
    memset(map, 0xFF, sizeof(map));
    for (int i = 0; i < bbox_cnt - 1; ++i) {
        // skip the dropped bbox
        if (!(map[i >> 5] & (1 << (i & 0x1F))))
            continue;
        for (int j = i + 1; j < bbox_cnt; ++j) {
            // skip the dropped bbox
            if (!(map[j >> 5] & (1 << (j & 0x1F))))
                continue;
            Dtype iou = box_iou(boxes[i], boxes[j]);
            if (iou >= threshold)
                map[j >> 5] &= ~(1 << (j & 0x1F));
        }
    }
    for (int i = 0; i < bbox_cnt; ++i) {
        if (map[i >> 5] & (1 << (i & 0x1F)))
            idxes.push_back(i);
    }
}

void normalize_bbox(
    vector<Dtype> &b, Dtype *x, Dtype *biases, int n,
    int i, int j, int lw, int lh, int w, int h) {
    b.clear();
    b.push_back((i + (x[0])) / lw);
    b.push_back((j + (x[1])) / lh);
    b.push_back(exp(x[2]) * biases[2 * n] / (w));
    b.push_back(exp(x[3]) * biases[2 * n + 1] / (h));
}

bool BoxSortDecendScore(const PredictionResult &box1,
                        const PredictionResult &box2) {
    return box1.confidence > box2.confidence;
}

int cpu_yolov3_detect_outlayer::process(void *param, int param_size) {
    setParam(param, param_size);
    const int num = input_shapes_[0][0];
    int len = 4 + num_classes_ + 1;
    int mask_offset = 0;
    int total_num = 0;
    vector< PredictionResult > total_preds;
    // calc the threshold for Po
    float po_thres = -std::log(1 / confidence_threshold_ - 1);
    char str[256];
    TIMER_START(t_total);
    vector<Dtype> class_score;
    for (int b = 0; b < num; b++) {
        vector< PredictionResult > predicts;
        predicts.clear();
        mask_offset = 0;
        TIMER_START(t0);
        for (int t = 0; t < num_inputs_; t++) {
            int h = input_shapes_[t][2];
            int w = input_shapes_[t][3];
            int stride = h * w;
            const Dtype *input_data = input_tensors_[t];
            for (int n = 0; n < num_boxes_; n++) {
                for (int cy = 0; cy < h; cy++) {
                    for (int cx = 0; cx < w; cx++) {
                        int index = b * input_shapes_[t][1] * stride +
                                    n * len * stride +
                                    cy * w + cx;
                        vector<Dtype> pred;
                        class_score.clear();
                        // Po/Pmax/tx/ty/tw/th
                        Dtype swap_data[6] = {0};
                        // filter bbxo by Po
                        int index_po = 4 * stride + index;
                        if (input_data[index_po] <= po_thres)
                            continue;
                        for (int c = 0; c < len; ++c) {
                            int index2 = c * stride + index;
                            if (c > 4)
                                class_score.push_back(input_data[index2]);
                            else {
                                if (c == 4)
                                    swap_data[0] = input_data[index2];
                                else
                                    swap_data[c + 2] = input_data[index2];
                            }
                        }
                        PredictionResult predict;
                        swap_data[1] = *std::max_element(class_score.begin(), class_score.end());
                        int arg_max = std::distance(class_score.begin(),
                                                    std::max_element(class_score.begin(), class_score.end()));
                        sigmoid_batch(swap_data, 4);
                        // Pmax = Pmax * Po
                        swap_data[1] = swap_data[0] * swap_data[1];
                        if (swap_data[1] > confidence_threshold_) {
                            normalize_bbox(
                                pred, &swap_data[2], biases_, mask_[n + mask_offset],
                                cx, cy, w, h, w * anchors_scale_[t], h * anchors_scale_[t]);
                            predict.idx = b;
                            predict.x = pred[0];
                            predict.y = pred[1];
                            predict.w = pred[2];
                            predict.h = pred[3];
                            predict.classType = arg_max;
                            predict.confidence = swap_data[1];
                            predicts.push_back(predict);
                        }
                    }
                }
            }
            mask_offset += mask_group_size_;
        }
        sprintf(str, "Filter %d Boxes (batch %d)", (int)predicts.size(), b);
        TIMER_END(t1, t0, str);
        // NMS for each image
        vector<int> idxes;
        idxes.clear();
        int num_kept = 0;
        if (predicts.size() > 0) {
            TIMER_START(t0);
            std::stable_sort(predicts.begin(), predicts.end(), BoxSortDecendScore);
            sprintf(str, "Sort Box (batch %d)", b);
            TIMER_END(t1, t0, str);
            TIMER_START(t0);
            ApplyNms(predicts, idxes, nms_threshold_);
            num_kept = idxes.size();
            sprintf(str, "NMS %d Boxes (batch %d)", num_kept, b);
            TIMER_END(t1, t0, str);
            if (num_kept > KEEP_TOP_K)    num_kept = KEEP_TOP_K;
            for (int i=0; i < num_kept; i++)
                total_preds.push_back(predicts[idxes[i]]);
            total_num += num_kept;
        }
    }
    num_out_boxes_ = total_num;
    Dtype *top_data = output_tensors_[0];
    if (total_num == 0) {
        (*output_shapes_)[0].clear();
        (*output_shapes_)[0].push_back(1);
        (*output_shapes_)[0].push_back(1);
        (*output_shapes_)[0].push_back(num);
        (*output_shapes_)[0].push_back(7);
        num_out_boxes_ = num;
        // Generate fake results per image.
        for (int i = 0; i < num; ++i) {
            top_data[0] = i;
            for (int j = 1; j < 7; ++j)
                top_data[j] = -1;
            top_data += 7;
        }
    } else {
        (*output_shapes_)[0].clear();
        (*output_shapes_)[0].push_back(1);
        (*output_shapes_)[0].push_back(1);
        (*output_shapes_)[0].push_back(total_num);
        (*output_shapes_)[0].push_back(7);
        for (int i = 0; i < total_num; i++) {
            *top_data = total_preds[i].idx;         // Image_Id
            ++top_data;
            *top_data = total_preds[i].classType;   // label
            ++top_data;
            *top_data = total_preds[i].confidence;  // confidence
            ++top_data;
            *top_data = total_preds[i].x;
            ++top_data;
            *top_data = total_preds[i].y;
            ++top_data;
            *top_data = total_preds[i].w;
            ++top_data;
            *top_data = total_preds[i].h;
            ++top_data;
        }
    }
    TIMER_END(t1, t_total, "Total Time");
    return 0;
}

void cpu_yolov3_detect_outlayer::setParam(void *param, int param_size) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_yolov3_detect_out_param_t, yolov3_detect_out_param,
                                   layer_param_, param_size);
    num_inputs_               = yolov3_detect_out_param->num_inputs_;
    num_classes_              = yolov3_detect_out_param->num_classes_;
    num_boxes_                = yolov3_detect_out_param->num_boxes_;
    nms_threshold_            = yolov3_detect_out_param->nms_threshold_;
    confidence_threshold_     = yolov3_detect_out_param->confidence_threshold_;
    mask_group_size_          = yolov3_detect_out_param->mask_group_size_;
    int i = 0;
    for (auto v : yolov3_detect_out_param->biases_)
        biases_[i++] = v;
    i = 0;
    for (auto v : yolov3_detect_out_param->anchors_scale_)
        anchors_scale_[i++] = v;
    i = 0;
    for (auto v : yolov3_detect_out_param->mask_)
        mask_[i++] = v;
}


int cpu_yolov3_detect_outlayer::reshape(
    void *param, int param_size,
    const vector<vector<int>> &input_shapes,
    vector<vector<int>> &output_shapes) {
    /* Since the number of detect bboxes to be kept is unknown before nms, using
     * max shape [1, 1, batch * keep_top_k, 7]
     */
    output_shapes[0].clear();
    output_shapes[0].push_back(1);
    output_shapes[0].push_back(1);
    output_shapes[0].push_back(input_shapes[0][0] * KEEP_TOP_K);
    output_shapes[0].push_back(7);
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_YOLOV3_DETECTION_OUTPUT, cpu_yolov3_detect_out)

} /* namespace bmcpu */
