#ifndef _CPU_ARG_SORT_LAYER_H
#define _CPU_ARG_SORT_LAYER_H
#include "cpu_layer.h"
#include <algorithm>
#include <vector>

namespace bmcpu{

class cpu_argsortlayer : public cpu_layer {
public:
    explicit cpu_argsortlayer() {}
    virtual ~cpu_argsortlayer() {}

    int process(void *param, int param_size);

    virtual string get_layer_name() const {
        return "ARGSORT";
    }

    int reshape(
          void* param, int param_size,
          const vector<vector<int>>& input_shapes,
          vector<vector<int>>& output_shapes) {
      output_shapes[0] = input_shapes[0];
      return 0;
    }
    int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
              vector<int>& output_dtypes)
    {
      output_dtypes.clear();
      output_dtypes.push_back(input_dtypes[0]);
      return 0;
    }

private:
    //argsort implementation in c++11 for 1-dim vector 
    template<typename T>
    std::vector<float> argsort(const std::vector<T>& array, bool ascend);

    //calculate the offset using the indice
    int calc_offset(int* base_offset, int *index, int dim);
    //calculate the indice using the offset
    int* calc_dim(int dims, int* base_offset, int offset);
    int* base_offset;
    int* base_offset_trans;
    int len;
};

}/* namespace bmcpu */

#endif  /* _CPU_ARG_SORT_LAYER_H */
