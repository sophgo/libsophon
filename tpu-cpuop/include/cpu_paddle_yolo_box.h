#ifndef _CPU_PADDLE_YOLO_BOXLAYER_H
#define _CPU_PADDLE_YOLO_BOXLAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_paddle_yolo_boxlayer : public cpu_layer {
public:
    cpu_paddle_yolo_boxlayer() {}
    ~cpu_paddle_yolo_boxlayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);

    void setParam(void *param, int psize);

protected:
    int anchors_[100];
    int anchors_size_;
    float conf_thresh_;
    int class_num_;
    int downsample_ratio_;
    bool iou_aware_;
    bool clip_bbox_;
    float iou_aware_factor_;
    float scale_;

};
} // namespace bmcpu
#endif  /* _CPU_PADDLE_YOLO_BOX_LAYER_H */
