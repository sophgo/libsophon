#ifndef _CPU_RPNPROPOSAL_LAYER_H
#define _CPU_RPNPROPOSAL_LAYER_H
#include "cpu_layer.h"

namespace bmcpu {

class cpu_rpnproposallayer : public cpu_layer {
public:
  explicit cpu_rpnproposallayer() {}
  virtual ~cpu_rpnproposallayer() {}

  /* dowork */
  int process(void *parm, int param_size);
  void setParam(void *param, int param_size);

  virtual string get_layer_name () const {
    return "RPN_PROPOSAL";
  }

  int reshape(void* param, int param_size,
              const vector<vector<int>>& input_shapes,
              vector<vector<int>>& output_shapes) { 
                if (output_shapes[0].size() == 0){
                  output_shapes[0].push_back(200);
                  output_shapes[0].push_back(5);
                  output_shapes[0].push_back(1);
                  output_shapes[0].push_back(1);
                }
                return 0; 
              }
  virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                    vector<int>& output_dtypes) override
  {
    output_dtypes.assign(1, CPU_DTYPE_FP32);
    return 0;
  }

protected:
  struct abox{
    float  batch_ind;
    float  x1;
    float  y1;
    float  x2;
    float  y2;
    float  score;
    float  area;
  };

  void generate_anchors(int* &anchor, int& anchors_nums);
  vector<vector<float>> ratio_enum(vector<float> anchor);
  vector<vector<float>> scale_enum(vector<float> anchor);
  void nms(vector<abox> &input_boxes, float nms_thresh);

  inline void rpn_set(const int N, const float alpha, float* Y)
  {
    if (alpha == 0) {
      memset(Y, 0, sizeof(float) * N);
      return;
    }
    for (int i = 0; i < N; ++i) {
      Y[i] = alpha;
    }
  }

  inline void rpn_axpy(const int N, const float alpha, const float* X, float* Y)
  {
    for(int i = 0; i < N; ++i)
      Y[i] += alpha * X[i];
  }

  inline void rpn_add_scalar(const int N, const float alpha, float* Y)
  {
    for (int i = 0; i < N; ++i) {
      Y[i] += alpha;
    }
  }

  inline void rpn_mul(const int N, const float* a, const float* b, float* y)
  {
    for (int i = 0; i < N; ++i) {
      y[i] = a[i] * b[i];
    }
  }

  inline void rpn_add(const int N, const float* a, const float* b, float* y)
  {
    for (int i = 0; i < N; ++i) {
      y[i] = a[i] + b[i];
    }
  }

  inline void rpn_exp(const int N, const float* a, float* y)
  {
    for (int i = 0; i < N; ++i) {
      y[i] = exp(a[i]);
    }
  }

protected:
  int feat_stride_;
  int min_size_;
  int pre_nms_topN_;
  int post_nms_topN_;
  float nms_thresh_;
  float score_thresh_;
  int base_size_;
  int scales_num_;
  int ratios_num_;
  int anchor_scales_[5];
  float ratios_[5];

};

} /* namespace bmcpu */
#endif /* _CPU_RPNPROPOSAL_LAYER_H */

