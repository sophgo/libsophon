#include <assert.h>
#include <cmath>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <cstring>
#include "bmcv_api.h"
#include "test_misc.h"
#ifdef __linux__
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

using namespace std;
//using namespace cv;

#define B_CONST_VALUE 0
#define S_CONST_VALUE -0.2f

#define CEHCK(x) { \
     if (x) {} \
     else {fprintf(stderr, "Check failed: %s, file %s, line: %d\n", #x, __FILE__, __LINE__); return -1; } \
}

#define N (10)
#define C 256 //(64 * 2 + (64 >> 1))
#define H 8
#define W 8
#define TENSOR_SIZE (N * C * H * W)
#define BMDNN_COMPARE_EPSILON (1e-4)

#define BM_API_ID_MD_LINEAR 5

int array_cmp_axpy(float *p_exp, float *p_got, int len, const char *info_label, float delta) {
  int idx = 0;
  int total = 0;
  for (idx = 0; idx < len; idx++) {
    if (bm_max(fabs(p_exp[idx]), fabs(p_got[idx])) > 1.0) {
      // compare rel
      if (bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) < 1e-20) {
        printf("%s rel error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
        total++;
        if (1024 < total) {return -1;}
      }
      if (fabs(p_exp[idx] - p_got[idx]) / bm_min(fabs(p_exp[idx]), fabs(p_got[idx])) > delta) {
        printf("%s rel error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
        total++;
        if (1024 < total) {return -1;}
      }
    } else {
      // compare abs
      if (fabs(p_exp[idx] - p_got[idx]) > delta) {
        printf("%s abs error at index %d exp %.20f got %.20f\n", info_label, idx, p_exp[idx], p_got[idx]);
        total++;
        if (1024 < total) {return -1;}
      }
    }

    IF_VAL if_val_exp, if_val_got;
    if_val_exp.fval = p_exp[idx];
    if_val_got.fval = p_got[idx];
    if (IS_NAN(if_val_got.ival) && !IS_NAN(if_val_exp.ival)) {
      printf("There are nans in %s idx %d\n", info_label, idx);
      printf("floating form exp %.10f got %.10f\n", if_val_exp.fval, if_val_got.fval);
      printf("hex form exp %8.8x got %8.8x\n", if_val_exp.ival, if_val_got.ival);
      return -2;
    }
  }
  if (0 < total) {return -1;}
  return 0;
}


int main(int argc, char* argv[]) {
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;

    bm_dev_request(&handle, 0);
    int trials = 0;
    if (argc == 1) {
        trials = 5;
    }else if(argc == 2){
        trials  = atoi(argv[1]);
    }else{
      std::cout << "command input error, please follow this "
                     "order:test_cv_axpy loop_num "
                  << std::endl;
      return -1;
    }
    float* tensor_X = new float[TENSOR_SIZE];
    float* tensor_A = new float[N*C];
    float* tensor_Y = new float[TENSOR_SIZE];
    float* tensor_F = new float[TENSOR_SIZE];
    float* tensor_F_cmp = new float[TENSOR_SIZE];

    for (int idx_trial = 0; idx_trial < trials; idx_trial++) {

        for (int idx = 0; idx < TENSOR_SIZE; idx++) {
        tensor_X[idx] = (float)idx - 5.0f;
        tensor_Y[idx] = (float)idx/3.0f - 8.2f;  //y
        }

        for (int idx = 0; idx < N*C; idx++) {
        tensor_A[idx] = (float)idx * 1.5f + 1.0f;
        }

        memset(tensor_F, 0, sizeof(float)*TENSOR_SIZE);
        for (int i=0; i< N; i++ ){
            for (int j=0; j <C; j++){
                for (int k=0; k<H*W; k++){
                    tensor_F_cmp[i*C*H*W + j*H*W +k] = tensor_A[i*C+j] * tensor_X[i*C*H*W + j*H*W +k] + tensor_Y[i*C*H*W + j*H*W +k];
                }
            }
        }

        struct timeval t1, t2;
        gettimeofday_(&t1);
        ret = bmcv_image_axpy(handle,
                              bm_mem_from_system((void *)tensor_A),
                              bm_mem_from_system((void *)tensor_X),
                              bm_mem_from_system((void *)tensor_Y),
                              bm_mem_from_system((void *)tensor_F),
                              N, C, H, W);
        gettimeofday_(&t2);
        cout << "The "<< idx_trial <<" loop "<< " axpy using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec)  << "us" << endl;

        if (ret){
            cout << "test Axpy failed" << endl;
            ret = BM_ERR_FAILURE;
            break;
        } else {
            int cmp_res = array_cmp_axpy(
                            (float*)tensor_F_cmp,
                            (float*)tensor_F,
                            TENSOR_SIZE, "axpy", BMDNN_COMPARE_EPSILON);
            if ( cmp_res != 0) {
                std::cout <<"Compare TPU with CPU: error, not equal,cmp fail" <<endl;
                ret = BM_ERR_FAILURE;
                break;
            } else {
                std::cout <<"Compare TPU with CPU: they are equal,cmp success" <<endl;
            }
        }

    }
    delete []tensor_A;
    delete []tensor_X;
    delete []tensor_Y;
    delete []tensor_F;
    delete []tensor_F_cmp;
    bm_dev_free(handle);

    return ret;
}