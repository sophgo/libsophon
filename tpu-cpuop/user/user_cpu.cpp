#include "user_cpu_layer.h"
#include "user_bmcpu.h"
#include "user_bmcpu_macro.h"
#include "user_bmcpu_common.h"
#ifdef __linux__
#include <dlfcn.h>
#endif

using usercpu::user_cpu_layer;
using usercpu::Registry;

typedef struct {
  map<int, USER_STD::shared_ptr<user_cpu_layer>> cpu_layers_;
} bmcpu_user_handle_t;


/* instance all cpu layer */
void* user_cpu_init()
{
    bmcpu_user_handle_t *bmcpu_user_handle = new bmcpu_user_handle_t();
    auto& cpu_layers_ = bmcpu_user_handle->cpu_layers_;

    cpu_layers_.clear();
    for (int layer_idx = 0; layer_idx < USER_CPU_UNKNOW; layer_idx++) {
        cpu_layers_[layer_idx] = Registry::createlayer(layer_idx);
    }

    return (void *)bmcpu_user_handle;
}

void user_cpu_uninit(void *bmcpu_user_handle)
{
    delete (bmcpu_user_handle_t *)bmcpu_user_handle;
}

/* call op_type user cpu layer's process handle.
 * return 0 if sucesse, else return -1.
 * separate user defined layer
 * open libuser
 */
int user_cpu_process(void *bmcpu_user_handle, void *param,
                  const vector<float *>& input_tensors,
                  const vector<vector<int>>& input_shapes,
                  const vector<float *>& output_tensors,
                  vector<vector<int>>& output_shapes
                  )
{
    int op_type;
    if (NULL == param) {
        return -1;
    }

    auto& cpu_layers_ = ((bmcpu_user_handle_t *)bmcpu_user_handle)->cpu_layers_;
    op_type = ((user_cpu_param *)param)->op_type;
     map<int, USER_STD::shared_ptr<user_cpu_layer> >::iterator iter = cpu_layers_.find(op_type);
     if (iter != cpu_layers_.end()) {
         iter->second->set_common_param(input_tensors,  input_shapes,
                                    output_tensors, output_shapes);
	 printf("get user define layer process %d \n",op_type);
         return iter->second->process(&((user_cpu_param *)param)->u);
     } else {
	 USER_ASSERT(0);
     }

    return -1;
}

/* call op_type user cpu layer's reshape handle.
 * return 0 if sucesse, else return -1.
 */
int user_cpu_reshape(void *bmcpu_user_handle, void *param,
                     const vector<vector<int>>& input_shapes,
                     vector<vector<int>>& output_shapes
                     )
{
    if (NULL == param)
        return -1;

    auto& cpu_layers_ = ((bmcpu_user_handle_t *)bmcpu_user_handle)->cpu_layers_;
    int op_type = ((user_cpu_param *)param)->op_type;
    map<int, USER_STD::shared_ptr<user_cpu_layer> >::iterator iter = cpu_layers_.find(op_type);
    if (iter != cpu_layers_.end()) {
        return iter->second->reshape(param, input_shapes, output_shapes);
    } else {
        USER_ASSERT(0);
    }

    return -1;
}

/* call op_type user cpu layer's dtype handle.
 * return 0 if sucesse, else return -1.
 */
int  user_cpu_dtype(void* bmcpu_user_handle, void *param,
                    const vector<int> &input_dtypes,
                    vector<int> &output_dtypes) {
    if (NULL == param)
        return -1;

    auto& cpu_layers_ = ((bmcpu_user_handle_t *)bmcpu_user_handle)->cpu_layers_;
    int op_type = ((user_cpu_param *)param)->op_type;
    map<int, USER_STD::shared_ptr<user_cpu_layer> >::iterator iter = cpu_layers_.find(op_type);
    if (iter != cpu_layers_.end()) {
        return iter->second->dtype(param, input_dtypes, output_dtypes);
    } else {
        USER_ASSERT(0);
    }

    return -1;
}
