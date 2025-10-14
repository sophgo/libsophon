#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include "bmcv_api_ext.h"

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

static void fill(
        unsigned char* input,
        int channel,
        int width,
        int height) {
    for (int i = 0; i < channel; i++) {
        for (int j = 0; j < height; j++) {
            for(int k = 0; k < width; k++) {
                unsigned char num =  rand() % 256;
                input[i * width * height + j * width + k] = num;
            }
        }
    }
}

static int rotate_cpu(
        unsigned char* input,
        unsigned char* output,
        int width,
        int height,
        int format,
        int rotation_angle,
        bm_image input_img,
        bm_handle_t handle) {
    switch (format) {
        case FORMAT_GRAY:
            switch (rotation_angle) {
                case 90:
                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            int newIndex = x * height + (height - y - 1);
                            int oldIndex = y * width + x;
                            output[newIndex] = input[oldIndex];
                        }
                    }
                    break;
                case 180:
                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            int newIndex = (height - y - 1) * width + (width - x - 1);
                            int oldIndex = y * width + x;
                            output[newIndex] = input[oldIndex];
                        }
                    }
                    break;
                case 270:
                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            int newIndex = height * (width - x - 1) + y;
                            int oldIndex = x + y * width;
                            output[newIndex] = input[oldIndex];
                        }
                    }
                    break;
            }

            break;
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_YUV444P:
        case FORMAT_NV12:
        case FORMAT_NV21:
            switch (rotation_angle) {
                case 90:
                    for(int channel = 0; channel < 3; channel++) {
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                int newX = height - y - 1;
                                int newY = x;
                                output[channel * width * height + newY * height + newX] = input[channel * width * height + y * width + x];
                            }
                        }
                    }
                    break;
                case 180:
                    for(int channel = 0; channel < 3; channel++) {
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                int newX = width - x - 1;
                                int newY = height - y - 1;
                                output[channel * width * height + newY * width + newX] = input[channel * width * height + y * width + x];
                            }
                        }
                    }
                    break;
                case 270:
                    for(int channel = 0; channel < 3; channel++) {
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                int newX = y;
                                int newY = width - x - 1;
                                output[channel * height * width + newY * height + newX] = input[channel * width * height + y * width + x];
                            }
                        }
                    }
                    break;
            }
            break;
        default:
            bm_image rgbp_img;
            bm_image_create(handle, height, width, FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &rgbp_img, NULL);
            bm_image_alloc_dev_mem(rgbp_img);
            int ret = bmcv_image_vpp_csc_matrix_convert(handle, 1, input_img, &rgbp_img, CSC_MAX_ENUM, NULL, BMCV_INTER_LINEAR, NULL);  //NV12->RGBP
            if(ret != BM_SUCCESS) {
                printf("cpu vpp_csc_matrix_convert error!\n");
                return -1;
            }
            unsigned char *input_addr[3] = {input, input + height * width, input + 2 * height * width};
            bm_image_copy_device_to_host(rgbp_img, (void **)input_addr);
            switch (rotation_angle) {
                case 90:
                    for(int channel = 0; channel < 3; channel++) {
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                int newX = height - y - 1;
                                int newY = x;
                                output[channel * width * height + newY * height + newX] = input[channel * width * height + y * width + x];
                            }
                        }
                    }
                    break;
                case 180:
                    for(int channel = 0; channel < 3; channel++) {
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                int newX = width - x - 1;
                                int newY = height - y - 1;
                                output[channel * width * height + newY * width + newX] = input[channel * width * height + y * width + x];
                            }
                        }
                    }
                    break;
                case 270:
                    for(int channel = 0; channel < 3; channel++) {
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                int newX = y;
                                int newY = width - x - 1;
                                output[channel * height * width + newY * height + newX] = input[channel * width * height + y * width + x];
                            }
                        }
                    }
                    break;
            }
            bm_image_destroy(rgbp_img);
            break;
    }
    return 0;
}

static int rotate_tpu(
        unsigned char* input,
        unsigned char* output,
        int width,
        int height,
        int format,
        int rotation_angle,
        bm_handle_t handle,
        long int *tpu_time) {
    bm_status_t ret;
    bm_image temp_in;
    struct timeval t1, t2;
    bm_image input_img, output_img, nv12_output_rgbp;
    if(rotation_angle == 180) {
        if(format == FORMAT_NV12 || format == FORMAT_NV21) {
            bm_image_create(handle, height, width, (bm_image_format_ext)FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &input_img, NULL);
            bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_create(handle, height, width, (bm_image_format_ext)FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &nv12_output_rgbp, NULL);
        } else{
            bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_img, NULL);
            bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
        }
    } else {
        if(format == FORMAT_NV12 || format == FORMAT_NV21) {
            bm_image_create(handle, height, width, (bm_image_format_ext)FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &input_img, NULL);
            bm_image_create(handle, width, height, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_create(handle, width, height, (bm_image_format_ext)FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &nv12_output_rgbp, NULL);
        } else{
            bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_img, NULL);
            bm_image_create(handle, width, height, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
        }
    }

    bm_image_alloc_dev_mem(input_img);
    bm_image_alloc_dev_mem(output_img);
    bm_image_alloc_dev_mem(nv12_output_rgbp);

    if (format == FORMAT_GRAY) {
        unsigned char *input_addr[1] = {input};
        bm_image_copy_host_to_device(input_img, (void **)(input_addr));
    } else {
        unsigned char *input_addr[3] = {input, input + height * width, input + 2 * height * width};
        bm_image_copy_host_to_device(input_img, (void **)(input_addr));
    }
    if (format == FORMAT_NV12 || format == FORMAT_NV21) {
        bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &temp_in, NULL);
        bmcv_image_vpp_csc_matrix_convert(handle, 1, input_img, &temp_in, CSC_MAX_ENUM, NULL, BMCV_INTER_LINEAR, NULL);  //RGBP->NV12
    }
    gettimeofday(&t1, NULL);
    if(format == FORMAT_NV12 || format == FORMAT_NV21) {
        ret = bmcv_image_rotate(handle, temp_in, output_img, rotation_angle);
    } else {
        ret = bmcv_image_rotate(handle, input_img, output_img, rotation_angle);
    }
    gettimeofday(&t2, NULL);
    printf("Rotate TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    *tpu_time += TIME_COST_US(t1, t2);
    if (ret != BM_SUCCESS) {
        printf("bmcv_image_rotate error!");
        bm_image_destroy(input_img);
        bm_image_destroy(output_img);
        bm_image_destroy(nv12_output_rgbp);
        bm_dev_free(handle);
        return -1;
    }
    if (format == FORMAT_NV12 || format == FORMAT_NV21) {
        bmcv_image_vpp_csc_matrix_convert(handle, 1, output_img, &nv12_output_rgbp, CSC_MAX_ENUM, NULL, BMCV_INTER_LINEAR, NULL);
    }

    if (format == FORMAT_GRAY) {
        unsigned char *output_addr[1] = {output};
        bm_image_copy_device_to_host(output_img, (void **)output_addr);
    } else if (format == FORMAT_NV12 || format == FORMAT_NV21) {
        unsigned char *output_addr[3] = {output, output + height * width, output + 2 * height * width};
        bm_image_copy_device_to_host(nv12_output_rgbp, (void **)output_addr);
    } else {
        unsigned char *output_addr[3] = {output, output + height * width, output + 2 * height * width};
        bm_image_copy_device_to_host(output_img, (void **)output_addr);
    }
    bm_image_destroy(input_img);
    bm_image_destroy(output_img);
    bm_image_destroy(nv12_output_rgbp);
    return 0;
}

static int cmp_rotate(
    unsigned char *got,
    unsigned char *exp,
    int len,
    int format) {
    if (format == FORMAT_NV12 || format == FORMAT_NV21) {
        for (int i = 0; i < len; i++) {
            if (abs(got[i] - exp[i]) > 255) {
                printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
                return -1;
            }
        }
    } else {
        for (int i = 0; i < len; i++) {
            if (got[i] != exp[i]) {
                printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
                return -1;
            }
        }
    }
    return 0;
}

static int test_rotate_random(
        int width,
        int height,
        int format,
        int rotation_angle,
        bm_handle_t handle,
        long int *cpu_time,
        long int *tpu_time) {
    int ret;
    struct timeval t1, t2;
    unsigned char* input_data;
    unsigned char* output_cpu;
    unsigned char* output_tpu;
    bm_image input_img;
    if(format == FORMAT_GRAY){
        input_data = (unsigned char*)malloc(width * height * sizeof(unsigned char));
        output_cpu = (unsigned char*)malloc(width * height * sizeof(unsigned char));
        output_tpu = (unsigned char*)malloc(width * height * sizeof(unsigned char));
    } else {
        input_data = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
        output_cpu = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
        output_tpu = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
    }

    if(format == FORMAT_GRAY){
        fill(input_data, 1, width, height);
    } else if (format == FORMAT_YUV444P ||
               format == FORMAT_NV12 ||
               format == FORMAT_NV21 ||
               format == FORMAT_RGB_PLANAR ||
               format == FORMAT_BGR_PLANAR ||
               format == FORMAT_RGBP_SEPARATE ||
               format == FORMAT_BGRP_SEPARATE) {
        fill(input_data, 3, width, height);
    } else {
        printf("not support input format random test!\n");
        free(input_data);
        free(output_cpu);
        free(output_tpu);
        bm_image_destroy(input_img);
        return -1;
    }

    gettimeofday(&t1, NULL);
    ret = rotate_cpu(input_data, output_cpu, width, height, format, rotation_angle, input_img, handle);
    gettimeofday(&t2, NULL);
    printf("Rotate CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    *cpu_time += TIME_COST_US(t1, t2);
    if(ret != 0){
        free(input_data);
        free(output_cpu);
        free(output_tpu);
        bm_image_destroy(input_img);
        return ret;
    }
    ret = rotate_tpu(input_data, output_tpu, width, height, format, rotation_angle, handle, tpu_time);

    if(ret != 0){
        free(input_data);
        free(output_cpu);
        free(output_tpu);
        bm_image_destroy(input_img);
        return ret;
    }
    if(format == FORMAT_GRAY){
        ret = cmp_rotate(output_tpu, output_cpu, width * height, format);
    } else {
        ret = cmp_rotate(output_tpu, output_cpu, width * height * 3, format);
    }

    free(input_data);
    free(output_cpu);
    free(output_tpu);
    bm_image_destroy(input_img);
    return ret;
}

int main() {
    printf("log will be saved in rotate_performance.txt\n");
    FILE *original_stdout = stdout;
    FILE *file = freopen("rotate_performance.txt", "w", stdout);
    if (file == NULL) {
        fprintf(stderr,"无法打开文件\n");
        return 1;
    }
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    int special_height[10] = {8, 400, 600, 720, 1080, 1440, 2160, 4096, 6000, 8192};
    int special_width[10] = {8, 400, 800, 1280, 1920, 2560, 3840, 4096, 6000, 8192};
    int format_num[] = {FORMAT_YUV444P, FORMAT_NV12, FORMAT_NV21, FORMAT_RGB_PLANAR, FORMAT_BGR_PLANAR,
                        FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE, FORMAT_GRAY};
    int rotation_angle_num[] = {90, 180, 270};

    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    long int cputime1 = 0;
    long int tputime1 = 0;
    long int *cpu_time1 = &cputime1;
    long int *tpu_time1 = &tputime1;

    printf("width: 8 , height: 8, format = 2, rotation_angle: 90\n");
    test_rotate_random(8, 8, 2, 90, handle, cpu_time1, tpu_time1);  //Excluding the time to load the dynamic operator library

    for(int k = 0; k < 8; k++){
        int format = format_num[k];
        for(int i = 0; i < 10; i++) {
            int width = special_width[i];
            int height = special_height[i];
            for(int j = 0; j < 3; j++) {
                int rotation_angle = rotation_angle_num[j];
                printf("width: %d , height: %d, format = %d, rotation_angle: %d\n", width, height, format, rotation_angle);
                long int cputime = 0;
                long int tputime = 0;
                long int *cpu_time = &cputime;
                long int *tpu_time = &tputime;
                for(int loop = 0; loop < 3; loop++) {
                    if (0 != test_rotate_random(width, height, format, rotation_angle, handle, cpu_time, tpu_time)){
                        printf("------TEST CV_ROTATE FAILED------\n");
                        exit(-1);
                    }
                }
                printf("------average CPU time = %ldus------\n", cputime / 3);
                printf("------average TPU time = %ldus------\n", tputime / 3);
            }

        }
    }
    bm_dev_free(handle);
    fclose(stdout);
    stdout = original_stdout;
    return ret;
}
