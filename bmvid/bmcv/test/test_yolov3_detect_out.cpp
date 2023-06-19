#include <time.h>
#include <random>
#include <algorithm>
#include <map>
#include <vector>
#include <iostream>
#include <cmath>
#include <getopt.h>
//#include "bmdnn_api.h"
//#include "common.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <iostream>
#include <new>
#include <new>
#include <fstream>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <windows.h>
  #include "time.h"
#endif
using namespace std;

static bm_handle_t handle;

using std::vector;
using std::map;

#define KEEP_TOP_K    200
#define Dtype float
#define TIME_PROFILE

// #define YOLOV3_DEBUG
#define YOLOV3_DEPLOY_TPU
#define BM1684X 0x1686

int DEV_ID = 0;
int B = 1;
int H = 19, W = 19;
int bottom_num = 3;
float CONF_THRESHOLD = 0.2f;
float NMS_THRESHOLD = 0.45f;
int dev_count;
int f_data_from_file = 0;
int f_tpu_forward = 1;

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
                                           c_cephes_exp_p4, c_cephes_exp_p5 };
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
  for (i = j; i < n; i++) {
    *p1++ = 1.0f / (1.0f + exp(-(*p0++)));
  }
}
#else

inline void sigmoid_batch(Dtype *x, const int n) {
  for (int i = 0; i < n; ++i) {
    x[i] = 1.0f / (1.0f + exp(-x[i]));
  }
}

#endif

inline Dtype sigmoid(Dtype x) {
  return 1.0f / (1.0f + exp(-x));
}

static int gen_test_size(int &batch_num,
                        int &num_classes,
                        int &keep_top_k,
                        float &nms_threshold,
                        float &conf_threshold,
                        int &H,
                        int &W,
                        int gen_mode) {

  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;

  // ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != bm_get_chipid(handle, &chipid))
    return ret;
  if (chipid == BM1684X){
    switch (gen_mode) {
      case (0): {
        #if 0
        batch_num = 2;//(rand() % 4 ) + 1
        num_classes = 14;//rand() % 86 + 1;
        keep_top_k = 15000;//rand() % top_k + 1;
        nms_threshold = 0.5f;//((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        conf_threshold = 0.01f;//tmp > 0.9 ? tmp : 0.9;
        H = 19;
        W = 19;
        #else
        #if defined(LOCAL_MEM_ADDRWIDTH) && LOCAL_MEM_ADDRWIDTH == 17
        batch_num = (rand() % 4 ) + 1;
        #else
        batch_num = (rand() % 12 ) + 1;
        #endif
        num_classes = rand() % 80 + 1;
        keep_top_k = rand() % 400 + 1;
        nms_threshold = 0.45f;// ((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        //conf_threshold =tmp > 0.9 ? tmp : 0.9;
        conf_threshold =  0.2f;//((float)(rand() % 10)) / 10 + 0.09;
        H = 13;
        W = 13;
        #endif
        break;
      }
      case (1):{
        #if defined(LOCAL_MEM_ADDRWIDTH) && LOCAL_MEM_ADDRWIDTH == 17
        batch_num = (rand() % 4 ) + 1;
        #else
        batch_num = (rand() % 12 ) + 1;
        #endif
        num_classes = rand() % 80 + 1;
        keep_top_k = rand() % 400 + 1;
        nms_threshold = 0.45f;//((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        //conf_threshold =tmp > 0.9 ? tmp : 0.9;
        conf_threshold =  0.2f;//((float)(rand() % 10)) / 10 + 0.09;
        H = 19;
        W = 19;
        break;
      }
      //test case for nms_v2
      case (2): {
        batch_num = (rand() % 2 ) + 1;
        num_classes = rand() % 80 + 1;
        keep_top_k = rand() % 1000 + 1;
        nms_threshold = ((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        conf_threshold = 0.01f;//tmp > 0.9 ? tmp : 0.9;
        H = 19;
        W = 19;
        break;
      }
      //test case for yolov7 post handle
      case (3): {
        batch_num = 32;//(rand() % 2 ) + 1;
        num_classes = 6;//rand() % 80 + 1;
        keep_top_k = 100;//rand() % 400 + 1;
        nms_threshold = 0.1;//((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        conf_threshold = 0.98f;//tmp > 0.9 ? tmp : 0.9;
        H = 16;
        W = 30;
        break;
      }
      default: {
        cout << "gen mode error" << endl;
        exit(-1);
      }
    }
  }else if (chipid == 0x1684){
    switch (gen_mode) {
      case (0): {
        #if 0
        batch_num = 2;//(rand() % 4 ) + 1
        num_classes = 14;//rand() % 86 + 1;
        keep_top_k = 100;//rand() % top_k + 1;
        nms_threshold = 0.5f;//((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        conf_threshold = 0.5f;//tmp > 0.9 ? tmp : 0.9;
        H = 19;
        W = 19;
        #else
        batch_num = 1;//(rand() % 16 ) + 1;
        num_classes = rand() % 80 + 1;
        keep_top_k = rand() % 400 + 1;
        nms_threshold = 0.45f;// ((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        //conf_threshold =tmp > 0.9 ? tmp : 0.9;
        conf_threshold =  0.2f;//((float)(rand() % 10)) / 10 + 0.09;
        H = 13;
        W = 13;
        #endif
        break;
      }
      case (1):{
        batch_num = 1;//(rand() % 16 ) + 1;
        num_classes = rand() % 80 + 1;
        keep_top_k = rand() % 400 + 1;
        nms_threshold = 0.45f;//((float)(rand() % 10)) / 10 + 0.01;
        //make sure the conf_threshold >= 0.9
        //float tmp = ((float)(rand() % 10)) / 10 + 0.09;
        //conf_threshold =tmp > 0.9 ? tmp : 0.9;
        conf_threshold =  0.2f;//((float)(rand() % 10)) / 10 + 0.09;
        H = 19;
        W = 19;
        break;
      }
      default: {
        cout << "gen mode error" << endl;
        exit(-1);
      }
    }
  }else {
    cout << "not supported !\n" << endl;
    ret = BM_NOT_SUPPORTED;
  }

  return 0;
}

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
  float l1 = x1 - w1 / 2;
  float l2 = x2 - w2 / 2;
  float left = l1 > l2 ? l1 : l2;
  float r1 = x1 + w1 / 2;
  float r2 = x2 + w2 / 2;
  float right = r1 < r2 ? r1 : r2;
  return right - left;
}

inline Dtype box_intersection(vector<Dtype> a, vector<Dtype> b) {
  float w = overlap(a[0], a[2], b[0], b[2]);
  float h = overlap(a[1], a[3], b[1], b[3]);

  if (w < 0 || h < 0) {
    return 0;
  }
  float area = w*h;
  return area;
}

inline Dtype box_union(vector<Dtype> a, vector<Dtype> b) {
  float i = box_intersection(a, b);
  float u = a[2] * a[3] + b[2] * b[3] - i;
  return u;
}

inline Dtype box_iou(vector<Dtype> a, vector<Dtype> b) {
  return box_intersection(a, b) / box_union(a, b);
}

inline Dtype box_intersection(float *a, float *b) {
  float w = overlap(a[0], a[2], b[0], b[2]);
  float h = overlap(a[1], a[3], b[1], b[3]);

  if (w < 0 || h < 0) {
    return 0;
  }
  float area = w * h;
  return area;
}

inline Dtype box_union(float *a, float *b) {
  float i = box_intersection(a, b);
  float u = a[2] * a[3] + b[2] * b[3] - i;
  return u;
}

inline Dtype box_iou(float *a, float *b) {
  return box_intersection(a, b) / box_union(a, b);
}

void ApplyNms(vector<PredictionResult>& boxes, vector<int>& idxes, Dtype threshold) {
  map<int, int> idx_map;
  for (int i = 0; i < (int)boxes.size() - 1; ++i) {
    if (idx_map.find(i) != idx_map.end()) {
      continue;
    }

    for (int j = i + 1; j < (int)boxes.size(); ++j) {
      if (idx_map.find(j) != idx_map.end()) {
        continue;
      }
      vector<Dtype> Bbox1, Bbox2;
      Bbox1.push_back(boxes[i].x);
      Bbox1.push_back(boxes[i].y);
      Bbox1.push_back(boxes[i].w);
      Bbox1.push_back(boxes[i].h);

      Bbox2.push_back(boxes[j].x);
      Bbox2.push_back(boxes[j].y);
      Bbox2.push_back(boxes[j].w);
      Bbox2.push_back(boxes[j].h);

      Dtype iou = box_iou(Bbox1, Bbox2);
      if (iou >= threshold) {
        idx_map[j] = 1;
      }
    }
  }

  for (int i = 0; i < (int)boxes.size(); ++i) {
    if (idx_map.find(i) == idx_map.end()) {
      idxes.push_back(i);
    }
  }
}

void ApplyNms_opt(vector<PredictionResult>& boxes, vector<int>& idxes, Dtype threshold) {
  int bbox_cnt = (int)boxes.size();
  #if 0
  Dtype* ptr = (float*)boxes.data();
  int interval = sizeof(PredictionResult) / sizeof(float);
  #endif

  // init the map
  u32 map[bbox_cnt / 32 + 1];
  memset(map, 0xFF, sizeof(map));

  for (int i = 0; i < bbox_cnt - 1; ++i) {
    // skip the dropped bbox
    if (!(map[i / 32] & (1 << (i % 32))))
      continue;

    for (int j = i + 1; j < bbox_cnt; ++j) {
      // skip the dropped bbox
      if (!(map[j / 32] & (1 << (j % 32))))
        continue;

      Dtype Bbox1[4], Bbox2[4];
      #if 0
      Bbox1[0] = *(Dtype*)(ptr + i * interval + 0);
      Bbox1[1] = *(Dtype*)(ptr + i * interval + 1);
      Bbox1[2] = *(Dtype*)(ptr + i * interval + 2);
      Bbox1[3] = *(Dtype*)(ptr + i * interval + 3);
      Bbox2[0] = *(Dtype*)(ptr + j * interval + 0);
      Bbox2[1] = *(Dtype*)(ptr + j * interval + 1);
      Bbox2[2] = *(Dtype*)(ptr + j * interval + 2);
      Bbox2[3] = *(Dtype*)(ptr + j * interval + 3);
      #else
      Bbox1[0] = boxes[i].x;
      Bbox1[1] = boxes[i].y;
      Bbox1[2] = boxes[i].w;
      Bbox1[3] = boxes[i].h;
      Bbox2[0] = boxes[j].x;
      Bbox2[1] = boxes[j].y;
      Bbox2[2] = boxes[j].w;
      Bbox2[3] = boxes[j].h;
      #endif

      Dtype iou = box_iou(Bbox1, Bbox2);
      if (iou >= threshold) {
        map[j / 32] &= ~(1 << (j % 32));
      }
    }
  }

  for (int i = 0; i < bbox_cnt; ++i) {
    if (map[i / 32] & (1 << (i % 32))) {
      idxes.push_back(i);
    }
  }
}

void get_region_box2(
    vector<Dtype> &b, Dtype* x, Dtype* biases, int n,
    int index, int i, int j, int lw, int lh, int w, int h, int stride) {
  b.clear();
  b.push_back((i + (x[index + 0 * stride])) / lw);
  b.push_back((j + (x[index + 1 * stride])) / lh);
  b.push_back(exp(x[index + 2 * stride]) * biases[2 * n] / (w));
  b.push_back(exp(x[index + 3 * stride]) * biases[2 * n + 1] / (h));
}

void get_region_box2_opt(
    vector<Dtype> &b, Dtype* x, Dtype* biases, int n,
    int i, int j, int lw, int lh, int w, int h) {
  b.clear();
  b.push_back((i + (x[0])) / lw);
  b.push_back((j + (x[1])) / lh);
  b.push_back(exp(x[2]) * biases[2 * n] / (w));
  b.push_back(exp(x[3]) * biases[2 * n + 1] / (h));
}

bool BoxSortDecendScore(const PredictionResult& box1, const PredictionResult& box2) {
  return box1.confidence > box2.confidence;
}

static bool result_cmp(float* output_bmdnn, float* output_native, int roi_num) {
  bool res = true;
  for (int i = 0; i < roi_num; i++) {
    for (int j = 0; j < 7; j++) {
      if (fabs(output_bmdnn[i * 7 + j] - output_native[i * 7 + j]) > 1e-6) {
        printf("ROI idx %d compare failed\n", i);
        res = false;
        break;
      }
    }
  }
  if (false == res) {
    printf("Expected:\n");
    for (int i = 0; i < roi_num; i++) {
      printf("%d: ", i);
      for (int k = 0; k < 7; k++) printf(" %f", output_native[i * 7 + k]);
      printf("\n");
    }
    printf("Got:\n");
    for (int i = 0; i < roi_num; i++) {
      printf("%d: ", i);
      for (int k = 0; k < 7; k++) printf(" %f", output_bmdnn[i * 7 + k]);
      printf("\n");
    }
  }
  return res;
}

#if defined(YOLOV3_DEBUG)
float* data_argmax[3];
float* index_argmax[3];
float* tx[3];
float* ty[3];
float* tw[3];
float* th[3];

vector< PredictionResult > predicts_filtered;
vector< PredictionResult > predicts;
#endif

static int native_yolov3_detect_out(
    float* data_bottom[3], int bottom_num,
    int batch_num, int hw_shape[3][2], int num_classes, int num_boxes,
    int mask_group_size, float nms_threshold, float conf_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    float* data_top) {
#ifdef TIME_PROFILE
  struct timeval tpstart;
  gettimeofday(&tpstart, NULL);
#endif

  const int num = batch_num;
  int len = 4 + num_classes + 1;
  int mask_offset = 0;
  int side_;
  int groups_num_ = mask_group_size;
  int total_num = 0;

  vector< PredictionResult > total_preds;
  total_preds.clear();

  char str[256];
  TIMER_START(t_total);
  vector<Dtype> class_score;
  for (int b = 0; b < batch_num; b++) {
    #if !defined(YOLOV3_DEBUG)
    vector< PredictionResult > predicts;
    #endif
    predicts.clear();
    mask_offset = 0;

    TIMER_START(t0);
    for (int t = 0; t < bottom_num; t++) {
      side_ = hw_shape[t][1];
      int stride = side_ * side_;

      Dtype* swap_data;
      try {
        swap_data = new Dtype[num * num_boxes * len *
                                   side_ * hw_shape[t][0]];
      }
      catch(std::bad_alloc &memExp)
      {
        std::cerr<<memExp.what()<<std::endl;
        exit(-1);
      }
      const Dtype* input_data = data_bottom[t];
      for (int s = 0; s < side_*side_; s++) {
        for (int n = 0; n < num_boxes; n++) {
          int index = n * len * stride + s +
                      b * num_boxes * len *
                      hw_shape[t][0] * side_;
          vector<Dtype> pred;
          class_score.clear();

          for (int c = 0; c < len; ++c) {
            int index2 = c*stride + index;
            if (c == 2 || c == 3) {
              swap_data[index2] = (input_data[index2 + 0]);
            } else {
              if (c > 4) {
                class_score.push_back(input_data[index2 + 0]);
              } else {
                swap_data[index2] = sigmoid(input_data[index2 + 0]);
              }
            }
          }

          int y2 = s / side_;
          int x2 = s % side_;
          Dtype obj_score = swap_data[index + 4 * stride];

          PredictionResult predict;
          Dtype max_score = *std::max_element(class_score.begin(), class_score.end());
          int arg_max = std::distance(class_score.begin(),
            std::max_element(class_score.begin(), class_score.end()));

          get_region_box2(
              pred, swap_data, bias, mask[n + mask_offset], index,
              x2, y2, side_, side_, side_*anchor_scale[t], side_*anchor_scale[t], stride);

          max_score = sigmoid(max_score);
          max_score *= obj_score;

          #if defined(YOLOV3_DEBUG)
          data_argmax[t][b * num_boxes * side_ * side_ + num_boxes * s + n] = max_score;
          index_argmax[t][b * num_boxes * side_ * side_ + num_boxes * s + n] = arg_max;
          tx[t][b * num_boxes * side_ * side_ + num_boxes * s + n] = pred[0];
          ty[t][b * num_boxes * side_ * side_ + num_boxes * s + n] = pred[1];
          tw[t][b * num_boxes * side_ * side_ + num_boxes * s + n] = pred[2];
          th[t][b * num_boxes * side_ * side_ + num_boxes * s + n] = pred[3];
          #endif

          if (max_score > conf_threshold) {
            predict.idx = b;
            predict.x = pred[0];
            predict.y = pred[1];
            predict.w = pred[2];
            predict.h = pred[3];
            predict.classType = arg_max;
            predict.confidence = max_score;
            predicts.push_back(predict);
          }
        }
      }
      mask_offset += groups_num_;
      delete[] swap_data;
    }
    sprintf(str, "Filter %d Boxes (batch %d)", (int)predicts.size(), b);
    TIMER_END(t1, t0, str);

    // NMS for each image
    vector<int> idxes;
    idxes.clear();

    int num_kept = 0;
    if (predicts.size() > 0) {
      #if defined(YOLOV3_DEBUG)
      predicts_filtered = predicts;
      #endif

      TIMER_START(t0);
      std::stable_sort(predicts.begin(), predicts.end(), BoxSortDecendScore);
      sprintf(str, "Sort Box (batch %d)", b);
      TIMER_END(t1, t0, str);

      TIMER_START(t0);
      ApplyNms(predicts, idxes, nms_threshold);
      num_kept = idxes.size();
      sprintf(str, "NMS %d Boxes (batch %d)", num_kept, b);
      TIMER_END(t1, t0, str);

      if (keep_top_k > 0) {
        if (num_kept > keep_top_k)    num_kept = keep_top_k;
      } else {
        if (num_kept > KEEP_TOP_K)    num_kept = KEEP_TOP_K;
      }

      for (int i=0; i < num_kept; i++) {
        total_preds.push_back(predicts[idxes[i]]);
      }
      total_num += num_kept;
    }
  }

  Dtype* top_data = data_top;

  if (total_num == 0) {
    total_num = num;
    // Generate fake results per image.
    for (int i = 0; i < num; ++i) {
      top_data[0] = i;
      for (int j = 1; j < 7; ++j)
        top_data[j] = -1;
      top_data += 7;
    }
  } else {
    for (int i = 0; i < total_num; i++) {
      top_data[i*7+0] = total_preds[i].idx;         // Image_Id
      top_data[i*7+1] = total_preds[i].classType;   // label
      top_data[i*7+2] = total_preds[i].confidence;  // confidence
      top_data[i*7+3] = total_preds[i].x;
      top_data[i*7+4] = total_preds[i].y;
      top_data[i*7+5] = total_preds[i].w;
      top_data[i*7+6] = total_preds[i].h;
    }
  }
  TIMER_END(t1, t_total, "Total Time");

  return total_num;
}

static int native_yolov3_detect_out_opt(
    float* data_bottom[3], int bottom_num,
    int batch_num, int hw_shape[3][2], int num_classes, int num_boxes,
    int mask_group_size, float nms_threshold, float conf_threshold,
    int keep_top_k, float bias[18], float anchor_scale[3], float mask[9],
    float* data_top) {
  UNUSED(conf_threshold);

  const int num = batch_num;
  int len = 4 + num_classes + 1;
  int mask_offset = 0;
  int total_num = 0;

  vector< PredictionResult > total_preds;
  total_preds.clear();

  // calc the threshold for Po
  float po_thres = -std::log(1 / conf_threshold - 1);

  char str[256];
  TIMER_START(t_total);
  vector<Dtype> class_score;
  for (int b = 0; b < batch_num; b++) {
    vector< PredictionResult > predicts;
    predicts.clear();
    mask_offset = 0;

    TIMER_START(t0);
    for (int t = 0; t < bottom_num; t++) {
      int h = hw_shape[t][0];
      int w = hw_shape[t][1];
      int stride = h * w;

      const Dtype* input_data = data_bottom[t];
      #if !defined(YOLOV3_DEPLOY_TPU)
      for (int n = 0; n < num_boxes; n++) {
      #endif
        for (int cy = 0; cy < hw_shape[t][0]; cy++) {
          for (int cx = 0; cx < hw_shape[t][1]; cx++) {
            #if defined(YOLOV3_DEPLOY_TPU)
            for (int n = 0; n < num_boxes; n++) {
            #endif
            int index = b * num_boxes * len * stride +
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
              if (c > 4) {
                class_score.push_back(input_data[index2]);
              } else {
                if (c == 4) {
                  swap_data[0] = input_data[index2];
                } else {
                  swap_data[c + 2] = input_data[index2];
                }
              }
            }

            PredictionResult predict;
            swap_data[1] = *std::max_element(class_score.begin(), class_score.end());
            int arg_max = std::distance(class_score.begin(),
              std::max_element(class_score.begin(), class_score.end()));

            sigmoid_batch(swap_data, 4);

            // Pmax = Pmax * Po
            swap_data[1] = swap_data[0] * swap_data[1];

            if (swap_data[1] > conf_threshold) {
              get_region_box2_opt(
                  pred, &swap_data[2], bias, mask[n + mask_offset],
                  cx, cy, w, h, w * anchor_scale[t], h * anchor_scale[t]);

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
      mask_offset += mask_group_size;
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
      ApplyNms_opt(predicts, idxes, nms_threshold);
      num_kept = idxes.size();
      sprintf(str, "NMS %d Boxes (batch %d)", num_kept, b);
      TIMER_END(t1, t0, str);

      if (keep_top_k > 0) {
        if (num_kept > keep_top_k)    num_kept = keep_top_k;
      } else {
        if (num_kept > KEEP_TOP_K)    num_kept = KEEP_TOP_K;
      }

      for (int i=0; i < num_kept; i++) {
        total_preds.push_back(predicts[idxes[i]]);
      }
      total_num += num_kept;
    }
  }

  Dtype* top_data = data_top;

  if (total_num == 0) {
    total_num = num;
    // Generate fake results per image.
    for (int i = 0; i < num; ++i) {
      top_data[0] = i;
      for (int j = 1; j < 7; ++j)
        top_data[j] = -1;
      top_data += 7;
    }
  } else {
    for (int i = 0; i < total_num; i++) {
      top_data[i*7+0] = total_preds[i].idx;         // Image_Id
      top_data[i*7+1] = total_preds[i].classType;   // label
      top_data[i*7+2] = total_preds[i].confidence;  // confidence
      top_data[i*7+3] = total_preds[i].x;
      top_data[i*7+4] = total_preds[i].y;
      top_data[i*7+5] = total_preds[i].w;
      top_data[i*7+6] = total_preds[i].h;
    }
  }
  TIMER_END(t1, t_total, "Total Time");

  return total_num;
}

static int test_yolov3_detect_out(int test_loop_times) {
  bm_status_t ret = BM_SUCCESS;
  int batch_num = B;
  int num_classes = 80;
  int num_boxes = 3;
  int yolo_flag = 0; //yolov3: 0, yolov5:1, yolov7: 2
  int len_per_batch = 0;
  int keep_top_k = 200;
  float nms_threshold = NMS_THRESHOLD;
  float conf_threshold = CONF_THRESHOLD;
  int mask_group_size = 3;
  float bias[18] = {10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326};
  float anchor_scale[3] = {32, 16, 8};
  float mask[9] = {6, 7, 8, 3, 4, 5, 0, 1, 2};
  int scale = 0; //for yolov7 post handle
  int model_h = 0;
  int model_w = 0;
  int mode_value_end = 0;
  bm_dev_request(&handle, 0);

  unsigned int chipid = BM1684X;
  if (BM_SUCCESS != bm_get_chipid(handle, &chipid))
    return ret;
  int mode_value_start = yolo_flag ==  2 ? 3 : 0;
  if (yolo_flag == 2)
    mode_value_end = 4;
  else
    mode_value_end = chipid == BM1684X ? 3 : 2;

  for (int i = 0; i < test_loop_times; i++){
    std::cout << "------[TEST yolov3_detect_out ] LOOP " << i << "------" << std::endl;
    for (int rand_mode = mode_value_start; rand_mode < mode_value_end; rand_mode++) {
    // for (int rand_mode = 3; rand_mode < 4; rand_mode++) {
      gen_test_size(batch_num, num_classes, keep_top_k, nms_threshold, conf_threshold, H, W, rand_mode);
      int hw_shape[3][2] = {
        {H*1, W*1},
        {H*2, W*2},
        {H*4, W*4},
      };
      std::cout << "rand_mode : " << rand_mode << std::endl;
      printf("Batch:%d, Class_num: %d, Height: %d, Width: %d\n", batch_num, num_classes, H, W);
      printf("Confidence threshold: %f\n", conf_threshold);
      printf("NMS threshold: %f\n", nms_threshold);
      printf("keep_top_k: %d\n", keep_top_k);
      int output_size = 0;
      if (keep_top_k > 0)
        output_size = keep_top_k;
      else if (keep_top_k == 0)
        output_size = 1;
      output_size *= batch_num * 7;

      int size_bottom[3];
      float* data_bottom[3];
      int origin_image_shape[batch_num * 2];
      if (yolo_flag == 1){
        num_boxes = 1;
        len_per_batch = 12096 * 18;
        bottom_num = 1;
        memset(origin_image_shape, 0, batch_num*2*sizeof(int));
      } else if (yolo_flag == 2){
        //yolov7 post handle;
        num_boxes = 1;
        bottom_num = 3;
        mask_group_size = 1;
        scale = 1;
        model_h = 512;
        model_w = 960;
        for (int i = 0 ; i < 3; i++){
          mask[i] = i;
        }

        for (int i = 0; i < 6; i++)
          bias[i] = 1;

        for (int i = 0; i < 3; i++)
          anchor_scale[i] = 1;

        for (int i = 0; i < batch_num; i++){
          origin_image_shape[i*2 + 0] = 1920;
          origin_image_shape[i*2 + 1] = 1080;
        }
      }
      // alloc input data
      for (int i = 0; i < 3; ++i) {
        if (yolo_flag == 1){
          size_bottom[i] = batch_num * len_per_batch;
        } else {
          size_bottom[i] = batch_num * num_boxes *
                          (num_classes + 5) * hw_shape[i][0] * hw_shape[i][1];
        }
        try {
          data_bottom[i] = new float[size_bottom[i]];
        }
        catch(std::bad_alloc &memExp)
        {
          std::cerr<<memExp.what()<<std::endl;
          exit(-1);
        }
      }
      if (f_data_from_file) {
        #if defined(__aarch64__)
        #define DIR     "./imgs/"
        #else
        #define DIR     "test/test_api_bmdnn/bm1684/imgs/"
        #endif
        printf("reading data from: \"" DIR "\"\n");
        char path[256];
        if (yolo_flag == 1) {
          FILE* fp = fopen("./output_ref_data.dat.bmrt", "rb");
          size_t cnt = fread(data_bottom[0],
                  sizeof(float), size_bottom[0]*batch_num, fp);
          cnt = cnt;
          fclose(fp);
        } else {
          for (int i = 0; i < batch_num; ++i) {
            sprintf(path, DIR "b%d_13.bin", i);
            FILE* fp = fopen(path, "rb");
            size_t cnt = fread(data_bottom[0] + i * size_bottom[0] / batch_num,
                  sizeof(float), size_bottom[0] / batch_num, fp);
            cnt = cnt;
            fclose(fp);

            sprintf(path, DIR "b%d_26.bin", i);
            fp = fopen(path, "rb");
            cnt = fread(data_bottom[1] + i * size_bottom[1] / batch_num,
                  sizeof(float), size_bottom[1] / batch_num, fp);
            cnt = cnt;
            fclose(fp);

            sprintf(path, DIR "b%d_52.bin", i);
            fp = fopen(path, "rb");
            cnt = fread(data_bottom[2] + i * size_bottom[2] / batch_num,
                  sizeof(float), size_bottom[2] / batch_num, fp);
            cnt = cnt;
            fclose(fp);
          }
        }
      } else {
  //#define TEST
  #ifdef TEST
        ifstream file_1("1.txt", std::ios::in);
        ifstream file_2("2.txt", std::ios::in);
        ifstream file_3("3.txt", std::ios::in);
  #else
        ofstream file_1("1.txt", std::ios::out);
        ofstream file_2("2.txt", std::ios::out);
        ofstream file_3("3.txt", std::ios::out);
  #endif

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dist(0, 1);

        // alloc and init input data
        for (int j = 0; j < size_bottom[0]; ++j){
          #ifndef TEST
          if (yolo_flag == 2){
            data_bottom[0][j] = dist(gen);
          } else {
            data_bottom[0][j] = (rand() % 1000 - 999.0f) / (124.0f);
          }
          file_1 << data_bottom[0][j] <<endl;
          #else
          file_1 >> data_bottom[0][j];
          #endif
        }

        for (int j = 0; j < size_bottom[1]; ++j){
          #ifndef TEST
          if (yolo_flag == 2){
            data_bottom[1][j] = dist(gen);
          } else {
            data_bottom[1][j] = (rand() % 1000 - 999.0f) / (124.0f);
          }
          file_2 << data_bottom[1][j] <<endl;
          #else
          file_2 >> data_bottom[1][j];
          #endif
        }

        for (int j = 0; j < size_bottom[2]; ++j){
          #ifndef TEST
          if (yolo_flag == 2){
            data_bottom[2][j] = dist(gen);
          } else {
            data_bottom[2][j] = (rand() % 1000 - 999.0f) / (124.0f);
          }
          file_3 << data_bottom[2][j] <<endl;
          #else
          file_3 >> data_bottom[2][j];
          #endif
        }
      }

      // alloc output data
      float* output_bmdnn;
      float* output_native;
      try {
        output_bmdnn = new float[output_size];
        output_native = new float[output_size];
      }
      catch(std::bad_alloc &memExp)
      {
        std::cerr<<memExp.what()<<std::endl;
        exit(-1);
      }
      memset(output_bmdnn, 0, output_size * sizeof(float));
      memset(output_native, 0, output_size * sizeof(float));

      #if defined(YOLOV3_DEBUG)
      for (int i = 0; i < 3; ++i) {
        int size = batch_num * num_boxes * hw_shape[i][0] * hw_shape[i][1];
        try {
          data_argmax[i] = new float[size];
          index_argmax[i] = new float[size];
          tx[i] = new float[size];
          ty[i] = new float[size];
          tw[i] = new float[size];
          th[i] = new float[size];
        }
        catch(std::bad_alloc &memExp)
        {
          std::cerr<<memExp.what()<<std::endl;
          exit(-1);
        }
      }
      #endif

      int total_det = 0;
      // doing forward_cpu
      if (yolo_flag == 0){
        total_det = native_yolov3_detect_out_opt(
          data_bottom, bottom_num,
          batch_num, hw_shape, num_classes, num_boxes,
          mask_group_size, nms_threshold, conf_threshold,
          keep_top_k, bias, anchor_scale, mask,
          output_native);
      }
      #if defined(YOLOV3_DEPLOY_TPU)
      if (f_tpu_forward) {
        // doing forward_tpu
        bm_dev_request(&handle, 0);
        bm_device_mem_t bottom[3] = {
          bm_mem_from_system((void*)data_bottom[0]),
          bm_mem_from_system((void*)data_bottom[1]),
          bm_mem_from_system((void*)data_bottom[2])
        };
        struct timeval t1, t2;
        gettimeofday_(&t1);
        if (yolo_flag == 0 || yolo_flag == 1){
          ret = bmcv_nms_yolov3(
          handle, bottom_num, bottom,
          batch_num, hw_shape, num_classes, num_boxes,
          mask_group_size, nms_threshold, conf_threshold,
          keep_top_k, bias, anchor_scale, mask,
          bm_mem_from_system((void*)output_bmdnn), yolo_flag, len_per_batch);
        }else if (yolo_flag == 2){
          yolov7_info_t *ext = (yolov7_info_t*) malloc (sizeof(yolov7_info_t));
          ext->scale = scale;
          ext->orig_image_shape = origin_image_shape;
          ext->model_h = model_h;
          ext->model_w = model_w;
          ret = bmcv_nms_yolo(
          handle, bottom_num, bottom,
          batch_num, hw_shape, num_classes, num_boxes,
          mask_group_size, nms_threshold, conf_threshold,
          keep_top_k, bias, anchor_scale, mask,
          bm_mem_from_system((void*)output_bmdnn), yolo_flag, len_per_batch, (void*)ext);
          free(ext);
        }else{
          printf("Not supported!");
          ret = BM_ERR_FAILURE;
        }
        gettimeofday_(&t2);
        cout << "nms yolo using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

        //bm_dev_free(handle);

        //if yolov5 post handle, cpu don't calc and don't cmp
        if (yolo_flag == 0) {
          if ((ret == BM_SUCCESS) && result_cmp(output_bmdnn, output_native, total_det)) {
            printf("result compare success\n");
            ret = BM_SUCCESS;
          } else {
            printf("result compare failed or bmdnn failed\n");
            exit(-1);
            ret = BM_ERR_FAILURE;
          }
        }
      }
      #else
      UNUSED(total_det);
      UNUSED(handle);
      #endif

      #if defined(YOLOV3_DEBUG)
      for (int i = 0; i < 3; ++i) {
        delete[] data_argmax[i];
        delete[] index_argmax[i];
        delete[] tx[i];
        delete[] ty[i];
        delete[] tw[i];
        delete[] th[i];
      }
      #endif
      delete[] data_bottom[0];
      delete[] data_bottom[1];
      delete[] data_bottom[2];
    }
  }
  bm_dev_free(handle);
  return 0;
}

void Usage() {
  printf(
      "Usage:\n"
      "\t--devid              : The number of device.\n"
      "\t--query              : Query the total number of device.\n"
      "\t--height             : Height of feature map.\n"
      "\t--width              : Width of feature map.\n"
      "\t--bottom_num         : Number of bottoms.\n"
      "\t--conf_thresh        : Confidence threshold.\n"
      "\t--nms_thresh         : NMS threshold.\n"
      "\t--file               : Data from file.\n"
      "\t--tpu                : TPU forward.\n"
      );
}

void get_dev_cnt() {
  bm_dev_getcount(&dev_count);
  printf("The toal number of DEV: %d\n", dev_count);
}

static char deal_with_options(int argc, char **argv) {
  int ch, option_index = 0;
  static struct option long_options[] = { {"devid", required_argument, NULL, 'i'},
                                          {"height", required_argument, NULL, 'H'},
                                          {"width", required_argument, NULL, 'W'},
                                          {"batch", required_argument, NULL, 'B'},
                                          {"bottom_num", required_argument, NULL, 'b'},
                                          {"conf_thresh", required_argument, NULL, 'c'},
                                          {"nms_thresh", required_argument, NULL, 'n'},
                                          {"file", required_argument, NULL, 'f'},
                                          {"tpu", required_argument, NULL, 't'},
                                          {"query", no_argument, NULL, 'q'},
                                          {"help", no_argument, NULL, 'h'},
                                          {0, 0, 0, 0}};

  while ((ch = getopt_long(argc, argv, "i:h:q:H:W:c:n", long_options,
                           &option_index)) != -1) {
    switch (ch) {
      case 'i':
        DEV_ID = atoi(optarg);
        break;
      case 'q':
        get_dev_cnt();
        exit(0);
        break;
      case 'h':
        Usage();
        exit(0);
        break;
      case 'H':
        H = atoi(optarg);
        break;
      case 'W':
        W = atoi(optarg);
        break;
      case 'B':
        B = atoi(optarg);
        break;
      case 'b':
        bottom_num = atoi(optarg);
        break;
      case 'f':
        f_data_from_file = atoi(optarg);
        break;
      case 't':
        f_tpu_forward = atoi(optarg);
        break;
      case 'c':
        CONF_THRESHOLD = atof(optarg);
        break;
      case 'n':
        NMS_THRESHOLD = atof(optarg);
        break;
      case '?':
        // unknown option
        exit(-1);
        break;
    }
  }

  return ch;
}

int main(int argc, char* argv[]) {
  int test_loop_times  = 1;
  if (argc == 1) {
      test_loop_times  = 1;
  } else if (argc == 2) {
      test_loop_times  = atoi(argv[1]);
  } else {
      std::cout << "command input error, please follow this "
                    "order:test_ssd_detect_out loop_num "
                << std::endl;
      exit(-1);
  }

  if (test_loop_times > 1500 || test_loop_times < 1) {
      std::cout << "[TEST ssd_detect_out] loop times should be 1~1500" << std::endl;
      exit(-1);
  }

  //deal_with_options(argc, argv);
  get_dev_cnt();

  if (DEV_ID >= dev_count) {
    printf("devic exceeds the maximum number: %d\n", dev_count);
    exit(-1);
  }

  int seed = (int)time(NULL);
  printf("random seed %d\n", seed);
  srand(seed);

  test_yolov3_detect_out(test_loop_times);
  return 0;
}
