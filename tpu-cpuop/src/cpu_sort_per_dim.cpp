#include "cpu_sort_per_dim.h"
#include <numeric>
#include <algorithm>

/* input  : array1 [0, 1, 2]
 *          array2 [0, 1]
 * output : [[0, 1, 2, 0, 1]
 */
template <typename Dtype>
vector<int> concat_1dim_array(vector<Dtype> array1, vector<Dtype> array2)
{
    vector<Dtype> concat_array;
    concat_array.reserve(array1.size() + array2.size() ); // preallocate memory
    concat_array.insert(concat_array.end(), array1.begin(), array1.end() );
    concat_array.insert(concat_array.end(), array2.begin(), array2.end() );

    return concat_array;
}


/* input  : array1 [[0, 1, 2]]
 *          array2 [[0, 1], [3]]
 * output : [[0, 1, 2, 0, 1], [0, 1, 2, 3]]
 */
template <typename Dtype>
vector<vector<Dtype>> conbine_2dim_array(vector<vector<Dtype>> array1, vector<vector<Dtype>> array2)
//vector<vector<int>>&& conbine_2dim_array(vector<vector<int>> array1, vector<vector<int>> array2)
{
    vector<vector<Dtype>> concat_array;

    /* input  : array1 []
     *          array2 [[0, 1], [3]]
     * output : [[0, 1], [3]]
     */
    if (array1.size() == 0) {
        return array2;
        //concat_array = array2;
        //return std::move(concat_array);
    }

    for (int idx1 = 0; idx1 < array1.size(); ++idx1)
        for (int idx2 = 0; idx2 < array2.size(); ++idx2)
            concat_array.push_back(concat_1dim_array(array1[idx1], array2[idx2]));

    return concat_array;
    //return std::move(concat_array);
}


/* input  : [[0, 1, 2], [0, 1], [0]]
 * output : [[0, 0, 0], [0, 1, 0], [1, 0, 0], [1, 1, 0], [2, 0, 0], [2, 1, 0]]
 * algrithm :
 *    (1) [[0], [1], [2]],                           depth 0 init
 *    (2) [[0, 0], [1, 0], [2, 0]],                  depth 1 += depth 1-0 concat depth 0
 *        [[0, 1], [1, 1], [2, 1]],                  depth 1 += depth 1-1 concat depth 0
 *    (3) [[0, 0, 0], [1, 0, 0], [2, 0, 0],          depth 2 += depth 2-0 concat depth 1
 *         [0, 1, 0], [1, 1, 0], [2, 1, 0]]
 * algrithm :
 *    (1) [[0], [1], [2]], [[0], [1]], [[0]]         preprocess, get array1/2/3
 *    (2) [[0, 0], [1, 0], [2, 0],                   combine array1/2, get array4
 *         [0, 1], [1, 1], [2, 1]],
 *    (3) [[0, 0, 0], [1, 0, 0], [2, 0, 0],          combine array4/3, get array5
 *         [0, 1, 0], [1, 1, 0], [2, 1, 0]]
 */
template <typename Dtype>
//vector<vector<int>>&& get_array_combinations(vector<vector<int>> array)
vector<vector<Dtype>> get_array_combinations(vector<vector<Dtype>> array)
{

    //vector<vector<int *>> split_array(array.size());
    //for (int i = 0; i < array.size(); ++i) {
    //    split_array[i].reserve(array[i].size());
    //    iota(split_array.begin(), split_array.end(), &array[i][0]);
    //}

    /* for preprocess */
    vector<vector<vector<Dtype>>> split_array(array.size());
    for (int i = 0; i < array.size(); ++i) {
        split_array[i].resize(array[i].size());
        for (int j = 0; j < array[i].size(); ++j) {
            //split_array[i][j].push_back(vector<int>(1, array[i][j]));
            split_array[i][j].push_back(array[i][j]);
        }
    }

    if (split_array.size() == 1) {
        //return std::move(split_array[0]);
        return split_array[0];
    }

    vector<vector<Dtype>> combine_array;
    for (int i = 0; i < split_array.size(); ++i) {
        //combine_array = conbine_2dim_array(combine_array, split_array[i]);
        vector<vector<Dtype>> temp_array;
        temp_array = conbine_2dim_array(combine_array, split_array[i]);
        combine_array = temp_array;
    }

    //return std::move(combine_array);
    return combine_array;
}

/* designed for call by python */
template <typename Dtype>
void compute_sort(
    Dtype* in_addr,
    int*   index_addr,
    Dtype* out_addr,
    int* shape_in_,
    int  shape_num_,
    bool is_argsort,
    bool stable,
    int dim,
    bool descending = false)
{
    /* sort alogithm :
     * (1) select input dim data, copy to temp
     * (2) sort(stable) indexes based on comparing values.
     * (option 2.5) get sort(stable) data from indexs in 2.
     * (3) copy to index dim data.
     * (option 3.5) copy to output dim data.
     */

    // if shape_in is [N], just reshape it to [N, 1]
    int shape_num = shape_num_;
    if (shape_num_ == 1) shape_num++;
    #ifdef __linux__
    int shape_in[shape_num];
    #else
    std::shared_ptr<int> _shape_in(new int[shape_num], std::default_delete<int[]>());
    int* shape_in = _shape_in.get();
    #endif
    memcpy(shape_in, shape_in_, sizeof(int) * shape_num_);
    if (shape_num_ == 1) shape_in[shape_num_] = 1;

    /* per dim block size : if N/C/H/W, then N dim, block size is C*H*W */
    vector<int> block_size(shape_num, 1);
    for (int i = shape_num - 2; i >= 0; --i) {
        block_size[i] = block_size[i+1] * shape_in[i + 1];
    }

    /* 1. fixed other dim, keep user specify dim change.
     */
    vector<vector<int>> array(shape_num);
    for (int i = 0; i < array.size(); ++i) {
        if (i == dim) continue;
        array[i].resize(shape_in[i]);
        iota(array[i].begin(), array[i].end(), 0);
    }
    array.erase(array.begin() + dim);  /* keep other dim */

    vector<vector<int>> other_dim_idx_combine = get_array_combinations(array);

    for (int i = 0; i < other_dim_idx_combine.size(); ++i) {  /* fix other dim */

        /* other dim contribute to element index */
        int ele_idx = 0;
        for (int j = 0; j < other_dim_idx_combine[i].size(); ++j) {
            if (j < dim) ele_idx += other_dim_idx_combine[i][j] * block_size[j];
            else ele_idx += other_dim_idx_combine[i][j] * block_size[j + 1];  /* bypass dim */
        }


        // initialize original index locations
        vector<int> idx(shape_in[dim]);
        iota(idx.begin(), idx.end(), 0);

        /* copy data to temp */
        vector<Dtype> temp;
        for (int j = 0; j < shape_in[dim]; ++j) {
            temp.push_back(in_addr[ele_idx + j * block_size[dim]]);
        }

        // sort indexes based on comparing values in temp
        if (stable)
            stable_sort(idx.begin(), idx.end(),
                        [&temp, &descending](size_t i1, size_t i2) {return descending ? temp[i1] > temp[i2] : temp[i1] < temp[i2];});
        else
            sort(idx.begin(), idx.end(),
                 [&temp, &descending](size_t i1, size_t i2) {return descending ? temp[i1] > temp[i2] : temp[i1] < temp[i2];});

        // copy index
        for (int j = 0; j < shape_in[dim]; ++j) {
            index_addr[ele_idx + j * block_size[dim]] = idx[j];
            if (!is_argsort) {
                out_addr[ele_idx + j * block_size[dim]] = in_addr[ele_idx + idx[j] * block_size[dim]];
            }
        }
    }

}

namespace bmcpu {

int cpu_sort_per_dimlayer::process(void *param, int param_size)
{
  setParam(param, param_size);

  //char* in_addr     = (char *)(input_tensors_[0]);
  float* in_addr     = input_tensors_[0];
  /* TODO : possible bug exist here for float* to int* */
  int*   index_addr  = reinterpret_cast<int *>(output_tensors_[0]);
  float* out_addr    = NULL;
  if (!is_argsort) out_addr = output_tensors_[1];

  compute_sort(
      in_addr,
      index_addr,
      out_addr,
      (int *)(&input_shapes_[0][0]),
      input_shapes_[0].size(),
      is_argsort,
      stable,
      dim,
      descending);

  (*output_shapes_)[0].clear();
  (*output_shapes_)[0].assign(input_shapes_[0].begin(), input_shapes_[0].end());

  if (!is_argsort) {
      (*output_shapes_)[1].clear();
      (*output_shapes_)[1].assign(input_shapes_[0].begin(), input_shapes_[0].end());
  }

  return 0;
}

void cpu_sort_per_dimlayer::setParam(void *param, int param_size)
{
  layer_param_ = param;
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_sort_per_dim_param_t, sort_per_dim_param, layer_param_, param_size);

  dim        = sort_per_dim_param->dim;
  is_argsort = sort_per_dim_param->is_argsort;
  stable     = sort_per_dim_param->stable;
  descending = sort_per_dim_param->descending;
}

REGISTER_CPULAYER_CLASS(CPU_SORT_PER_DIM, cpu_sort_per_dim)

} /* namespace bmcpu */
