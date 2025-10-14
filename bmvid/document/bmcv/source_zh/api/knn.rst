bmcv_knn
==================

KNN。 kmeans++随机初始化需要额外实现。


**处理器型号支持：**

该接口支持BM1684X。


**接口形式：**

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


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t centroids_global_addr

  输出参数。存放聚类距离。 形状为[m_obs, m_code]。

* bm_device_mem_t labels_global_addr

  输出参数。存放KNN结果标签。形状为[m_obs]。

  只有labels存储为int32!

* bm_device_mem_t input_global_addr

  输入参数。存放输入矩阵。 形状为[m_obs,  n_feat]。

* bm_device_mem_t weight_global_addr

  输入参数。存放初始化的聚类中心权值矩阵。 形状为[m_code,  n_feat]。

* bm_device_mem_t buffer_global_addr

  输入参数。输入参数。存放buffer。

* const int*  Shape_Input

  输入参数。存放输入矩阵形参, 为[m_obs,  n_feat]。

* const int*  Shape_Weight

  输入参数。存放权值矩阵的形参, 为[m_code,  n_feat]。

* int   dims_Input

  输入参数。存放输入矩阵维度。

* int   dims_Weight

  输入参数。存放权值矩阵的维度。

* int   n_feat

  输入参数。存放输入矩阵, 初始化权值矩阵 的公有列数, 或最后维度, 。

  是来自上层 qr_cpu block 的num_spks_global_addr[0] .

* int   k

  输入参数。存放KNN的聚类簇个数。

* int   num_iter

  输入参数。存放KNN迭代次数。

* int   buffer_coeff

  输入参数。buffer面积为buffer_coeff * buffer_max_cnt 。

* int   buffer_max_cnt

  输入参数。单个buffer默认有效面积, 形状具体为[m_obs, m_code]。

* int   dtype

  输入参数。数据类型。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型支持：**

输入数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

输出数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+


**注意事项：**

1. default seed 为 42.

2、buffer_coeff必须至少为3。

3、对KNN例, buffer_max_cnt必须覆盖输入矩阵、初始化权值矩阵,  即两矩阵元素的最大个数。

4、每个dims_Input, dims_Weight只支持描述单个二维矩阵, 一般如[1,1,n,n] 。

5、kmeans++初始化  应绑定随机种子和随机分布具体实现:

6、kmeans++初始化例:

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