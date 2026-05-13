#include <stdio.h>
#include "bmcv_api_ext_c.h"
#include "stdlib.h"
#include "string.h"
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

typedef struct {
    int loop_num;
    int use_real_img;
    int channel;
    int input_height;
    int input_width;
    int output_height;
    int output_width;
    int format;
    int interpolation_mode;
    const char *mapx_data_path;
    const char *mapy_data_path;
    const char *input_path;
    const char *output_path;
    bm_handle_t handle;
} remap_thread_arg_t;

static void fill_map(float* map_x, float* map_y, int input_width, int input_height) {
    for (int y = 0; y < input_height; y++) {
        for (int x = 0; x < input_width; x++) {
            map_x[y * input_width + x] = input_width - x - 1;//;
            map_y[y * input_width + x] = y;
        }
    }
}

unsigned char bilinear_interpolation(unsigned char* src, int src_width, int src_height, float x, float y, int c) {
    const float x_clamped = fmaxf(0.0f, fminf(x, src_width - 1.0f));
    const float y_clamped = fmaxf(0.0f, fminf(y, src_height - 1.0f));
    const int x0 = (int)floorf(x_clamped);
    const int y0 = (int)floorf(y_clamped);
    float dx = x_clamped - x0;
    float dy = y_clamped - y0;
    int x1 = (int)fminf(x0 + 1, src_width  - 1);
    int y1 = (int)fminf(y0 + 1, src_height - 1);
    unsigned char* channel_base = src + c * src_width * src_height;
    const unsigned char p00 = channel_base[y0 * src_width + x0];
    const unsigned char p01 = channel_base[y0 * src_width + x1];
    const unsigned char p10 = channel_base[y1 * src_width + x0];
    const unsigned char p11 = channel_base[y1 * src_width + x1];

    const float val = p00 * (1 - dx) * (1 - dy) + p01 * dx * (1 - dy) + p10 * (1 - dx) * dy + p11 * dx * dy;

    return (unsigned char)fminf(255.0f, fmaxf(0.0f, val + 0.5f));
}

void remap_cpu_ref(unsigned char *input, unsigned char *output, float *mapx, float *mapy, int input_height,
                   int input_width, int output_height, int output_width, int channel, int interpolation_mode) {
    if (interpolation_mode) {
        //bilinear interpolation
        for (int c = 0; c < channel; ++c) {
            int channel_offset = c * output_width * output_height;
            for (int y = 0; y < output_height; ++y) {
                for (int x = 0; x < output_width; ++x) {
                    const int map_idx = y * output_width + x;
                    const float src_x = mapx[map_idx];
                    const float src_y = mapy[map_idx];
                    if (src_x < 0 || src_x >= input_width ||
                        src_y < 0 || src_y >= input_height) {
                        output[channel_offset + map_idx] = 0;
                    } else {
                        output[channel_offset + map_idx] = bilinear_interpolation(input, input_width, input_height, src_x, src_y, c);
                    }
                }
            }
        }
    } else {
        //nearest interpolation
        for (int c = 0; c < channel; ++c) {
            const int input_channel_offset = c * input_width * input_height;
            const int output_channel_offset = c * output_width * output_height;
            for (int y = 0; y < output_height; ++y) {
                for (int x = 0; x < output_width; ++x) {
                    const int map_idx = y * output_width + x;
                    const float src_x = mapx[map_idx];
                    const float src_y = mapy[map_idx];
                    int src_x_round = static_cast<int>(src_x + 0.5f);
                    int src_y_round = static_cast<int>(src_y + 0.5f);
                    if (src_x_round < 0 || src_x_round >= input_width ||
                        src_y_round < 0 || src_y_round >= input_height) {
                        output[output_channel_offset + map_idx] = 0;
                    } else {
                        output[output_channel_offset + map_idx] = input[input_channel_offset + src_y_round * input_width + src_x_round];
                    }
                }
            }
        }
    }
}

static void fill_image(unsigned char *input, int input_width, int input_height, int channel) {
    for (int i = 0; i < channel; i++) {
        int num = 1;
        for (int j = 0; j < input_height; j++) {
            for (int k = 0; k < input_width; k++) {
                input[i * input_width * input_height + j * input_width + k] = num % 128;
                num++;
            }
        }
    }
}

static int remap_tpu(bm_handle_t handle, unsigned char *input, unsigned char *output, int input_height, int input_width,
                     int output_height, int output_width, int format, float *mapx_data, float *mapy_data, int interpolation_mode) {
    bm_status_t ret = BM_SUCCESS;
    bm_image input_image, output_image;
    bm_device_mem_t mapx_data_global_addr, mapy_data_global_addr;
    struct timeval t1, t2;
    ret = bm_image_create(handle, input_height, input_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_image, NULL);
    if (ret != BM_SUCCESS) {
        printf("bm_image_create input_image error\n");
        return ret;
    }
    ret = bm_image_create(handle, output_height, output_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_image, NULL);
    if (ret != BM_SUCCESS) {
        printf("bm_image_create output_image error\n");
        return ret;
    }
    ret = bm_image_alloc_dev_mem(input_image, BMCV_HEAP_ANY);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem input_image error\n");
        return ret;
    }
    ret = bm_image_alloc_dev_mem(output_image, BMCV_HEAP_ANY);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem output_image error\n");
        return ret;
    }
    if (format == 14) {
        unsigned char *input_addr[1] = {input};
        ret = bm_image_copy_host_to_device(input_image, (void **)(input_addr));
    } else {
        unsigned char *input_addr[3] = {input, input + input_height * input_width, input + 2 * input_height * input_width};
        ret = bm_image_copy_host_to_device(input_image, (void **)(input_addr));
    }
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_host_to_device input_image error\n");
        bm_image_destroy(input_image);
        bm_image_destroy(output_image);
        return ret;
    }
    ret = bm_malloc_device_byte(handle, &mapx_data_global_addr, output_width * output_height * sizeof(float));
    if (BM_SUCCESS != ret) {
        printf("bm_malloc_device_byte mapx_data_global_addr error\n");
        bm_image_destroy(input_image);
        bm_image_destroy(output_image);
        return ret;
    }
    ret = bm_malloc_device_byte(handle, &mapy_data_global_addr, output_width * output_height * sizeof(float));
    if (BM_SUCCESS != ret) {
        printf("bm_malloc_device_byte mapy_data_global_addr error\n");
        bm_image_destroy(input_image);
        bm_image_destroy(output_image);
        bm_free_device(handle, mapx_data_global_addr);
        return ret;
    }
    ret = bm_memcpy_s2d(handle, mapx_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(mapx_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d mapx_data error\n");
        bm_image_destroy(input_image);
        bm_image_destroy(output_image);
        bm_free_device(handle, mapx_data_global_addr);
        bm_free_device(handle, mapy_data_global_addr);
        return ret;
    }
    ret = bm_memcpy_s2d(handle, mapy_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(mapy_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d mapy_data error\n");
        bm_image_destroy(input_image);
        bm_image_destroy(output_image);
        bm_free_device(handle, mapx_data_global_addr);
        bm_free_device(handle, mapy_data_global_addr);
        return ret;
    }
    gettimeofday(&t1, NULL);
    ret = bmcv_image_remap(handle, input_image, output_image, mapx_data_global_addr, mapy_data_global_addr, interpolation_mode);
    if(ret != BM_SUCCESS){
        printf("bmcv_image_remap error\n");
        bm_image_destroy(input_image);
        bm_image_destroy(output_image);
        bm_free_device(handle, mapx_data_global_addr);
        bm_free_device(handle, mapy_data_global_addr);
        return ret;
    }
    gettimeofday(&t2, NULL);
    printf("remap TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (format == 14) {
        unsigned char *output_addr[1] = {output};
        ret = bm_image_copy_device_to_host(output_image, (void **)output_addr);
    } else {
        unsigned char *output_addr[3] = {output, output + output_height * output_width, output + 2 * output_height * output_width};
        ret = bm_image_copy_device_to_host(output_image, (void **)output_addr);
    }
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_device_to_host output_image error\n");
    }
    bm_image_destroy(input_image);
    bm_image_destroy(output_image);
    bm_free_device(handle, mapx_data_global_addr);
    bm_free_device(handle, mapy_data_global_addr);
    return ret;
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

static void read_bin_float(const char *input_path, float *input_data, int width, int height, int channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("无法打开输出文件 %s\n", input_path);
        return;
    }
    if(fread(input_data, sizeof(float), width * height * channel, fp_src) != 0) {
        printf("read map success\n");
    }
    fclose(fp_src);
}

static void read_bin(const char *input_path, unsigned char *input_data, int input_width, int input_height, int channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("无法打开输入文件 %s\n", input_path);
        return;
    }
    if(fread(input_data, sizeof(char), input_width * input_height * channel, fp_src) != 0) {
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
    printf("write image success\n");
}

static int test_remap_random(int use_real_img, int input_height, int input_width, int output_height, int output_width,
                             int format, int interpolation_mode, const char *mapx_path, const char *mapy_path, const char *input_path,
                             const char *output_path, bm_handle_t handle) {
    printf("use_real_img       = %d\n", use_real_img);
    printf("input_width        = %d\n", input_width);
    printf("input_height       = %d\n", input_height);
    printf("output_width       = %d\n", output_width);
    printf("output_height      = %d\n", output_height);
    printf("format             = %d\n", format);
    printf("interpolation_mode = %d\n", interpolation_mode);
    int ret = 0;
    int channel = format == FORMAT_GRAY ? 1 : 3;
    struct timeval t1, t2;
    unsigned char *input_data, *output_tpu, *output_cpu;
    float *mapx_data = (float*)malloc(output_width * output_height * sizeof(float));
    float *mapy_data = (float*)malloc(output_width * output_height * sizeof(float));
    if (format == FORMAT_GRAY) {
        input_data = (unsigned char*)malloc(input_width * input_height);
        output_tpu = (unsigned char*)malloc(output_width * output_height);
        output_cpu = (unsigned char*)malloc(output_width * output_height);
    } else {
        input_data = (unsigned char*)malloc(input_width * input_height * 3);
        output_tpu = (unsigned char*)malloc(output_width * output_height * 3);
        output_cpu = (unsigned char*)malloc(output_width * output_height * 3);
    }
    if (use_real_img) {
        read_bin(input_path, input_data, input_width, input_height, channel);
        read_bin_float(mapx_path, mapx_data, output_width, output_height, channel);
        read_bin_float(mapy_path, mapy_data, output_width, output_height, channel);
    } else {
        fill_image(input_data, input_width, input_height, channel);
        fill_map(mapx_data, mapy_data, output_width, output_height);
    }
    gettimeofday(&t1, NULL);
    remap_cpu_ref(input_data, output_cpu, mapx_data, mapy_data, input_height, input_width, output_height, output_width, channel, interpolation_mode);
    gettimeofday(&t2, NULL);
    printf("remap CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if(0 != remap_tpu(handle, input_data, output_tpu, input_height, input_width, output_height, output_width, format, mapx_data, mapy_data, interpolation_mode)){
        free(input_data);
        free(output_tpu);
        free(output_cpu);
        free(mapx_data);
        free(mapy_data);
        return -1;
    }
    ret = cmp(output_tpu, output_cpu, output_width * output_height * channel);
    if (ret == 0) {
        printf("Compare TPU result with CPU result success!\n");
        if (use_real_img == 1) {
            write_bin(output_path, output_tpu, output_width, output_height, channel);
        }
    } else {
        printf("cpu and tpu failed to compare \n");
    }
    free(input_data);
    free(output_tpu);
    free(output_cpu);
    free(mapx_data);
    free(mapy_data);
    return ret;
}

void* test_remap(void* args) {
    remap_thread_arg_t* remap_thread_arg = (remap_thread_arg_t*)args;
    int loop_num = remap_thread_arg->loop_num;
    int use_real_img = remap_thread_arg->use_real_img;
    int input_height = remap_thread_arg->input_height;
    int input_width = remap_thread_arg->input_width;
    int output_height = remap_thread_arg->output_height;
    int output_width = remap_thread_arg->output_width;
    int format = remap_thread_arg->format;
    int interpolation_mode = remap_thread_arg->interpolation_mode;
    const char* mapx_path = remap_thread_arg->mapx_data_path;
    const char* mapy_path = remap_thread_arg->mapy_data_path;
    const char* input_path = remap_thread_arg->input_path;
    const char* output_path = remap_thread_arg->output_path;
    bm_handle_t handle = remap_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if(loop_num > 1) {
            interpolation_mode = rand() % 2;
            if (interpolation_mode == 1) {
                input_width = 8 + rand() % 8185;
                input_height = 8 + rand() % 8185;
                output_width = 8 + rand() % 4089;
                output_height = 8 + rand() % 4089;
            } else {
                input_width = 8 + rand() % 8185;
                input_height = 8 + rand() % 8185;
                output_width = 8 + rand() % 8185;
                output_height = 8 + rand() % 8185;
            }
            int format_num[] = {FORMAT_YUV444P, FORMAT_RGB_PLANAR, FORMAT_BGR_PLANAR, FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
            int size = sizeof(format_num) / sizeof(format_num[0]);
            int rand_num = rand() % size;
            format = format_num[rand_num];
        }
        if (0 != test_remap_random(use_real_img, input_height, input_width, output_height, output_width,
                                   format, interpolation_mode, mapx_path, mapy_path, input_path, output_path, handle)) {
            printf("------TEST REMAP FAILED------\n");
            exit(-1);
        }
        printf("------TEST REMAP PASSED!------\n");
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
    int input_width = 1920;
    int input_height = 1080;
    int output_width = 1920;
    int output_height = 1080;
    int format_num[] = {FORMAT_YUV444P, FORMAT_RGB_PLANAR, FORMAT_BGR_PLANAR, FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
    int size = sizeof(format_num) / sizeof(format_num[0]);
    int rand_num = rand() % size;
    int format = format_num[rand_num];
    int interpolation_mode = 1; //0-nearest 1-bilinear
    const char *mapx_path = "mapx.bin";
    const char *mapy_path = "mapy.bin";
    const char *input_path = "rgbplanar_1920_1080.bin";
    const char *output_path = "remap_output.bin";
    int ret = 0;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return -1;
    }
    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: \n");
        printf("%s thread_num loop use_real_img interpolation_mode format input_width input_height output_width output_height mapx mapy input_path output_path(when use_real_img = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 1\n", args[0]);
        printf("%s 2 1 0 1 8 512 512 1024 1024 \n", args[0]);
        printf("%s 1 1 1 0 8 1920 1080 1920 1080 mapx.bin mapy.bin 1920x1080_rgb.bin out_remap.bin \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) interpolation_mode = atoi(args[4]);
    if (argc > 5) format = atoi(args[5]);
    if (argc > 6) input_width = atoi(args[6]);
    if (argc > 7) input_height = atoi(args[7]);
    if (argc > 8) output_width = atoi(args[8]);
    if (argc > 9) output_height = atoi(args[9]);
    if (argc > 10) mapx_path = args[10];
    if (argc > 11) mapy_path = args[11];
    if (argc > 12) input_path = args[12];
    if (argc > 13) output_path = args[13];

    printf("thread_num = %d\n", thread_num);
    printf("loop_num   = %d\n", loop);
    // test for multi-thread
    pthread_t pid[thread_num];
    remap_thread_arg_t remap_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        remap_thread_arg[i].loop_num = loop;
        remap_thread_arg[i].use_real_img = use_real_img;
        remap_thread_arg[i].input_height = input_height;
        remap_thread_arg[i].input_width = input_width;
        remap_thread_arg[i].output_height = output_height;
        remap_thread_arg[i].output_width = output_width;
        remap_thread_arg[i].format = format;
        remap_thread_arg[i].interpolation_mode = interpolation_mode;
        remap_thread_arg[i].mapx_data_path = mapx_path;
        remap_thread_arg[i].mapy_data_path = mapy_path;
        remap_thread_arg[i].input_path = input_path;
        remap_thread_arg[i].output_path = output_path;
        remap_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_remap, remap_thread_arg + i) != 0) {
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
