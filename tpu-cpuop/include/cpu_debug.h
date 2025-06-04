#ifndef CPU_DEBUG_H
#define CPU_DEBUG_H

#include "cpu_layer.h"
#include "cpu_utils.h"

namespace bmcpu {

class cpu_debuglayer: public cpu_layer {
public:
    explicit cpu_debuglayer() {}
    virtual ~cpu_debuglayer() {}

    int process(void *param, int param_size){
        auto layer_param = (cpu_debug_param_t*)param;
        string filename = "tensor_";
        filename +=std::to_string(layer_param->tensor_id);
        filename +="_dtype"+std::to_string(layer_param->tensor_dtype);
        if(layer_param->from_layer_id>=0){
            filename +="_from_layer_id"+std::to_string(layer_param->from_layer_id);
            filename +="_type"+std::to_string(layer_param->from_layer_type);
        }
        filename += ".dat";
        cpu_print_tensor_ex(input_tensors_[0], input_shapes_[0].data(),
                input_shapes_[0].size(), (CPU_DATA_TYPE_T)layer_param->tensor_dtype, filename.c_str());
        size_t mem_size = 1;
        for(auto s: input_shapes_[0]){
            mem_size *= s;
        }
        if(layer_param->tensor_dtype == CPU_DTYPE_FP32 || layer_param->tensor_dtype == CPU_DTYPE_INT32){
            mem_size *= 4;
        } else if(layer_param->tensor_dtype == CPU_DTYPE_INT16 || layer_param->tensor_dtype == CPU_DTYPE_FP16){
            mem_size *= 2;
        }
        memcpy(output_tensors_[0], input_tensors_[0], mem_size);
        output_shapes_->assign(input_shapes_.begin(), input_shapes_.end());
        return 0;
    }

    virtual string get_layer_name () const {
        return "DEBUG";
    }

    // cpu_layer interface
    int reshape(void *param, int param_size,
                const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes){
        output_shapes = input_shapes;
        return 0;
    }

    int dtype(const vector<int> &input_dtypes, vector<int> &output_dtypes) {
        output_dtypes = input_dtypes;
        return 0;
    }
};

}

#endif // CPU_DEBUG_H
