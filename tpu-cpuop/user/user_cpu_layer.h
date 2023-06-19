#ifndef _USER_CPU_LAYER_H_
#define _USER_CPU_LAYER_H_

#include "user_bmcpu_macro.h"
#include "user_bmcpu_common.h"
#include "user_cpu_layer_factory.h"

namespace usercpu {

inline int usercpu_shape_count(vector<int> shape_v)
{
    int count = 1;
    for(auto& shape : shape_v)
        count *= shape;

    return count;
}

class user_cpu_layer {
public:
    user_cpu_layer() {}
    ~user_cpu_layer() {}

    /* dowork */
    virtual int process(void *parm)=0;

    int set_common_param(
          const vector<float *>& input_tensors,
          const vector<vector<int>>& input_shapes,
          const vector<float *>& output_tensors,
          vector<vector<int>>& output_shape);

    virtual int reshape(
          void* param,
          const vector<vector<int>>& input_shapes,
          vector<vector<int>>& output_shapes)
    {
        cout << "default reshape: do nothing "<< endl;
        return 0;
    }

    virtual int dtype(void* param,
          const vector<int> &input_dtypes,
          vector<int> &output_dtypes)
    {
        cout << "default dtype: do nothing "<< endl;
        return 0;
    }

protected:

    vector<float *> input_tensors_;
    vector<vector<int>> input_shapes_;

    vector<float *> output_tensors_; 
    //vector<vector<int>> output_shapes_;
    vector<vector<int>>* output_shapes_;

    /* layer specific param */
    void* layer_param_;
};

} /* namespace usercpu */

#endif /* _USER_CPU_LAYER_H_ */
