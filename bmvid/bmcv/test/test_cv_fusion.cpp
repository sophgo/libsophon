#include <iostream>
#include <thread>
#include <mutex>
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
#include <windows.h>
#include "time.h"
#endif

using namespace std;

static void fill(
        unsigned char* input,
        int channel,
        int width,
        int height,
        int flag) {
    switch(flag){
        case 1:
            {
                int count = 10;
                for(int i = 0; i< width * height * channel; i++ ){
                    input[i] = count;
                    count++;
                    if(count == 101)
                        count = 10;
                }
                break;
            }
        case 2:
            {
                int count = 1;
                for(int i = 0; i< width * height * channel; i++){
                    input[i] = count;
                    count++;
                    if(count == 9)
                        count = 1;
                }
                break;
            }
         default:
            break;
    }
}

static int fusion_tpu(
        unsigned char* input_1,
        unsigned char* input_2,
        unsigned char* output,
        int format,   // 0:gray  1:rgb-planar  2:rgb-packed
        int height,
        int width,
        unsigned char thresh,
        unsigned char max_value,
        bm_thresh_type_t type,
        // int op,
        int shape,
        int kh,
        int kw) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    // ret = bmcv_open_cpu_process(handle);
    // if (ret != BM_SUCCESS) {
    //     printf("BMCV enable CPU failed. ret = %d\n", ret);
    //     bm_dev_free(handle);
    //     return -1;
    // }
    bm_device_mem_t kmem = bmcv_get_structuring_element(handle, (bmcv_morph_shape_t)shape, kw, kh);
    // bm_image_format_ext fmt = FORMAT_GRAY;
    bm_image_format_ext fmt = format == 0 ? FORMAT_GRAY :
                              (format == 1 ? FORMAT_RGB_PLANAR : FORMAT_RGB_PACKED);
    bm_image img_i_1;
    bm_image img_i_2;
    bm_image img_o;
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i_1);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i_2);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    bm_image_alloc_dev_mem(img_i_1);
    bm_image_alloc_dev_mem(img_i_2);
    bm_image_alloc_dev_mem(img_o);
    unsigned char* in_ptr_1 = input_1;
    bm_image_copy_host_to_device(img_i_1, (void **)(&in_ptr_1));
    unsigned char* in_ptr_2 = input_2;
    bm_image_copy_host_to_device(img_i_2, (void **)(&in_ptr_2));
    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_image_fusion(handle, img_i_1,img_i_2, img_o, thresh, max_value, type, kw, kh, kmem);
    // else
        // bmcv_image_erode(handle, img_i, img_o, kw, kh, kmem);
    gettimeofday_(&t2);
    cout << "fusion TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    unsigned char* out_ptr[3] = {output, output + height * width, output + 2 * height * width};
    bm_image_copy_device_to_host(img_o, (void **)out_ptr);
    bm_image_destroy(img_i_1);
    bm_image_destroy(img_i_2);
    bm_image_destroy(img_o);
    bm_free_device(handle, kmem);
    // ret = bmcv_close_cpu_process(handle);
    // if (ret != BM_SUCCESS) {
    //     printf("BMCV disable CPU failed. ret = %d\n", ret);
    //     bm_dev_free(handle);
    //     return -1;
    // }
    bm_dev_free(handle);
    return 0;
}

static int cmp(
        unsigned char* got,
        unsigned char* exp,
        int len) {
    for (int i = 0; i < len; i++) {
        if (got[i] != exp[i]) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
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

static int test_morph_random(
                             int format,
                             int height,
                             int width) {
    int kh = 3;
    int kw = 3;
    int shape = 0;
    // int op = 0;
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    srand(seed);
    cout << "seed = " << seed << endl;
    format = 0; //rand() % 3;
    kh = 3; //rand() % 5 + 1;
    kw = 3; //rand() % 5 + 1;
    shape = 0; //rand() % 3;
    // op = rand() % 2;

    // string op_str[2] = {"erode", "dilate"};
    string shape_str[3] = {"RECT", "CROSS", "ELLIPSE"};
    string format_str[3] = {"gray", "rgb-planar", "rgb-packed"};
    // cout << "op: " << op_str[op] << endl;
    cout << "format: " << format_str[format] << endl;
    cout << "width: " << width << "  height: " << height << endl;
    cout << "kh: " << kh << "  kw: " << kw << "  shape: " << shape_str[shape] << endl;
    int channel = format == 0 ? 1 : 3;
    cout << "channel: " << channel <<endl;
    unsigned char* input_data_1 = new unsigned char [width * height * channel];
    unsigned char* input_data_2 = new unsigned char [width * height * channel];
    unsigned char* output_tpu = new unsigned char [width * height * channel];
    unsigned char thresh = 128;
    unsigned char max_value = 255;
    bm_thresh_type_t type  = BM_THRESH_BINARY;
    fill(input_data_1, channel, width, height, 1);
    fill(input_data_2, channel, width, height, 2);
    fusion_tpu(
        input_data_1,
        input_data_2,
        output_tpu,
        format,
        height,
        width,
        thresh,
        max_value,
        type,
        shape,
        kh,
        kw);
    unsigned char output_opencv_md5[] = "3075760cecc0e7d7f1109e71783e1b63";
    int ret = cmpv2(output_tpu, output_opencv_md5, width, height, channel);
    delete [] input_data_1;
    delete [] input_data_2;
    delete [] output_tpu;
    return ret;
}

int main(int argc, char* args[]) {
    int loop = 1;
    int height = 1080;
    int width = 1920;
    int format = 0;
    if (argc > 1) loop = atoi(args[1]);
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_morph_random(format, height, width);
        if (ret) {
            cout << "test morph failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with OpenCV successfully!" << endl;
    return 0;
}
