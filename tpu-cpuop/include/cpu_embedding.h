#ifndef CPU_EMBEDDING_H
#define CPU_EMBEDDING_H

#include "cpu_layer.h"

/*  TODO
 *  there are two tensor，tensor[0] is params, tensor[1] is indices.
 *  param is axis.
 */

namespace bmcpu {

class cpu_embeddinglayer : public cpu_layer {
public:
    explicit cpu_embeddinglayer() {}
    virtual ~cpu_embeddinglayer() {}

    int process(void* param, int param_size);
    void setParam(void* param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) ;
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
              vector<int>& output_dtypes) override
    {
      output_dtypes.assign(1, input_dtypes[1]);
      return 0;
    }

protected:
    int* padding_idx_;
};
} /* namespace bmcpu */

#endif // CPU_EMBEDDING_H
