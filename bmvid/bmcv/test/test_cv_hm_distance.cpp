#include <map>
#include <cmath>
#include <memory>
#include <random>
#include <vector>
#include <iostream>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <unordered_map>

#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include "test_misc.h"

using std::make_shared;
using std::shared_ptr;
using namespace std;
static bm_handle_t handle;

int hammiingDistance(int x, int y){
    int cnt = 0;
    int z = x ^ y;
    while (z != 0){
        cnt += z & 1;
        z = z >> 1;
    }
    return cnt;
}

static void native_cal_hammiingDistance(int *input1,
                                        int *input2,
                                        int *output,
                                        int bits_len,
                                        int input1_num,
                                        int input2_num){
    for(int i = 0; i < input1_num; i++){
        for(int j = 0; j < input2_num; j++){
                for(int d = 0; d < bits_len; d++){
                    output[i * input2_num + j] += hammiingDistance(input1[i * bits_len + d], input2[j * bits_len + d]);
            }
        }
    }
}

static int array_cmp_int32(int *p_exp,
                           int *p_got,
                           int len,
                           const char *info_label,
                           int delta) {
    int idx = 0;
    for (idx = 0; idx < len; idx++) {
        if ((int)fabs(p_exp[idx] - (int)p_got[ idx]) > delta) {
            printf("%s abs error at index %d exp %d got %d\n",
                   info_label,
                   idx,
                   p_exp[idx],
                   p_got[idx]);
            return -1;
        }
    }
    return 0;
}

static int hamming_distance_single_test(int bits_len, int input1_num, int input2_num){
    int* input1_data = new int[input1_num * bits_len];
    int* input2_data = new int[input2_num * bits_len];
    int* output_ref  = new int[input1_num * input2_num];
    int* output_tpu  = new int[input1_num * input2_num];

    memset(input1_data, 0, input1_num * bits_len * sizeof(int));
    memset(input2_data, 0, input2_num * bits_len * sizeof(int));
    memset(output_ref,  0,  input1_num * input2_num * sizeof(int));
    memset(output_tpu,  0,  input1_num * input2_num * sizeof(int));

    // fill data
    for(int i = 0; i < input1_num * bits_len; i++) input1_data[i] = rand() % 10;
    for(int i = 0; i < input2_num * bits_len; i++) input2_data[i] = rand() % 20 + 1;

    // nativa cal
    native_cal_hammiingDistance(input1_data, input2_data, output_ref, bits_len, input1_num, input2_num);

    // tpu_cal
    bm_device_mem_t input1_dev_mem;
    bm_device_mem_t input2_dev_mem;
    bm_device_mem_t output_dev_mem;
    struct timeval t1, t2;
    int ret = -1;
    bm_status_t status = BM_SUCCESS;
    std::vector<bm_device_mem_t*> internal_mem_v;

    if(BM_SUCCESS != bm_malloc_device_byte(handle, &input1_dev_mem, input1_num * bits_len * sizeof(int))){
        std::cout << "malloc input fail" << std::endl;
        goto free_devmem;
    }
    internal_mem_v.push_back(&input1_dev_mem);

    if(BM_SUCCESS != bm_malloc_device_byte(handle, &input2_dev_mem, input2_num * bits_len * sizeof(int))){
        std::cout << "malloc input fail" << std::endl;
        goto free_devmem;
    }
    internal_mem_v.push_back(&input2_dev_mem);

    if(BM_SUCCESS != bm_malloc_device_byte(handle, &output_dev_mem, input1_num * input2_num * sizeof(int))){
        std::cout << "malloc input fail" << std::endl;
        goto free_devmem;
    }
    internal_mem_v.push_back(&output_dev_mem);

    if(BM_SUCCESS != bm_memcpy_s2d(handle, input1_dev_mem, input1_data)){
        std::cout << "copy input1 to device fail" << std::endl;
        goto free_devmem;
    }

    if(BM_SUCCESS != bm_memcpy_s2d(handle, input2_dev_mem, input2_data)){
        std::cout << "copy input2 to device fail" << std::endl;
        goto free_devmem;
    }

    gettimeofday_(&t1);
    status = bmcv_hamming_distance(handle,
                                               input1_dev_mem,
                                               input2_dev_mem,
                                               output_dev_mem,
                                               bits_len,
                                               input1_num,
                                               input2_num);
    gettimeofday_(&t2);
    cout << "--bmcv_hamming_distance using time = " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)--" << endl;

    if(status != BM_SUCCESS){
        printf("run bmcv_hamming_distance failed status = %d \n", status);
        goto free_devmem;
    }

    if(BM_SUCCESS != bm_memcpy_d2s(handle, output_tpu, output_dev_mem)){
        std::cout << "bm_memcpy_d2s fail" << std::endl;
        goto free_devmem;
    }

    // ref
    ret = array_cmp_int32(output_ref,
                              output_tpu,
                              input1_num * input2_num,
                              "tpu_hamming_distance",
                              0);
free_devmem:
    delete [] input1_data;
    delete [] input2_data;
    delete [] output_ref;
    delete [] output_tpu;
    for (auto mem : internal_mem_v) {
        bm_free_device(handle, *mem);
    }

    return ret;
}

int main(int argc, char *argv[]){
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);
    std::cout << "random seed = " << seed << std::endl;

    int bits_len = 8;
    int input1_num = 2;
    int input2_num = 2562;

    if(argc > 1) bits_len = atoi(argv[1]);
    if(argc > 2) input1_num = atoi(argv[2]);
    if(argc > 3) input2_num = atoi(argv[3]);

    int dev_id = 0;
    bm_status_t status = bm_dev_request(&handle, dev_id);
    if (status != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", status);
        exit(-1);
    }

    std::cout << "bits_len is " << bits_len << std::endl;
    std::cout << "input1_data len is " << input1_num << std::endl;
    std::cout << "input2_data len is " << input2_num << std::endl;

    int ret = hamming_distance_single_test(bits_len, input1_num, input2_num);
    if(ret != 0){
        cout << "------[HAMMING_DISTANCE TEST FAILED!]------" << endl;
    }else{
        cout << "------[HAMMING_DISTANCE TEST PASSED!]------" << endl;
    }

    bm_dev_free(handle);
    return ret;
}