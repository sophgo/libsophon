#ifndef _CPU_ANAKIN_DETECT_OUT_LAYER_H
#define _CPU_ANAKIN_DETECT_OUT_LAYER_H
#include "cpu_layer.h"

#include "bbox_util_opt.hpp"

namespace bmcpu {

class cpu_anakin_detect_outlayer : public cpu_layer {
public:
    explicit cpu_anakin_detect_outlayer() {}
    virtual ~cpu_anakin_detect_outlayer() {}

    /* dowork */
    //virtual void process() {}
    int process(void *param, int param_size);
    void setParam(void *param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    virtual string get_layer_name () const {
        return "ANAKIN DETECTION OUTPUT";
    }

protected:
    int num_classes_;
    bool share_location_;

    int background_label_id_;

    float nms_threshold_;
    int top_k_;

    CodeType code_type_;
    int keep_top_k_;
    float confidence_threshold_;

    //int num_;
    int num_priors_;
    int num_loc_classes_;
    bool variance_encoded_in_target_;
    float eta_;
    float objectness_score_;

};

} /* namespace bmcpu */

#endif /* _CPU_ANAKIN_DETECT_OUT_LAYER_H */

