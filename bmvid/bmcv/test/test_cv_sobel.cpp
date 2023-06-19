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
#endif

using namespace std;

static void fill(
        unsigned char* input,
        int channel,
        int width,
        int height) {
    int count = 1;
    for (int i = 0; i < height * channel; i++) {
        for (int j = 0; j < width; j++) {
            //input[i * width + j] = rand() % 5;
            //input[i * width + j] = (j + i)%256;
            input[i * width + j] = count;
            count++;
            if(count == 6)
                count = 1;
        }
    }
}

static int sobel_tpu(
        unsigned char* input,
        unsigned char* output,
        int format,   // 0:gray  1:rgb-planar  2:rgb-packed
        int height,
        int width,
        int dx,
        int dy,
        int ksize,
        float scale,
        float delta) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    bm_image_format_ext fmt = format == 0 ? FORMAT_GRAY :
                              (format == 1 ? FORMAT_RGB_PLANAR : FORMAT_RGB_PACKED);
    bm_image img_i;
    bm_image img_o;
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    bm_image_alloc_dev_mem(img_i);
    bm_image_alloc_dev_mem(img_o);
    unsigned char* in_ptr[3] = {input, input + height * width, input + 2 * height * width};
    bm_image_copy_host_to_device(img_i, (void **)(in_ptr));
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        bmcv_image_sobel(handle, img_i, img_o, dx, dy, ksize, scale, delta);
        gettimeofday(&t2, NULL);
        cout << "Sobel TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    #else
        struct timespec tp1, tp2;
        clock_gettime_win(0, &tp1);
        bmcv_image_sobel(handle, img_i, img_o, dx, dy, ksize, scale, delta);
        clock_gettime_win(0, &tp2);
        cout << "Sobel TPU using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << endl;
    #endif
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

static int test_sobel_random(
                             int format,
                             int height,
                             int width,
                             int ksize,
                             int dx,
                             int dy) {
    struct timespec tp;
    #ifdef __linux__
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
    #else
    clock_gettime_win(0, &tp);
    #endif
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    cout << "seed = " << seed << endl;
    float scale = 1;
    float delta = 0;
    //cout << "format: " << format << "  0-gray 1-rgbplanar 2-rgbpacked" << endl;
    cout << "width: " << width << "  height: " << height << endl;
    cout << "dx: " << dx << "  dy: " << dy << "  ksize: " << ksize << endl;
    int channel = format == 0 ? 1 : 3;
    cout << "channel: " << channel << endl;
    unsigned char* input_data = new unsigned char [width * height * channel];
    unsigned char* output_tpu = new unsigned char [width * height * channel];
    fill(input_data, channel, width, height);
    sobel_tpu(
        input_data,
        output_tpu,
        format,
        height,
        width,
        dx,
        dy,
        ksize,
        scale,
        delta);
    int ret = 1;
    switch (format)
    {
    case 0:
    {
        unsigned char ocv_gary_md5[] = "061ae407f0aa9493afc668472bd9ff5c";
        ret = cmpv2(output_tpu, ocv_gary_md5, width , height, channel);
        if(ret != 0)
            return -1;
        break;
    }
    case 1:
    {
        unsigned char ocv_rgbplanar_md5[] = "48b41fc5161ba227f039271351a62efe";
        ret = cmpv2(output_tpu, ocv_rgbplanar_md5, width, height, channel);
        if(ret != 0)
            return -1;
        break;
    }
    case 2:
    {
        unsigned char ocv_rgbpacked_md5[] = "7390ccb6654a4519652584f08324eaae";
        ret = cmpv2(output_tpu, ocv_rgbpacked_md5, width, height, channel);
        if(ret != 0)
            return -1;
        break;
    }
    default:
        break;
    }
    delete [] input_data;
    delete [] output_tpu;
    return ret;
}

int main(int argc, char* args[]) {
    int loop = 3;
    int height = 1080;
    int width = 1920;
    int ksize = 3;
    int dx = 1;
    int dy = 0;
    int format[3] = {0, 1, 2};
    string format_message[3] = {"gray", "rgbplanar", "rgbpacked"};
    if (argc > 1) loop = atoi(args[2]);
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        cout << "========format: " << format[i] << "-" << format_message[i] << "========" << endl;
        ret = test_sobel_random(format[i], height, width, ksize, dx, dy);
        if (ret) {
            cout << "test sobel failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with OpenCV successfully!" << endl;
    return 0;
}