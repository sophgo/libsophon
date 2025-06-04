#ifndef CPU_NMS_H
#define CPU_NMS_H

#include "cpu_layer.h"

#define MIN(x, y) (((x)) < ((y)) ? (x) : (y))

/*
 *  input tensor: float box[num_boxes, 4], float score[num_boxes], int max_output_size,
 *  input param: float iou_threshold, float score_threshold
 *  output tensor: int indices[M], M <= max_output_size
 *
 */

namespace bmcpu {

class cpu_nmslayer : public cpu_layer {
public:
    explicit cpu_nmslayer() {}
    virtual ~cpu_nmslayer() {}

    int process(void* param, int param_size);
    void setParam(void* param, int param_size);
//    inline bool IOU_greater_than_threshold(const float *box_i, const float *box_j);
//    bool check_boxes_suppression(const float* box, int i, int j);

    int reshape(void* param, int psize,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) {
        BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_nms_t, rip, param, psize);
        output_shapes = {{MIN(input_shapes[0][0], rip->max_output_size)}};
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
};

} /* namespace bmcpu */


#endif // CPU_NMS_H
