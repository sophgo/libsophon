#include <iostream>
#include <fstream>
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
#include "time.h"
#endif

using namespace std;

char* opencvFile_path = NULL;

static void fill(
        unsigned char* input,
        int channel,
        int width,
        int height) {
    int conut = 1;
    for (int i = 0; i < channel; i++) {
        for (int j = 0; j < height; j++) {
            for (int k = 0; k < width; k++) {
                //input[i * width * height + j * width + k] = rand() % 256;
                input[i * width * height + j * width + k] = conut;
                conut++;
                if(conut == 256)
                    conut = 1;
            }
        }
    }
}


static int gaussian_blur_tpu(
        unsigned char* input,
        unsigned char* output,
        int channel,
        bool is_packed,
        int height,
        int width,
        int kw,
        int kh,
        float sigmaX,
        float sigmaY) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    bm_image_format_ext fmt = channel == 3 ? FORMAT_RGB_PLANAR : FORMAT_GRAY;
    fmt = is_packed ? FORMAT_RGB_PACKED : fmt;
    bm_image img_i;
    bm_image img_o;
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    bm_image_alloc_dev_mem(img_i);
    bm_image_alloc_dev_mem(img_o);
    unsigned char* in_ptr[3] = {input, input + height * width, input + 2 * height * width};
    bm_image_copy_host_to_device(img_i, (void **)(in_ptr));
    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_image_gaussian_blur(handle, img_i, img_o, kw, kh, sigmaX, sigmaY);
    gettimeofday_(&t2);
    cout << "GaussianBlur TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    unsigned char* out_ptr[3] = {output, output + height * width, output + 2 * height * width};
    bm_image_copy_device_to_host(img_o, (void **)out_ptr);
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
        if (abs(got[i] - exp[i]) > 2) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
            return -1;
        }
    }
    return 0;
}

static int test_gaussian_blur_random(
        int channel,
        bool is_packed,
        int height,
        int width,
        int kw,
        int kh) {
    cout << "===== test gaussian blur =====" << endl;
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    srand(seed);
    cout << "seed = " << seed << endl;
    float sigmaX = 4;
    float sigmaY = 3;
    cout << "channel: " << channel << "  width: " << width << "  height: " << height << endl;
    cout << "kw: " << kw << "  kh: " << kh << "  is_packed: " << is_packed << endl;
    cout << "simgaX: " << sigmaX << "  sigmaY: " << sigmaY << endl;
    unsigned char* input_data = new unsigned char [width * height * 3];
    unsigned char* output_tpu = new unsigned char [width * height * 3];
    unsigned char* output_opencv = new unsigned char [width * height * 3];
    fill(input_data, 3, width, height);
    ifstream opencv_readfile((string(opencvFile_path) + string("/opencv_gaussian_blur.bin")), ios :: in | ios :: binary);
    if( ! opencv_readfile){
        cout << "Error opening file" << endl;
        return -1;
    }
    int i = 0;
    while(opencv_readfile.read((char *)&output_opencv[i], sizeof(unsigned char))){
        i++;
    }
    opencv_readfile.close();
    gaussian_blur_tpu(
        input_data,
        output_tpu,
        channel,
        is_packed,
        height,
        width,
        kw,
        kh,
        sigmaX,
        sigmaY);
    int ret = cmp(output_tpu, output_opencv, width * height * (is_packed ? 3 : channel));
    delete [] input_data;
    delete [] output_tpu;
    delete [] output_opencv;
    return ret;
}

int main(int argc, char* args[]) {
    int loop = 1;
    int channel = 3;
    int height = 1080;
    int width = 1920;
    int kh = 3;
    int kw = 3;
    int is_packed = 1;
    if (argc > 1) loop = atoi(args[1]);
    opencvFile_path = getenv("BMCV_TEST_FILE_PATH");
    if (opencvFile_path == NULL) {
        printf("please set environment vairable: BMCV_TEST_FILE_PATH !\n");
        return -1;
    }
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_gaussian_blur_random(channel, is_packed, height, width, kw, kh);
        if (ret) {
            cout << "test gaussian_blur failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with OpenCV successfully!" << endl;
    return 0;
}
