#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include "bmcv_api_ext.h"
#include <sys/time.h>
#include <random>
#include <algorithm>
#include <vector>
#include <iostream>

#define OVERLAY_MAX 10  // assuming maximum overlay_num = 10
typedef struct {
    int loop_num;
    int use_real_img;
    int overlay_num;
    int src_w;
    int src_h;
    int start_x[10];
    int start_y[10];
    int overlay_w[10];
    int overlay_h[10];
    const char* input_path;
    const char* output_path_cpu;
    const char* output_path_tpu;
    char* overlay_path[10];
} cv_overlay_thread_arg_t;

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

static void fill(unsigned char* input, int width, int height, int flag)
{
    int pixelCount = width * height;

    switch(flag) {
        case 1: { /* Base: Fill with RGB packed */
            int count = 10;
            for(int i = 0; i < pixelCount * 3; i++) {
                input[i] = count;
                count++;
                if(count == 256)
                    count = 10;
            }
            break;
        }
        case 2: { /* Overlay: Fill with ABGR packed */
            int count = 10; // Start from 1 + overlay_num
            for(int i = 0; i < pixelCount * 4; i++) {
                if(i % 4 == 3) { // Alpha channel
                    input[i] = 255; // Fully opaque
                } else {
                    input[i] = count;
                    count++;
                    if(count == 256)
                        count = 10;
                }
            }
            break;
        }
        case 3: { /* Overlay: Fill with ARGB1555 */
            uint16_t* argb1555_data = (uint16_t*)input;
            for (int i = 0; i < pixelCount; i++) {
                argb1555_data[i] = (0x8000 | ((rand() % 32) << 10) | ((rand() % 32) << 5) | (rand() % 32));
            }
            break;
        }
        case 4: { /*Overlay: Fill with ARGB4444 packed */
            uint16_t* argb4444_data = (uint16_t*)input;
            // 组合成 16 位的 ARGB4444 值
            for (int i = 0; i < pixelCount; i++) {
                argb4444_data[i] = ((rand() % 16) << 12) | ((rand() % 16) << 8) | ((rand() % 16)<< 4) | (rand() % 16);
            }
            break;
        }
        default:
            std::cerr << "Invalid flag. Use 1 for base (RGB packed) or 2 for overlay (ABGR packed) or 3 for overlay (ARGB1555 packed) or 4 for overlay (ARGB4444 packed)." << std::endl;
            break;
    }
}

static int cmp(unsigned char *got, unsigned char *exp, int len)
{
    for (int i = 0; i < len; i++) {
        if(abs(got[i] - exp[i]) > 1) {
            printf("the index[%d]: tpu = %d, cpu = %d\n", i, got[i], exp[i]);
            return -1;
        }
    }
    return 0;
}

static void read_bin(const char *input_path, unsigned char *input_data, int width, int height, int format)
{
    int size = 0;
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("unable to open input file %s\n", input_path);
        return;
    }
    if (format == FORMAT_RGB_PACKED) {
        size = width * height * 3;
        if(fread(input_data, sizeof(unsigned char), size, fp_src) < (unsigned int)size) {
            printf("file size is less than %d required bytes\n", size);
        }
    } else if (format == FORMAT_ARGB1555_PACKED || format == FORMAT_ARGB4444_PACKED) {
        size = width * height * 2;
        if(fread(input_data, sizeof(unsigned char), size, fp_src) < (unsigned int)size) {
            printf("file size is less than %d required bytes\n", size);
        }
    } else if (format == FORMAT_ABGR_PACKED) {
        size = width * height * 4;
        if(fread(input_data, sizeof(unsigned char), size, fp_src) < (unsigned int)size) {
            printf("file size is less than %d required bytes\n", size);
        }
    }
    fclose(fp_src);
}

static void write_bin(const char *output_path, unsigned char *output_data, int width, int height)
{
    FILE *fp_dst = fopen(output_path, "wb");

    if (fp_dst == NULL) {
        printf("unable to open output file %s\n", output_path);
        return;
    }

    if(fwrite(output_data, sizeof(unsigned char), width * height * 3, fp_dst) != 0) {
        printf("write image success\n");
    }
    fclose(fp_dst);
}

void abgr_to_argb(uint32_t *abgr, uint32_t *rgba, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        uint32_t abgr_pixel = abgr[i];
        uint8_t a = (abgr_pixel >> 24) & 0xFF; // Alpha
        uint8_t b = (abgr_pixel >> 16) & 0xFF; // Blue
        uint8_t g = (abgr_pixel >> 8) & 0xFF; // Green
        uint8_t r = abgr_pixel & 0xFF; // Red

        rgba[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

void argb1555_to_rgba(uint16_t *argb1555, uint32_t *rgba, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        uint16_t argb_pixel = argb1555[i];

        uint8_t a = (argb_pixel & 0x8000) ? 0xFF : 0x00; // Alpha: 1 bit
        uint8_t r = (argb_pixel >> 10) & 0x1F; // Red: 5 bits
        uint8_t g = (argb_pixel >> 5) & 0x1F; // Green: 5 bits
        uint8_t b = argb_pixel & 0x1F; // Blue: 5 bits

        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);

        rgba[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

void argb4444_to_argb(uint16_t *argb4444, uint32_t *rgba, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        uint16_t argb_pixel = argb4444[i];

        uint8_t a = (argb_pixel >> 12) & 0x0F; // Alpha: 4 bits
        uint8_t r = (argb_pixel >> 8) & 0x0F; // Red: 4 bits
        uint8_t g = (argb_pixel >> 4) & 0x0F; // Green: 4 bits
        uint8_t b = argb_pixel & 0x0F; // Blue: 4 bits

        a = (a << 4) | a;
        r = (r << 4) | r;
        g = (g << 4) | g;
        b = (b << 4) | b;

        rgba[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

bm_status_t overlay_cpu(int overlay_num, int base_width, int base_height, unsigned char* output_cpu,
                        int* overlay_height, int* overlay_width, unsigned char** overlay_image,
                        int* pos_x, int* pos_y, int format)
{
    unsigned char* over_img = NULL;

    for (int i = 0; i < overlay_num; i++) {
        over_img = (unsigned char*)malloc(overlay_height[i] * overlay_width[i] * 4);
        if (format == FORMAT_ABGR_PACKED) {
            abgr_to_argb((uint32_t*)(overlay_image[i]), (uint32_t*)over_img, overlay_width[i], overlay_height[i]);
        } else if (format == FORMAT_ARGB1555_PACKED) {
            argb1555_to_rgba((uint16_t*)(overlay_image[i]), (uint32_t*)over_img, overlay_width[i], overlay_height[i]);
        } else if (format == FORMAT_ARGB4444_PACKED) {
            argb4444_to_argb((uint16_t*)(overlay_image[i]), (uint32_t*)over_img, overlay_width[i], overlay_height[i]);
        }

        for (int y = 0; y < overlay_height[i]; y++) {
            for (int x = 0; x < overlay_width[i]; x++) {
                int base_x = pos_x[i] + x;
                int base_y = pos_y[i] + y;
                if (base_x >= 0 && base_x < base_width && base_y >= 0 && base_y < base_height) {
                    int base_index = (base_y * base_width + base_x) * 3;
                    int overlay_index = (y * overlay_width[i] + x) * 4;
                    float alpha = over_img[overlay_index + 3] / 255.0f;

                    output_cpu[base_index] = (unsigned char)((1 - alpha) * output_cpu[base_index] + alpha * over_img[overlay_index]); //b
                    output_cpu[base_index + 1] = (unsigned char)((1 - alpha) * output_cpu[base_index + 1] + alpha * over_img[overlay_index + 1]); //g
                    output_cpu[base_index + 2] = (unsigned char)((1 - alpha) * output_cpu[base_index + 2] + alpha * over_img[overlay_index + 2]); //r
                } else {
                    printf("Overlay image out of bounds!\n");
                    free(over_img);
                    return BM_ERR_FAILURE;
                }
            }
        }
        free(over_img);
    }

    return BM_SUCCESS;
}

bm_status_t overlay_tpu(bm_handle_t handle, int overlay_num, int base_width, int base_height,
                        unsigned char* output_tpu, int* overlay_height, int* overlay_width,
                        unsigned char** overlay_image, int* pos_x, int* pos_y, int format)
{
    bm_image input_base_img;
    bm_image input_overlay_img[overlay_num];
    struct timeval t1, t2;

    for (int i = 0; i < overlay_num; i++) {
        bm_image_create(handle, overlay_height[i], overlay_width[i], (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, input_overlay_img + i, NULL);
    }

    for (int i = 0; i < overlay_num; i++) {
        bm_image_alloc_dev_mem(input_overlay_img[i], 2);
    }
    unsigned char** in_overlay_ptr[overlay_num];
    for (int i = 0; i < overlay_num; i++) {
        in_overlay_ptr[i] = new unsigned char*[1];
        in_overlay_ptr[i][0] = overlay_image[i];
    }
    for (int i = 0; i < overlay_num; i++) {
        bm_image_copy_host_to_device(input_overlay_img[i], (void **)in_overlay_ptr[i]);
    }

    bm_image_create(handle, base_height, base_width, (bm_image_format_ext)FORMAT_RGB_PACKED, DATA_TYPE_EXT_1N_BYTE, &input_base_img, NULL);
    bm_image_alloc_dev_mem(input_base_img, 2);
    unsigned char* in_base_ptr[1] = {output_tpu};
    bm_image_copy_host_to_device(input_base_img, (void **)in_base_ptr);

    bmcv_rect rect_array[overlay_num];
    for (int i = 0; i < overlay_num; i++) {
        rect_array[i].start_x = pos_x[i];
        rect_array[i].start_y = pos_y[i];
        rect_array[i].crop_w = overlay_width[i];
        rect_array[i].crop_h = overlay_height[i];
    }

    gettimeofday(&t1, NULL);
    bmcv_image_overlay(handle, input_base_img, overlay_num, rect_array, input_overlay_img);
    gettimeofday(&t2, NULL);
    printf("Overlay TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    unsigned char* out_ptr[1] = {output_tpu};
    bm_image_copy_device_to_host(input_base_img, (void **)out_ptr);
    bm_image_destroy(input_base_img);

    for (int i = 0; i < overlay_num; i++) {
        bm_image_destroy(input_overlay_img[i]);
    }
    return BM_SUCCESS;
}

static void* test_image_overlay(void* args) {
    cv_overlay_thread_arg_t* thread_args = (cv_overlay_thread_arg_t*)args;
    int loop_num = thread_args->loop_num;
    int use_real_img = thread_args->use_real_img;
    int overlay_num = thread_args->overlay_num;
    const char* base_image_path = thread_args->input_path;
    int base_width = thread_args->src_w;
    int base_height = thread_args->src_h;
    int* overlay_w = thread_args->overlay_w;
    int* overlay_h = thread_args->overlay_h;
    char** overlay_image_path = thread_args->overlay_path;
    const char* output_image_cpu_path = thread_args->output_path_cpu;
    const char* output_image_tpu_path = thread_args->output_path_tpu;
    int* pos_x = thread_args->start_x;
    int* pos_y = thread_args->start_y;
    unsigned char* base_image = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
    unsigned char* output_cpu = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
    unsigned char* output_tpu = (unsigned char*)malloc(base_width * base_height * 3 * sizeof(unsigned char));
    unsigned char* overlay_image[overlay_num];
    int overlay_width[overlay_num], overlay_height[overlay_num];
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;
    int res = 0;
    int format = FORMAT_ARGB1555_PACKED; /*FORMAT_ARGB1555_PACKED*/ /*FORMAT_ARGB4444_PACKED / FORMAT_ABGR_PACKED */

    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return (void*)-1;
    }

    printf("base width=%d, base height=%d, overlay width=%d, overlay height=%d, pox_x=%d, pox_y=%d\n",
            base_width, base_height, overlay_w[0], overlay_h[0], pos_x[0], pos_y[0]);

    if (use_real_img == 1) {
        read_bin(base_image_path, base_image, base_width, base_height, FORMAT_RGB_PACKED);
        for (int i = 0; i < overlay_num; i++) {
            overlay_width[i] = overlay_w[i];
            overlay_height[i] = overlay_h[i];
            if (format == FORMAT_ARGB1555_PACKED || format == FORMAT_ARGB4444_PACKED) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 2 * sizeof(unsigned char));
            } else if (format == FORMAT_ABGR_PACKED) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 4 * sizeof(unsigned char));
            }
            read_bin(overlay_image_path[i], overlay_image[i], overlay_width[i], overlay_height[i], format);
        }
    } else {
        fill(base_image, base_width, base_height, 1); // Fill base image
        for (int i = 0; i < overlay_num; i++) {
            overlay_width[i] = overlay_w[i];
            overlay_height[i] = overlay_h[i];
            if (format == FORMAT_ARGB1555_PACKED) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 2 * sizeof(unsigned char));
                fill(overlay_image[i], overlay_width[i], overlay_height[i], 3); // Fill overlay images
            } else if (format == FORMAT_ARGB4444_PACKED) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 2 * sizeof(unsigned char));
                fill(overlay_image[i], overlay_width[i], overlay_height[i], 4); // Fill overlay images
            } else if (format == FORMAT_ABGR_PACKED) {
                overlay_image[i] = (unsigned char*)malloc(overlay_width[i] * overlay_height[i] * 4 * sizeof(unsigned char));
                fill(overlay_image[i], overlay_width[i], overlay_height[i], 2); // Fill overlay images
            }
        }
    }
    memcpy(output_cpu, base_image, base_width * base_height * 3);
    memcpy(output_tpu, base_image, base_width * base_height * 3);

    for (int loop = 0; loop < loop_num; loop ++) {
        gettimeofday(&t1, NULL);
        ret = overlay_cpu(overlay_num, base_width, base_height, output_cpu, overlay_height, overlay_width,
                            overlay_image, pos_x, pos_y, format);
        if (BM_SUCCESS != ret) {
            printf("overlay_cpu failed!");
            goto free_mem;
        }
        gettimeofday(&t2, NULL);
        printf("Overlay CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

        ret = overlay_tpu(handle, overlay_num, base_width, base_height, output_tpu, overlay_height, overlay_width,
                        overlay_image, pos_x, pos_y, format);
        if (BM_SUCCESS != ret) {
            printf("overlay_tpu failed!");
            goto free_mem;
        }
    }

    res = cmp(output_tpu, output_cpu, base_width * base_height * 3);
    if (res != 0) {
        printf("cpu and tpu failed to compare!\n");
        goto free_mem;
    }

    if (output_image_cpu_path != NULL) {
        write_bin(output_image_cpu_path, output_cpu, base_width, base_height);
    }
    if (output_image_cpu_path != NULL) {
        write_bin(output_image_tpu_path, output_tpu, base_width, base_height);
    }

free_mem:
    free(base_image);
    free(output_cpu);
    free(output_tpu);
    for (int i = 0; i < overlay_num; i++) {
        free(overlay_image[i]);
    }
    bm_dev_free(handle);
    return nullptr;
}


int main(int argc, char *args[]) {

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis_base_w(8, 3840);
    std::uniform_int_distribution<> dis_base_h(8, 2160);
    int thread_num = 1;
    int loop = 1;
    int use_real_img = 0;
    int overlay_num = 1;
    int base_width = dis_base_w(gen);
    int base_height = dis_base_h(gen);
    int pos_x[OVERLAY_MAX];
    int pos_y[OVERLAY_MAX];
    int overlay_width[OVERLAY_MAX];
    int overlay_height[OVERLAY_MAX];
    int dis_x_max[OVERLAY_MAX];
    int dis_y_max[OVERLAY_MAX];

    char* overlay_image_path[OVERLAY_MAX];
    char* oip = NULL;

    for (int i = 0; i < overlay_num; i++) {
        int w = rand() % (base_width);
        overlay_width[i] = w > base_width ? base_width : w;
        int h = rand() % (base_height);
        overlay_height[i] = h > base_height ? base_height : h;
        dis_x_max[i] = base_width - overlay_width[i];
        dis_y_max[i] = base_height - overlay_height[i];
        overlay_image_path[i] = oip;
    }

    for (int i = 0; i < overlay_num; i++) {
        pos_x[i] = rand() % (dis_x_max[i] + 1);
        pos_y[i] = rand() % (dis_y_max[i] + 1);
    }

    const char* base_image_path = NULL;
    const char* output_image_cpu_path = NULL;
    const char* output_image_tpu_path = NULL;

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s thread_num loop use_real_img overlay_num src_w src_h start_x start_y overlay_width overlay_height src_path dst_cpu_path dst_tpu_path overlay_image_path\n", args[0]);
        printf("Example: overlay_img_num = 2:\n");
        printf("%s 1 1 1 2 1920 1080 50 150 50 150 400 400 400 400 path/of/src_img  path/of/dst_cpu_img  path/of/dst_tpu_img  path/of/overlay_img_1  path/of/overlay_img_2\n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) overlay_num = atoi(args[4]);

    // start_pos of param
    int base_width_pos = 5;
    int base_height_pos = 6;
    int pos_x_start = 7;
    int pos_y_start = pos_x_start + overlay_num;
    int overlay_width_pos = pos_y_start + overlay_num;
    int overlay_height_pos = overlay_width_pos + overlay_num;
    int base_image_path_pos = overlay_height_pos + overlay_num;
    int output_image_cpu_path_pos = base_image_path_pos + 1;
    int output_image_tpu_path_pos = output_image_cpu_path_pos + 1;
    int overlay_image_path_start = output_image_tpu_path_pos + 1;

    if (argc > base_width_pos) {
        base_width = atoi(args[base_width_pos]);
    }

    if (argc > base_height_pos) {
        base_height = atoi(args[base_height_pos]);
    }

    base_image_path = argc > base_image_path_pos ? args[base_image_path_pos] : base_image_path;
    output_image_cpu_path = argc > output_image_cpu_path_pos ? args[output_image_cpu_path_pos] : output_image_cpu_path;
    output_image_tpu_path = argc > output_image_tpu_path_pos ? args[output_image_tpu_path_pos] : output_image_tpu_path;


    for (int i = 0; i < overlay_num; i++) {
        pos_x[i] = argc > pos_x_start + i ? atoi(args[pos_x_start + i]) : pos_x[i];
        pos_y[i] = argc > pos_y_start + i ? atoi(args[pos_y_start + i]) : pos_y[i];
        overlay_width[i] = argc > overlay_width_pos + i ? atoi(args[overlay_width_pos + i]) : overlay_width[i];
        overlay_height[i] = argc > overlay_height_pos + i ? atoi(args[overlay_height_pos + i]) : overlay_height[i];
        overlay_image_path[i] = argc > overlay_image_path_start + i ? args[overlay_image_path_start + i] : overlay_image_path[i];
    }
    pthread_t pid[thread_num];
    cv_overlay_thread_arg_t cv_overlay_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        for (int j = 0; j < overlay_num; j++) {
            cv_overlay_thread_arg[i].start_x[j] = pos_x[j];
            cv_overlay_thread_arg[i].start_y[j] = pos_y[j];
            cv_overlay_thread_arg[i].overlay_w[j] = overlay_width[j];
            cv_overlay_thread_arg[i].overlay_h[j] = overlay_height[j];
            cv_overlay_thread_arg[i].overlay_path[j] = overlay_image_path[j];
            }
        cv_overlay_thread_arg[i].loop_num = loop;
        cv_overlay_thread_arg[i].use_real_img = use_real_img;
        cv_overlay_thread_arg[i].overlay_num = overlay_num;
        cv_overlay_thread_arg[i].src_w = base_width;
        cv_overlay_thread_arg[i].src_h = base_height;
        cv_overlay_thread_arg[i].input_path = base_image_path;
        cv_overlay_thread_arg[i].output_path_cpu = output_image_cpu_path;
        cv_overlay_thread_arg[i].output_path_tpu = output_image_tpu_path;
        if (pthread_create(pid + i, NULL, test_image_overlay, cv_overlay_thread_arg + i) != 0) {
            printf("create thread failed\n");
            return -1;
        }
    }

    for (int i = 0; i < thread_num; i++) {
        int ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            exit(-1);
        }
    }

    return 0;
}
