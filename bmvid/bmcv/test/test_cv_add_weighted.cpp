#include <iostream>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "test_misc.h"
#include <stdint.h>
#include <cmath>
#include <assert.h>
#include <vector>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

template <typename T>
void fill_img(T* input, int size)
{
    for (int i = 0; i < size; i++)
        input[i] = rand() % 255;
}

template <typename T>
static int readBin(const char* path, T *input_data, int size)
{
    FILE *fp_src = fopen(path, "rb");

    if (fp_src == NULL) {
        printf("[ERROR] %s unable open\n", path);
        return -1;
    }

    if (fread((void *)input_data, sizeof(T), size, fp_src) < (unsigned int)size) {
        printf("file size is less than %d required bytes\n", size);
        return -1;
    };

    fclose(fp_src);

    return 0;
}

template <typename T>
static int write_bin(const char *output_path, T *output_data, int size)
{
    FILE *fp_dst = fopen(output_path, "wb");

    if (fp_dst == NULL) {
        printf("unable to open output file %s\n", output_path);
        return -1;
    }

    if(fwrite(output_data, sizeof(T), size, fp_dst) < (unsigned int) size) {
        printf("file size is less than %d required bytes\n", size);
        return -1;
    }

    fclose(fp_dst);
    return 0;
}

template <typename SRC_T, typename DST_T>
int add_weight_cpu(SRC_T *input1, SRC_T *input2, DST_T *output, int img_size, float alpha, float beta, float gamma)
{
    for (int i = 0; i < img_size; i++) {
        float res = (float)(input1[i] * alpha + input2[i] * beta + gamma);

        if (std::is_same<SRC_T, uint8_t>::value)
            res = (res > 255) ? (255) : ((res < 0) ? (0) : (res));

        output[i] = (DST_T) res;
    }

    return 0;
}


template <typename SRC_T, typename DST_T>
int add_weighted_tpu(
        SRC_T* input1,
        SRC_T* input2,
        DST_T* output,
        int height,
        int width,
        int format,
        int data_type,
        float alpha,
        float beta,
        float gamma)
{
    struct timeval t1, t2;
    bm_handle_t handle;
    bm_image input1_img;
    bm_image input2_img;
    bm_image output_img;

    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }

    bm_image_create(handle, height, width, (bm_image_format_ext)format, (bm_image_data_format_ext)data_type, &input1_img);
    bm_image_create(handle, height, width, (bm_image_format_ext)format, (bm_image_data_format_ext)data_type, &input2_img);
    bm_image_create(handle, height, width, (bm_image_format_ext)format, (bm_image_data_format_ext)data_type, &output_img);

    if (bm_image_alloc_dev_mem(input1_img) != BM_SUCCESS) {
        printf("input1_img alloc dev mem failed\n");
        return -1;
    }

    if (bm_image_alloc_dev_mem(input2_img) != BM_SUCCESS) {
        printf("input2_img alloc dev mem failed\n");
        bm_image_destroy(input1_img);
        bm_dev_free(handle);
        return -1;
    }

    if (bm_image_alloc_dev_mem(output_img) != BM_SUCCESS) {
        printf("output_img alloc dev mem failed\n");
        bm_image_destroy(input1_img);
        bm_image_destroy(input2_img);
        bm_dev_free(handle);
        return -1;
    }

    int image_byte_size[4] = {0};
    bm_image_get_byte_size(input1_img, image_byte_size);
    void* input_addr1[4] = {(void *)input1,
                        (void *)((SRC_T*)input1 + image_byte_size[0]/sizeof(SRC_T)),
                        (void *)((SRC_T*)input1 + (image_byte_size[0] + image_byte_size[1])/sizeof(SRC_T)),
                        (void *)((SRC_T*)input1 + (image_byte_size[0] + image_byte_size[1] + image_byte_size[2])/sizeof(SRC_T))};

    void* input_addr2[4] = {(void *)input2,
                        (void *)((SRC_T*)input2 + image_byte_size[0]/sizeof(SRC_T)),
                        (void *)((SRC_T*)input2 + (image_byte_size[0] + image_byte_size[1])/sizeof(SRC_T)),
                        (void *)((SRC_T*)input2 + (image_byte_size[0] + image_byte_size[1] + image_byte_size[2])/sizeof(SRC_T))};

    if (bm_image_copy_host_to_device(input1_img, (void **)input_addr1) != BM_SUCCESS) {
        printf("input1_img copy host to device failed\n");
        bm_image_destroy(input1_img);
        bm_image_destroy(input2_img);
        bm_image_destroy(output_img);
        bm_dev_free(handle);
        return -1;
    }

    if (bm_image_copy_host_to_device(input2_img, (void **)input_addr2) != BM_SUCCESS) {
        printf("input2_img copy host to device failed\n");
        bm_image_destroy(input1_img);
        bm_image_destroy(input2_img);
        bm_image_destroy(output_img);
        bm_dev_free(handle);
        return -1;
    }

    gettimeofday_(&t1);
    ret = bmcv_image_add_weighted(handle, input1_img, alpha, input2_img, beta, gamma, output_img);
    if (ret != BM_SUCCESS) {
        printf("bmcv image add weighted failed\n");
        bm_image_destroy(input1_img);
        bm_image_destroy(input2_img);
        bm_image_destroy(output_img);
        bm_dev_free(handle);
        return -1;
    }
    gettimeofday_(&t2);
    printf("add_weight TPU using time = %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    int output_image_byte[4] = {0};
    bm_image_get_byte_size(output_img, output_image_byte);
    void* out_addr[4] = {(void *)output,
                        (void *)((DST_T*)output + image_byte_size[0]/sizeof(DST_T)),
                        (void *)((DST_T*)output + (image_byte_size[0] + image_byte_size[1])/sizeof(DST_T)),
                        (void *)((DST_T*)output + (image_byte_size[0] + image_byte_size[1] + image_byte_size[2])/sizeof(DST_T))};

    if (bm_image_copy_device_to_host(output_img, (void **)out_addr) != BM_SUCCESS) {
        printf("output image copy device to host failed\n");
        bm_image_destroy(input1_img);
        bm_image_destroy(input2_img);
        bm_image_destroy(output_img);
        bm_dev_free(handle);
        return -1;
    }

    bm_image_destroy(input1_img);
    bm_image_destroy(input2_img);
    bm_image_destroy(output_img);
    bm_dev_free(handle);

    return ret;
}

static int get_image_size(int format, int width, int height)
{
    int size = 0;
    switch (format) {
        case FORMAT_YUV420P:
            size = width * height + (height * width / 4) * 2;
            break;
        case FORMAT_YUV422P:
            size = width * height + (height * width / 2) * 2;
            break;
        case FORMAT_YUV444P:
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PACKED:
        case FORMAT_BGR_PACKED:
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
            size = width * height * 3;
            break;
        case FORMAT_NV12:
        case FORMAT_NV21:
            size = width * height + width * height / 2;
            break;
        case FORMAT_NV16:
        case FORMAT_NV61:
        case FORMAT_NV24:
            size = width * height * 2;
            break;
        case FORMAT_GRAY:
            size = width * height;
            break;
        default:
            printf("image format error \n");
            break;
    }
    return size;
}

template<typename T>
static int cmp_result(T *got, T *exp, int len)
{
    for (int i = 0; i < len; ++i) {
        T p_got = got[i];
        T p_exp = exp[i];
        if (abs(p_got - p_exp) > 1) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, (int)p_got, (int)p_got);
            return -1;
        }
    }
    return 0;
}

template <typename SRC_T, typename DST_T>
static int test_add_weighted_random(
        int height,
        int width,
        int format,
        int data_type,
        float alpha,
        float beta,
        float gamma,
        int real_img,
        char *input1_path,
        char *input2_path,
        char *output_path)
{
    int ret;
    struct timeval t1, t2;

    printf("TEST Info:\n");
    printf("format: %d, data_type %s, img_size %d x %d\n", format, (data_type == 0 ? "float" : "uint_8"), width, height);
    printf("alpha:%f, beta:%f, gamma:%f\n", alpha, beta, gamma);

    int img_size = get_image_size(format, width, height);

    SRC_T *input1 = (SRC_T*)malloc(sizeof(SRC_T) * img_size);
    SRC_T *input2 = (SRC_T*)malloc(sizeof(SRC_T) * img_size);
    DST_T *output_cpu = (DST_T*) malloc(sizeof(DST_T) * img_size);
    DST_T *output_tpu = (DST_T*) malloc(sizeof(DST_T) * img_size);

    memset(output_cpu, 0, sizeof(DST_T) * img_size);
    memset(output_tpu, 0, sizeof(DST_T) * img_size);

    if (real_img) {
        if (readBin<SRC_T>(input1_path, input1, img_size))
            return -1;
        if (readBin<SRC_T>(input2_path, input2, img_size))
            return -1;
    } else {
        fill_img<SRC_T>(input1, img_size);
        fill_img<SRC_T>(input2, img_size);
    }

    gettimeofday_(&t1);
    ret = add_weight_cpu<SRC_T, DST_T>(input1, input2, output_cpu, img_size, alpha, beta, gamma);
    if (ret) {
        printf("[ERROR] add weight cpu failed\n");
        goto failed;
    }
    gettimeofday_(&t2);
    printf("add_weight CPU using time = %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    ret = add_weighted_tpu<SRC_T, DST_T>(input1, input2,
            output_tpu, height, width, format, data_type, alpha, beta, gamma);
    if (ret) {
        printf("[ERROR] add weight tpu failed\n");
        goto failed;
    }

    ret = cmp_result<DST_T>(output_cpu, output_tpu, img_size);
    if (ret) {
        printf("[ERROR] cmp failed\n");
        goto failed;
    }

    if (real_img)
        write_bin(output_path, output_tpu, img_size);

failed:
   free(input1);
   free(input2);
   free(output_cpu);
   free(output_tpu);

   return ret;
}


int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime_(0, &tp);
    int seed = tp.tv_nsec;
    srand(seed);
    int loop = 1;
    int if_use_img = 0;
    int width = 1 + rand() % 4096;
    int height = 1 + rand() % 4096;
    float alpha = roundf((float)rand() / RAND_MAX * 10)/ 10.0;
    float beta = 1.0f - alpha;
    float gamma = ((float)rand()/RAND_MAX) * 255.0f;
    char* src1_name = NULL;
    char* src2_name = NULL;
    char* dst_name = NULL;
    int ret = 0;

    int format_num[] = {1,2,8,9,10,11,12,13,14};
    int size = sizeof(format_num) / sizeof(format_num[0]);
    int rand_num = rand() % size;
    int format = format_num[rand_num];

    while (1) {
        if (format == 1 && (width % 2 != 0 || height % 2 != 0)) {
            width = 1 + rand() % 4096;
            height = 1 + rand() % 4096;
        } else {
            break;
        }
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s loop width height format alpha beta gamma real_img input1_path input2_path output_path (when use_real_img = 1, need to set height, width and input imgs & output img path)\n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 1 512 512 8\n", args[0]);
        printf("%s 1 1920 1080 8 0.75 0.25 0.78 1 ./src1_img.bin ./src2_img.bin ./dst_img.bin\n", args[0]);
        return 0;
    }

    if (argc > 1) loop = atoi(args[1]);
    if (argc > 2) width = atoi(args[2]);
    if (argc > 3) height = atoi(args[3]);
    if (argc > 4) format = atoi(args[4]);
    if (argc > 5) alpha = atof(args[5]);
    if (argc > 6) beta = atof(args[6]);
    if (argc > 7) gamma = atof(args[7]);
    if (argc > 8) if_use_img = atoi(args[8]);
    if (argc > 9) src1_name = args[9];
    if (argc > 10) src2_name = args[10];
    if (argc > 11) dst_name = args[11];

    if (width > 4096 || height > 4096) {
        printf("img size max 4096 x 4096\n");
        return -1;
    }

    if (alpha + beta > 1.0f) {
        printf("alpha + beta should be 1.0f, alpha %f, beta %f\n", alpha, beta);
        return -1;
    }

    for (int i = 0; i < loop; i++) {
        ret = test_add_weighted_random<float_t, float_t>(height, width, format,
            0, alpha, beta, gamma, if_use_img, src1_name, src2_name, dst_name);
        if (ret) {
            printf("===== [float] test_add_weighted_random failed =====\n");
            return -1;
        }

        printf("===== [float] test_add_weighted_random successful =====\n\n");

        ret = test_add_weighted_random<uint8_t, uint8_t>(height, width, format,
                    1, alpha, beta, gamma, if_use_img, src1_name, src2_name, dst_name);

        if (ret) {
            printf("===== [uint_8] test_add_weighted_random failed =====\n");
            return -1;
        }

        printf("===== [uint_8] test_add_weighted_random successful =====\n");
    }

    return 0;
}