#include <algorithm>
#include <numeric>
#include "cpu_topk_mx.h"

namespace bmcpu{

template<typename DType>
void TopKSort(DType* sorted_vals, int* indices,
              const DType* in_vals, int flatten_len,
              int K, int N, bool is_ascend, bool need_mask) {
    // Use full sort when K is relatively large.
    const bool full_sort(K*8 > N);
    // Batch size.
    const int M(flatten_len/N);
    auto vals = in_vals;
    if(need_mask){
        memset(sorted_vals, 0, flatten_len*sizeof(DType));
    }
    for (int i = 0; i < M; ++i) {
        // Tensor `in_vals` stores the flattened source data, while `dat` stores the sorted result.
        int ind_offset = i*N;
        int *ind = indices +ind_offset;
        if (is_ascend) {
            auto cmp =  [&](const int& i1, const int& i2){ return vals[i1] < vals[i2]; };
            if (full_sort) {
                std::sort(ind, ind+N, cmp);
            } else {
                std::partial_sort(ind, ind+K, ind+N, cmp);
            }
        } else {
            auto cmp =  [&](const int& i1, const int& i2){ return vals[i1] > vals[i2]; };
            if (full_sort) {
                std::sort(ind, ind+N, cmp);
            } else {
                std::partial_sort(ind, ind+K, ind+N, cmp);
            }
        }
        if(need_mask){
            for (int j = 0; j < K; ++j) {
                sorted_vals[ind[j]] = 1;
            }
        } else {
            DType *sorted_ptr = sorted_vals +ind_offset;
            for (int j = 0; j < K; ++j) {
                sorted_ptr[j] = vals[ind[j]];
                ind[j] -= ind_offset;
            }
        }
    }
}
static void ParseTopKParam(const vector<int>& src_shape, const cpu_topk_mx_param_t& param,
                           std::vector<int>& target_shape, int *batch_size, int *element_num,
                           int *axis, int *k, bool *do_transpose, bool *is_ascend) {
    *do_transpose = false;
    *k = param.k;
    *is_ascend = param.is_ascend;
    bool has_axis = param.axis < (int)src_shape.size();
    // get batch_size, axis and element_num
    int src_size = 1;
    for(auto s: src_shape){
        src_size *= s;
    }
    if (!has_axis) {  // No axis given, flatten
        *axis = 0;
        *batch_size = 1;
        *element_num = src_size;
    } else {
        *axis = param.axis;
        if (*axis < 0) {
            *axis += src_shape.size();
        }
        CPU_ASSERT(*axis >= 0 && *axis < static_cast<int>(src_shape.size()));
        *batch_size = src_size / src_shape[*axis];
        *element_num = src_shape[*axis];
        if (*axis != (int)src_shape.size() - 1) {
            *do_transpose = true;
        }
    }
    // get k
    if (param.k <= 0) {
        *k = *element_num;
    }
    // get target_shape
    target_shape = src_shape;
    if (param.ret_type != MX_TOPK_RET_MASK) {
        if (!has_axis) {
            target_shape.assign(1, *k);
        } else {
            target_shape[*axis] = *k;
        }
    }
    CPU_ASSERT(*k >= 1 && *k <= *element_num);
}

template<typename DType, typename SType>
void assign_slice(DType* dst, const SType* src, int batch, int hslice, int height, int wslice, int width, bool do_transpose){
    int batch_in_stride = height*width;
    int batch_out_stride = hslice*wslice;
    int hstride = wslice;
    int wstride = 1;
    if(do_transpose){
        hstride = 1;
        wstride = hslice;
    }
    for(int b =0 ; b<batch; b++){
        auto in_ptr = src+ b *batch_in_stride;
        auto out_ptr = dst+ b *batch_out_stride;
        for(int h=0; h<hslice; h++){
            for(int w=0; w<wslice; w++){
                int in_idx = h*width + w;
                int out_idx = h*hstride+w*wstride;
                out_ptr[out_idx] =  (DType)in_ptr[in_idx];
            }
        }
    }
}

template<typename SType>
void assign_slice_by_dtype(void* dst, const SType* src, int batch, int hslice, int height, int wslice, int width, bool do_transpose, int dtype){
    if(dtype == 0){
        auto out_ptr = (float*)dst;
        assign_slice(out_ptr, src, batch, hslice, height, wslice, width, do_transpose);
    } else {
        auto out_ptr = (int*)dst;
        assign_slice(out_ptr, src, batch, hslice, height, wslice, width, do_transpose);
    }
}
template<typename DType>
int cpu_topk_mx_template<DType>::process(void *param, int param_size)
{
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_topk_mx_param_t, topk_param, param, param_size);
    //only support FP32, INT32, UINT32 from bmcompiler.def.h
    CPU_ASSERT(topk_param->dtype == 0 || topk_param->dtype == 6 || topk_param->dtype == 7);
    vector<int> target_shape;
    auto src_shape = input_shapes_[0];
    int batch_size, N, axis, k;
    bool do_transpose, is_ascend;
    ParseTopKParam(src_shape, *topk_param, target_shape,
            &batch_size, &N, &axis, &k, &do_transpose, &is_ascend);

    int flatten_len = batch_size * N;
    auto flatten_data = (DType*)input_tensors_[0];
    int inner_len = 1;
    if(do_transpose){
        for(size_t i=axis+1; i<src_shape.size(); i++){
            inner_len *= input_shapes_[0][i];
        }
    }
    int outer_len = batch_size/inner_len;
    if(do_transpose){
        flatten_data = new DType[flatten_len];
        assign_slice(flatten_data, input_tensors_[0], outer_len, N, N, inner_len, inner_len, do_transpose);
    }
    auto indices = new int[flatten_len];
    auto sorted_data = new DType[flatten_len];
    std::iota(indices, indices+flatten_len, 0);
    bool need_mask = topk_param->ret_type == MX_TOPK_RET_MASK;

    TopKSort(sorted_data, indices, flatten_data, flatten_len, k, N, is_ascend, need_mask);

    if(topk_param->ret_type == MX_TOPK_RET_MASK){
        auto out_data = (DType*)output_tensors_[0];
        assign_slice(out_data, sorted_data, outer_len, inner_len, inner_len, N, N, do_transpose);
    } else {
        auto out_data = (DType*)output_tensors_[0];
        void* out_indice = output_tensors_[topk_param->ret_type == MX_TOPK_RET_BOTH];
        if(topk_param->ret_type == MX_TOPK_RET_VALUE || topk_param->ret_type == MX_TOPK_RET_BOTH){
            assign_slice(out_data, sorted_data, outer_len, inner_len, inner_len, k, N, do_transpose);
        }
        if(topk_param->ret_type == MX_TOPK_RET_INDICES || topk_param->ret_type == MX_TOPK_RET_BOTH){
            assign_slice_by_dtype(out_indice, indices, outer_len, inner_len, inner_len, k, N, do_transpose, topk_param->dtype);
        }
    }
    for(size_t i=0; i < (*output_shapes_).size(); i++) {
        (*output_shapes_)[i] = target_shape;
    }
    delete []indices;
    delete [] sorted_data;
    if(do_transpose){
        delete [] flatten_data;
    }
    return 0;
}

template<typename DType>
int cpu_topk_mx_template<DType>::reshape(void *param, int param_size,
  const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes)
{
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_topk_mx_param_t, topk_param, param, param_size);
  vector<int> target_shape;
  int batch_size, element, axis, k;
  bool do_transpose, is_ascend;
  ParseTopKParam(input_shapes[0], *topk_param, target_shape,
          &batch_size, &element, &axis, &k, &do_transpose, &is_ascend);
  CPU_ASSERT(output_shapes.size() == 1+(topk_param->ret_type==MX_TOPK_RET_BOTH));
  for(size_t i=0; i < output_shapes.size(); i++) {
    output_shapes[i] = target_shape;
  }
  return 0;
}

template<typename DType>
int cpu_topk_mx_template<DType>::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_topk_mx_param_t, topk_param, param, param_size);
    output_dtypes.clear();
    switch (topk_param->ret_type) {
    case MX_TOPK_RET_BOTH:
        output_dtypes = {input_dtypes[0], CPU_DTYPE_FP32};
        break;
    case MX_TOPK_RET_VALUE:
        output_dtypes = {input_dtypes[0]};
        break;
    case MX_TOPK_RET_INDICES:
    case MX_TOPK_RET_MASK:
        output_dtypes = {input_dtypes[0]};
        break;
    default:
        break;
    }
    return 0;
}

using cpu_topk_mx_layer = cpu_topk_mx_template<float>;
REGISTER_CPULAYER_CLASS(CPU_TOPK_MX, cpu_topk_mx_)
}
