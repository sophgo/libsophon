#include <iostream>
#include <fstream>
#include <assert.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#include <memory>
#include <string>
#include <numeric>
#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>
#include "bmcv_api_ext.h"
#include "test_misc.h"

using namespace std;

char* opencvFile_path = NULL;

static void fill(
        float* input,
        int width,
        int height){
    ifstream input_read((string(opencvFile_path) + string("/dct_input.bin")), ios :: in | ios :: binary);
    if(!input_read)
        cout << "Error opening file" << endl;
    input_read.read((char*)input, sizeof(float) * width * height);
    input_read.close();
}

static  void cmp(
        float* data1,
        float* data2,
        int width,
        int height,
        string cmsg){
    vector<float> diff;
    for(int i = 0; i  < width * height; i++)
        diff.push_back(abs(data1[i]-data2[i]));
    float diff_avg = accumulate(diff.begin(), diff.end(), diff[0]) / (width * height);
    float diff_max = *max_element(diff.begin(), diff.end());
    cout << cmsg <<": " <<"diff_avg = " << diff_avg << ", ";
    cout << "diff_max = " << diff_max << endl;

}

static int test_tpu_dct(
        float* input,
        float* output,
        int width,
        int height,
        bool is_inversed){
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        exit(-1);
    }
    bm_image bm_input, bm_output;
    bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &bm_input);
    bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &bm_output);
    bm_image_alloc_dev_mem(bm_input);
    bm_image_alloc_dev_mem(bm_output);
    float* input_ptr = input;
    bm_image_copy_host_to_device(bm_input, (void **)(&input_ptr));
    bm_device_mem_t hcoeff_mem;
    bm_device_mem_t wcoeff_mem;
    bm_malloc_device_byte(handle, &hcoeff_mem, height * height * sizeof(float));
    bm_malloc_device_byte(handle, &wcoeff_mem, width * width * sizeof(float));

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_dct_coeff(handle, height, width, hcoeff_mem, wcoeff_mem, is_inversed);
    gettimeofday_(&t2);
    cout << "DCT COEFF TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    gettimeofday_(&t1);
    bmcv_image_dct_with_coeff(handle, bm_input, hcoeff_mem, wcoeff_mem, bm_output);
    gettimeofday_(&t2);
    cout << "DCT TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    float* output_ptr = output;
    bm_image_copy_device_to_host(bm_output, (void **)(&output_ptr));
    bm_image_destroy(bm_input);
    bm_image_destroy(bm_output);
    bm_free_device(handle, hcoeff_mem);
    bm_free_device(handle, wcoeff_mem);
    bm_dev_free(handle);
    return 0;
}

static int test_tpu_idct(
        float* input,
        float* output,
        int width,
        int height,
        bool is_inversed){
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        exit(-1);
    }
    bm_image bm_input, bm_output;
    bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &bm_input);
    bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &bm_output);
    bm_image_alloc_dev_mem(bm_input);
    bm_image_alloc_dev_mem(bm_output);
    float* input_ptr = input;
    bm_image_copy_host_to_device(bm_input, (void **)(&input_ptr));

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_image_dct(handle, bm_input, bm_output, is_inversed);
    gettimeofday_(&t2);
    cout << "DCT all TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    float* output_ptr = output;
    bm_image_copy_device_to_host(bm_output, (void **)(&output_ptr));
    bm_image_destroy(bm_input);
    bm_image_destroy(bm_output);
    bm_dev_free(handle);
    return 0;
}

int main(int argc, char* argv[]){
    (void)argc;
    (void)argv;
    opencvFile_path = getenv("BMCV_TEST_FILE_PATH");
    if (opencvFile_path == NULL) {
        printf("please set environment vairable: BMCV_TEST_FILE_PATH !\n");
        return -1;
    }
    int height = 1920;
    int width = 1920;
    float* temp = new float[height * width];
    float* input_img = new float[height * width];
    float* dct_opencv = new float[height * width];
    float* idct_opencv = new float[height * width];
    float* dct_tpu = new float[height * width];
    float* idct_tpu = new float[height * width];
    fill(input_img, width, height);
    #ifdef __linux__
        ifstream dctopencv_read((string(opencvFile_path) + string("/dct_opencv.bin")), ios :: in | ios :: binary);
        ifstream idctopencv_read((string(opencvFile_path) + string("/idct_opencv.bin")), ios :: in | ios :: binary);
        if(!dctopencv_read || !idctopencv_read){
            cout << "Error opening file" << endl;
            return -1;
        }
        dctopencv_read.read((char*)dct_opencv, sizeof(float) * width * height);
        dctopencv_read.close();
        cout << "DCT CPU using time: " << "48712" << "us" << endl;
        idctopencv_read.read((char*)idct_opencv, sizeof(float) * width * height);
        idctopencv_read.close();
        cout << "DCT CPU using time: " << "53348" << "us" << endl;
    #else
        ifstream dctopencv_read((string(opencvFile_path) + string("/dct_opencv.bin")), ios :: in | ios :: binary);
        ifstream idctopencv_read((string(opencvFile_path) + string("/idct_opencv.bin")), ios :: in | ios :: binary);
        if(!dctopencv_read || !idctopencv_read)
        {
            cout << "Error opening file" << endl;
            return -1;
        }
        dctopencv_read.read((char*)dct_opencv, sizeof(float) * width * height);
        dctopencv_read.close();
        cout << "DCT CPU using time: " << "48712" << "us" << endl;
        idctopencv_read.read((char*)idct_opencv, sizeof(float) * width * height);
        idctopencv_read.close();
        cout << "DCT CPU using time: " << "53348" << "us" << endl;
    #endif
    test_tpu_dct(input_img, dct_tpu, width, height, false);
    test_tpu_idct(dct_tpu, idct_tpu, width, height, true);
    cmp(dct_opencv, dct_tpu, width, height, "DCT diff");
    cmp(idct_opencv, idct_tpu, width, height, "iDCT diff");
    delete [] temp;
    delete [] input_img;
    delete [] dct_opencv;
    delete [] idct_opencv;
    delete [] dct_tpu;
    delete [] idct_tpu;
    return 0;
}
