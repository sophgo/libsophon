#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include "bmlib_runtime.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

using namespace std;

void fill ( float* data, int* index, int batch, int batch_num, int batch_stride){
  for(int i = 0; i < batch; i++){
    for(int j = 0; j < batch_num; j++){
      data[i * batch_stride + j] = rand() % 10000 * 1.0f;
      index[i * batch_stride + j] = i * batch_stride + j;
    }
  }
}

bool GreaterSort (std::pair<int, float>a, std::pair<int, float>b) {return (a.second > b.second);}
bool LessSort (std::pair<int, float>a, std::pair<int, float>b) {return (a.second < b.second);}

void topk_reference( int k,
                     int batch,
                     int batch_num,
                     int batch_stride,
                     int descending,
                     bool bottom_index_valid,
                     float* bottom_data,
                     int* bottom_index,
                     float* top_data_ref,
                     int* top_index_ref) {
    for(int b = 0 ;b < batch; b++){
      std::vector<std::pair<int, float>> bottom_vec(batch_num);
      for(int i = 0; i < batch_num; i++){
        int offset = b * batch_stride + i;
        float data;
        data = bottom_data[offset];
        if(bottom_index_valid){
          bottom_vec[i] = std::make_pair(bottom_index[offset], data);
        }
        else{
          bottom_vec[i] = std::make_pair(i, data);
        }
      }
      std::stable_sort(bottom_vec.begin(), bottom_vec.end(), descending ? GreaterSort : LessSort);
      for(int i = 0; i < k; i++){
        top_index_ref[b * k + i] = bottom_vec[i].first;
        top_data_ref[b * k + i] = bottom_vec[i].second;
      }
    }
}

int test_topk_v1( bm_handle_t handle, int group_num, int stride, int k, int per_batch_size){
    bm_device_mem_t src_data_mem;
    bm_device_mem_t dst_data_mem;
    bm_device_mem_t dst_index_mem;
    bm_device_mem_t buffer_mem;

    bm_malloc_device_byte(handle, &src_data_mem, group_num * stride * sizeof(float));
    bm_malloc_device_byte(handle, &buffer_mem, stride * 3 * sizeof(float));
    bm_malloc_device_byte(handle, &dst_data_mem, k * group_num * sizeof(float));
    bm_malloc_device_byte(handle, &dst_index_mem, k * group_num * sizeof(int));

    float* data = new float [per_batch_size];
    for (int i = 0; i < per_batch_size; ++i) {
      data[i] = i * 0.01f;
    }
    unsigned long long src_addr = bm_mem_get_device_addr(src_data_mem);
    for (int i = 0; i < group_num; ++i) {
      bm_device_mem_t tmp_mem;
      bm_set_device_mem(&tmp_mem, stride * sizeof(float),
                      src_addr + i * stride * sizeof(float));
      bm_memcpy_s2d_partial(handle, tmp_mem, data, per_batch_size * sizeof(float));
    }
    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_batch_topk(handle, src_data_mem, src_data_mem, dst_data_mem, dst_index_mem,
              buffer_mem, false, k, group_num, &per_batch_size, true,
              stride);
    gettimeofday_(&t2);
    long used = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;

    float* out_data = new float [group_num * k];
    int* out_index = new int [group_num * k];

    bm_memcpy_d2s(handle, out_data, dst_data_mem);
    bm_memcpy_d2s(handle, out_index, dst_index_mem);
    for (int i = 0; i < group_num; ++i) {
      std::cout << "Group " << i << std::endl;
      for (int j = 0; j < k; ++j) {
        std::cout << out_data[j + i * k] << " ";
      }
      std::cout << std::endl;
      for (int j = 0; j < k; ++j) {
        std::cout << out_index[j + i * k] << " ";
      }
      std::cout << std::endl;
    }
    printf("batch topk used time: %.3f ms\n", (float)used / 1000);
    delete [] data;
    delete [] out_data;
    delete [] out_index;
    bm_free_device(handle, src_data_mem);
    bm_free_device(handle, dst_data_mem);
    bm_free_device(handle, dst_index_mem);
    bm_free_device(handle, buffer_mem);
    bm_dev_free(handle);
    return 0;
}

int test_topk_v2( bm_handle_t handle, int k, int batch, int batch_num, int batch_stride, int descending, bool bottom_index_valid){
  std::cout << "k: " << k << std::endl;
  std::cout << "batch: " << batch << std::endl;
  std::cout << "batch_num: " << batch_num << std::endl;
  std::cout << "batch_stride: " << batch_stride << std::endl;
  float* bottom_data = new float[batch * batch_stride * sizeof(float)];
  int* bottom_index = new int[batch * batch_stride];
  float* top_data = new float[batch * batch_stride * sizeof(float)];
  int* top_index = new int[batch * batch_stride];
  float* top_data_ref = new float[batch * k * sizeof(float)];
  int* top_index_ref = new int[batch * k];
  float* buffer = new float[3 * batch_stride * sizeof(float)];

  // fill
  fill( bottom_data, bottom_index, batch, batch_num, batch_stride);
  // assume bottom_index is natural  ref
  topk_reference( k, batch, batch_num, batch_stride, descending, bottom_index_valid,bottom_data,
                  bottom_index, top_data_ref, top_index_ref);

  bm_status_t ret = BM_SUCCESS;
  struct timeval t1, t2;
  gettimeofday_(&t1);
  ret = bmcv_batch_topk( handle,
                          bm_mem_from_system((void*)bottom_data),
                          bm_mem_from_system((void*)bottom_index),
                          bm_mem_from_system((void*)top_data),
                          bm_mem_from_system((void*)top_index),
                          bm_mem_from_system((void*)buffer),
                          bottom_index_valid,
                          k,
                          batch,
                          &batch_num,
                          true,
                          batch_stride,
                          descending);
  gettimeofday_(&t2);
  long used = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;

  if(ret != BM_SUCCESS){
    printf("batch topk false \n");
    ret = BM_ERR_FAILURE;
    goto exit;
  }

  printf("batch topk used time: %.3f ms\n", (float)used / 1000);
  if(ret == BM_SUCCESS){
    int data_cmp = -1;
    int index_cmp = -1;
    data_cmp = array_cmp_( (float*)top_data_ref,
                          (float*)top_data,
                          batch * k,
                          "topk data",
                          0);
    index_cmp = array_cmp_( (float*)top_index_ref,
                           (float*)top_index,
                           batch * k,
                           "topk index",
                           0);
    if (data_cmp == 0 && index_cmp == 0) {
          printf("Compare success for topk data and index!\n");
      } else {
          printf("Compare failed for topk data and index!\n");
          ret = BM_ERR_FAILURE;
          goto exit;
    }
  } else {
    printf("Compare failed for topk data and index!\n");
    ret = BM_ERR_FAILURE;
    goto exit;
  }

exit:
  delete [] bottom_data;
  delete [] bottom_index;
  delete [] top_data;
  delete [] top_data_ref;
  delete [] top_index;
  delete [] top_index_ref;
  delete [] buffer;
  bm_dev_free(handle);
  return ret;
}


int main() {
  bm_handle_t handle;
  bm_status_t ret = bm_dev_request(&handle, 0);
  if (ret != BM_SUCCESS) {
    std::cout << "Create bm handle failed. ret = " << ret << std::endl;
    return -1;
  }
  unsigned int chipid = BM1684X;
  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
      return ret;
  switch (chipid)
  {
  case 0x1684:{
    int group_num = 64;
    int stride = 1000000;
    int k = 100;
    int per_batch_size = 3907;
    if(0 != test_topk_v1( handle,group_num, stride, k, per_batch_size)){
      std::cout << "test topk false! " << std::endl;
      ret = BM_ERR_FAILURE;
    }else{
      std::cout << "test topk success! " << std::endl;
    }
    break;
  }
  case BM1684X:{
    int batch_num = 100000;
    int k = batch_num / 10;
    int descending = rand() % 2;
    int batch = rand() % 20 + 1;
    int batch_stride = batch_num;
    bool bottom_index_valid = true;
    if(0 != test_topk_v2( handle, k, batch, batch_num, batch_stride, descending, bottom_index_valid)){
      std::cout << "test topk false! " << std::endl;
      ret = BM_ERR_FAILURE;
    }else{
      std::cout << "test topk success! " << std::endl;
    }
    break;
  }
  default:
    std::cout << "chipid false! " << std::endl;
    ret = BM_ERR_FAILURE;
    break;
  }
  return ret;
}
