#ifndef CPU_EMBEDDING_BAG_H
#define CPU_EMBEDDING_BAG_H

#include "cpu_layer.h"


namespace bmcpu {

class cpu_embedding_baglayer : public cpu_layer {
public:
    explicit cpu_embedding_baglayer() {}
    virtual ~cpu_embedding_baglayer() {}

    int process(void* param, int param_size);
    void setParam(void* param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) ;
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
              vector<int>& output_dtypes) override
    {
        int cpu_dtype = input_dtypes[0];
        output_dtypes.assign(1, cpu_dtype);
        return 0;
    }

protected:
    int num_embeddings_;
    int embedding_dim_;
    EMB_MODE_T mode_;
};
} /* namespace bmcpu */

#endif // CPU_EMBEDDING_BAG_H
