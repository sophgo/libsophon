#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <assert.h>
#ifdef __linux__
#include <sys/time.h>
#else
  #include <windows.h>
  #include "opencv2/imgproc/types_c.h"
#endif

using namespace std;

char* opencvFile_path = NULL;

static void fill(
        unsigned char* input,
        int channel,
        int width,
        int height) {
    int count = 1;
    for (int i = 0; i < height * channel; i++) {
        for (int j = 0; j < width; j++) {
            input[i * width + j] = count;
            count++;
            if(count == 101)
                count = 1;
        }
    }
}

static int pyramid_down_tpu(
        unsigned char* input,
        unsigned char* output,
        int height,
        int width,
        int oh,
        int ow) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    bm_image_format_ext fmt = FORMAT_GRAY;
    bm_image img_i;
    bm_image img_o;
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
    bm_image_create(handle, oh, ow, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    bm_image_alloc_dev_mem(img_i);
    bm_image_alloc_dev_mem(img_o);
    bm_image_copy_host_to_device(img_i, (void **)(&input));

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_image_pyramid_down(handle, img_i, img_o);
    gettimeofday_(&t2);
    cout << "pyramid down TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    bm_image_copy_device_to_host(img_o, (void **)(&output));
    bm_image_destroy(img_i);
    bm_image_destroy(img_o);
    bm_dev_free(handle);
    return 0;
}

static int cmp(
        unsigned char* got,
        unsigned char* exp,
        int len) {
    for (int i = 0; i < len; i++) {
        if (abs(got[i] - exp[i]) > 1) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
            return -1;
        }
    }
    return 0;
}

static int test_pyramid_down_random(
        int height,
        int width) {
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    srand(seed);
    cout << "seed = " << seed << endl;
    int ow = 960;
    int oh = 540;
    cout << "width: " << width << "  height: " << height << endl;
    cout << "ow: " << ow << "  oh: " << oh << endl;
    int channel = 1;
    cout << "channel: " << channel << endl;
    unsigned char* input_data = new unsigned char [width * height * channel];
    unsigned char* output_tpu = new unsigned char [ow * oh * channel];
    unsigned char* output_ocv = new unsigned char [ow * oh * channel];
    fill(input_data, channel, width, height);
    ifstream opencv_readfile((string(opencvFile_path) + string("/opencv_pyramid.bin")), ios :: in | ios :: binary);
    if( ! opencv_readfile){
        cout << "Error opening file" << endl;
        return -1;
    }
    int i = 0;
    while(opencv_readfile.read((char *)&output_ocv[i], sizeof(unsigned char))){
        i++;
    }
    opencv_readfile.close();
    pyramid_down_tpu(
        input_data,
        output_tpu,
        height,
        width,
        oh,
        ow);
    int ret = cmp(output_tpu, output_ocv, ow * oh * channel);
    delete [] input_data;
    delete [] output_tpu;
    delete [] output_ocv;
    return ret;
}

int main(int argc, char* args[]) {
    int loop = 1;
    int height = 1080;
    int width = 1920;
    if (argc > 1) loop = atoi(args[1]);
    opencvFile_path = getenv("BMCV_TEST_FILE_PATH");
    if (opencvFile_path == NULL) {
        printf("please set environment vairable: BMCV_TEST_FILE_PATH !\n");
        return -1;
    }
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_pyramid_down_random(height, width);
        if (ret) {
            cout << "test pyramid_down failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with OpenCV successfully!" << endl;
    return 0;
}

// test performance:
// 1080P  GRAY
// gray:       880um

