#ifndef _CPU_GATHER_ND_LAYER_H
#define _CPU_GATHER_ND_LAYER_H
#include "cpu_layer.h"
#include <vector>

namespace bmcpu{
//gather_nd layer for Mxnet
class cpu_gatherndlayer : public cpu_layer {
public:
    explicit cpu_gatherndlayer() {}
    virtual ~cpu_gatherndlayer() {}

    int process(void *param, int param_size);

    virtual string get_layer_name() const {
        return "GATHER_ND(MXNET)";
    }

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    virtual int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                      vector<int> &output_dtypes) override {
        output_dtypes.assign(1, input_dtypes[0]);
        return 0;
    }
private:
    int extract_from_indice(const int* ptr, vector<int> shape, int i0, int i1);

};

}/* namespace bmcpu */

#endif  /* _CPU_GATHER_ND_LAYER_H */
