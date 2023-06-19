#include <numeric>
#include <functional>
#include <algorithm>
#include "cpu_box_nms.h"
#include "bmcpu_macro.h"

namespace bmcpu {

struct ParallelExecutor {
    template<typename Func, typename... ArgTypes>
    static void run(Func functor, const int N, ArgTypes... args){
        //TODO: use std::thread or openmp
        for(int i=0; i<N; i++){
            functor(i, args...);
        }
    }
};

template<typename VVecType, typename KVecType>
void sort_with_key(VVecType& vvec, KVecType& kvec, int n, bool is_asc=true){
    using KeyType = typename std::remove_reference<decltype(kvec[0])>::type;
    using ValueType = typename std::remove_reference<decltype(vvec[0])>::type;
    using ItemType = typename std::pair<KeyType, ValueType>;
    std::vector<ItemType> kvList(kvec.size());
    for(int i=0; i<n; i++){
        kvList[i].first = kvec[i];
        kvList[i].second = vvec[i];
    }
    auto comp = [is_asc](const ItemType& a, const ItemType& b){
        return is_asc?(a.first<b.first):(a.first>b.first);
    };
    std::stable_sort(std::begin(kvList), std::end(kvList), comp);
    for(int i=0; i<n; i++){
        kvec[i] = kvList[i].first;
        vvec[i] = kvList[i].second;
    }
}


void assign_nms_batch(int batch, float* out_ptr, const float* in_ptr, int item_len, const int* indice, int* batch_start, int topk, int num_elem){
    int index_base = batch_start[batch];
    int out_base = batch * num_elem;
    int num_item = std::min(topk, batch_start[batch+1]-index_base);
    int count = 0;
    for(int i=0; i<num_item; i++){
        int in_pos = indice[index_base+i];
        if(in_pos<0) continue;
        int out_pos = out_base + count;
        memcpy(out_ptr+item_len*out_pos, in_ptr+item_len*in_pos, item_len*sizeof(float));
        count++;
    }
}

void corner_to_center(int i, float* data, int stride){
    float* real_data = data + i*stride;
    if(real_data[0]<0) return;
    float left = real_data[0];
    float top = real_data[1];
    float right = real_data[2];
    float bottom = real_data[3];
    real_data[0] = (left+right)*0.5;
    real_data[1] = (top+bottom)*0.5;
    real_data[2] = right-left;
    real_data[3] = bottom-top;
}

static void center_to_corner(int i, float* data, int stride){
    float* real_data = data + i * stride;
    if(real_data[0]<0) return;
    float x = real_data[0];
    float y = real_data[1];
    float w_half = real_data[2]*.5;
    float h_half = real_data[3]*.5;
    real_data[0] = x-w_half;
    real_data[1] = y-h_half;
    real_data[2] = x+w_half;
    real_data[3] = y+h_half;
}

static float box_area(const float* box, int format){
    float width, height;
    if(format == BOX_FORMAT_CENTER){
        width = box[2];
        height = box[3];
    } else {
        width = box[2]- box[0];
        height = box[3] - box[1];
    }
    if(width<0 || height<0){
        return 0;
    }
    return height*width;
}
static void compute_area(int i, float* areas, const float* in_data, const int* indice,
                         const int* batch_start, int topk, int stride, int format){
    int b = i/topk;
    int k = i%topk;
    int pos = batch_start[b]+k;
    if(pos>=batch_start[b+1]) return;
    int index = indice[pos];
    int in_offset = index * stride;
    areas[pos] = box_area(in_data+in_offset, format);
}

float compute_overlap(const float* box0, const float* box1, int format){
    float left, right, top, bottom;
    if(format == BOX_FORMAT_CENTER){
        int w_half0 = box0[2]/2;
        int h_half0 = box0[3]/2;
        int w_half1 = box1[2]/2;
        int h_half1 = box1[3]/2;
        left = std::max(box0[0]-w_half0, box1[0]-w_half1);
        right = std::min(box0[0]+w_half0, box1[0]+w_half1);
        top = std::max(box0[1]-h_half0, box1[1]-h_half1);
        bottom = std::min(box0[1]+h_half0, box1[1]+h_half1);
    } else {
        left = std::max(box0[0], box1[0]);
        top = std::max(box0[1], box1[1]);
        right = std::min(box0[2], box1[2]);
        bottom = std::min(box0[3], box1[3]);
    }
    if(left>right || top>bottom){
        return 0;
    }
    return (right-left) * (bottom-top);
}

void nms_mark(int i, int* index,
              const int* batch_start, const float *input, const float* areas,
              int num_worker, int ref, int stride, int offset_box, int offset_id,
              float thresh, bool force, int format){
    int b = i/num_worker;
    int pos = i%num_worker+ ref+1;
    ref = batch_start[b]+ref;
    pos = batch_start[b]+pos;
    if(ref>=batch_start[b+1]) return;
    if(pos>=batch_start[b+1]) return;
    if(index[ref]<0) return;
    if(index[pos]<0) return;
    int ref_offset = index[ref] * stride + offset_box;
    int pos_offset = index[pos] * stride + offset_box;
    if(!force && offset_id>=0){
        int ref_id = input[ref_offset-offset_box + offset_id];
        int pos_id = input[pos_offset-offset_box + offset_id];
        if(ref_id != pos_id) return;
    }
    float overlap = compute_overlap(input+ref_offset, input+pos_offset, format);
    float iou = overlap / (areas[ref]+ areas[pos]- overlap);
    if(iou> thresh){
        index[pos] = -1;
    }
}
void compute_nms(int num_batch, int topk,
                 int* valid_index, const int* batch_start,
                 const float* in_ptr, const float* areas,
                 int stride, int coord_start, int id_index,
                 float threshold, bool force_suppress, int in_format){
    for(int ref=0; ref<topk; ref++){
        int num_worker = topk - ref -1;
        if(num_worker<1) continue;
        ParallelExecutor::run(nms_mark, num_batch*num_worker,
                              valid_index, batch_start, in_ptr, areas,
                              num_worker, ref, stride, coord_start, id_index,
                              threshold, force_suppress, in_format);

    }
}

int bmcpu::cpu_boxnmslayer::process(void *raw_param, int param_size)
{
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_box_nms_param_t, param, raw_param, param_size);

    CPU_ASSERT_EQ(input_tensors_.size(),1);
    CPU_ASSERT_EQ(output_tensors_.size(),1);
    CPU_ASSERT_GE(input_shapes_[0].size(), 2);
    int indim = input_shapes_[0].size();
    int num_batch = 1;
    for(int i=0; i<indim-2; i++){
        num_batch *= input_shapes_[0][i];
    }
    int num_elem = input_shapes_[0][indim-2];
    int total_elem = num_batch * num_elem;
    int stride = input_shapes_[0][indim-1];
    int topk = param->topk<0? num_elem: std::min(param->topk, num_elem);

    const float* in_ptr = input_tensors_[0];
    float* out_ptr = output_tensors_[0];

    int score_index = param->score_index;
    int coord_start = param->coord_start;
    int id_index = param->id_index;
    bool class_exist = id_index>=0;
    int in_format = param->in_format;

    std::vector<int> valid_indice(total_elem, -1);
    std::vector<float> valid_scores(total_elem);
    int num_valid = 0;
    const float* score_ptr = in_ptr + score_index;
    const float* id_ptr = in_ptr + id_index;
    for(int i=0; i<total_elem; i++){
        if((*score_ptr>param->valid_thresh) && (!class_exist || *id_ptr != param->background_id)){
            valid_scores[num_valid] = *score_ptr;
            valid_indice[num_valid] = i;
            num_valid++;
        }
        score_ptr += stride;
        id_ptr += stride;
    }
    if(num_valid == 0){
        std::fill_n(out_ptr, total_elem*stride, -1);
        return 0;
    }
    sort_with_key(valid_indice, valid_scores, num_valid, false);
    std::vector<int> batch_ids(num_valid);
    for(int i=0; i<num_valid; i++){
        batch_ids[i] = valid_indice[i]/num_elem;
    }
    sort_with_key(valid_indice, batch_ids, num_valid, true);
    std::vector<int> batch_start(num_batch+1);
    batch_start[0] = 0;
    for(int i=1; i<batch_start.size(); i++){
        batch_start[i]=batch_start[i-1];
        while((batch_start[i] < num_valid) && (batch_ids[batch_start[i]]== i-1)){
            batch_start[i]++;
        }
    }

    std::vector<float> areas(num_valid);
    ParallelExecutor::run(
                compute_area,
                num_batch*topk, areas.data(),
                in_ptr+coord_start, valid_indice.data(), batch_start.data(),
                topk, stride, in_format);
    compute_nms(num_batch, topk,
                valid_indice.data(), batch_start.data(), in_ptr, areas.data(),
                stride, coord_start, id_index, param->overlap_thresh,
                param->force_suppress, in_format);
    std::fill(out_ptr, out_ptr + total_elem*stride, -1.0);
    ParallelExecutor::run(assign_nms_batch,
                          num_batch, out_ptr, in_ptr, stride, valid_indice.data(), batch_start.data(), topk, num_elem);
    if(in_format != param->out_format){
        if(in_format == BOX_FORMAT_CORNER){
            ParallelExecutor::run(corner_to_center, total_elem, out_ptr + coord_start, stride);
        } else {
            ParallelExecutor::run(center_to_corner, total_elem, out_ptr + coord_start, stride);
        }
    }
    (*output_shapes_)[0] = input_shapes_[0];
    return 0;
}

int cpu_boxnmslayer::reshape(void *param, int param_size,
    const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes)
{
    output_shapes.assign(1, input_shapes[0]);
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_BOXNMS, cpu_boxnms)

}
