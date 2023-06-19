#ifndef _CPU_YOLOV3_DETECT_OUT_LAYER_H
#define _CPU_YOLOV3_DETECT_OUT_LAYER_H
#include "cpu_layer.h"

namespace bmcpu {

class cpu_yolov3_detect_outlayer : public cpu_layer {
public:
    explicit cpu_yolov3_detect_outlayer() {}
    virtual ~cpu_yolov3_detect_outlayer() {}

    /* dowork */
    //virtual void process() {}
    int process(void *param, int param_size);
    void setParam(void *param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    virtual string get_layer_name () const {
        return "YOLOV3 DETECTION OUTPUT";
    }
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes.assign(1, input_dtypes[0]);
      return 0;
    }

protected:
    int num_inputs_;
    int num_classes_;
    int num_boxes_;
    int num_out_boxes_;

    float confidence_threshold_;
    float nms_threshold_;

    int mask_group_size_;

    float biases_[18];
    float anchors_scale_[3];
    float mask_[9];
};

} /* namespace bmcpu */

#endif /* _CPU_YOLOV3_DETECT_OUT_LAYER_H */

