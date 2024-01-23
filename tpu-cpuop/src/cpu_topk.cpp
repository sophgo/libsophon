#include <algorithm>
#include <functional>
#include <queue>
#include <utility>
#include <numeric>
#include "cpu_topk.h"

namespace bmcpu {

template <typename T>
struct Descending {
    bool operator()(
        const std::pair<T, int>& lhs,
        const std::pair<T, int>& rhs) const {
        return lhs.first > rhs.first ||
               (lhs.first == rhs.first && lhs.second < rhs.second);
    }
};

template <typename T>
struct Ascending {
    bool operator()(
        const std::pair<T, int>& lhs,
        const std::pair<T, int>& rhs) const {
        return lhs.first < rhs.first ||
               (lhs.first == rhs.first && lhs.second < rhs.second);
    }
};

template <typename T, typename I, class ValueComp>
bool TopK(const T* input_data,
          const vector<int>& input_dims,
          const int k,
          const int axis,
          const bool sorted,
          T* values_data,
          I* indices_data);

int cpu_topklayer::process(void *param, int param_size) {
    setParam(param, param_size);
    CPU_ASSERT(output_tensors_.size() >= 2);
    const float* bottom_data = input_tensors_[0];
    int k = k_;
    if(input_tensors_.size()==2)
        k = *(reinterpret_cast<int *>(input_tensors_[1]));
    int    axis    = axis_==-1 ? input_shapes_[0].size()-1 : axis_;

    if(k>0){
        float* values  = output_tensors_[0];
        int*   indices = reinterpret_cast<int *>(output_tensors_[1]);
        if (descending_) {
          TopK<float, int, Descending<float>>(bottom_data,
                                              input_shapes_[0],
              k,
              axis,
              sorted_,
              values,
              indices);
        } else {
          TopK<float, int, Ascending<float>>(bottom_data,
                                             input_shapes_[0],
              k,
              axis,
              sorted_,
              values,
              indices);
        }
    }
    for(int i=0; i<output_shapes_->size(); i++)
        (*output_shapes_)[i][axis] = k;
    return 0;
}

int cpu_topklayer::reshape(void* param, int param_size,
                           const vector<vector<int>>& input_shapes,
                           vector<vector<int>>& output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_topk_param_t, topk_param, param, param_size);
    k_ = topk_param->k;
    axis_ = topk_param->axis;
    int axis = (axis_==-1) ? (input_shapes[0].size() - 1) : axis_;
    CPU_ASSERT(output_shapes.size() == 2);
    for(size_t i=0; i < output_shapes.size(); i++) {
        output_shapes[i] = input_shapes[0];
        if (k_ != -1)
            output_shapes[i][axis] = k_;
    }
    return 0;
}

void cpu_topklayer::setParam(void *param, int param_size) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_topk_param_t, topk_param, layer_param_, param_size);
    k_      = topk_param->k;
    axis_   = topk_param->axis;
    sorted_ = topk_param->sorted;
}

REGISTER_CPULAYER_CLASS(CPU_TOPK, cpu_topk)

template <typename T, typename I>
struct IndexComp {
    bool operator()(
        const std::pair<T, I>& lhs,
        const std::pair<T, I>& rhs) const {
        return lhs.second < rhs.second;
    }
};

template <typename T, typename I, class ValueComp>
void GetTopK(
    const T* input,
    const int n,
    const int k,
    const bool sorted,
    const int src_offset,
    const int dst_offset,
    const int stride,
    T* values,
    I* indices) {
    const T* src_ptr = input + src_offset;
    std::vector<std::pair<T, I>> heap_data;
    heap_data.reserve(k);
    for (int i = 0; i < k && i < n; ++i) {
        heap_data.emplace_back(*src_ptr, i);
        src_ptr += stride;
    }
    std::priority_queue<
    std::pair<T, I>,
        std::vector<std::pair<T, I>>,
        ValueComp>
        pq(ValueComp(), std::move(heap_data));
    for (int i = k; i < n; ++i) {
        if (pq.top().first < *src_ptr) {
            pq.pop();
            pq.emplace(*src_ptr, i);
        }
        src_ptr += stride;
    }
    int dst_pos = dst_offset + (std::min(k, n) - 1) * stride;
    if(!sorted) {
        std::priority_queue<
        std::pair<T, I>,
            std::vector<std::pair<T, I>>,
            IndexComp<T, I> > pq_index;
        while(!pq.empty()) {
            pq_index.push(pq.top());
            pq.pop();
        }
        while (!pq_index.empty()) {
            const auto& item = pq_index.top();
            values[dst_pos] = item.first;
            indices[dst_pos] = item.second;
            pq_index.pop();
            dst_pos -= stride;
        }
        return;
    }
    while (!pq.empty()) {
        const auto& item = pq.top();
        values[dst_pos] = item.first;
        indices[dst_pos] = item.second;
        pq.pop();
        dst_pos -= stride;
    }
}

template <typename T, typename I, class ValueComp>
bool TopK(const T* input_data,
          const vector<int>& input_dims,
          const int k,
          const int axis,
          const bool sorted,
          T* values_data,
          I* indices_data) {
    if (k == 0) return true;
    CPU_ASSERT(axis >= 0);
    CPU_ASSERT(axis < input_dims.size());
    const int prev_size = std::accumulate(
                              input_dims.cbegin(),
                              input_dims.cbegin() + axis,
                              int(1),
                              std::multiplies<int>());
    const int next_size = std::accumulate(
                              input_dims.cbegin() + axis + 1,
                              input_dims.cend(),
                              int(1),
                              std::multiplies<int>());
    const int src_offset_stride = input_dims[axis] * next_size;
    const int dst_offset_stride = k * next_size;
    int src_offset = 0;
    int dst_offset = 0;
    for (int i = 0; i < prev_size; ++i) {
        for (int j = 0; j < next_size; ++j) {
            GetTopK<T, I, ValueComp>(
                input_data,
                input_dims[axis],
                k,
                sorted,
                src_offset + j,
                dst_offset + j,
                next_size,
                values_data,
                indices_data);
        }
        src_offset += src_offset_stride;
        dst_offset += dst_offset_stride;
    }
    return true;
}

} //  namespace bmcpu
