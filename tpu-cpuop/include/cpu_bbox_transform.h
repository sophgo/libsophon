#ifndef CPU_BBOX_TRANSFORM_H
#define CPU_BBOX_TRANSFORM_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_bbox_transformlayer : public cpu_layer {
public:
    cpu_bbox_transformlayer() {}
    virtual ~cpu_bbox_transformlayer() {}
    int process(void *param, int psize) override;
    int reshape(void *param, int psize, const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes) override;
    int dtype(const void *param, size_t psize, const vector<int> &input_dtypes,
              vector<int> &output_dtypes) override;
};
} // namespace bmcpu
#endif // CPU_BBOX_TRANSFORM_H
