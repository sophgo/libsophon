#ifndef CPU_UTILS_H
#define CPU_UTILS_H

#include <iostream>
#include <vector>
#include "bmcpu_common.h"
template<typename T>
void cpu_print_tensor(const T* data, const int* shape, const int dim, std::ostream& out);
extern template void cpu_print_tensor<float>(const float*, const int*, const int, std::ostream&);
extern template void cpu_print_tensor<int>(const int*, const int*, const int, std::ostream&);
void cpu_print_tensor_ex(const void* data, const int* shape, const int dims, CPU_DATA_TYPE_T dtype, const char* filename);

const char* get_cpu_layer_name(CPU_LAYER_TYPE_T t);
std::vector<CPU_DATA_TYPE_T> get_input_dtype_vector(CPU_LAYER_TYPE_T t);
std::vector<CPU_DATA_TYPE_T> get_output_dtype_vector(CPU_LAYER_TYPE_T t);
#endif // CPU_UTILS_H

