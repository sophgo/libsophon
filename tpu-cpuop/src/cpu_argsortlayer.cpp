#include "cpu_argsortlayer.h"

namespace bmcpu {
int cpu_argsortlayer::process(void *raw_param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_argsort_param_t, param, raw_param, param_size);
    
    auto input_shape0 = input_shapes_[0];
    
    int dim = input_shape0.size();
    const float* input_prt = input_tensors_[0];

    int axis = param->axis;
    bool is_ascend = param->is_ascend;
    (*output_shapes_)[0] = input_shape0;
    
    //transpose the dim between [axis] and [dim-1]
    auto trans_shape0 = input_shape0;
    int temp_shape = input_shape0[dim-1];
    trans_shape0[dim-1] = trans_shape0[axis];
    trans_shape0[axis] = temp_shape; 

    //calculate the offset coefficient of the original and transposed sequence
    base_offset = new int[dim];
    base_offset[dim - 1] = 1;
    for (int i = 0; i < dim ; i++) {
        int temp_base = 1;
        for (int i_temp_base = i; i_temp_base < dim - 1; i_temp_base++) {temp_base *= input_shape0[i_temp_base + 1];}
        base_offset[i] = temp_base;
    }
    base_offset_trans = new int[dim];
    base_offset_trans[dim - 1] = 1;
    for (int i = 0; i < dim; i++) {
        int temp_base = 1;
        for (int i_temp_base = i; i_temp_base < dim - 1; i_temp_base++) {temp_base *= trans_shape0[i_temp_base + 1];}
        base_offset_trans[i] = temp_base;
    }

    len = 1;
    for (int i = 0; i < dim; i++) {len *= input_shape0[i];}

    //transpose the sequence between axis and last dim
    float* trans_prt = new float[len];  //store the sequence after transpose
    for (int i = 0; i < len; i++) {
        int* index = calc_dim(dim, base_offset, i);
        int* index_trans = new int[dim];
        memcpy(index_trans, index, dim * sizeof(int));
        index_trans[axis] = index[dim - 1];
        index_trans[dim - 1] = index[axis];
        int offset_trans = calc_offset(base_offset_trans, index_trans, dim);
        trans_prt[offset_trans] = input_prt[i];
        delete []index;
        delete []index_trans;
    }

    //since the dimension needed to argsort has been transposed to the last dimension
    //argsort the sequence in every continuous cutting list
    int cutting_len = trans_shape0[dim - 1];
    float* out_prt_temp = new float[len];

    for (int i = 0; i < len; i += cutting_len) {
        vector<float> temp_sort(trans_prt + i, trans_prt + i + cutting_len);
        auto out_sort = argsort(temp_sort, is_ascend);
        memcpy(out_prt_temp + i, &out_sort[0], out_sort.size() * sizeof(float));
    }
    delete []trans_prt;

    //transpse again
    float* out_prt = output_tensors_[0];
    for (int i = 0; i < len; i++) {
        int* index = calc_dim(dim, base_offset_trans, i);
        int* index_trans_again = new int[dim];
        memcpy(index_trans_again, index, dim * sizeof(int));
        index_trans_again[axis] = index[dim - 1];
        index_trans_again[dim - 1] = index[axis];
        int offset_trans_again = calc_offset(base_offset, index_trans_again, dim);
        out_prt[offset_trans_again] = out_prt_temp[i];
        delete []index;
        delete []index_trans_again;
    }
    delete []out_prt_temp;
    delete []base_offset;
    delete []base_offset_trans;

    return 0;
} /*process*/


template<typename T>
std::vector<float> cpu_argsortlayer::argsort(const std::vector<T>& array, bool ascend) {
	const int array_len(array.size());
	std::vector<float> array_index(array_len, 0);
	for (int i = 0; i < array_len; ++i)
		array_index[i] = float(i);
    if (ascend == true) {
	    std::sort(array_index.begin(), array_index.end(),
		[&array](int pos1, int pos2) {return (array[pos1] < array[pos2]);});
    }
    else {
        std::sort(array_index.begin(), array_index.end(),
		[&array](int pos1, int pos2) {return (array[pos1] > array[pos2]);});
    }

	return array_index;
}


int cpu_argsortlayer::calc_offset(int* base_offset, int *index, int dims) {
    int prt_loc = 0;
    for (int i = 0; i < dims; ++i) {
        prt_loc += index[i] * base_offset[i];
    }
    return prt_loc;
}

int* cpu_argsortlayer::calc_dim(int dims, int* base_offset, int offset) {
    int* index = new int[dims];
    for (int i = 0; i < dims; i++) {
        index[i] = offset / base_offset[i];
        offset = offset % base_offset[i];
    }
    return index;
}
    
REGISTER_CPULAYER_CLASS(CPU_ARGSORT, cpu_argsort)
}/* namespace bmcpu*/
