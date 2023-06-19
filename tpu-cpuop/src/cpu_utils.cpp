#include <fstream>
#include <iomanip>
#include "bmcpu_common.h"
#include "cpu_utils.h"

int increase_indice(int* indice, const int* limit, int len){
    int carry_value = 1;
    for(int i=len-1; i>=0; i--){
        indice[i] += carry_value;
        if(indice[i]>=limit[i]){
            carry_value = 1;
            indice[i] = 0;
        } else {
            carry_value = 0;
            break;
        }
    }
    return carry_value == 0;
}
template<typename T>
void cpu_print_tensor(const T* data, const int* shape, const int dim, std::ostream& out){
    auto env_str = getenv("BMCPU_DEBUG_DUMP_BIN");
    bool save_bin = env_str && env_str[0] == '1';
    if(save_bin){
        size_t len =  1;
        for(int i=0; i<dim; i++){
            len *= shape[i];
        }
        out.write((const char*)data, len*sizeof(T));
        return;
    }
    out<<"shape=[";
    for(int i=0; i<dim; i++){
        out<<shape[i]<<",";
    }
    out<<"];"<<std::endl;
    if(dim == 0){
        out<<"data="<<data[0];
        return;
    }

    size_t elem_num = 1;
    for(int i=0; i<dim; i++){
        elem_num *= shape[i];
    }
    out<<"data=["<<std::endl;
    for(size_t i=0; i<elem_num; i++){
        out<<data[i]<<","<<std::endl;
    }
    out<<"]";
}

template void cpu_print_tensor<float>(const float*, const int*, const int, std::ostream& out);
template void cpu_print_tensor<int>(const int*, const int*, const int, std::ostream& out);
void cpu_print_tensor_ex(const void* data, const int* shape, const int dims, CPU_DATA_TYPE_T dtype, const char *filename)
{
    std::ofstream ofs(filename);
    if(dtype == CPU_DTYPE_FP32){
        cpu_print_tensor<float>((const float*) data, shape, dims, ofs);
    } else if(dtype == CPU_DTYPE_INT32){
        cpu_print_tensor<int>((const int*) data, shape, dims, ofs);
    }
}

#define BEGIN_CASE(t) \
    switch(t) {
#define CASE_NAME(name)\
    case name: \
    return #name;
#define END_CASE() \
    default: \
        return "UNKNOWN_CPU_LAYER";\
    }\
    return nullptr;

const char* get_cpu_layer_name(CPU_LAYER_TYPE_T t){
    BEGIN_CASE(t)
    CASE_NAME(CPU_SSD_DETECTION_OUTPUT)
    CASE_NAME(CPU_ANAKIN_DETECT_OUTPUT)
    CASE_NAME(CPU_RPN)
    CASE_NAME(CPU_USER_DEFINED)
    CASE_NAME(CPU_ROI_POOLING)
    CASE_NAME(CPU_ROIALIGN)
    CASE_NAME(CPU_BOXNMS)
    CASE_NAME(CPU_YOLO)
    CASE_NAME(CPU_CROP_AND_RESIZE)
    CASE_NAME(CPU_GATHER)
    CASE_NAME(CPU_NON_MAX_SUPPRESSION)
    CASE_NAME(CPU_ARGSORT)
    CASE_NAME(CPU_GATHERND)
    CASE_NAME(CPU_YOLOV3_DETECTION_OUTPUT)
    CASE_NAME(CPU_WHERE)
    CASE_NAME(CPU_ADAPTIVE_AVERAGE_POOL)
    CASE_NAME(CPU_ADAPTIVE_MAX_POOL)
    CASE_NAME(CPU_TOPK)
    CASE_NAME(CPU_RESIZE_INTERPOLATION)
    CASE_NAME(CPU_GATHERND_TF)
    CASE_NAME(CPU_SORT_PER_DIM)
    CASE_NAME(CPU_WHERE_SQUEEZE_GATHER)
    CASE_NAME(CPU_MASKED_SELECT)
    CASE_NAME(CPU_UNARY)
    CASE_NAME(CPU_EMBEDDING)
    CASE_NAME(CPU_TOPK_MX)
    CASE_NAME(CPU_INDEX_PUT)
    CASE_NAME(CPU_REPEAT_INTERLEAVE)
    CASE_NAME(CPU_DEBUG)
    END_CASE()
}

std::vector<CPU_DATA_TYPE_T> get_input_dtype_vector(CPU_LAYER_TYPE_T t)
{
    std::vector<CPU_DATA_TYPE_T> dtypes{CPU_DTYPE_FP32};
    if(t == CPU_GATHER){
        return {CPU_DTYPE_FP32, CPU_DTYPE_INT32};
    } else if(t==CPU_CROP_AND_RESIZE){
        return {CPU_DTYPE_FP32, CPU_DTYPE_FP32, CPU_DTYPE_INT32};
    } else if(t==CPU_NON_MAX_SUPPRESSION){
        return {CPU_DTYPE_FP32, CPU_DTYPE_FP32, CPU_DTYPE_INT32};
    }
    return dtypes;
}

std::vector<CPU_DATA_TYPE_T> get_output_dtype_vector(CPU_LAYER_TYPE_T t)
{
    std::vector<CPU_DATA_TYPE_T> dtypes{CPU_DTYPE_FP32};
    if(t == CPU_NON_MAX_SUPPRESSION){
        return {CPU_DTYPE_INT32};
    } else if(t==CPU_TOPK){
        return {CPU_DTYPE_FP32, CPU_DTYPE_INT32};
    }
    return dtypes;
}
