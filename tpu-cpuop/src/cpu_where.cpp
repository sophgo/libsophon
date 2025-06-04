#include <string>
#include <stdio.h>
#include <cmath>
#include "cpu_where.h"
#include "cpu_layer.h"

namespace bmcpu {

inline int cpu_wherelayer::where1d(const float *input,
                                   const std::vector<int>& shape,
                                   int* output)
{
    CPU_ASSERT(shape.size() == 1);
    int index_out = 0;
    for (int i=0; i<shape[0]; ++i) {
        if (*input!=0.f){
            *output = i;
            ++output;
            ++index_out;
        }
        ++input;
    }
    return index_out;
}

inline int cpu_wherelayer::where2d(const float *input,
                                   const std::vector<int>& shape,
                                   int* output)
{
    CPU_ASSERT(shape.size() == 2);
    int index_out = 0;
    for (int n=0; n<shape[0]; ++n) {
        for (int c=0; c<shape[1]; ++c) {
            if (*input!=0.f){
                *output = n;
                ++output;
                *output = c;
                ++output;
                ++index_out;
            }
            ++input;
        }
    }
    return index_out;
}

inline int cpu_wherelayer::where3d(const float *input,
                                   const std::vector<int>& shape,
                                   int* output)
{
    CPU_ASSERT(shape.size() == 3);
    int index_out = 0;
    for (int n=0; n<shape[0]; ++n) {
        for (int c=0; c<shape[1]; ++c) {
            for (int h=0; h<shape[2]; ++h) {
                if (*input!=0.f){
                    *output = n;
                    ++output;
                    *output = c;
                    ++output;
                    *output = h;
                    ++output;
                    ++index_out;
                }
                ++input;
            }
        }
    }
    return index_out;
}

inline int cpu_wherelayer::where4d(const float *input,
                                   const std::vector<int>& shape,
                                   int* output)
{
    CPU_ASSERT(shape.size() == 4);
    int index_out = 0;
    for (int n=0; n<shape[0]; ++n) {
        for (int c=0; c<shape[1]; ++c) {
            for (int h=0; h<shape[2]; ++h) {
                for (int w=0; w<shape[3]; ++w) {
                    if (*input!=0.f){
                        *output = n;
                        ++output;
                        *output = c;
                        ++output;
                        *output = h;
                        ++output;
                        *output = w;
                        ++output;
                        ++index_out;
                    }
                    ++input;
                }
            }
        }
    }
    return index_out;
}

inline int cpu_wherelayer::where5d(const float *input,
                                   const std::vector<int>& shape,
                                   int* output)
{
    CPU_ASSERT(shape.size() == 5);
    int index_out = 0;
    for (int n=0; n<shape[0]; ++n) {
        for (int c=0; c<shape[1]; ++c) {
            for (int h=0; h<shape[2]; ++h) {
                for (int w=0; w<shape[3]; ++w) {
                    for (int i=0; i<shape[4]; ++i) {
                        if (*input!=0.f){
                            *output = n;
                            ++output;
                            *output = c;
                            ++output;
                            *output = h;
                            ++output;
                            *output = w;
                            ++output;
                            *output = i;
                            ++output;
                            ++index_out;
                        }
                        ++input;
                    }
                }
            }
        }
    }
    return index_out;
}

int cpu_wherelayer::process(void *param, int param_size)
{
    setParam(param, param_size);
    const float* input = input_tensors_[0];

    std::vector<int> shape = input_shapes_[0];
    dim = shape.size();
    int output_size=1;
    for (int i = 0; i < dim; ++i) {
        output_size *= input_shapes_[0][i];
    }

    int *output = reinterpret_cast<int *>(output_tensors_[0]);
    int out_shape;
    switch(dim) {
    case 1:
        out_shape = where1d(input, shape, output);
        break;
    case 2:
        out_shape = where2d(input, shape, output);
        break;
    case 3:
        out_shape = where3d(input, shape, output);
        break;
    case 4:
        out_shape = where4d(input, shape, output);
        break;
    case 5:
        out_shape = where5d(input, shape, output);
        break;
    default:
        printf("error: %s: %d: input dim is %d, exceeds maximum dim. \n", __FILE__, __LINE__, dim);
        exit(-1);
    }
    for (int i=out_shape*dim; i<output_size*dim; ++i) {
        output[i] = -1;
    }

    (*output_shapes_)[0][0] = out_shape;
    (*output_shapes_)[0][1] = dim;
    return 0;
}

void cpu_wherelayer::setParam(void* param, int param_size)
{
}

int cpu_wherelayer::reshape( 
    void* param, int param_size,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes)
{
  int output_size=1;
  for (int i = 0; i < input_shapes[0].size(); ++i) {
    output_size *= input_shapes[0][i];
  }
  output_shapes[0].clear();
  output_shapes[0].push_back(output_size);
  output_shapes[0].push_back(input_shapes[0].size());
  return 0;
}

REGISTER_CPULAYER_CLASS(CPU_WHERE, cpu_where)

} /* namespace bmcpu */
