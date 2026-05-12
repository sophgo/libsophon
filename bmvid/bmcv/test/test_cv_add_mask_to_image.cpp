#include <iostream>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "md5.h"
#include "test_misc.h"
#include <assert.h>
#include <vector>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

typedef struct {
    int loop_num;
    int use_real_img;
    int height;
    int width;
    char *image_name;
    char *dst_image_name;
    bm_handle_t handle;
} cv_add_mask_to_image_args_t;

static void read_bin(const char* path, unsigned char* input_data, int size)
{
    FILE *fp_src = fopen(path, "rb");
    if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
        printf("file size is less than %d required bytes\n", size);
    };

    fclose(fp_src);
}

static void write_bin(const char * path, unsigned char* input_data, int size)
{
    FILE *fp_dst = fopen(path, "wb");
    if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
        printf("file size is less than %d required bytes\n", size);
    };

    fclose(fp_dst);
}

void add_mask_to_image_cpu(unsigned char *src, float *dst, mask_info_t *mask_val, int dw, int dh)
{
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int src_idx = y * dw + x;
            int dst_idx = y * dw * 3 + x * 3;

            unsigned char mask_value = src[src_idx];
            if (mask_value == mask_val[0].mask_val) {
                dst[dst_idx] = mask_val[0].rgb[0];    // R
                dst[dst_idx + 1] = mask_val[0].rgb[1];  // G
                dst[dst_idx + 2] = mask_val[0].rgb[2];  // B
            } else if (mask_value == mask_val[1].mask_val) {
                dst[dst_idx] = mask_val[1].rgb[0];
                dst[dst_idx + 1] = mask_val[1].rgb[1];
                dst[dst_idx + 2] = mask_val[1].rgb[2];
            }
        }
    }
}

static int add_mask_to_image_tpu(bm_handle_t handle,
                     unsigned char *mask_data,
                     float *image_data,
                     int height, int width,
                     int mask_num, mask_info_t *mask_val)
{
    bm_status_t ret;
    bm_image bm_mask_data, bm_image_data;

    ret = bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &bm_mask_data);
    if (ret) {
        printf("bm_image: bm_mask_data create failed\n");
        return -1;
    }

    ret = bm_image_create(handle, height, width, FORMAT_RGB_PACKED, DATA_TYPE_EXT_FLOAT32, &bm_image_data);
    if (ret) {
        printf("bm_image: bm_image_data create failed\n");
        return -1;
    }

    ret = bm_image_alloc_dev_mem(bm_mask_data);
    if (ret) {
        printf("bm_image:bm_mask_data alloc dev mem failed\n");
        return -1;
    }

    ret = bm_image_alloc_dev_mem(bm_image_data);
    if (ret) {
        printf("bm_image:bm_image_data alloc dev mem failed\n");
        bm_image_destroy(bm_mask_data);
        return -1;
    }

    ret = bm_image_copy_host_to_device(bm_mask_data, (void**)(&mask_data));
    if (ret) {
        printf("bm_image:bm_mask_data copy host to device failed\n");
        bm_image_destroy(bm_mask_data);
        bm_image_destroy(bm_image_data);
        return -1;
    }

    ret = bm_image_copy_host_to_device(bm_image_data, (void**)(&image_data));
    if (ret) {
        printf("bm_image:bm_image_data copy host to device failed\n");
        bm_image_destroy(bm_mask_data);
        bm_image_destroy(bm_image_data);
        return -1;
    }

    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_add_mask_to_image(handle, bm_mask_data, bm_image_data, mask_num, mask_val);
    gettimeofday_(&t2);
    printf("bmcv_add_mask_to_image TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (ret) {
        printf("bmcv_add_mask_to_image failed\n");
        bm_image_destroy(bm_mask_data);
        bm_image_destroy(bm_image_data);
        return -1;
    }

    ret = bm_image_copy_device_to_host(bm_image_data, (void**)(&image_data));
    if (ret) {
        printf("bm_image:bm_image_data copy device to host failed\n");
        bm_image_destroy(bm_mask_data);
        bm_image_destroy(bm_image_data);
        return -1;
    }

    bm_image_destroy(bm_mask_data);
    bm_image_destroy(bm_image_data);

    return ret;
}

static bool rectangles_overlap(int x1_start, int x1_end, int y1_start, int y1_end,
                              int x2_start, int x2_end, int y2_start, int y2_end) {
    if (x1_end <= x2_start || x2_end <= x1_start ||
        y1_end <= y2_start || y2_end <= y1_start) {
        return false;
    }
    return true;
}

void create_random_mask(unsigned char *mask, int width, int height, int include_100_region)
{
    srand(time(NULL));

    memset(mask, 0, width * height);

    int num_regions = 3 + rand() % 4;

    int *region_bounds = (int *)malloc(num_regions * 4 * sizeof(int));
    if (!region_bounds) return;
    int regions_generated = 0;

    int max_attempts = 100;

    for (int r = 0; r < num_regions; r++) {
        unsigned char value;

        if (include_100_region && r == 0) {
            value = 100;
        } else {
            value = (rand() % 2 == 0) ? 127 : 255;
        }

        int attempts = 0;
        bool region_valid = false;
        int start_x, start_y, end_x, end_y;

        while (!region_valid && attempts < max_attempts) {
            attempts++;

            int region_width = width / 8 + rand() % (width / 4);
            int region_height = height / 8 + rand() % (height / 4);
            int center_x = rand() % width;
            int center_y = rand() % height;

            start_x = center_x - region_width / 2;
            start_y = center_y - region_height / 2;
            end_x = center_x + region_width / 2;
            end_y = center_y + region_height / 2;

            if (start_x < 0) start_x = 0;
            if (start_y < 0) start_y = 0;
            if (end_x > width) end_x = width;
            if (end_y > height) end_y = height;

            if (end_x - start_x < 2 || end_y - start_y < 2) {
                continue;
            }

            region_valid = true;
            for (int i = 0; i < regions_generated; i++) {
                int *existing_bounds = &region_bounds[i * 4];
                int ex_start_x = existing_bounds[0];
                int ex_end_x = existing_bounds[1];
                int ex_start_y = existing_bounds[2];
                int ex_end_y = existing_bounds[3];

                if (rectangles_overlap(start_x, end_x, start_y, end_y,
                                      ex_start_x, ex_end_x, ex_start_y, ex_end_y)) {
                    region_valid = false;
                    break;
                }
            }
        }

        if (region_valid) {
            for (int y = start_y; y < end_y; y++) {
                for (int x = start_x; x < end_x; x++) {
                    mask[y * width + x] = value;
                }
            }

            region_bounds[regions_generated * 4] = start_x;
            region_bounds[regions_generated * 4 + 1] = end_x;
            region_bounds[regions_generated * 4 + 2] = start_y;
            region_bounds[regions_generated * 4 + 3] = end_y;
            regions_generated++;

            printf("Region %d: value=%d, bounds=[%d,%d]x[%d,%d]\n",
                   r, value, start_x, end_x, start_y, end_y);
        } else {
            printf("Warning: Failed to generate non-overlapping region %d after %d attempts\n",
                   r, max_attempts);
        }
    }

    free(region_bounds);
}

static int cmp(float *got, float *exp, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < 3 * width; x++) {
            int idx = y * 3 * width + x;
            if (got[idx] != exp[idx]) {
                printf("cmp error: idx=%d exp=%f got=%f\n", idx, exp[idx], got[idx]);
                return -1;
            }
        }
    }

    return 0;
}


static int test_cv_add_mask_to_image_random(int use_real_img,
                                            int width,
                                            int height,
                                            char *image_name,
                                            char *dst_image_name,
                                            bm_handle_t handle)
{
    printf("width %d, height %d\n", width, height);

    int mask_num = 2;
    mask_info_t mask_val[mask_num]; // mask_r mask_g mask_b
    memset(mask_val, 0, sizeof(mask_info_t) * mask_num);

    mask_val[0].mask_val = 127;
    mask_val[0].rgb[0] = 255; // R
    mask_val[0].rgb[1] = 255; // G
    mask_val[0].rgb[2] = 0;   // B

    mask_val[1].mask_val = 255;
    mask_val[1].rgb[0] = 0;   // R
    mask_val[1].rgb[1] = 0;   // G
    mask_val[1].rgb[2] = 255; // B

    unsigned char *mask_data = (unsigned char*)malloc(width * height * sizeof(unsigned char));
    float *image_cpu_res = (float *) malloc (width * height * 3 * sizeof(float));
    float *image_tpu_res = (float *) malloc (width * height * 3 * sizeof(float));

    create_random_mask(mask_data, width, height, 0);

    if (use_real_img) {
        if (image_name) {
            unsigned char *u8_image = (unsigned char *) malloc(width * height * 3 * sizeof(unsigned char));
            read_bin(image_name, u8_image, width * height * 3 * sizeof(unsigned char));
            for (int i = 0; i < width * height * 3; i++) {
                image_cpu_res[i] = (float)u8_image[i];
                image_tpu_res[i] = (float)u8_image[i];
            }

            free(u8_image);
        }
    } else {
        for(int i = 0; i < width * height * 3; i++) {
            float val = (float)(rand() % 255);
            image_cpu_res[i] = image_tpu_res[i] = val;
        }
    }

    struct timeval t1, t2;
    gettimeofday_(&t1);
    add_mask_to_image_cpu(mask_data, image_cpu_res, mask_val, width, height);
    gettimeofday_(&t2);
    printf("add_mask_to_image CPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    int ret = add_mask_to_image_tpu(handle, mask_data, image_tpu_res, height, width, mask_num, mask_val);
    if (ret) {
        printf("add_mask_to_image_tpu failed\n");
        free(mask_data);
        free(image_cpu_res);
        free(image_tpu_res);
        return -1;
    }

    if (cmp(image_cpu_res, image_tpu_res, width, height)) {
        printf("add_mask_to_image cmp failed\n");
        return -1;
    }
    printf("add_maks_to_image cmp successful\n");

    if (use_real_img) {
        unsigned char *u8_tpu_res = (unsigned char *) malloc(width * height * 3 * sizeof(unsigned char));

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * 3 * width + x * 3;
                u8_tpu_res[idx] = (float)image_tpu_res[idx];
                u8_tpu_res[idx + 1] = (float)image_tpu_res[idx + 1];
                u8_tpu_res[idx + 2] = (float)image_tpu_res[idx + 2];
            }
        }

        write_bin(dst_image_name, u8_tpu_res, width * height * 3);

        free(u8_tpu_res);
    }

    free(mask_data);
    free(image_cpu_res);
    free(image_tpu_res);

    return 0;
}

void *test_thread_add_mask_to_image(void *args)
{
    cv_add_mask_to_image_args_t *api = (cv_add_mask_to_image_args_t*)args;
    int loop_num = api->loop_num;
    int use_real_img = api->use_real_img;
    int height = api->height;
    int width = api->width;
    char *image_name = api->image_name;
    char *dst_image_name = api->dst_image_name;
    bm_handle_t handle = api->handle;

    for (int i = 0; i < loop_num; i++) {
        if (test_cv_add_mask_to_image_random(use_real_img, width, height, image_name, dst_image_name, handle)) {
            printf("===== Test Add Mask To Image FAILED\n =====\n");
            exit(-1);
        }
        printf("===== loop %d, Test Add Mask To Image SUCCED =====\n", i);
    }

    return (void*)0;
}

int main(int argc, char *args[])
{
    struct timespec tp;
    clock_gettime_(0, &tp);
    int seed = tp.tv_nsec;
    srand(seed);

    int loop = 1;
    int thread_num = 1;
    int use_real_img = 0;
    int height = 1 + rand() % 4096;
    int width = 1 + rand() % 4096;
    char *image_name = NULL;
    char *dst_image_name = NULL;

    printf("seed = %d\n", seed);
    if (argc == 2 && atoi(args[1]) == -1) {
        printf("%s: thread_num loop use_real_img width height image_name dst_image_name (when use_real_img = 1, need to set mask_name and image_name)\n", args[0]);
        printf("example:");
        printf("%s \n", args[0]);
        printf("%s 1 100\n", args[0]);
        printf("%s 1 100 0 256 256\n", args[0]);
        printf("%s 1 100 1 1920 1080 image_1920_1080.bin image_1920_1080_res.bin\n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    if (argc > 5) height = atoi(args[5]);
    if (argc > 6) image_name = args[6];
    if (argc > 7) dst_image_name = args[7];

    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm_handle failed. ret = %d\n", ret);
        return -1;
    }

    pthread_t pid[thread_num];
    cv_add_mask_to_image_args_t param[thread_num];
    for (int i = 0; i < thread_num; i++) {
        param[i].loop_num = loop;
        param[i].use_real_img = use_real_img;
        param[i].height = height;
        param[i].width = width;
        param[i].image_name = image_name;
        param[i].dst_image_name = dst_image_name;
        param[i].handle = handle;
        if (pthread_create(&pid[i], NULL, test_thread_add_mask_to_image, &param[i]) != 0) {
            printf("create thread failed\n");
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
    return 0;
}