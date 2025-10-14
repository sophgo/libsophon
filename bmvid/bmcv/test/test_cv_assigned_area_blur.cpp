#include <cerrno>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include "bmcv_api_ext_c.h"
#define ASSIGNED_AREA_MAX_NUM 200
#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

typedef struct {
    int loop_num;
    int use_real_img;
    int height;
    int width;
    int format;
    int ksize;
    int start_x[ASSIGNED_AREA_MAX_NUM];
    int start_y[ASSIGNED_AREA_MAX_NUM];
    int assigned_width[ASSIGNED_AREA_MAX_NUM];
    int assigned_height[ASSIGNED_AREA_MAX_NUM];
    int assigned_area_num;
    float center_weight_scale;
    const char* input_path;
    const char* output_path;
    bm_handle_t handle;
} assigned_area_blur_thread_arg_t;

static int read_bin_yuv420p(const char *input_path, unsigned char *input_data, int width, int height, float channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("无法打开输入文件 %s: %s\n", input_path, strerror(errno));
        return -1;
    }
    if(fread(input_data, sizeof(char), width * height * channel, fp_src) != 0) {
        printf("read image success\n");
    }
    fclose(fp_src);
    return 0;
}

static int read_bin(const char *input_path, unsigned char *input_data, int width, int height, int channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("无法打开输入文件 %s: %s\n", input_path, strerror(errno));
        return -1;
    }
    if(fread(input_data, sizeof(char), width * height * channel, fp_src) != 0) {
        printf("read image success\n");
    }
    fclose(fp_src);
    return 0;
}

static int write_bin_yuv420p(const char *output_path, unsigned char *output_data, int width, int height, float channel) {
    FILE *fp_dst = fopen(output_path, "wb");
    if (fp_dst == NULL) {
        printf("无法打开输出文件 %s\n", output_path);
        return -1;
    }
    fwrite(output_data, sizeof(char), width * height * channel, fp_dst);
    fclose(fp_dst);
    return 0;
}

static int write_bin(const char *output_path, unsigned char *output_data, int width, int height, int channel) {
    FILE *fp_dst = fopen(output_path, "wb");
    if (fp_dst == NULL) {
        printf("无法打开输出文件 %s\n", output_path);
        return -1;
    }
    fwrite(output_data, sizeof(char), width * height * channel, fp_dst);
    fclose(fp_dst);
    return 0;
}

static void create_kernel(float* kernel, int ksize, float center_weight_scale) {
    int half = ksize / 2;
    float max_distance = sqrt(2) * half;
    float weight_sum = 0.0f;

    for (int i = 0; i < ksize; i++) {
        for (int j = 0; j < ksize; j++) {
            int x = i - half;
            int y = j - half;
            float distance = sqrt((float)(x * x + y * y));

            float weight = distance / max_distance;

            if (distance <= half * 0.5f) {
                weight *= center_weight_scale;
            }

            kernel[i * ksize + j] = weight;
            weight_sum += weight;
        }
    }
    for (int i = 0; i < ksize * ksize; i++) {
        kernel[i] /= weight_sum;
    }
}

static int assigned_area_blur_tpu(
    unsigned char *input,
    unsigned char *output,
    int format,
    int base_height,
    int base_width,
    int ksize,
    int assigned_area_num,
    int *start_x,
    int *start_y,
    int *assigned_width,
    int *assigned_height,
    float center_weight_scale,
    bm_handle_t handle) {
    bm_status_t ret;
    bm_image img_i, img_o;
    struct timeval t1, t2;
    bmcv_rect assigned_area[assigned_area_num];
    for (int i = 0; i < assigned_area_num; i++) {
        assigned_area[i].start_x = start_x[i];
        assigned_area[i].start_y = start_y[i];
        assigned_area[i].crop_w = assigned_width[i];
        assigned_area[i].crop_h = assigned_height[i];
    }
    if(format == FORMAT_YUV420P) {
        bm_image_create(handle, base_height, base_width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img_i, NULL);
        bm_image_create(handle, base_height, base_width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img_o, NULL);
    } else {
        bm_image_create(handle, base_height, base_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_i, NULL);
        bm_image_create(handle, base_height, base_width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_o, NULL);
    }
    bm_image_alloc_dev_mem(img_i, 2);
    bm_image_alloc_dev_mem(img_o, 2);

    if (format == FORMAT_GRAY || format == FORMAT_YUV420P) {
        unsigned char *input_addr[1] = {input};
        bm_image_copy_host_to_device(img_i, (void **)(input_addr));
        bm_image_copy_host_to_device(img_o, (void **)(input_addr));
    } else {
        unsigned char *input_addr[3] = {input, input + base_height * base_width, input + 2 * base_height * base_width};
        bm_image_copy_host_to_device(img_i, (void **)(input_addr));
        bm_image_copy_host_to_device(img_o, (void **)(input_addr));
    }
    gettimeofday(&t1, NULL);
    ret = bmcv_image_assigned_area_blur(handle, img_i, img_o, ksize, assigned_area_num, center_weight_scale, assigned_area);
    if (ret != BM_SUCCESS) {
        printf("bmcv_image_assigned_area_blur error!");
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        return -1;
    }
    gettimeofday(&t2, NULL);
    printf("assigned_area_blur TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (format == FORMAT_YUV420P) {
        memcpy(output, input, base_height * base_width * 3 / 2);
    }
    if (format == FORMAT_GRAY || format == FORMAT_YUV420P) {
        unsigned char *output_addr[1] = {output};
        bm_image_copy_device_to_host(img_o, (void **)output_addr);
    } else {
        unsigned char *output_addr[3] = {output, output + base_height * base_width, output + 2 * base_height * base_width};
        bm_image_copy_device_to_host(img_o, (void **)output_addr);
    }
    bm_image_destroy(img_i);
    bm_image_destroy(img_o);
    return 0;
}
static void fill_yuv420(unsigned char *input, int width, int height) {
    int num = 0;
    for (int i = 0; i < width * height * 1.5; i++){
        input[i] = num % 256;
        num++;
    }
}

static void fill(unsigned char *input, int width, int height, int channel) {
    for (int i = 0; i < channel; i++) {
        int num = 10;
        for (int j = 0; j < height; j++) {
            for (int k = 0; k < width; k++) {
                input[i * width * height + j * width + k] = num % 128;
                num++;
            }
        }
    }
}

static int assigned_area_blur_cpu(
    unsigned char *input,
    unsigned char *output,
    int channel,
    int base_height,
    int base_width,
    int ksize,
    int assigned_area_num,
    int *start_x,
    int *start_y,
    int *assigned_width,
    int *assigned_height,
    float center_weight_scale,
    int format) {
    if (!input || !output || !start_x || !start_y || !assigned_width || !assigned_height) {
        return -1;
    }
    if (ksize <= 0 || ksize % 2 == 0) {
        return -2;
    }
    float *kernel = (float *)malloc(ksize * ksize * sizeof(float));
    if (!kernel) {
        return -3;
    }
    create_kernel(kernel, ksize, center_weight_scale);
    int kernel_half = ksize / 2;
    int channel_offset[3];
    for (int c = 0; c < channel; c++) {
        channel_offset[c] = c * base_width * base_height;
    }
    memcpy(output, input, base_height * base_width * channel * sizeof(unsigned char));
    if(format == FORMAT_YUV420P) {
        // only process y channel
        unsigned char *plane_y = input;
        unsigned char *output_y = output;
        for (int area = 0; area < assigned_area_num; area++) {
            int sx = start_x[area];
            int sy = start_y[area];
            int aw = assigned_width[area];
            int ah = assigned_height[area];
            for (int y = sy; y < sy + ah; y++) {
                for (int x = sx; x < sx + aw; x++) {
                    if (x < 0 || x >= base_width || y < 0 || y >= base_height)
                        continue;
                    float scaled_value = 0.0f;
                    for (int ky = -kernel_half; ky <= kernel_half; ky++) {
                        for (int kx = -kernel_half; kx <= kernel_half; kx++) {
                            int nx = x + kx;
                            int ny = y + ky;
                            if (nx >= 0 && nx < base_width && ny >= 0 && ny < base_height) {
                                float weight_val = kernel[(ky + kernel_half) * ksize + (kx + kernel_half)];
                                scaled_value += plane_y[ny * base_width + nx] * weight_val;
                            }
                        }
                    }
                    output_y[y * base_width + x] = (unsigned char)scaled_value;
                }
            }
        }
    } else {
        for (int area = 0; area < assigned_area_num; area++) {
            int sx = start_x[area];
            int sy = start_y[area];
            int aw = assigned_width[area];
            int ah = assigned_height[area];
            for (int y = sy; y < sy + ah; y++) {
                for (int x = sx; x < sx + aw; x++) {
                    if (x < 0 || x >= base_width || y < 0 || y >= base_height)
                        continue;
                    for (int c = 0; c < channel; c++) {
                        float scaled_value = 0.0f;
                        for (int ky = -kernel_half; ky <= kernel_half; ky++) {
                            for (int kx = -kernel_half; kx <= kernel_half; kx++) {
                                int nx = x + kx;
                                int ny = y + ky;
                                if (nx > 0 && nx < base_width && ny >= 0 && ny < base_height) {
                                    float weight_val = kernel[(ky + kernel_half) * ksize + (kx + kernel_half)];
                                    scaled_value += input[channel_offset[c] + ny * base_width + nx] * weight_val;
                                }
                            }
                        }
                        output[channel_offset[c] + y * base_width + x] = (unsigned char)(scaled_value);
                    }
                }
            }
        }
    }
    free(kernel);
    return 0;
}

static int cmp(unsigned char *got, unsigned char *exp, int width, int height, int ksize, int channel) {
    for(int c = 0; c < channel; c++) {
        int channel_offset = width * height * c;
        for(int i = 0; i < height; i++) {
            for(int j = 0; j < width; j++) {
                if((i > ksize / 2) && (i < height - ksize / 2) && (j > ksize / 2) && (j < width - ksize / 2)){
                    if (abs(got[channel_offset + i * width + j] - exp[channel_offset + i * width + j]) > 1) {
                        for (int j = 0; j < 5; j++)
                            printf("cmp error: idx=%d  exp=%d  got=%d, \n", channel_offset + i + j, exp[channel_offset + i + j], got[channel_offset + i + j]);
                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

static int test_assigned_area_blur_random(int use_real_img, int height, int width, int format, int ksize, int assigned_area_num,
                                          int *start_x, int *start_y, int *assigned_width, int *assigned_height, float center_weight_scale,
                                          const char *input_path, const char *output_path, bm_handle_t handle) {
    struct timeval t1, t2;
    printf("use_real_img        = %d\n", use_real_img);
    printf("width               = %d\n", width);
    printf("height              = %d\n", height);
    printf("format              = %d\n", format);
    printf("ksize               = %d\n", ksize);
    printf("assigned_area_num   = %d\n", assigned_area_num);
    printf("center_weight_scale = %f\n", center_weight_scale);
    for(int i = 0; i < assigned_area_num; i++) {
        printf("object%d: start_x:%d, start_y:%d, area_w:%d, area_h:%d", i+1, start_x[i], start_y[i], assigned_width[i], assigned_height[i]);
        printf("\n");
    }
    int channel = format == 14 ? 1 : 3;
    int ret;
    unsigned char *input_data = (unsigned char*)malloc(width * height * channel);
    unsigned char *output_tpu = (unsigned char*)malloc(width * height * channel);
    unsigned char *output_cpu = (unsigned char*)malloc(width * height * channel);
    if (use_real_img) {
        if(format == FORMAT_YUV420P) {
            read_bin_yuv420p(input_path, input_data, width, height, 1.5);
        } else {
            ret = read_bin(input_path, input_data, width, height, channel);
            if (ret != 0) {
                printf("read image failed!\n");
                free(input_data);
                free(output_cpu);
                free(output_tpu);
                return -1;
            }
        }
    } else {
        if(format == FORMAT_YUV420P) {
            fill_yuv420(input_data, width, height);
        } else {
            fill(input_data, width, height, channel);
        }
    }
    gettimeofday(&t1, NULL);
    ret = assigned_area_blur_cpu(input_data, output_cpu, channel, height, width, ksize, assigned_area_num, start_x, start_y, assigned_width, assigned_height, center_weight_scale, format);
    if(ret != 0) {
        printf("assigned_area_blur_cpu failed\n");
    }

    gettimeofday(&t2, NULL);
    printf("assigned_area_blur CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if(0 != assigned_area_blur_tpu(input_data, output_tpu, format, height, width, ksize, assigned_area_num, start_x,
                                   start_y, assigned_width, assigned_height, center_weight_scale, handle)){
        free(input_data);
        free(output_tpu);
        free(output_cpu);
        return -1;
    }
    if(format == FORMAT_YUV420P) {
        channel = 1;
    }
    ret = cmp(output_tpu, output_cpu, width, height, ksize, channel);
    if (ret == 0) {
        printf("Compare TPU result with CPU result successfully!\n");
        if (use_real_img == 1) {
            if(format == FORMAT_YUV420P) {
                ret = write_bin_yuv420p(output_path, output_tpu, width, height, 1.5);
            } else {
                ret = write_bin(output_path, output_tpu, width, height, channel);
            }
            if (ret != 0) {
                printf("write image failed!\n");
                free(input_data);
                free(output_tpu);
                free(output_cpu);
                return -1;
            }
        }
    } else {
        printf("cpu and tpu failed to compare \n");
    }
    free(input_data);
    free(output_tpu);
    free(output_cpu);
    return ret;
}

void* test_assigned_area_blur(void* args) {
    assigned_area_blur_thread_arg_t* thread_args = (assigned_area_blur_thread_arg_t*)args;
    int loop_num = thread_args->loop_num;
    int use_real_img = thread_args->use_real_img;
    int height = thread_args->height;
    int width = thread_args->width;
    int format = thread_args->format;
    int ksize = thread_args->ksize;
    int assigned_area_num = thread_args->assigned_area_num;
    int *start_x = thread_args->start_x;
    int *start_y = thread_args->start_y;
    int *assigned_width = thread_args->assigned_width;
    int *assigned_height = thread_args->assigned_height;
    float center_weight_scale = thread_args->center_weight_scale;
    const char* input_path = thread_args->input_path;
    const char* output_path = thread_args->output_path;
    bm_handle_t handle = thread_args->handle;
    int dis_x_max[ASSIGNED_AREA_MAX_NUM];
    int dis_y_max[ASSIGNED_AREA_MAX_NUM];
    for (int i = 0; i < loop_num; i++) {
        if (loop_num > 1 && use_real_img == 0) {
            width = 8 + rand() % 4088;
            height = 8 + rand() % 4088;
            ksize = 3 + 2 * (rand() % 5);
            int format_num[] = {FORMAT_YUV420P, FORMAT_YUV444P, FORMAT_RGB_PLANAR, FORMAT_BGR_PLANAR, FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
            int size = sizeof(format_num) / sizeof(format_num[0]);
            int rand_num = rand() % size;
            format = format_num[rand_num];
            assigned_area_num = 1 + rand() % 10;
            for (int i = 0; i < assigned_area_num; i++) {
                int w = 1 + rand() % (500);
                int h = 1 + rand() % (500);
                assigned_width[i] = w > width ? width : w;
                assigned_height[i] = h > height ? height : h;
                dis_x_max[i] = width - assigned_width[i];
                dis_y_max[i] = height - assigned_height[i];
                start_x[i] = rand() % (dis_x_max[i] + 1);
                start_y[i] = rand() % (dis_y_max[i] + 1);
            }
        }
        if (0 != test_assigned_area_blur_random(use_real_img, height, width, format, ksize, assigned_area_num, start_x, start_y,
                                                assigned_width, assigned_height, center_weight_scale, input_path, output_path, handle)) {
            printf("------TEST assigned_area_blur FAILED------\n");
            exit(-1);
        }
        printf("------TEST assigned_area_blur PASSED!------\n");
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
    int base_width = 8 + rand() % 4088;
    int base_height = 8 + rand() % 4088;
    float center_weight_scale = 0.01;
    int format_num[] = {FORMAT_YUV420P, FORMAT_YUV444P, FORMAT_RGB_PLANAR, FORMAT_BGR_PLANAR, FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
    int size = sizeof(format_num) / sizeof(format_num[0]);
    int rand_num = rand() % size;
    int format = format_num[rand_num];
    int ksize = 3 + 2 * (rand() % 5);
    int assigned_area_num = 1 + rand() % 5;
    int start_x[ASSIGNED_AREA_MAX_NUM];
    int start_y[ASSIGNED_AREA_MAX_NUM];
    int dis_x_max[ASSIGNED_AREA_MAX_NUM];
    int dis_y_max[ASSIGNED_AREA_MAX_NUM];
    int assigned_width[ASSIGNED_AREA_MAX_NUM];
    int assigned_height[ASSIGNED_AREA_MAX_NUM];
    int ret = 0;
    const char *input_path = NULL;
    const char *output_path = NULL;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return -1;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: \n");
        printf("%s thread_num loop use_real_img width height format ksize center_weight_scale assigned_area_num start_x[assigned_area_num] start_y[assigned_area_num] assigned_width[assigned_area_num] assigned_height[assigned_area_num] input_path output_path(when use_real_img = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 1 1 0 640 436 12 5 0.01 1 273 203 46 87 \n", args[0]);
        return 0;
    }
    for (int i = 0; i < assigned_area_num; i++) {
        int w = 1 + rand() % (500);
        int h = 1 + rand() % (500);
        assigned_width[i] = w > base_width ? base_width : w;
        assigned_height[i] = h > base_height ? base_height : h;
        dis_x_max[i] = base_width - assigned_width[i];
        dis_y_max[i] = base_height - assigned_height[i];
        start_x[i] = rand() % (dis_x_max[i] + 1);
        start_y[i] = rand() % (dis_y_max[i] + 1);
    }
    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) base_width = atoi(args[4]);
    if (argc > 5) base_height = atoi(args[5]);
    if (argc > 6) format = atoi(args[6]);
    if (argc > 7) ksize = atoi(args[7]);
    if (argc > 8) center_weight_scale = atof(args[8]);
    if (argc > 9) assigned_area_num = atoi(args[9]);
    int start_x_idx = 10;
    int start_y_idx = start_x_idx + assigned_area_num;
    int assigned_width_idx = start_y_idx + assigned_area_num;
    int assigned_height_idx = assigned_width_idx + assigned_area_num;
    for (int i = 0; i < assigned_area_num; i++) {
        start_x[i] = argc > start_x_idx + i ? atoi(args[start_x_idx + i]) : start_x[i];
        start_y[i] = argc > start_y_idx + i ? atoi(args[start_y_idx + i]) : start_y[i];
        assigned_width[i] = argc > assigned_width_idx + i ? atoi(args[assigned_width_idx + i]) : assigned_width[i];
        assigned_height[i] = argc > assigned_height_idx + i ? atoi(args[assigned_height_idx + i]) : assigned_height[i];
    }
    int input_path_idx = assigned_height_idx + assigned_area_num;
    int output_path_idx = input_path_idx + 1;
    if (argc > input_path_idx) input_path = args[input_path_idx];
    if (argc > output_path_idx) output_path = args[output_path_idx];

    printf("thread_num = %d\n", thread_num);
    printf("loop_num   = %d\n", loop);
    // test for multi-thread
    pthread_t pid[thread_num];
    assigned_area_blur_thread_arg_t assigned_area_blur_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        assigned_area_blur_thread_arg[i].loop_num = loop;
        assigned_area_blur_thread_arg[i].use_real_img = use_real_img;
        assigned_area_blur_thread_arg[i].height = base_height;
        assigned_area_blur_thread_arg[i].width = base_width;
        assigned_area_blur_thread_arg[i].format = format;
        assigned_area_blur_thread_arg[i].ksize = ksize;
        assigned_area_blur_thread_arg[i].assigned_area_num = assigned_area_num;
        assigned_area_blur_thread_arg[i].center_weight_scale = center_weight_scale;
        assigned_area_blur_thread_arg[i].input_path = input_path;
        assigned_area_blur_thread_arg[i].output_path = output_path;
        assigned_area_blur_thread_arg[i].handle = handle;
        for (int j = 0; j < assigned_area_num; j++) {
            assigned_area_blur_thread_arg[i].start_x[j] = start_x[j];
            assigned_area_blur_thread_arg[i].start_y[j] = start_y[j];
            assigned_area_blur_thread_arg[i].assigned_width[j] = assigned_width[j];
            assigned_area_blur_thread_arg[i].assigned_height[j] = assigned_height[j];
        }
        if (pthread_create(pid + i, NULL, test_assigned_area_blur, assigned_area_blur_thread_arg + i) != 0) {
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
