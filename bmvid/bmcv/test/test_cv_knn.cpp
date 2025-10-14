#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <cmath>
#include "bmcv_api.h"
#include "test_misc.h"
#include <random>
#include <fstream>
#include <cstring>
#ifdef __linux__
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

using namespace std;

#define CONST_WEIGHT    0
#define POISSON_CPP      1
#define MT19937_CPP     2

#define UNIVERSAL_SEED  42

#define SCIPY_SOURCE    0
#define CPP_SOURCE      1

#if defined(__linux__) && (USING_OPENBLAS)
enum tpudnnStatus_t
{
    TPUDNN_STATUS_SUCCESS,
    TPUDNN_STATUS_FAILED
};

enum CMP_Status_t
{
    FLAG_SUCCESS,
    FLAG_FAILED,
};
static bool _cmp_simple_float(float* reference, float* tpu_result, int length, const char *info_label,  float delta) {
    for(int i = 0; i < length; i++) {
            if(fabs(reference[i] - tpu_result[i]) > delta) {
                printf("[Error] %s index: i = %d, reference: %f, tpu_result: %f\n", info_label , i,  reference[i], tpu_result[i]);
                return FLAG_FAILED;
            }
    }
    return FLAG_SUCCESS;
}

static bool _cmp_simple_int(int* reference, int* tpu_result, int length, const char *info_label,  float delta) {
    for(int i = 0; i < length; i++) {
            if(abs(reference[i] - tpu_result[i]) > delta) {
               if (i<30)
                printf("[Error] %s index: i = %d, reference: %d, tpu_result: %d\n", info_label , i,  reference[i], tpu_result[i]);
            }
    }
    for(int i = 0; i < length; i++) {
            if(abs(reference[i] - tpu_result[i]) > delta) {
              return FLAG_FAILED;
            }
    }
    return FLAG_SUCCESS;
}

#define ERR_MAX 1e-3

int tpu_knn(
        bm_handle_t handle,
        float* centroids_global_addr, //[m_code,  n_feat]
        int* labels_global_addr,      //[m_obs]
        float* input_global_addr,     //[m_obs,   n_feat]
        float* weight_global_addr,    //[m_code,  n_feat]
        float* buffer_global_addr,
        int* Shape_Input,
        int* Shape_Weight,
        int   dims_Input,
        int   dims_Weight,
        int   n_feat,
        int   k,
        int   num_iter,
        int   buffer_coeff,
        unsigned int buffer_max_cnt,
        int dtype){
        bm_device_mem_t centroids_global_addr_tpu; //[m_code,  n_feat]
        bm_device_mem_t labels_global_addr_tpu;    //[m_obs]
        bm_device_mem_t input_global_addr_tpu;     //[m_obs,   n_feat]
        bm_device_mem_t weight_global_addr_tpu;    //[m_code,  n_feat]
        bm_device_mem_t buffer_global_addr_tpu;
    unsigned int shape_cnt_mobs_nfeat = 1;
    for (int i = 0; i < dims_Input; i++)
    {
        shape_cnt_mobs_nfeat *= Shape_Input[i];
    }
    unsigned int shape_cnt_mcode_nfeat = 1;
    for (int i = 0; i < dims_Weight; i++)
    {
        shape_cnt_mcode_nfeat *= Shape_Weight[i];
    }
    unsigned int shape_labels = shape_cnt_mobs_nfeat/n_feat;
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1 = {0, 0}, t2 = {0, 0};
    ret = bm_malloc_device_byte(handle, &centroids_global_addr_tpu, shape_cnt_mcode_nfeat * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte centroids_global_addr float failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &labels_global_addr_tpu, shape_labels * sizeof(int));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte labels_global_addr int failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &input_global_addr_tpu, shape_cnt_mobs_nfeat * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte input_global_addr float failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }


    ret = bm_malloc_device_byte(handle, &weight_global_addr_tpu, shape_cnt_mcode_nfeat * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte weight_global_addr float failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &buffer_global_addr_tpu, buffer_coeff * buffer_max_cnt * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte buffer_global_add float failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_memcpy_s2d(handle, input_global_addr_tpu,  input_global_addr);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, centroids_global_addr_tpu);
        bm_free_device(handle, labels_global_addr_tpu);
        bm_free_device(handle, input_global_addr_tpu);
        bm_free_device(handle, weight_global_addr_tpu);
        bm_free_device(handle, buffer_global_addr_tpu);
        bm_dev_free(handle);
        return -1;
    }
    ret = bm_memcpy_s2d(handle, weight_global_addr_tpu, weight_global_addr);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, centroids_global_addr_tpu);
        bm_free_device(handle, labels_global_addr_tpu);
        bm_free_device(handle, input_global_addr_tpu);
        bm_free_device(handle, weight_global_addr_tpu);
        bm_free_device(handle, buffer_global_addr_tpu);
        bm_dev_free(handle);
        return -1;
    }
    ret = bm_memcpy_s2d(handle, buffer_global_addr_tpu, buffer_global_addr);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, centroids_global_addr_tpu);
        bm_free_device(handle, labels_global_addr_tpu);
        bm_free_device(handle, input_global_addr_tpu);
        bm_free_device(handle, weight_global_addr_tpu);
        bm_free_device(handle, buffer_global_addr_tpu);
        bm_dev_free(handle);
        return -1;
    }


    gettimeofday_(&t1);
    ret = bmcv_knn(
        handle,
        //output
        centroids_global_addr_tpu,//[m_code,  n_feat]
        labels_global_addr_tpu,   //[m_obs]
        //input nxn
        input_global_addr_tpu,    //[m_obs,   n_feat]
        weight_global_addr_tpu,   //[m_code,  n_feat]
        //buffer 2 * max(m_obs, m_code) * 4
        buffer_global_addr_tpu,
        Shape_Input,
        Shape_Weight,
        dims_Input,
        dims_Weight,
        n_feat,
        k,
        num_iter,
        buffer_coeff,
        buffer_max_cnt,
        dtype);

    if (ret != BM_SUCCESS) {
        printf("bmcv_knn float failed. ret = %d\n", ret);
        bm_free_device(handle, centroids_global_addr_tpu);
        bm_free_device(handle, labels_global_addr_tpu);
        bm_free_device(handle, input_global_addr_tpu);
        bm_free_device(handle, weight_global_addr_tpu);
        bm_free_device(handle, buffer_global_addr_tpu);
        bm_dev_free(handle);
        return -1;
    }
    gettimeofday_(&t2);
    printf("bmcv_cv_knn TPU using time = %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    ret = bm_memcpy_d2s(handle, centroids_global_addr, centroids_global_addr_tpu);
    ret = bm_memcpy_d2s(handle, labels_global_addr,    labels_global_addr_tpu);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed. ret = %d\n", ret);
        bm_free_device(handle, centroids_global_addr_tpu);
        bm_free_device(handle, labels_global_addr_tpu);
        bm_free_device(handle, input_global_addr_tpu);
        bm_free_device(handle, weight_global_addr_tpu);
        bm_free_device(handle, buffer_global_addr_tpu);
        bm_dev_free(handle);
        return -1;
    }

    bm_free_device(handle, centroids_global_addr_tpu);
    bm_free_device(handle, labels_global_addr_tpu);
    bm_free_device(handle, input_global_addr_tpu);
    bm_free_device(handle, weight_global_addr_tpu);
    bm_free_device(handle, buffer_global_addr_tpu);
    return 0;
}


int cmp_res(float* tpu_out, float* cpu_out, int row, int col)
{
    int i, j;

    for (i = 0; i < row; ++i) {
        for (j = 0; j < col; ++j) {
            if (abs(cpu_out[i * col + j] - tpu_out[i * col + j]) > ERR_MAX) {
                printf("index[%d][%d]: cpu_output = %f, tpu_output = %f\n", \
                        i, j, cpu_out[i * col + j], tpu_out[i * col + j]);
                return -1;
            }
        }
    }
    return 0;
}


typedef struct {
  int Shape_Input[8] = {0, 0, 0,0,0,0,0,0};
  int Shape_Weight[8]= {0, 0, 0,0,0,0,0,0};
  int dims_Input     = 0;
  int dims_Weight    = 0;
  unsigned  n_feat   = 0;
  int num_iter       = 0;
  int buffer_coeff   = 0;
  int k              = 0;
  int weight_mode    = 0;
  int mode_ref       = 0;
} knn_naive_param_t;


//D2_0[m * m_obs + idx_data] = sqeucliden(out, data,  m ,idx_data, dims);
static inline float sqeucliden(
    float*              out,
    const float*        data,
    const int       idx_out,
    const int       idx_data,
    const int       n_feats) {
  float  psum = 0.0;
  for (int i = 0; i < n_feats; i += 1) {
    float p21 = data[idx_data * n_feats + i]  - out[idx_out * n_feats +i];
     psum += p21 * p21;
  }
  return psum;
}

/*
 p1 idx_out  1*dims
 p2 idx_data 1*dims
*/
static inline float standard_euclidean(
    const float*        p1,
    const float*        p2,
    const int       idx_p1,
    const int       idx_p2,
    const int       dims) {
  float  psum = 0.0;
  for (int i = 0; i < dims; i += 1) {
    float p21 = p1[idx_p1 * dims + i]  - p2[idx_p2 * dims +i];
    psum += p21 * p21;
  }
  psum = std::sqrt(psum);
  return psum;
}

static inline void py_vq_sophon(
  int*         code,       //[m_obs]
  float*       min_dist,   //[m_obs]
  const float* data,       //[m_obs,  n_feat]
  const float* code_book,  //[m_code, n_feat]
  float* dist,             //[m_obs,  m_code]
  const int m_obs,
  const int m_code,
  const int n_feat
) {
    // float* dist = new float [m_obs * m_code];
    for(int m = 0; m < m_obs; m++) {
        for(int idx_data = 0; idx_data < m_code; idx_data++) {
            dist[m * m_code + idx_data] = standard_euclidean(data, code_book,  m ,idx_data, n_feat);
        }
    }
    //code = dist.argmin(axis=1) #every code element < code_book.shape[-1] == obs.shape[-1]
    //[m_obs] <= [m_obs,  m_code]
    for(int i = 0; i < m_obs; i++) {
      float tmp_min = dist[i * m_code];
      int   index   = 0;
      for(int idx = 1; idx < m_code; idx++) {
         if (dist[i * m_code + idx] < tmp_min) {
            tmp_min = dist[i * m_code + idx];
            index   = idx;
         }
      }
      code[i] = index;
    }
    //min_dist = dist[np.arange(len(code)), code]
    //  [m_obs] [m_obs,  m_code]
    assert(m_obs >= m_code);
    for(int i = 0; i < m_obs; i++) {
          int idx_code = code[i];
          min_dist[i] = dist[i * m_code + idx_code];
    }
}

static inline void _update_cluster_means(
  float* cb,            //[nc, n_feat]
  int*   has_members,   //[nc]
  const float* obs,     //[m_obs, n_feat]
  const int*   labels,  //[m_obs]
  const int    nc,
  const int    m_obs,
  const int    n_feat
) {
  //cb = np.zeros((nc, nfeat), dtype=obs.dtype)
  for(int i = 0; i < nc; i++) {
    for(int j = 0; j < n_feat; j++) {
      cb[i * n_feat + j] = 0.0;
    }
  }
  // obs_count = np.zeros(nc, np.intc)
  // obs_count = has_members
  for(int i = 0; i < nc; i++) {
    has_members[i] = 0;
  }
  for(int i = 0; i < m_obs; i++) {
    int label = labels[i];
    assert(label<= nc);
    //cb[label, :] +=  obs_p[i, :]
    for(int j = 0; j < n_feat; j++) {
      cb[label * n_feat + j] += obs[i * n_feat + j];
    }
    has_members[label] += 1;
  }
  for(int i = 0; i < nc; i++) {
    int cluster_size = has_members[i];
    if (cluster_size > 0) {
      for(int j = 0; j < n_feat; j++) {
         cb[i * n_feat + j] /= 1.0*cluster_size;
      }
    }
  }
  for(int i = 0; i < nc; i++) {
    has_members[i] = has_members[i] > 0 ? 1 : 0;
  }
}

void naive_KNN_forward(
  int* labels,            //[m_obs]
  const float* data,      //[m_obs,  n_feat]
  const float* code_book, //[m_code, n_feat]
  const int m_obs,
  const int m_code,
  const int n_feat,
  const int num_iter,
  const int k
) {
  assert(k == m_code);
  float*    min_dist = new float [m_obs];
  int nc             = k;
  float*    new_code_book    = new float [nc * n_feat];
  float*    update_code_book = new float [nc * n_feat];
  int*      has_members      = new int   [nc];
  float*    dist             = new float [m_obs * m_code];
  for (int i = 0; i < nc * n_feat; i ++) {
    update_code_book[i] = code_book[i];
  }
  for (int i = 0; i < num_iter; i ++) {
    py_vq_sophon(
      labels,     //[m_obs]
      min_dist,   //[m_obs]
      data,       //[m_obs,  n_feat]
      update_code_book,  //[m_code, n_feat]
      dist,         //[[m_obs, m_code]]
      m_obs,
      m_code,
      n_feat);
    _update_cluster_means(
      new_code_book,  //[nc, n_feat]
      has_members,   //[nc]
      data,     //[m_obs, n_feat]
      labels,  //[m_obs]
      nc,
      m_obs,
      n_feat);
    int flag = 0;
    for (int i = 0; i < nc; i ++) {
      if (has_members[i] < 1) {
        flag =1;
        break;
      }
    }
    if(flag) {
      assert(0);
      //new_code_book[~has_members] = code_book[~has_members]
      for (int i = 0; i < nc; i ++) {
        if(has_members[i] < 1) {
            for (int j = 0; j < n_feat; j ++) {
              new_code_book[i * n_feat + j] =  update_code_book[i * n_feat + j];
          }
        }
        update_code_book[i] = new_code_book[i];
      }
    }
    //code_book = new_code_book
    for (int i = 0; i < nc * n_feat; i ++) {
      update_code_book[i] = new_code_book[i];
    }
  }
  delete [] min_dist;
  delete [] new_code_book;
  delete [] update_code_book;
  delete [] has_members;
  delete [] dist;
}

float sqeuclidean(const float* a, const float* b, int dims) {
    float dist = 0.0f;
    for(int d = 0; d < dims; ++d) {
        float diff = a[d] - b[d];
        dist += diff * diff;
    }
    return dist;
}

void _kpp_weight_generator_kmeans(
        const float *data,
        float       *out,
        int         *Shape_Output,
        const int   *Shape_Input,
        const int    dims_Input,
        const int    dims_Output,
        const int    k,
        const int    random_mode) {

    if (random_mode == POISSON_CPP) {
        std::cout << "[INFO] start random_mode: POISSON_CPP.\n";
    } else if (random_mode == MT19937_CPP) {
        std::cout << "[INFO] start random_mode: MT19937_CPP.\n";
    } else if (random_mode == CONST_WEIGHT) {
        std::cout << "[INFO] start random_mode: CONST_WEIGHT.\n";
    } else {
        std::cerr << "[ERROR] Unsupported random mode.\n";
        assert(0);
    }
    int dims = (dims_Input > 1) ? Shape_Input[dims_Input - 1] : 1;
    int shape_cnt = 1;
    for (int i = 0; i < dims_Input; i++) {
        shape_cnt *= Shape_Input[i];
    }
    int m_obs = shape_cnt / dims;

    for (int i = 0; i < dims_Output; i++) {
        Shape_Output[i] = 1;
    }
    Shape_Output[dims_Output - 1] = dims;
    Shape_Output[dims_Output - 2] = k;

    std::mt19937 mt(std::random_device{}());
    std::poisson_distribution<int> dist_poisson(m_obs);
    std::uniform_int_distribution<int> dist_uniform(0, m_obs - 1);
    std::uniform_real_distribution<float> dist_float(0.0f, 1.0f);

    int first_centroid_idx;
    if (random_mode == POISSON_CPP) {
        first_centroid_idx = dist_poisson(mt) % m_obs;
                first_centroid_idx = dist_uniform(mt);

    } else if (random_mode == MT19937_CPP) {
        first_centroid_idx = dist_uniform(mt);
    } else if (random_mode == CONST_WEIGHT) {
        first_centroid_idx = m_obs / 2;
    } else {
        assert(0);
    }

    // Copy the first centroid
    std::memcpy(out, data + first_centroid_idx * dims, dims * sizeof(float));

    // Iterate to select remaining centroids
    for (int i = 1; i < k; ++i) {
        // Vector to store the minimum squared distance for each data point
        std::vector<float> min_dist_sq(m_obs, std::numeric_limits<float>::max());

        // Compute the squared distances to the nearest existing centroid
        for (int j = 0; j < m_obs; ++j) {
            for (int m = 0; m < i; ++m) {
                float dist_sq = sqeuclidean(out + m * dims, data + j * dims, dims);
                if (dist_sq < min_dist_sq[j]) {
                    min_dist_sq[j] = dist_sq;
                }
            }
        }

        // Compute the cumulative distribution
        std::vector<float> cumprobs(m_obs, 0.0f);
        float total = 0.0f;
        for (int j = 0; j < m_obs; ++j) {
            total += min_dist_sq[j];
            cumprobs[j] = total;
        }

        // Normalize cumulative probabilities
        for (int j = 0; j < m_obs; ++j) {
            cumprobs[j] /= total;
        }

        // Sample a random number
        float r;
        if (random_mode == CONST_WEIGHT) {
            // Deterministic middle value
            r = 0.5f; // Middle of [0,1]
        } else {
            r = dist_float(mt);
        }

        // Binary search to find the appropriate centroid index
        int sort_idx = std::lower_bound(cumprobs.begin(), cumprobs.end(), r) - cumprobs.begin();

        // Safety check
        sort_idx = std::min(sort_idx, m_obs - 1);

        // Copy the selected centroid
        std::memcpy(out + i * dims, data + sort_idx * dims, dims * sizeof(float));
    }
}
void static_gen_param(knn_naive_param_t* param, int max_len, int num_iter, int k, int n_feat, int buffer_coeff) {
  param->k            = k;
  param->n_feat       = n_feat;
  param->dims_Input   = 4;
  param->dims_Weight  = 4;
  param->num_iter     = num_iter;
  for (int i = 0; i   < 8; i++) {
    param->Shape_Weight[i]   = 0;
  }
  for (int i = 0; i   < 8; i++) {
    param->Shape_Input[i]   = 0;
  }
  for (int i = 0; i   < param->dims_Input; i++) {
    param->Shape_Input[i]   = 1;
  }
  param->Shape_Input[3] = param->n_feat;
  param->Shape_Input[2] = max_len;
  for (int i = 0; i   < param->dims_Weight; i++) {
    param->Shape_Weight[i]   = 1;
  }
  param->Shape_Weight[3] = param->n_feat;
  param->Shape_Weight[2] = param->k;
  param->buffer_coeff    = buffer_coeff;
}


int test_knn(bm_handle_t handle) {

    int ret = 0;
    struct timeval t1 = {0, 0}, t2 = {0, 0};
    knn_naive_param_t param;
    param.mode_ref            = SCIPY_SOURCE;
    param.weight_mode         = CONST_WEIGHT;//MT19937_CPP;
    int buffer_coeff    = 3;
    int num_iter        = 2;
    int n               = 256;
    int n_feats         = 8;
    int k               = n_feats;
    assert(k == n_feats);
    if(param.mode_ref == CPP_SOURCE) {
      num_iter        = 2;
      // n               = 128;
      // n_feats         = 8;
      n               = 4;
      n_feats         = 3;
      k               = n_feats;
      param.weight_mode = CONST_WEIGHT;
    }
    static_gen_param(&param, n, num_iter, k, n_feats, buffer_coeff);
    printf("---------------------[Info] params for KNN_Naive------------------------\n");

    printf("\n");
    printf("dims_Input :%d\n", param.dims_Input);
    for (int i = 0; i < param.dims_Input; i++) {
      printf("Shape_Input[%d]:%d ", i, param.Shape_Input[i]);
    }
    printf("\n");
    printf("dims_Weight :%d\n", param.dims_Weight);
    for (int i = 0; i < param.dims_Weight; i++) {
      printf("Shape_Weight[%d]:%d ", i, param.Shape_Weight[i]);
    }
    printf("\n");
    printf("num_iter :%d\n", param.num_iter);
    if(param.mode_ref == SCIPY_SOURCE) {
        printf("mode_ref: SCIPY_SOURCE\n");
    } else if (param.mode_ref == CPP_SOURCE){
        printf("mode_ref: CPP_RANDOM_GEN\n");
    } else { assert(0); }

    if(param.weight_mode == CONST_WEIGHT) {
        printf("weight_mode: CONST_WEIGHT\n");
    } else if (param.weight_mode == POISSON_CPP){
        printf("weight_mode: POISSON_CPP\n");
    } else if (param.weight_mode == MT19937_CPP){
        printf("weight_mode: MT19937_CPP\n");
    } else { assert(0); }
    printf("\n");

    unsigned int shape_cnt_mobs_nfeat = 1;
    for (int i = 0; i < param.dims_Input; i++)
    {
        shape_cnt_mobs_nfeat    *= param.Shape_Input[i];
    }
    unsigned int shape_cnt_mcode_nfeat = 1;
    for (int i = 0; i < param.dims_Weight; i++)
    {
        shape_cnt_mcode_nfeat   *= param.Shape_Weight[i];
    }
    unsigned int m_code          = shape_cnt_mcode_nfeat/param.n_feat;
    unsigned int m_obs           = shape_cnt_mobs_nfeat/param.n_feat;
    unsigned int shape_labels    = m_obs;
    unsigned int buffer_max_cnt  = std::max(m_obs, m_code) * std::max(m_code, param.n_feat);

    float* R_tpu_centroids       = new float[shape_cnt_mcode_nfeat];
    std::fill(R_tpu_centroids, R_tpu_centroids + shape_cnt_mcode_nfeat, 0.0f);
    int*   R_tpu_labels          = new int[shape_labels];
    std::fill(R_tpu_labels, R_tpu_labels + shape_labels, 0);
    float* buffer                = new float[buffer_coeff * buffer_max_cnt];
    std::fill(buffer, buffer + buffer_coeff * buffer_max_cnt, 0.0f);
    int*   R_scipy_random        = new int[shape_labels];
    int*   R_scipy_sophon_mt1377 = new int[shape_labels];
    int*   R_cpu_naive           = new int[shape_labels];

    float* blob_input            = new float[shape_cnt_mobs_nfeat];
    float* blob_weight           = new float[shape_cnt_mcode_nfeat];
    if(param.mode_ref == SCIPY_SOURCE) {
      string path_prefix = "/mnt/software/code_eigh/";
      std::ifstream file_data(path_prefix + "./KNN_input_data.bin", std::ios::in | std::ios::binary);
      std::ifstream file_weight_scipy(path_prefix + "./KNN_weight.bin", std::ios::in | std::ios::binary);
      std::ifstream file_result_labels_scipy(path_prefix + "./result_label.bin", std::ios::in | std::ios::binary);
      std::ifstream file_result_labels_sophon(path_prefix + "./result_label_DQ.bin", std::ios::in | std::ios::binary);

      file_weight_scipy.read((char*)blob_weight,                   shape_cnt_mcode_nfeat  * sizeof(float));
      file_data.read((char*)blob_input,                            shape_cnt_mobs_nfeat   * sizeof(float));
      file_result_labels_scipy.read((char*)R_scipy_random,         shape_labels           * sizeof(int));
      file_result_labels_sophon.read((char*)R_scipy_sophon_mt1377, shape_labels           * sizeof(int));
    }
    else if (param.mode_ref == CPP_SOURCE) {
      assert(shape_cnt_mobs_nfeat == 12);
      float blob_input_static[12] = {0.4999999,   0.68819,    0.49999985,
                  0.50000006, -0.16245992 ,-0.5       ,
                  0.50000006, -0.6881908  , 0.5000001 ,
                  0.4999999 ,  0.16245997 ,-0.49999988};
      for (int i = 0; i < (int)shape_cnt_mobs_nfeat; i++) {
        blob_input[i] = blob_input_static[i];
      }
      // std::ifstream file_data("/workspace/code_eigh/DQ_KNN_input.bin", std::ios::in | std::ios::binary);
      // file_data.read((char*)blob_input, shape_cnt_mobs_nfeat*sizeof(float));

    }
    gettimeofday_(&t1);
    float* fake_weight          = new float[shape_cnt_mcode_nfeat];
    assert(param.weight_mode    != POISSON_CPP);
    int Shape_Weight_fake[8];
    _kpp_weight_generator_kmeans(
      blob_input,
      fake_weight,
      Shape_Weight_fake,
      param.Shape_Input,
      param.dims_Input,
      param.dims_Weight,
      param.k,
      param.weight_mode);
    if (param.mode_ref == SCIPY_SOURCE) {
      int cmp_ret_weight = _cmp_simple_float( blob_weight, fake_weight,    shape_cnt_mcode_nfeat, "weight cmp", 1e-6);
      if(cmp_ret_weight != 0) {
        printf("[Warning]_kpp_weight_generator_kmeans is not same as _kpp scipy! \n");
        printf("----------------------------------------------------\n");
        // assert(0);
      } else {
        printf("----------------------------------------------------\n");
        printf("[Correct] correctness of _kpp_scipy vs _kpp_cpp! \n");
        printf("----------------------------------------------------\n");
      }
    }
    float* adapt_weight = param.weight_mode == CONST_WEIGHT ?  fake_weight : blob_weight;
    naive_KNN_forward(
            R_cpu_naive, //[m_obs]
            blob_input,  //[m_obs,  n_feat]
            adapt_weight, //[m_code, n_feat]
            m_obs,
            m_code,
            param.n_feat,
            param.num_iter,
            param.k);
    printf("[Note]  naive_KNN_forward is done!\n");
    gettimeofday_(&t2);
    printf("bmcv_knn CPU using time = %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    ret = tpu_knn(
            handle,
            R_tpu_centroids,
            R_tpu_labels,
            blob_input,
            adapt_weight,
            buffer,
            param.Shape_Input,
            param.Shape_Weight,
            param.dims_Input,
            param.dims_Weight,
            param.n_feat,
            param.k,
            param.num_iter,
            param.buffer_coeff,
            buffer_max_cnt,
            DT_FP32);

    if (ret == TPUDNN_STATUS_SUCCESS) {
      //int cmp_ret_0 = sg_array_cmp_float(R_scipy_random, R_tpu_centroids, shape_cnt_mobs_nfeat, "float cmp", 1e-6);
      printf("-------------------------------------------------------------\n");
      if (param.weight_mode == POISSON_CPP || param.weight_mode == MT19937_CPP) {
        int cmp_ret_00  = _cmp_simple_int(R_scipy_random,     R_cpu_naive,    shape_labels, "labels: scipy_random vs cpu-c++", 1e-6);
        cmp_ret_00 += 0;
      }
      if (param.mode_ref == SCIPY_SOURCE) {
        printf("[Note]Start labels: scipy_random vs kernel!\n");
        int cmp_ret_1  = _cmp_simple_int(R_scipy_random,       R_tpu_labels,    shape_labels, "labels: scipy_random vs kernel", 1e-6);
        if ((cmp_ret_1 != 0) && (param.weight_mode == CONST_WEIGHT))  {
          printf("[Ignore]I fixed weight, so that you can't compare kernel with pure scipy!\n");
        }
        printf("-------------------------------------------------------------\n");
        int cmp_ret_3 = _cmp_simple_int(R_scipy_sophon_mt1377, R_cpu_naive,    shape_labels, "label: scipy_fixed vs cpu-c++;", 1e-6);
        if (cmp_ret_3 != 0) {
          printf("[Warning]scipy_fixed vs cpu-c++ wrong!\n");
        } else {
          printf("[Correct]scipy_fixed vs cpu-c++!\n");
        }
        printf("-------------------------------------------------------------\n");
        printf("[Note] Start label: scipy_fixed vs kernel!\n");
        int cmp_ret_2 = _cmp_simple_int(R_scipy_sophon_mt1377, R_tpu_labels,   shape_labels, "label: scipy_fixed vs kernel", 1e-6);
        if (cmp_ret_2 != 0) {
          printf("[Warning]scipy_fixed vs tpu wrong!\n");
        }
        printf("-------------------------------------------------------------\n");
      }

      int cmp_ret_01  = _cmp_simple_int(R_cpu_naive, R_tpu_labels,   shape_labels, "labels: cpu-c++ vs kernel", 1e-6);
      if (cmp_ret_01 != 0) {
        printf("[Warning]cpu-c++ vs kernel wrong!\n");
      } else {
        printf("[Correct]cpu-c++ vs kernel!\n");
      }
      printf("-------------------------------------------------------------\n");
      int cmp_ret_result = cmp_ret_01;
      if (cmp_ret_result!= 0) {
          ret = TPUDNN_STATUS_FAILED;
      }
    }
    delete[] R_tpu_centroids;
    delete[] R_tpu_labels;
    delete[] blob_input;
    delete[] blob_weight;
    delete[] buffer;
    delete[] R_scipy_random;
    delete[] R_scipy_sophon_mt1377;
    delete[] R_cpu_naive;
    delete[] fake_weight;
    return ret;
}

int main(int argc, char* args[])
{
    struct timespec tp;
    clock_gettime_(0, &tp);
    srand(tp.tv_nsec);

    int loop = 1;
    int ret = 0;
    int i = 0;
    bm_handle_t handle;

    ret = bm_dev_request(&handle, 0);
    if (ret) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return ret;
    }

    if (argc > 1) loop = atoi(args[1]);

    for (i = 0; i < loop; ++i) {
        ret = test_knn(handle);
        if (ret) {
            printf("------Test KNN Failed!------\n");
            bm_dev_free(handle);
            return ret;
        }
    }

    printf("------Test KNN Success!------\n");
    bm_dev_free(handle);
    return ret;
}
#else
  int main(int argc, char* args[])
  {
      struct timespec tp;
      clock_gettime_(0, &tp);
      srand(tp.tv_nsec);

      int loop = 1;
      int ret = 0;
      int i = 0;
      bm_handle_t handle;

      ret = bm_dev_request(&handle, 0);
      if (ret) {
          printf("bm_dev_request failed. ret = %d\n", ret);
          return ret;
      }

      if (argc > 1) loop = atoi(args[1]);

      for (i = 0; i < loop; ++i) {
          if (ret) {
              printf("------Test KNN Failed!------\n");
              bm_dev_free(handle);
              return ret;
          }
      }

      printf("------Test KNN Skip!------\n");
      bm_dev_free(handle);
      return ret;
}
#endif