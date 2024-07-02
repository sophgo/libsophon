#include <stdio.h>
#include "bmcv_api_ext_c.h"
#include "stdlib.h"
#include "string.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

typedef struct {
    int loop_num;
    int random;
    int channel;
    int height;
    int width;
    int format;
    float sigmaX;
    float sigmaY;
    bool is_packed;
    char* input_path;
    char* output_path;
    bm_handle_t handle;
} gaussian_blur_thread_arg_t;

static int parameters_check(int height, int width)
{
    if (height > 8192 || width > 4096){
        printf("Unsupported size : size_max = 4096 x 8192 \n");
        return -1;
    }
    return 0;
}

#if 0 // Use opencv for comparison to prove that the cpu side's own implementation of ref is correct.
#include "opencv2/opencv.hpp"
using namespace cv;
#endif

#if 0
static int gaussian_blur_opencv(unsigned char *input, unsigned char *output, int channel, bool is_packed,
                                int height, int width, int kw, int kh, float sigmaX, float sigmaY, int format) {
    int type = is_packed ? CV_8UC3 : CV_8UC1;
    channel = is_packed ? 1 : channel;
    if (format == 0) {
        unsigned char *input_addr[3] = {input, input + width * height, input + width * height * 5 / 4};
        unsigned char *output_addr[3] = {output, output + width * height, output + width * height * 5 / 4};
        for (int i = 0; i < channel; i++){
            Mat mat_i = Mat(height, width, type, input_addr[i]);
            Mat mat_o = Mat(height, width, type, output_addr[i]);
            GaussianBlur(mat_i, mat_o, Size(kw, kh), sigmaX, sigmaY);
        }
    } else if (format == 1) {
        unsigned char *input_addr[3] = {input, input + width * height, input + width * height * 3 / 2};
        unsigned char *output_addr[3] = {output, output + width * height, output + width * height * 3 / 2};
        for (int i = 0; i < channel; i++) {
            Mat mat_i = Mat(height, width, type, input_addr[i]);
            Mat mat_o = Mat(height, width, type, output_addr[i]);
            GaussianBlur(mat_i, mat_o, Size(kw, kh), sigmaX, sigmaY);
        }
    } else {
        for (int i = 0; i < channel; i++) {
            Mat mat_i = Mat(height, width, type, input + i * height * width);
            Mat mat_o = Mat(height, width, type, output + i * height * width);
            GaussianBlur(mat_i, mat_o, Size(kw, kh), sigmaX, sigmaY);
        }
    }
    return 0;
}
#endif

static int get_format_size(int format,int width, int height) {
    switch (format) {
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PACKED:
        case FORMAT_BGR_PACKED:
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
            return width * height * 3;
        case FORMAT_GRAY:
            return width * height;
            break;
        default:
            printf("format error\n");
            return 0;
    }
}

static void fill(unsigned char *input, int channel, int width, int height, int is_packed) {
    for (int i = 0; i < (is_packed ? 3 : channel); i++) {
        int num = 10;
        for (int j = 0; j < height; j++) {
            for (int k = 0; k < width; k++) {
                input[i * width * height + j * width + k] = num % 128;
                num++;
            }
        }
    }
}

static int get_gaussian_sep_kernel(int n, float sigma, float *k_sep) {
    const int SMALL_GAUSSIAN_SIZE = 3;
    static const float small_gaussian_tab[3] = {0.25f, 0.5f, 0.25f};
    const float* fixed_kernel = n % 2 == 1 && n <= SMALL_GAUSSIAN_SIZE && sigma <= 0 ? small_gaussian_tab : 0;
    float sigmaX = sigma > 0 ? sigma : ((n - 1) * 0.5 - 1) * 0.3 + 0.8;
    float scale2X = -0.5 / (sigmaX * sigmaX);
    float sum = 0;
    int i;

    for (i = 0; i < n; i++) {
        float x = i - (n - 1) * 0.5;
        float t = fixed_kernel ? fixed_kernel[i] : exp(scale2X * x * x);
        k_sep[i] = t;
        sum += k_sep[i];
    }
    sum = 1./sum;
    for (i = 0; i < n; i++) {
        k_sep[i] = k_sep[i] * sum;
    }
    return 0;
}

static void create_gaussian_kernel(float* kernel, int kw, int kh, float sigma1, float sigma2) {
    float* k_sep_x = (float* )malloc(sizeof(float) * kw);
    float* k_sep_y = (float* )malloc(sizeof(float) * kh);

    if(sigma2 <= 0) sigma2 = sigma1;
    // automatic detection of kernel size from sigma
    if (kw <= 0 && sigma1 > 0 ) kw = (int)round(sigma1 * 3 * 2 + 1) | 1;
    if (kh <= 0 && sigma2 > 0 ) kh = (int)round(sigma2 * 3 * 2 + 1) | 1;
    sigma1 = sigma1 < 0 ? 0 : sigma1;
    sigma2 = sigma2 < 0 ? 0 : sigma2;
    get_gaussian_sep_kernel(kw, sigma1, k_sep_x);
    if (kh == kw && abs(sigma1 - sigma2) < DBL_EPSILON) {
        get_gaussian_sep_kernel(kw, sigma1, k_sep_y);
    } else {
        get_gaussian_sep_kernel(kh, sigma2, k_sep_y);
    }
    for (int i = 0; i < kh; i++) {
        for (int j = 0; j < kw; j++) {
            kernel[i * kw + j] = k_sep_y[i] * k_sep_x[j];
        }
    }
    free(k_sep_x);
    free(k_sep_y);
}

static void borderfill(int width, int height, int ksize_w, int ksize_h,unsigned char *input_data,
                       unsigned char* input_data_border) {
    int border_w = ksize_w / 2;
    int border_h = ksize_h / 2;
    int col = 0;
    int row = 0;
    int temp = 0;
    // first fill left and right
    for (int i = border_h; i < border_h + height; i++) {
        temp = border_w;
        for (int j = 0; j < border_w; j++) {
            input_data_border[i * (width + 2 * border_w) + j] = input_data[row * width + temp];
            temp--;
        }
        for (int j = border_w; j < border_w + width; j++) {
            input_data_border[i * (width + 2 * border_w) + j] = input_data[row * width + col];
            col++;
        }
        temp = width - 2;
        for (int j = border_w + width; j < 2 * border_w + width; j++) {
            input_data_border[i * (width + 2 * border_w) + j] = input_data[row * width + temp];
            temp--;
        }
        row++;
        col = 0;
    }
    // fill top and bottom
    temp = 2 * border_h;
    for (int i = 0; i < border_h; i++) {
        for (int j = 0; j < 2 * border_w + width; j++) {
            input_data_border[i * (2 * border_w + width) + j] =
            input_data_border[(i + temp) * (2 * border_w + width) + j];
        }
        temp -= 2;
    }
    temp = 2;
    for (int i = border_h + height; i < 2 * border_h + height; i++) {
        for (int j = 0; j < 2 * border_w + width; j++) {
            input_data_border[i * (2 * border_w + width) + j] =
            input_data_border[(i - temp) * (2 * border_w + width) + j];
        }
        temp += 2;
    }
}

static void conv(int width, int height, int kw, int kh, float *kernel,
                 unsigned char *input_data_border, unsigned char *output) {
    // The cpu side does the convolution on the input_img
    float sum_ = 0;
    for (int i = 0; i < (kh / 2 + height); i++) {
        for (int j = 0; j < (kw / 2 + width); j++) {
            for (int g = 0; g < kh; g++) {
                for (int k = 0; k < kw; k++) {
                    sum_ += input_data_border[(i + g) * (2 * (kw / 2) + width) + j + k] * kernel[g * kw + k];
                }
            }
            if (sum_ < 0) {sum_ = 0;}
            if (sum_ > 255) {sum_ = 255;}
            if ((i < height) && (j < width)) {
                output[i * width + j] = (unsigned char)(sum_ + 0.5); // opencv是四舍五入
            }
            sum_ = 0;
        }
    }
}

static int gaussian_blur_ref_single_channel(unsigned char *input, unsigned char *output, int height,
                                            int width, int kw, int kh, float *kernel) {
    // 1.get kernel   --get from last param
    // 2.border fill  --different from tpu,but the result is the same.
    int border_w = kw / 2;
    int border_h = kh / 2;
    unsigned char* input_data_border = (unsigned char*)malloc((width + 2 * border_w) * (height + 2 * border_h + 1));
    memset(input_data_border, 0, (width + 2 * border_w) * (height + 2 * border_h));
    borderfill(width, height, kw, kh, input, input_data_border);
#if 0 // check borderfill
            for(int i = 0; i < 10; i++){
                for(int j = 0; j < 10; j++){
                    printf("%3d ",input_data_border[i * 10 + j]);
                }
                printf("\n");
            }
#endif
    // 3.convolution
    conv(width, height, kw, kh, kernel, input_data_border, output);
    free(input_data_border);
    return 0;
}

static int gaussian_blur_cpu_ref(unsigned char *input, unsigned char *output, int channel, bool is_packed,
                                 int height, int width, int kw, int kh, float sigmaX, float sigmaY) {
    // create kernel
    float *kernel = (float*)malloc(kw * kh * sizeof(float));
    memset(kernel, 0, kw * kh * sizeof(float));
    create_gaussian_kernel(kernel, kw, kh, sigmaX, sigmaY);
    if(is_packed) {
        unsigned char *input_temp = (unsigned char *)malloc(width * height * 3);
        unsigned char *output_temp = (unsigned char *)malloc(width * height * 3);
        // Adjusting the order of rgb alignment
        for (int i = 0; i < 3; i++) {
            int temp = 0;
            for (int j = 0; j < width * height; j++) {
                input_temp[i * width * height + j] = input[i + temp];
                temp += 3;
            }
        }
        for (int i = 0; i < 3; i++) {
            gaussian_blur_ref_single_channel(input_temp + i * width * height, output_temp + i * width * height,
                                             height, width, kw, kh, kernel);
        }
        for (int i = 0; i < 3; i++) {
            int tep = 0;
            for (int j = 0; j < width * height; j++) {
                output[i + tep] = output_temp[i * width * height + j];
                tep += 3;
            }
        }
        free(input_temp);
        free(output_temp);
    } else {
        for (int i = 0; i < channel; i++) {
            gaussian_blur_ref_single_channel(input + i * width * height, output + i * width * height,
                                             height, width, kw, kh, kernel);
        }
    }
    free(kernel);
    return 0;
}

static int gaussian_blur_tpu(unsigned char *input, unsigned char *output, int height, int width, int kw,
                             int kh, float sigmaX, float sigmaY, int format, bm_handle_t handle) {
    bm_image img_i;
    bm_image img_o;
    struct timeval t1, t2;

    bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_i, NULL);
    bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_o, NULL);
    bm_image_alloc_dev_mem(img_i, 2);
    bm_image_alloc_dev_mem(img_o, 2);

    if(format == 14){
        unsigned char *input_addr[1] = {input};
        bm_image_copy_host_to_device(img_i, (void **)(input_addr));
    }else{
        unsigned char *input_addr[3] = {input, input + height * width, input + 2 * height * width};
        bm_image_copy_host_to_device(img_i, (void **)(input_addr));
    }

    gettimeofday(&t1, NULL);
    if(BM_SUCCESS != bmcv_image_gaussian_blur(handle, img_i, img_o, kw, kh, sigmaX, sigmaY)){
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        bm_dev_free(handle);
        return -1;
    }
    gettimeofday(&t2, NULL);
    printf("Gaussian_blur TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (format == 14){
        unsigned char *output_addr[1] = {output};
        bm_image_copy_device_to_host(img_o, (void **)output_addr);
    } else {
        unsigned char *output_addr[3] = {output, output + height * width, output + 2 * height * width};
        bm_image_copy_device_to_host(img_o, (void **)output_addr);
    }
    bm_image_destroy(img_i);
    bm_image_destroy(img_o);
    return 0;
}

static int cmp(unsigned char *got, unsigned char *exp, int len) {
    for (int i = 0; i < len; i++) {
        if (abs(got[i] - exp[i]) > 2) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
            return -1;
        }
    }
    return 0;
}

static void read_bin(const char *input_path, unsigned char *input_data, int width, int height, int format) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("无法打开输出文件 %s\n", input_path);
        return;
    }
    if(fread(input_data, sizeof(char), get_format_size(format, width, height), fp_src) != 0) {
        printf("read image success\n");
    }
    fclose(fp_src);
}

static void write_bin(const char *output_path, unsigned char *output_data, int width, int height, int format) {
    FILE *fp_dst = fopen(output_path, "wb");
    if (fp_dst == NULL) {
        printf("无法打开输出文件 %s\n", output_path);
        return;
    }
    fwrite(output_data, sizeof(char), get_format_size(format, width, height), fp_dst);
    fclose(fp_dst);
}

static int test_gaussian_blur_random(int random, int channel, bool is_packed, int height, int width, int format,
                                     float sgmX, float sgmY, char *input_path, char *output_path, bm_handle_t handle) {
    struct timeval t1, t2;
    int kw = 3, kh = 3;
    float sigmaX = sgmX;
    float sigmaY = sgmY;
    printf("===== test gaussian blur =====\n");
    // 1.Define the parameters
    printf("channel: %d,  width: %d,  height: %d\n", channel, width, height);
    printf("kw: %d,  kh: %d,  is_packed: %d\n", kw, kh, is_packed);
    printf("sigmaX: %f,  sigmaY: %f,  format: %d\n", sigmaX, sigmaY, format);
    unsigned char *input_data = (unsigned char*)malloc(width * height * (is_packed ? 3 : channel));
    unsigned char *output_tpu = (unsigned char*)malloc(width * height * (is_packed ? 3 : channel));
    unsigned char *output_cpu = (unsigned char*)malloc(width * height * (is_packed ? 3 : channel));
    // 2.fill input_img
    if (random) {
        fill(input_data, (is_packed ? 3 : channel), width, height, is_packed);
    } else {
        // Input file test, can be written as image observation
        read_bin(input_path, input_data, width, height, format);
    }
    // 3.gaussian_blur --cpu reference
    gettimeofday(&t1, NULL);
    gaussian_blur_cpu_ref(input_data, output_cpu, channel, is_packed, height,
                          width, kw, kh, sigmaX, sigmaY);
    gettimeofday(&t2, NULL);
    printf("Gaussian_blur CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
#if 0 // Use opencv for comparison to prove that the cpu side's own implementation of ref is correct.
    unsigned char *output_opencv = (unsigned char*)malloc(width * height * (is_packed ? 3 : channel));
    gaussian_blur_opencv(input_data, output_opencv, channel, is_packed,
                         height, width, kw, kh, sigmaX, sigmaY, format);
    // cmp opencv & cpu reference
    if (0 != cmp(output_cpu, output_opencv, get_format_size(format, width, height))) {
        printf("opencv and cpu failed to compare \n");
    } else {
        printf("Compare CPU result with OpenCV successfully!\n");
    }
    free(output_opencv);
#endif
    if(0 != gaussian_blur_tpu(input_data, output_tpu, height, width, kw, kh, sigmaX, sigmaY, format, handle)){
        free(input_data);
        free(output_tpu);
        free(output_cpu);
        return -1;
    }
    int ret = cmp(output_tpu, output_cpu, get_format_size(format, width, height));
    if (ret == 0) {
        printf("Compare TPU result with CPU result successfully!\n");
        if (random == 0) {
            write_bin(output_path, output_tpu, width, height, format);
        }
    } else {
        printf("cpu and tpu failed to compare \n");
    }
    free(input_data);
    free(output_tpu);
    free(output_cpu);
    return ret;
}

void* test_gaussian_blur(void* args) {
    gaussian_blur_thread_arg_t* gaussian_blur_thread_arg = (gaussian_blur_thread_arg_t*)args;
    int loop_num = gaussian_blur_thread_arg->loop_num;
    int random = gaussian_blur_thread_arg->random;
    int channel = gaussian_blur_thread_arg->channel;
    int height = gaussian_blur_thread_arg->height;
    int width = gaussian_blur_thread_arg->width;
    int format = gaussian_blur_thread_arg->format;
    float sigmaX = gaussian_blur_thread_arg->sigmaX;
    float sigmaY = gaussian_blur_thread_arg->sigmaY;
    bool is_packed = gaussian_blur_thread_arg->is_packed;
    char* input_path = gaussian_blur_thread_arg->input_path;
    char* output_path = gaussian_blur_thread_arg->output_path;
    bm_handle_t handle = gaussian_blur_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        // if (loop_num > 1) {
        //     width = 2 + rand() % 4094;
        //     height = 2 + rand() % 8190;
        //     int format_num[] = {8,9,10,11,12,13,14};
        //     int size = sizeof(format_num) / sizeof(format_num[0]);
        //     int rand_num = rand() % size;
        //     format = format_num[rand_num];
        //     sigmaX = (float)rand() / RAND_MAX * 5.0f;
        //     sigmaY = (float)rand() / RAND_MAX * 5.0f;
        //     channel = ((format == 10 || format == 11 || format == 14) ? 1 : 3);
        //     is_packed = ((format == 10 || format == 11) ? 1 : 0);
        // }
        if (0 != test_gaussian_blur_random(random, channel, is_packed, height, width, format, sigmaX,
                                           sigmaY, input_path, output_path, handle)) {
            printf("------TEST GAUSSIAN_BLUR FAILED------\n");
            return (void*)-1;
        }
        printf("------TEST GAUSSIAN_BLUR PASSED!------\n");
    }
    return (void*)0;
}


int main(int argc, char *args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    printf("seed = %d\n", seed);
    int random = 1;
    int loop = 1;
    int width = 2 + rand() % 4094;
    int height = 2 + rand() % 8190;
    int format_num[] = {8,9,10,11,12,13,14};
    int size = sizeof(format_num) / sizeof(format_num[0]);
    int rand_num = rand() % size;
    int format = format_num[rand_num];
    float sigmaX = (float)rand() / RAND_MAX * 5.0f;
    float sigmaY = (float)rand() / RAND_MAX * 5.0f;
    int channel;
    int is_packed;
    int thread_num = 1;
    int check = 0;
    int ret = 0;
    char *input_path = NULL;
    char *output_path = NULL;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return -1;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: \n");
        printf("%s thread_num loop random height width format sigmaX sigmaY input_path output_path(when random = 0,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 1\n", args[0]);
        printf("%s 2 1 1 512 512 8 0.5 0.5\n", args[0]);
        printf("%s 1 1 1 1080 1920 8 2 0 res/1920x1080_rgb.bin out/out_gaussian_blur.bin \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) random = atoi(args[3]);
    if (argc > 4) height = atoi(args[4]);
    if (argc > 5) width = atoi(args[5]);
    if (argc > 6) format = atoi(args[6]);
    if (argc > 7) sigmaX = atof(args[7]);
    if (argc > 8) sigmaY = atof(args[8]);
    if (argc > 9) input_path = args[9];
    if (argc > 10) output_path = args[10];
    channel = ((format == 10 || format == 11 || format == 14) ? 1 : 3);
    is_packed = ((format == 10 || format == 11) ? 1 : 0);
    check = parameters_check(height, width);
    if (check) {
        printf("Parameters Failed! \n");
        return check;
    }

    // test for multi-thread
    pthread_t pid[thread_num];
    gaussian_blur_thread_arg_t gaussian_blur_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        gaussian_blur_thread_arg[i].loop_num = loop;
        gaussian_blur_thread_arg[i].random = random;
        gaussian_blur_thread_arg[i].channel = channel;
        gaussian_blur_thread_arg[i].is_packed = is_packed;
        gaussian_blur_thread_arg[i].height = height;
        gaussian_blur_thread_arg[i].width = width;
        gaussian_blur_thread_arg[i].format = format;
        gaussian_blur_thread_arg[i].sigmaX = sigmaX;
        gaussian_blur_thread_arg[i].sigmaY = sigmaY;
        gaussian_blur_thread_arg[i].input_path = input_path;
        gaussian_blur_thread_arg[i].output_path = output_path;
        gaussian_blur_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_gaussian_blur, gaussian_blur_thread_arg + i) != 0) {
            printf("create thread failed\n");
            bm_dev_free(handle);
            return -1;
        }
    }
    for (int i = 0; i < thread_num; i++) {
        int ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            bm_dev_free(handle);
            exit(-1);
        }
    }

    bm_dev_free(handle);
    return ret;
}