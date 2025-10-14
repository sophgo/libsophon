#include <stdio.h>
#include "bmcv_api_ext.h"
#include "stdlib.h"
#include "string.h"
#include <sys/time.h>
#include <pthread.h>

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

typedef struct {
    int loop_num;
    int use_real_img;
    int channel;
    int height;
    int width;
    int format;
    int ksize;
    float sigmaX;
    float sigmaY;
    bool is_packed;
    char* input_path;
    char* output_path;
    bm_handle_t handle;
} median_blur_thread_arg_t;

void median_blur_cpu_ref(unsigned char *input, unsigned char *output, int height, int width, int ksize, int channel) {
    const int GRAY_LEVELS = 256;
    int edge = ksize / 2;
    int padded_width = width + 2 * edge;
    int padded_height = height + 2 * edge;

    unsigned char *padded_input = (unsigned char *)malloc(sizeof(unsigned char) * padded_width * padded_height * channel);
    if (padded_input == NULL) {
        fprintf(stderr, "Failed to allocate memory for padded_input.\n");
    }
    memset(padded_input, 0, sizeof(unsigned char) * padded_width * padded_height * channel);
    for (int c = 0; c < channel; c++) {
        int src_channel_offset = c * width * height;
        int dst_channel_offset = c * padded_width * padded_height;
        for (int y = 0; y < height; y++) {
            int src_offset = src_channel_offset + y * width;
            int dst_offset = dst_channel_offset + (y + edge) * padded_width + edge;
            memcpy(&padded_input[dst_offset], &input[src_offset], sizeof(unsigned char) * width);
        }
    }
    unsigned char *histogram = (unsigned char *)malloc(sizeof(unsigned char) * GRAY_LEVELS);
    if (histogram == NULL) {
        fprintf(stderr, "Failed to allocate memory for histogram.\n");
    }
    for (int c = 0; c < channel; c++) {
        int channel_offset = c * padded_width * padded_height;
        for (int y = edge; y < padded_height - edge; y++) {
            for (int x = edge; x < padded_width - edge; x++) {
                memset(histogram, 0, sizeof(unsigned char) * GRAY_LEVELS);
                int window_area_idx = 0;
                for (int j = -edge; j <= edge; j++) {
                    int idx_y = y + j;
                    for (int i = -edge; i <= edge; i++) {
                        int idx_x = x + i;
                        unsigned char pixel = padded_input[idx_y * padded_width + idx_x + channel_offset];
                        histogram[pixel]++;
                        window_area_idx++;
                    }
                }
                int medianPos = (window_area_idx - 1) / 2;
                unsigned char count = 0;
                unsigned char median = 0;
                for (int k = 0; k < GRAY_LEVELS; k++) {
                    count += histogram[k];
                    if (count > medianPos) {
                        median = (unsigned char)k;
                        break;
                    }
                }
                output[(y - edge) * width + (x - edge) + c * width * height] = median;
            }
        }
    }
    free(padded_input);
    free(histogram);
}

static void fill_image(unsigned char *input, int width, int height, int channel) {
    for (int i = 0; i < channel; i++) {
        int num = 1;
        for (int j = 0; j < height; j++) {
            for (int k = 0; k < width; k++) {
                input[i * width * height + j * width + k] = num % 128;
                num++;
            }
        }
    }
}

static int median_blur_tpu(unsigned char *input, unsigned char *output, int height, int width,
                           int ksize, int format, bm_handle_t handle) {
    int channel = format == FORMAT_GRAY ? 1 : 3;
    int padd_width = ksize - 1 + width;
    int padd_height = ksize - 1 + height;
    bm_device_mem_t input_data_global_addr, padded_input_data_global_addr;
    bm_malloc_device_byte(handle, &input_data_global_addr, channel * width * height);
    bm_malloc_device_byte(handle, &padded_input_data_global_addr, channel * padd_width * padd_height);
    struct timeval t1, t2;
    bm_memcpy_s2d(handle, input_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(input)));
    gettimeofday(&t1, NULL);
    if(BM_SUCCESS != bmcv_image_median_blur(handle, input_data_global_addr, padded_input_data_global_addr, output, width, height, format, ksize)){
        printf("bmcv_image_median_blur error\n");
        bm_free_device(handle, input_data_global_addr);
        bm_free_device(handle, padded_input_data_global_addr);
        return -1;
    }
    gettimeofday(&t2, NULL);
    printf("median_blur TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    bm_free_device(handle, input_data_global_addr);
    bm_free_device(handle, padded_input_data_global_addr);
    return 0;
}

static int cmp(unsigned char *got, unsigned char *exp, int len) {
    for (int i = 0; i < len; i++) {
        if (abs(got[i] - exp[i]) > 1) {
            for (int j = 0; j < 5; j++)
                printf("cmp error: idx=%d  exp=%d  got=%d\n", i + j, exp[i + j], got[i + j]);
            return -1;
        }
    }
    return 0;
}

static void read_bin(const char *input_path, unsigned char *input_data, int width, int height, int channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("无法打开输出文件 %s\n", input_path);
        return;
    }
    if(fread(input_data, sizeof(char), width * height * channel, fp_src) != 0) {
        printf("read image success\n");
    }
    fclose(fp_src);
}

static void write_bin(const char *output_path, unsigned char *output_data, int width, int height, int channel) {
    FILE *fp_dst = fopen(output_path, "wb");
    if (fp_dst == NULL) {
        printf("无法打开输出文件 %s\n", output_path);
        return;
    }
    fwrite(output_data, sizeof(unsigned char), width * height * channel, fp_dst);
    fclose(fp_dst);
}

static int test_median_blur_random(int use_real_img, int height, int width, int ksize, int format,
                                   char *input_path, char *output_path, bm_handle_t handle) {
    printf("use_real_img   = %d\n", use_real_img);
    printf("width          = %d\n", width);
    printf("height         = %d\n", height);
    printf("format         = %d\n", format);
    printf("ksize          = %d\n", ksize);
    int ret = 0;
    int channel = format == FORMAT_GRAY ? 1 : 3;
    struct timeval t1, t2;
    unsigned char *input_data, *output_tpu, *output_cpu;
    if(format == FORMAT_GRAY) {
        input_data = (unsigned char*)malloc(width * height);
        output_tpu = (unsigned char*)malloc(width * height);
        output_cpu = (unsigned char*)malloc(width * height);
    } else {
        input_data = (unsigned char*)malloc(width * height * 3);
        output_tpu = (unsigned char*)malloc(width * height * 3);
        output_cpu = (unsigned char*)malloc(width * height * 3);
    }
    if (use_real_img) {
        read_bin(input_path, input_data, width, height, channel);
    } else {
        fill_image(input_data, width, height, channel);
    }
    gettimeofday(&t1, NULL);
    median_blur_cpu_ref(input_data, output_cpu, height, width, ksize, channel);
    gettimeofday(&t2, NULL);
    printf("median_blur CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if(0 != median_blur_tpu(input_data, output_tpu, height, width, ksize, format, handle)){
        free(input_data);
        free(output_tpu);
        free(output_cpu);
        return -1;
    }
    ret = cmp(output_tpu, output_cpu, width * height * channel);
    if (ret == 0) {
        printf("Compare TPU result with CPU result success!\n");
        if (use_real_img == 1) {
            write_bin(output_path, output_tpu, width, height, channel);
        }
    } else {
        printf("cpu and tpu failed to compare \n");
    }
    free(input_data);
    free(output_tpu);
    free(output_cpu);
    return ret;
}

void* test_median_blur(void* args) {
    median_blur_thread_arg_t* median_blur_thread_arg = (median_blur_thread_arg_t*)args;
    int loop_num = median_blur_thread_arg->loop_num;
    int use_real_img = median_blur_thread_arg->use_real_img;
    int height = median_blur_thread_arg->height;
    int width = median_blur_thread_arg->width;
    int format = median_blur_thread_arg->format;
    int ksize = median_blur_thread_arg->ksize;
    char* input_path = median_blur_thread_arg->input_path;
    char* output_path = median_blur_thread_arg->output_path;
    bm_handle_t handle = median_blur_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if(loop_num > 1) {
            width = 8 + rand() % 4089;
            height = 8 + rand() % 2993;
            int k_size_num[] = {3, 5, 7, 9};
            int size = sizeof(k_size_num) / sizeof(k_size_num[0]);
            int rand_num = rand() % size;
            ksize = k_size_num[rand_num];
            int format_num[] = {FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
            size = sizeof(format_num) / sizeof(format_num[0]);
            rand_num = rand() % size;
            format = format_num[rand_num];
        }
        if (0 != test_median_blur_random(use_real_img, height, width, ksize, format,
                                         input_path, output_path, handle)) {
            printf("------TEST median_blur FAILED------\n");
            exit(-1);
        }
        printf("------TEST median_blur PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char *args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    printf("seed = %d\n", seed);
    int thread_num = 1;
    int loop = 1;
    int use_real_img = 0;
    int width = 8 + rand() % 4089;
    int height = 8 + rand() % 2993;
    int k_size_num[] = {3, 5, 7, 9};
    int size = sizeof(k_size_num) / sizeof(k_size_num[0]);
    int rand_num = rand() % size;
    int ksize = k_size_num[rand_num];
    int format_num[] = {FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
    size = sizeof(format_num) / sizeof(format_num[0]);
    rand_num = rand() % size;
    int format = format_num[rand_num];
    char *input_path = NULL;
    char *output_path = NULL;
    int ret = 0;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return -1;
    }
    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: \n");
        printf("%s thread_num loop use_real_img width height format ksize input_path output_path(when use_real_img = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 1\n", args[0]);
        printf("%s 2 1 0 512 512 8 3 \n", args[0]);
        printf("%s 1 1 1 1920 1080 8 3 res/1920x1080_rgb.bin out/out_median_blur.bin \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    if (argc > 5) height = atoi(args[5]);
    if (argc > 6) format = atoi(args[6]);
    if (argc > 7) ksize = atoi(args[7]);
    if (argc > 8) input_path = args[8];
    if (argc > 9) output_path = args[9];

    printf("thread_num = %d\n", thread_num);
    printf("loop_num   = %d\n", loop);
    // test for multi-thread
    pthread_t pid[thread_num];
    median_blur_thread_arg_t median_blur_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        median_blur_thread_arg[i].loop_num = loop;
        median_blur_thread_arg[i].use_real_img = use_real_img;
        median_blur_thread_arg[i].height = height;
        median_blur_thread_arg[i].width = width;
        median_blur_thread_arg[i].format = format;
        median_blur_thread_arg[i].ksize = ksize;
        median_blur_thread_arg[i].input_path = input_path;
        median_blur_thread_arg[i].output_path = output_path;
        median_blur_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_median_blur, median_blur_thread_arg + i) != 0) {
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
