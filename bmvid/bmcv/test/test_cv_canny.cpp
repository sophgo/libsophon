#include <iostream>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "md5.h"
#include <assert.h>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include<windows.h>
  #include "time.h"
#endif

using namespace std;

static void fill(
        unsigned char* input,
        int channel,
        int width,
        int height) {
    int count = 0;
    for (int i = 0; i < height * channel; i++) {
        for (int j = 0; j < width; j++) {
            // input[i * width + j] = (j + i) % 256;
            input[i * width + j] = count;
            if(count == 256)
                count = 0;
        }
    }
}

static int canny_tpu(
        unsigned char* input,
        unsigned char* output,
        int height,
        int width,
        int ksize,
        float low_thresh,
        float high_thresh,
        bool l2gradient) {
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
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    bm_image_alloc_dev_mem(img_i);
    bm_image_alloc_dev_mem(img_o);
    bm_image_copy_host_to_device(img_i, (void **)(&input));

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_image_canny(handle, img_i, img_o, low_thresh, high_thresh, ksize, l2gradient);
    gettimeofday_(&t2);
    cout << "Canny TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

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
        if (got[i] != exp[i]) {
            for (int j = 0; j < 10; j++)
                printf("cmp error: idx=%d  exp=%d  got=%d\n", i + j, exp[i + j], got[i + j]);
            return -1;
        }
    }
    return 0;
}

static string  unsignedCharToHex(unsigned char ch[16]){
    const char hex_chars[] = "0123456789abcdef";
    string result = "";
    for(int i = 0; i < 16; i++){
        unsigned int highHalfByte = (ch[i] >> 4) & 0x0f;
        unsigned int lowHalfByte = (ch[i] & 0x0f);
        result += hex_chars[highHalfByte];
        result += hex_chars[lowHalfByte];
    }
    return result;
}

static int cmpv2(unsigned char* got, unsigned char* exp ,int width, int height, int channel){
    unsigned char* md5_tpuOut = new unsigned char[16];
    md5_get(got, (sizeof(unsigned char) * channel * height * width), md5_tpuOut);
    if(0 != strcmp((unsignedCharToHex(md5_tpuOut).c_str()), (const char*)exp)){
        cout << "cmp error!" << endl;
        return -1;
    }
    return 0;
}

static int test_canny_random(
                             int height,
                             int width,
                             int ksize,
                             bool l2gradient) {
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    srand(seed);
    cout << "seed = " << seed << endl;
    ksize = 3;
    float low_thresh = 0;
    float high_thresh = 100;
    cout << "width: " << width << "  height: " << height << endl;
    cout << "ksize: " << ksize << endl;
    unsigned char* input_data = new unsigned char [width * height];
    unsigned char* output_tpu = new unsigned char [width * height];
    fill(input_data, 1, width, height);
    unsigned char ocv_md5[] = "3075760cecc0e7d7f1109e71783e1b63";
    canny_tpu(
        input_data,
        output_tpu,
        height,
        width,
        ksize,
        low_thresh,
        high_thresh,
        l2gradient);
    int ret = cmpv2(output_tpu, ocv_md5, width, height, 1);
    delete [] input_data;
    delete [] output_tpu;
    return ret;
}

int main(int argc, char* args[]) {
    int loop = 1;
    int height = 1080;
    int width = 1920;
    int ksize = 3;
    int l2gradient = 1;
    if (argc > 1) loop = atoi(args[1]);
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_canny_random(height, width, ksize, l2gradient);
        if (ret) {
            cout << "test canny failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with OpenCV successfully!" << endl;
    return 0;
}