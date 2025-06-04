#ifndef CPU_GRID_SAMPLER_H
#define CPU_GRID_SAMPLER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_grid_samplerlayer : public cpu_layer {
public:
    cpu_grid_samplerlayer() {}
    virtual ~cpu_grid_samplerlayer() {}
    int process(void *param, int psize) override;
    int reshape(void *param, int psize, const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes) override;
    int dtype(const void *param, size_t psize, const vector<int> &input_dtypes,
              vector<int> &output_dtypes) override;
};
} // namespace bmcpu
#endif // CPU_GRID_SAMPLER_H
