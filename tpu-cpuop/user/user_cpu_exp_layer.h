#ifndef _USER_CPU_EXP_LAYER_H
#define _USER_CPU_EXP_LAYER_H
#include "user_cpu_layer.h"

namespace usercpu {

/*
*  notice: define new cpu layer, must like xxx_layer 
* 
*/
class cpu_exp_layer : public user_cpu_layer {
public:
    explicit cpu_exp_layer() {}
    virtual ~cpu_exp_layer() {}

    /* dowork */
    int process(void *parm);
    void setParam(void *param);

    int reshape(void* param,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    int dtype(void* param,
              const vector<int>& input_dtypes,
              vector<int>& output_dtypes);

    virtual string get_layer_name () const {
        return "USEREXP";
    }

protected:
    float inner_scale_;
    float outer_scale_;
};

} /* namespace usercpu */
#endif /* _USER_CPU_EXP_LAYER_H */

