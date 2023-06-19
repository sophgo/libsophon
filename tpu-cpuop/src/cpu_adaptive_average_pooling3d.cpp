#include "cpu_adaptive_average_pooling3d.h"
#include "cpu_layer.h"
#include <vector>
#include <cmath>

namespace bmcpu {

inline float cpu_adaptive_average_pooling3dlayer::begin_index(int out_index, int out_size, int in_size) {
    return ((float)(out_index*in_size) / out_size);
}

template <typename T>
void cpu_adaptive_average_pooling3dlayer::adaptive_average_pooling(
    const T *input,
    const vector<int> &in_shape,
    const int *out_shape,
    const vector<int> &strides,
    T *output) {
    int osizet = out_shape[0];
    int osizeh = out_shape[1];
    int osizew = out_shape[2];
    vector<int> beginT(osizet), endT(osizet);
    vector<int> beginH(osizeh), endH(osizeh);
    vector<int> beginW(osizew), endW(osizew);
    beginT[0] = 0;
    endT[osizet-1] = in_shape[2];
    for (int i=1; i<osizet; ++i) {
        float point = begin_index(i, osizet, in_shape[2]);
        beginT[i] = (int)std::floor(point);
        endT[i-1] = (int)std::ceil(point);
    }
    beginH[0] = 0;
    endH[osizeh-1] = in_shape[3];
    for (int i=0; i<osizeh; ++i) {
        float point = begin_index(i, osizeh, in_shape[3]);
        beginH[i] = (int)std::floor(point);
        endH[i-1] = (int)std::ceil(point);
    }
    beginW[0] = 0;
    endW[osizew-1] = in_shape[4];
    for (int i=0; i<osizew; ++i) {
        float point = begin_index(i, osizew, in_shape[4]);
        beginW[i] = (int)std::floor(point);
        endW[i-1] = (int)std::ceil(point);
    }
    for (int nc=0; nc<in_shape[0]*in_shape[1]; ++nc) {
        for(int t=0; t<osizet; ++t) {
            int bt = beginT[t];
            int et = endT[t];
            int lent = et-bt;
            for (int h=0; h<osizeh; ++h) {
                int bh = beginH[h];
                int eh = endH[h];
                int lenh = eh - bh;
                for (int w=0; w<osizew; ++w) {
                    int bw = beginW[w];
                    int ew = endW[w];
                    int lenw = ew - bw;
                    const T *ip = input + nc*strides[1] + bt*strides[2] + bh*strides[3] + bw;
                    T sum = 0;
                    for(int it=0; it<lent; ++it) {
                        for (int ih=0; ih<lenh; ++ih) {
                            for (int iw=0; iw<lenw; ++iw) {
                                T val = *(ip+it*strides[2]+ih*strides[3]+iw);
                                sum += val;
                            }
                        }
                    }
                    int lenthw = lent * lenh * lenw;
                    *output = sum / lenthw;
                    ++output;
                }
            }
        }
    }
}


int cpu_adaptive_average_pooling3dlayer::process(void* param, int param_size) {

    const float *tensor = input_tensors_[0];
    std::vector<int> shape = input_shapes_[0];
    CPU_ASSERT(shape.size() == 5);

    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_adaptive_pool_param_t, layer_param, param, param_size);
    const int *output_shape = nullptr;
    if(input_tensors_.size()>1){
        output_shape = reinterpret_cast<int *>(input_tensors_[1]);
    } else {
        output_shape = layer_param->output_shape;
    }
    std::vector<int> strides(5);
    strides[4] = 1;
    for (int i=3; i>=0; --i) {
        strides[i] = shape[i+1] * strides[i+1];
    }

    float *output = output_tensors_[0];
    adaptive_average_pooling(tensor, shape, output_shape, strides, output);

    (*output_shapes_)[0][0] = shape[0];
    (*output_shapes_)[0][1] = shape[1];
    (*output_shapes_)[0][2] = output_shape[0];
    (*output_shapes_)[0][3] = output_shape[1];
    (*output_shapes_)[0][4] = output_shape[2];

    return 0;
}
int cpu_adaptive_average_pooling3dlayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
        vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_ADAPTIVE_AVERAGE_POOL_3D, cpu_adaptive_average_pooling3d);

} /* namespace bmcpu*/
