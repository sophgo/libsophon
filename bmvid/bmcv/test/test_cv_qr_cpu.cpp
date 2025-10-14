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
#include <cassert>
#ifdef __linux__
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

#if defined(__linux__) && (USING_OPENBLAS)

#define GLOBAL_PRINT        0
#define SORT_CMP            1
#define ENABLE_REF_DATA     0

int test_qr_cpu(int n, int user_num_spks,  int max_num_spks, int min_num_spks) {

    assert(n > 0 );
    int total_loop    = 1;
    float *input_A    = (float *)malloc(n * n * sizeof(float));
    float *arm_sorted_eign_value = (float *)malloc(n * sizeof(float));

    std::string path_input      = "/mnt/software/code_eigh/ref_QR_input_M.bin";
    std::string path_eign_value = "/mnt/software/code_eigh/ARM_eig_value.bin";
    std::string path_eign_vector_select = "/mnt/software/code_eigh/ARM_seelect_vector.bin";
    float *ref_sorted_eign_value = (float *)malloc(n * sizeof(float));
    float *ref_sorted_eign_vector= (float *)malloc(n * n *sizeof(float));
    size_t n_squared = static_cast<size_t>(n) * static_cast<size_t>(n);
    if(ENABLE_REF_DATA) {
        FILE *file;
        file = fopen(path_input.c_str(), "rb");
        size_t result = fread(input_A, sizeof(float), n * n, file);
        if (result != n_squared) {
            fprintf(stderr, "Failed to read %d elements from the file\n", n * n);
            exit(EXIT_FAILURE);
        }
        fclose(file);

        file = fopen(path_eign_value.c_str(), "rb");
        result = fread(ref_sorted_eign_value, sizeof(float), n, file);
        if (result != n_squared) {
            fprintf(stderr, "Failed to read %d elements from the file\n", n * n);
            exit(EXIT_FAILURE);
        }
        fclose(file);
    } else {
        n = 4;
        user_num_spks = -1;
        max_num_spks  = 3;
        min_num_spks  = 2;
        float blob_input_static[16] = {17.5, -1.,   0. ,  0.,
                                        -1.,  15.5, -0.5,  0.,
                                        0.,  -0.5, 12.,  -1.,
                                        0.,   0.,  -1.,  18.5};
        for (int i = 0; i < n * n; i++) {
            input_A[i] = blob_input_static[i];
        }
        ref_sorted_eign_value[0] = 11.780645;
        ref_sorted_eign_value[1] = 15.147478;
        ref_sorted_eign_value[2] = 17.919054;
        ref_sorted_eign_value[3] = 18.652824;
    }

    printf("[INFO]max_num_spks %d\n", max_num_spks);
    printf("[INFO]min_num_spks %d\n", min_num_spks);
    printf("[INFO]user_num_spks %d\n", user_num_spks);
    printf("[INFO]n %d\n", n);
    printf("[INFO]total_loop %d\n", total_loop  );
    if (ENABLE_REF_DATA)
        printf("[INFO]Using alien data as ref\n");
    else
        printf("[INFO]Using static small n as static utest\n");

    float *arm_select_egin_vector = (float *)malloc(n * n * sizeof(float));
    int num_spks_output;
    struct timeval start, end;
    gettimeofday_(&start);
    printf("[Timer]Begin\n");
    for (int loop = 0; loop < total_loop; loop ++)
        bmcv_qr_cpu(arm_select_egin_vector, &num_spks_output, input_A, arm_sorted_eign_value, n, user_num_spks,max_num_spks, min_num_spks);
    gettimeofday_(&end);
    double elapsed_time_postprocess = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("[INFO]avg latency of %d times is %fs\n", total_loop,elapsed_time_postprocess );

    if (SORT_CMP){
            int flag = 1;
            for(int i = 0; i < n; i++) {
                if(abs(ref_sorted_eign_value[i]-arm_sorted_eign_value[i]) >1e-2) {
                    printf("[Error-CMP-sorted-eign-value]%d,%f,%f\n",i,ref_sorted_eign_value[i],arm_sorted_eign_value[i]);
                    flag =0;
                    break;
                }
            }
            if (flag) printf("[CMP]eig_value Success!\n");
            else      printf("[CMP]eig_value Error!\n");
            if(ENABLE_REF_DATA) {
                FILE *file;
                file = fopen(path_eign_vector_select.c_str(), "rb");
                size_t result = fread(ref_sorted_eign_vector, sizeof(float), n * num_spks_output, file);
                if (result != n_squared) {
                    fprintf(stderr, "Failed to read %d elements from the file\n", n * n);
                    exit(EXIT_FAILURE);
                }
                fclose(file);
            } else {
                float ref_sorted_eign_vector_static[8] = {
                    0.02415439 , 0.38745552
                    ,0.1381475,   0.9114977,
                    0.97933114, -0.13226356,
                    0.14574778, -0.03945195};
                for (int i = 0; i < 8; i++) {
                    ref_sorted_eign_vector[i] = ref_sorted_eign_vector_static[i];
                }
            }
            flag = 1;
            for(int i = 0; i < n * num_spks_output; i++) {
                if(abs(ref_sorted_eign_vector[i]-arm_select_egin_vector[i]) >1e-4) {
                    printf("[Error-CMP-sorted_abs_eignvector] no.%d, row:%d,col:%d,%f,%f\n",i, i/num_spks_output,i%num_spks_output,ref_sorted_eign_vector[i],arm_select_egin_vector[i]);
                    flag =0;
                    break;
                }
            }
            if (flag) printf("[CMP]select_eig_vector Success!\n");
            else      printf("[CMP]select_eig_vector Error!\n");

            printf("[INFO]num_spks_output is %d\n", num_spks_output);
            printf("[INFO]output arm_select_egin_vector is nxnum_spks_output\n");
    }
    free(input_A);
    free(arm_sorted_eign_value);
    free(ref_sorted_eign_vector);
    free(ref_sorted_eign_value);
    return 0;
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


    int n = 2400;
    int max_num_spks  = 8;
    int min_num_spks  = 2;
    int user_num_spks = -1;
    if (argc > 1) n = atoi(args[1]);

    ret = bm_dev_request(&handle, 0);
    if (ret) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return ret;
    }

    if (argc > 1) loop = atoi(args[1]);

    for (i = 0; i < loop; ++i) {
        ret = test_qr_cpu(n, user_num_spks,  max_num_spks, min_num_spks);
        if (ret) {
            printf("------Test QR Failed!------\n");
            bm_dev_free(handle);
            return ret;
        }
    }

    printf("------Test QR Success!------\n");
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
            printf("------Test QR Failed!------\n");
            bm_dev_free(handle);
            return ret;
        }
    }

    printf("------Test QR Skip as OPENBLAS/platform-pcie/soc not defined!------\n");
    bm_dev_free(handle);
    return ret;
}
#endif

//g++ sophon_spectral_cpp.cpp -o sophon_spectral_cpp -L/mnt/software/code_eigh/OpenBLAS -lopenblas -I/mnt/software/code_eigh/OpenBLAS
//gcc -g -o sophon_spectral_cpp sophon_spectral_cpp.cpp -I/$PWD/OpenBLAS/include/ -L/$PWD/^CenBLAS/lib -Wl,-rpath,$PWD/OpenBLAS/lib -lopenblas

// use C,++  real matrix A is nxn random float
//A is not  symmetric
// n is set as 2164
// let OpenBLAS use sgees_ to computes eginvalues of A in ascending  order
// let OpenBLAS compute A's eginvectors ;
// eginvalues of A are in ascending
// the code exists no sort.  ; compute openblas time. A is read form a bin path called ref_QR_input_M.bin
// do not use lapacke.h,
//output eginvalues are real
//output eginvectors are real
//print output eginvalues
//print output eginvectors
