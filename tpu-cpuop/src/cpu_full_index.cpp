#include "cpu_full_index.h"
#include "bmcpu_utils.hpp"

namespace bmcpu {

vector<size_t> calc_stride(const vector<int>& shape){
    vector<size_t> stride(shape.size());
    stride.back() = 1;
    for(int i=shape.size()-1; i>0; i--){
        stride[i-1] = stride[i]*shape[i];
    }
    for(int i=shape.size()-1; i>=0; i--){
        if(shape[i]==1) stride[i] = 0;
    }
    return stride;
};

int calc_elem_num(const vector<int>& shape){
    int elem_num = 1;
    for(auto s: shape){
        elem_num *= s;
    }
    return elem_num;
}

void update_shape(vector<int>& slice_shape, const vector<int>& shape){
  auto tensor_shape = shape;
  if(tensor_shape.size()<slice_shape.size()){
    tensor_shape.insert(tensor_shape.begin(), slice_shape.size()-tensor_shape.size(), 1);
  } else {
    slice_shape.insert(slice_shape.begin(), tensor_shape.size()-slice_shape.size(), 1);
  }
  for(size_t s=0; s<slice_shape.size(); s++){
    CPU_ASSERT(slice_shape[s] == tensor_shape[s] || tensor_shape[s] == 1 || slice_shape[s] == 1);
    slice_shape[s] = std::max(slice_shape[s], tensor_shape[s]);
  }
}


int get_type_len(CPU_DATA_TYPE_T t){
    if(t == CPU_DTYPE_FP32 || t == CPU_DTYPE_UINT32 || t == CPU_DTYPE_INT32){
        return 4;
    } else if(t == CPU_DTYPE_FP16 || t == CPU_DTYPE_UINT16 || t == CPU_DTYPE_INT16){
        return 2;
    } else {
        return 1;
    }
}

class base_index_t {
public:
  virtual void init() = 0;
  virtual bool incr() = 0;
  void add_stride(size_t& src_stride, size_t& dst_stride) {
    src_stride += this->src_stride;
    dst_stride += this->dst_stride;
  }
  virtual ~base_index_t() {};
protected:
  size_t src_stride=0;
  size_t dst_stride=0;
};

class slice_index_t: public base_index_t {
public:
  size_t in_stride;
  size_t out_stride;
  int begin;
  int step;
  int end;
  virtual bool incr() override {
    current++;
    bool res = false;
    if(begin+ current*step>= end) {
      current = 0;
      res = true;
    }
    src_stride = (begin + current*step)*in_stride;
    dst_stride = current * out_stride;
    return res;
  }
  virtual void init() override {
    current = 0;
  }
  virtual ~slice_index_t() {};
private:
  int current = 0;
};

typedef struct {
  size_t in_stride;
  std::vector<size_t> stride;
  const int* data;
} sub_tensor_index_t;

class tensor_index_t: public base_index_t{
public:
  std::vector<sub_tensor_index_t> tensors;
  std::vector<int> shape;
  size_t out_stride;
  virtual void init() override {
    //initialize
    stride = calc_stride(shape);
    indice.assign(shape.size(),0);
    src_stride = 0;
    for(size_t t=0; t<tensors.size(); t++){
      auto& tensor = tensors[t];
      src_stride += tensor.data[0]*tensor.in_stride;
    }
    dst_stride = 0;
  }
  virtual bool incr() override{
    bool res = false;
    for(int i=indice.size()-1; i>=0; i--){
      indice[i]++;
      if(indice[i] != shape[i]) break;
      indice[i] = 0;
      res = (i==0);
    }
    src_stride = 0;
    for(size_t t=0; t<tensors.size(); t++){
      auto& tensor = tensors[t];
      size_t pos = 0;
      for(size_t i=0; i<indice.size(); i++){
        pos += indice[i]*tensor.stride[i];
      }
      src_stride += tensor.data[pos]*tensor.in_stride;
    }
    size_t out_pos=0;
    for(size_t i=0; i<indice.size(); i++){
      out_pos += indice[i]*stride[i];
    }
    dst_stride = out_pos * out_stride;
    return res;
  }
  virtual ~ tensor_index_t() {};
private:
  std::vector<size_t> stride;
  std::vector<int> indice;
};

void slicing_by_index(const unsigned char* in_data, unsigned char* out_data, size_t elem_size, const std::vector<std::shared_ptr<base_index_t>>& indice){
  bool is_end = false;
  for(auto index: indice){
    index->init();
  }
  do {
    size_t full_src_stride = 0, full_dst_stride = 0;
    for(auto index: indice){
      index->add_stride(full_src_stride, full_dst_stride);
    }
    memcpy(out_data + full_dst_stride*elem_size, in_data + full_src_stride*elem_size, elem_size);
    for(int i = indice.size()-1; i>=0; i--){
      if(!indice[i]->incr()) break;
      is_end = (i == 0);
    }
  } while(!is_end);
}


struct shape_info_t {
    std::vector<int> out_shape;
    std::vector<int> slice_shape;
    int tensor_index_pos;
};

shape_info_t calc_output_shape(cpu_full_index_param_t* index_info_, const vector<vector<int>>& input_shapes){
    shape_info_t info;
    auto& out_shape = info.out_shape;

    auto& in_shape = input_shapes[0];
    assert(index_info_->count <= (int)in_shape.size());
    auto& index_pos = info.tensor_index_pos;
    index_pos = -1;
    int last_index_pos = -1;
    int tensor_count = 0;
    auto& slice_shape = info.slice_shape;
    for(size_t i=0; i<in_shape.size(); i++){
      if((int)i>= index_info_->count){
        out_shape.push_back(in_shape[i]);
        continue;
      }
      auto& index = index_info_->info[i];
      if(index.type == INDEX_NONE){
        out_shape.push_back(in_shape[i]);
      } else if(index.type == INDEX_TENSOR){

        if(index_pos<0) {
          index_pos = i;
        }
        tensor_count++;
        last_index_pos = i;
        // update slice shape
        update_shape(slice_shape, input_shapes[index.input_index]);
      } else {
        CPU_ASSERT(0);
      }
    }
    // Here is a wierd rule from pytorch or numpy: if None is in middle of two Tensor index, index_pos = 0
    if(tensor_count != last_index_pos-index_pos+1) index_pos = 0;
    if(index_pos>=0){
      out_shape.insert(out_shape.begin()+index_pos, slice_shape.begin(), slice_shape.end());
    }
    return info;
}

int cpu_full_index_layer::reshape(void *param, int psize, const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes) {
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_full_index_param_t, index_info_, param, psize);
  output_shapes.resize(1);
  output_shapes[0] = calc_output_shape(index_info_, input_shapes).out_shape;
  return 0;
}

int cpu_full_index_layer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes)
{
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_full_index_param_t, index_info_, param, param_size);
  output_dtypes.assign(1, input_dtypes[0]);
  return 0;
}


int cpu_full_index_layer::process(void *param, int param_size)
{
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_full_index_param_t, index_info_, param, param_size);

  auto in_shape = input_shapes_.at(0);
  output_shapes_->resize(1);
  auto shape_info = calc_output_shape(index_info_, input_shapes_);
  (*output_shapes_)[0] = shape_info.out_shape;

  // remove useless info, just keep slice and tensor indice
  // convert None to full Slice index
  std::vector<index_info_t> index_info;
  bool useless = true;
  for(int i=index_info_->count-1; i>=0; i--){
      auto index = index_info_->info[i];
      // last several NONE indice are useless
      if(index.type == INDEX_NONE && useless) continue;
      useless = false;
      if(index.type == INDEX_NONE){
          index.type = INDEX_SLICE;
          index.none_mask = -1;
          index.step = 1;
      }
      index_info.insert(index_info.begin(), index);
  }

  assert(index_info.size()<= in_shape.size());
  auto elem_size = get_type_len(index_info_->dtype);
  for(size_t i = index_info.size(); i<in_shape.size(); i++){
      elem_size *= in_shape[i];
  }

  auto new_in_shape = in_shape;
  size_t current_in_stride = 1;
  size_t current_out_stride = 1;

  auto& slice_shape = shape_info.slice_shape;
  auto& tensor_index_pos = shape_info.tensor_index_pos;
  std::vector<std::shared_ptr<base_index_t>> indice;
  std::shared_ptr<tensor_index_t> tensor_index = nullptr;
  if(tensor_index_pos>=0) {
    tensor_index = std::make_shared<tensor_index_t>();
    tensor_index->shape = slice_shape;
    indice.push_back(std::dynamic_pointer_cast<base_index_t>(tensor_index));
  }
  for(int i=index_info.size()-1; i>=0; i--){
    auto& info = index_info[i];
    if(info.type == INDEX_SLICE){
        auto index = std::make_shared<slice_index_t>();
        index->in_stride = current_in_stride;
        index->out_stride = current_out_stride;
        index->begin = (info.none_mask & 0x1)==0?info.begin:(info.step>0? 0: new_in_shape[i]-1);
        index->end = (info.none_mask & 0x2)==0?info.end:(info.step<0? -1: new_in_shape[i]);
        index->step = info.step;
        indice.push_back(std::dynamic_pointer_cast<base_index_t>(index));
        int out_size = (index->end - index->begin + index->step-1)/index->step;
        current_out_stride *= out_size;
    } else if(info.type == INDEX_TENSOR){
        sub_tensor_index_t sub_index;
        sub_index.data = (int*)input_tensors_[info.input_index];
        auto in_shape = input_shapes_[info.input_index];
        if(in_shape.size()!=slice_shape.size()){
            in_shape.insert(in_shape.begin(), slice_shape.size()-in_shape.size(), 1);
        }
        sub_index.stride = calc_stride(in_shape);
        sub_index.in_stride = current_in_stride;
        tensor_index->tensors.push_back(sub_index);
    } else {
        CPU_ASSERT(0);
    }
    if(i == tensor_index_pos){
        tensor_index->out_stride = current_out_stride;
        current_out_stride *= calc_elem_num(slice_shape);
    }
    current_in_stride *= in_shape[i];
  }
  slicing_by_index(
        (const unsigned char*)input_tensors_[0],
      (unsigned char*)output_tensors_[0], elem_size, indice);
  return 0;
}
REGISTER_CPULAYER_CLASS(CPU_FULL_INDEX, cpu_full_index_);

}
