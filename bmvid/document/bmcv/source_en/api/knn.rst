bmcv_knn
==================

KNN. kmeans++ initialization needs an addtional implementation.


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_knn(
                    bm_handle_t handle,
                    bm_device_mem_t centroids_global_addr,//[m_code,  n_feat]
                    bm_device_mem_t labels_global_addr,   //[m_obs]
                    bm_device_mem_t input_global_addr,    //[m_obs,   n_feat]
                    bm_device_mem_t weight_global_addr,   //[m_code,  n_feat]
                    bm_device_mem_t buffer_global_addr,
                    int* Shape_Input,
                    int* Shape_Weight,
                    int dims_Input,
                    int dims_Weight,
                    int n_feat,
                    int k,
                    int num_iter,
                    int buffer_coeff,
                    unsigned int buffer_max_cnt,
                    int dtype);


**Parameter description:**

* bm_handle_t handle

  Input parameter. bm_handle handle.

* bm_device_mem_t centroids_global_addr

  Output parameter. Save KNN distance, whose shape is [m_obs, m_code].

* bm_device_mem_t labels_global_addr

  Output parameter. Save KNN labels as results, whose shape is [m_obs].

  labels_global_addr is stored as int32!

* bm_device_mem_t input_global_addr

  Input parameter. Save input matrix, whose shape is [m_obs,  n_feat].

* bm_device_mem_t weight_global_addr

  Input parameter. Save inital cluster centers as weight, whose shape is [m_code,  n_feat].

* bm_device_mem_t buffer_global_addr

  Input parameter. Save buffer for intermediate computation.

* const int*  Shape_Input

  Input parameter. Save input matrix shape as [m_obs,  n_feat].

* const int*  Shape_Weight

  Input parameter. Save weight shape as [m_code,  n_feat].

* int   dims_Input

  Input parameter. Save input matrix dims.

* int   dims_Weight

  Input parameter. Save weight matrix dims.

* int   n_feat

  Input parameter. Save width for weight or input matrix, which last-dims are same.

  Exactly num_spks_global_addr[0] from upper qr_cpu block.

* int   k

  Input parameter. Nums of given clusters.

* int   num_iter

  Input parameter. Iterations of KNN.

* int   buffer_coeff

  Input parameter. The complete buffer size is buffer_max_cnt * buffer_coeff.

* int   buffer_max_cnt

  Input parameter. Maximum valid shape for one single buffer, which is [m_obs, m_code].

* int   dtype

  Input parameter. data type.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Data type support:**

Input data currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

Output data currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |


**Notes:**

1. default seed is 42.

2. buffer_coeff >= 3.

3. For this specific KNN, buffer_max_cnt is [m_obs, m_code], which is the maximum size of input matrix and weight matrix.

4. dims_Input, dims_Weight only describe 2d-matrix, such as [1,1,n,n] .

5. kmeans++ initialization should fix random seed and choose a specific random generator when comparing data.

6. Implementation of kmeans++ initialization:

    .. code-block:: c

      #define UNIVERSAL_SEED 42
      void _kpp_weight_generator_kmeans(
          const float*        data,
          float*              out,
          int*            Shape_Output,
          const int*      Shape_Input,
          const int       dims_Input,
          const int       dims_Output,
          const int       k,
          const int       random_mode) {
          int dims = 1;
          if (dims_Input > 1) dims = Shape_Input[dims_Input - 1];
          int shape_cnt = 1;
          for (int i = 0; i < dims_Input; i++) {
              shape_cnt *= Shape_Input[i];
          }
          for (int i = 0; i < dims_Output; i++) {
              Shape_Output[i] = 1;
          }
          int m_obs = shape_cnt/dims;
          Shape_Output[dims_Output - 1] = dims;
          Shape_Output[dims_Output - 2] = k;

          std::mt19937 mt(UNIVERSAL_SEED);
          std::cout << "[Random info]"<<mt() << std::endl;
          std::uniform_real_distribution<float> dist_float(0.0, 1.0);
          for (int i = 0; i < k ; i++) {
              if (i == 0) {
                  std::uniform_int_distribution<int> dist(0, m_obs);
                  int randint  = dist(mt);
                  if (random_mode == POISSON_CPP || random_mode == MT19937_CPP) {
                  memcpy(out, data + randint * dims, dims * sizeof(float));
                  } else if (random_mode == CONST_WEIGHT) {
                  int fake_rng_randint = int(m_obs/2);
                  memcpy(out, data + fake_rng_randint * dims, dims * sizeof(float));
                  } else { assert(0); }
              } else {
                  //sqeuclidean(init[:i,:], data)
                  float* D2_0 = new float [i * m_obs];
                  for(int m = 0; m < i; m++) {
                      for(int idx_data = 0; idx_data < m_obs; idx_data++) {
                          D2_0[m * m_obs + idx_data] = sqeucliden(out, data,  m ,idx_data, dims);
                      }
                  }
                  float* D2 = new float [m_obs];
                  for (int j = 0; j < m_obs; j++) {
                      float min = D2_0[j];
                      for(int m = 0; m < i; m++) {
                          min = std::min(D2_0[m*m_obs + j], min);
                      }
                      D2[j] = min;
                  }
                  float T_sum = 0.0;
                  for (int j = 0; j < m_obs; j++) {
                      T_sum +=  D2[j];
                  }
                  float* probs = new float [m_obs];
                  for (int j = 0; j < m_obs; j++) {
                      probs[j] =  D2[j]/T_sum;
                  }
                  float* cumprobs = new float [m_obs];
                  cumprobs[0] = probs[0];
                  for (int j = 1; j < m_obs; j++) {
                      cumprobs[j] =  cumprobs[j-1] + probs[j];
                  }
                  //r = rng.uniform()
                  float r = 0.0;//dist_float(mt);
                  if (random_mode == MT19937_CPP || random_mode == POISSON_CPP) {
                  r = dist_float(mt);
                  } else if (random_mode == CONST_WEIGHT) {
                  //np.min(cumprobs) + (np.max(cumprobs)- np.min(cumprobs))/2
                  float max_temp = cumprobs[0];
                  float min_temp = cumprobs[0];
                  for (int idx = 1; idx < m_obs; idx++) {
                      min_temp = cumprobs[idx] > min_temp ? min_temp : cumprobs[idx];
                      max_temp = cumprobs[idx] > max_temp ? cumprobs[idx] : max_temp;
                  }
                  r = min_temp + (max_temp - min_temp)/2.0;//0.5072551558477147;
                  } else { assert(0); }
                  int sort_idx = 0;
                  for (int j = 1; j < m_obs; j++) {
                      if ((cumprobs[j - 1] < r) && (r <= cumprobs[j]))  {
                          sort_idx = j;
                          break;
                      }
                  }
                  memcpy(out + i * dims, data + sort_idx * dims, dims * sizeof(float));
                  delete [] D2;
                  delete [] D2_0;
                  delete [] probs;
                  delete [] cumprobs;
              }
          }
      }