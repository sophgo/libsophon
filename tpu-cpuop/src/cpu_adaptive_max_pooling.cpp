#include "cpu_adaptive_max_pooling.h"
#include "cpu_layer.h"
#include <vector>
#include <cmath>

namespace bmcpu {

inline float cpu_adaptive_max_poollayer::begin_index(int out_index, int out_size, int in_size) {
    return ((float)(out_index*in_size) / out_size);
}

template <typename T>
void cpu_adaptive_max_poollayer::adaptive_max_pooling(const T *input,
                                                      const vector<int> &in_shape,
                                                      const int *out_shape,
                                                      const vector<int> &strides,
                                                      T *output,
                                                      float *index) {
    int osizeh = out_shape[0];
    int osizew = out_shape[1];
    vector<int> beginH(osizeh), endH(osizeh);
    vector<int> beginW(osizew), endW(osizew);
    beginH[0] = 0;
    endH[osizeh-1] = in_shape[2];
    for (int i=1; i<osizeh; ++i) {
        float point = begin_index(i, osizeh, in_shape[2]);
        beginH[i] = (int)std::floor(point);
        endH[i-1] = (int)std::ceil(point);
    }
    beginW[0] = 0;
    endW[osizew-1] = in_shape[3];
    for (int i=0; i<osizew; ++i) {
        float point = begin_index(i, osizew, in_shape[3]);
        beginW[i] = (int)std::floor(point);
        endW[i-1] = (int)std::ceil(point);
    }
    if (output_num_ == 1) {
        for (int nc=0; nc<in_shape[0]*in_shape[1]; ++nc) {
            for (int h=0; h<osizeh; ++h) {
                int bh = beginH[h];
                int eh = endH[h];
                int lenh = eh - bh;
                for (int w=0; w<osizew; ++w) {
                    int bw = beginW[w];
                    int ew = endW[w];
                    int lenw = ew - bw;
                    const T *ip = input + nc*strides[1] + bh*strides[2] + bw;
                    T max_val = *ip;
                    for (int ih=0; ih<lenh; ++ih) {
                        for (int iw=0; iw<lenw; ++iw) {
                            T val = *(ip+ih*strides[2]+iw);
                            if (val > max_val) {
                                max_val = val;
                            }
                        }
                    }
                    *output = max_val;
                    ++output;
                }
            }
        }
        return ;
    }
    for (int nc=0; nc<in_shape[0]*in_shape[1]; ++nc) {
        for (int h=0; h<osizeh; ++h) {
            int bh = beginH[h];
            int eh = endH[h];
            int lenh = eh - bh;
            for (int w=0; w<osizew; ++w) {
                int bw = beginW[w];
                int ew = endW[w];
                int lenw = ew - bw;
                const T *ip = input + nc*strides[1] + bh*strides[2] + bw;
                T *op = output + nc*strides[1] + h*strides[2] + w;
                float *idxp = index + nc*strides[1] + h*strides[2] + w;
                int maxindex = 0;
                T max_val = *ip;
                for (int ih=0; ih<lenh; ++ih) {
                    for (int iw=0; iw<lenw; ++iw) {
                        T val = *(ip+ih*strides[2]+iw);
                        if (val > max_val) {
                            max_val = val;
                            maxindex = (ih+bh)*strides[2] + (iw+bw);
                        }
                    }
                }
                *op = max_val;
                *idxp = static_cast<float>(maxindex);
            }
        }
    }
    return ;
}

int cpu_adaptive_max_poollayer::process(void* param, int param_size) {
    output_num_ = static_cast<int>(output_tensors_.size());
    const float *tensor = input_tensors_[0];
    std::vector<int> shape = input_shapes_[0];
    CPU_ASSERT(shape.size() == 4);

    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_adaptive_pool_param_t, layer_param, param, param_size);
    const int *output_shape = layer_param->output_shape;
    if(input_tensors_.size()>1){
        output_shape = reinterpret_cast<int *>(input_tensors_[1]);
    }
    std::vector<int> strides(4);
    strides[3] = 1;
    for (int i=2; i>=0; i--) {
        strides[i] = shape[i+1] * strides[i+1];
    }

    float *output = output_tensors_[0];
    float *index = output_tensors_[1];
    adaptive_max_pooling(tensor, shape, output_shape, strides, output, index);

    (*output_shapes_)[0][0] = shape[0];
    (*output_shapes_)[0][1] = shape[1];
    (*output_shapes_)[0][2] = output_shape[0];
    (*output_shapes_)[0][3] = output_shape[1];
    if (output_num_ == 1)
        return 0;
    (*output_shapes_)[1][0] = shape[0];
    (*output_shapes_)[1][1] = shape[1];
    (*output_shapes_)[1][2] = output_shape[0];
    (*output_shapes_)[1][3] = output_shape[1];

    return 0;
}
int cpu_adaptive_max_poollayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                                         vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_ADAPTIVE_MAX_POOL, cpu_adaptive_max_pool)

} /* namespace bmcpu*/
