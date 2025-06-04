#ifndef CPU_ONNX_NMS_H
#define CPU_ONNX_NMS_H

#include "cpu_layer.h"

/*
 *  input tensor: float box[batch_num, num_boxes, 4], float score[batch_num, num_calss, num_boxes],
 *                int max_output_boxes_per_class, iou_threshold, score_threshold
 *  input param: int center_point_box, 0: [y1, x1, y2, x2](tf model); 1: [x_center, y_center, width, height]
 *  output tensor: int indices[batch_index, class_index, box_index], len(indices) <= max_output_boxes_per_class*batch_num*num_class
 *
 */

namespace bmcpu {

class cpu_onnx_nmslayer : public cpu_layer {
public:
    explicit cpu_onnx_nmslayer() {}
    virtual ~cpu_onnx_nmslayer() {}

    int process(void* param, int param_size);
    void setParam(void* param, int param_size);
//    inline bool IOU_greater_than_threshold(const float *box_i, const float *box_j);
//    bool check_boxes_suppression(const float* box, int i, int j);

    int reshape(void* param, int psize,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) {
        BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_onnx_nms_param_t, rip, param, psize);
        output_shapes = {{rip->max_output_size * input_shapes[1][0] * input_shapes[1][1], 3}};
        return 0;
    }
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes.assign(1, CPU_DTYPE_INT32);
      return 0;
    }

private:
    float iou_threshold;
    int max_output_size;
    float score_threshold;
    bool center_point_box;
};

} /* namespace bmcpu */

#endif
