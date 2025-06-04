#ifndef CPU_BOX_WITH_NMS_LIMIT_H
#define CPU_BOX_WITH_NMS_LIMIT_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_box_with_nms_limitlayer : public cpu_layer {
public:
    cpu_box_with_nms_limitlayer() {}
    virtual ~cpu_box_with_nms_limitlayer() {}
    int process(void *param, int psize) override;
    int reshape(void *param, int psize, const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes) override;
    int dtype(const void *param, size_t psize, const vector<int> &input_dtypes,
              vector<int> &output_dtypes) override;
private:
    bool cls_agnostic_bbox_reg_;
    bool input_boxes_include_bg_cls_;
    int input_scores_fg_cls_starting_id_;
    int get_box_cls_index(int bg_fg_cls_id);
    int get_score_cls_index(int bg_fg_cls_id);
};
} // namespace bmcpu
#endif // CPU_BOX_WITH_NMS_LIMIT_H
