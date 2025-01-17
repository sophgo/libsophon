
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <math.h>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
typedef struct {
    int loop_num;
    int use_real_img;
    int width;
    int height;
    int format;
    int k_size;
    int dx;
    int dy;
    float scale;
    float delta;
    char* input_path;
    char* output_path;
    bm_handle_t handle;
} cv_sobel_thread_arg_t;

static void get_sobel_kernel(
    float* kernel,
    int dx,
    int dy,
    int _ksize,
    float scale) {

    int i, j;
    int ksizeX = _ksize, ksizeY = _ksize;

    if (ksizeX == 1 && dx > 0)
        ksizeX = 3;
    if (ksizeY == 1 && dy > 0)
        ksizeY = 3;

    signed char* kx = (signed char*)malloc(ksizeX * sizeof(signed char));
    signed char* ky = (signed char*)malloc(ksizeY * sizeof(signed char));
    signed char* kerI = (signed char*)malloc((int)fmax(ksizeX, ksizeY) + 1 * sizeof(signed char));

    for (int k = 0; k < 2; k++) {
        signed char* tmp = k == 0 ? kx : ky;
        int order = k == 0 ? dx : dy;
        int ksize = k == 0 ? ksizeX : ksizeY;

        if (ksize == 1)
            kerI[0] = 1;
        else if (ksize == 3) {
            if (order == 0)
                kerI[0] = 1, kerI[1] = 2, kerI[2] = 1;
            else if (order == 1)
                kerI[0] = -1, kerI[1] = 0, kerI[2] = 1;
            else
                kerI[0] = 1, kerI[1] = -2, kerI[2] = 1;
        } else {
            int oldval, newval;
            kerI[0] = 1;
            for (i = 0; i < ksize; i++)
                kerI[i + 1] = 0;

            for (i = 0; i < ksize - order - 1; i++) {
                oldval = kerI[0];
                for (j = 1; j <= ksize; j++) {
                    newval = kerI[j] + kerI[j - 1];
                    kerI[j - 1] = oldval;
                    oldval = newval;
                }
            }
            for (i = 0; i < order; i++) {
                oldval = -kerI[0];
                for (j = 1; j <= ksize; j++) {
                    newval = kerI[j - 1] - kerI[j];
                    kerI[j - 1] = oldval;
                    oldval = newval;
                }
            }
        }
        memcpy(tmp, kerI, ksize * sizeof(char));
    }

    for (int i = 0; i < ksizeY; i++) {
        for (int j = 0; j < ksizeX; j++) {
            kernel[i * ksizeX + j] = ky[i] * kx[j] * scale;
        }
    }

    free(kx);
    free(ky);
    free(kerI);
}

static int sobel_tpu(
        int width,
        int height,
        int format,
        int ksize,
        int dx,
        int dy,
        float scale,
        float delta,
        unsigned char* input,
        unsigned char* output,
        bm_handle_t handle) {
    int ret;
    bm_image img_i;
    bm_image img_o;

    if (format == FORMAT_YUV420P ||
        format == FORMAT_YUV422P ||
        format == FORMAT_YUV444P ||
        format == FORMAT_NV12    ||
        format == FORMAT_NV21    ||
        format == FORMAT_NV16    ||
        format == FORMAT_NV61    ||
        format == FORMAT_NV24    ||
        format == FORMAT_GRAY) {
        bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img_i);
        bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img_o);
        bm_image_alloc_dev_mem(img_i);
        bm_image_alloc_dev_mem(img_o);
        unsigned char *in_ptr[1] = {input};
        bm_image_copy_host_to_device(img_i, (void **)(in_ptr));
    } else {
        bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_i);
        bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &img_o);
        bm_image_alloc_dev_mem(img_i);
        bm_image_alloc_dev_mem(img_o);
        unsigned char *in_ptr[3] = {input, input + height * width, input + 2 * height * width};
        bm_image_copy_host_to_device(img_i, (void **)(in_ptr));
    }
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_image_sobel(handle, img_i, img_o, dx, dy, ksize, scale, delta);
    gettimeofday(&t2, NULL);
    printf("Sobel TPU using time: %ldus\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if(ret != BM_SUCCESS) {
        printf("bmcv_image_sobel error\n");
    }
    if (format == FORMAT_YUV420P ||
        format == FORMAT_YUV422P ||
        format == FORMAT_YUV444P ||
        format == FORMAT_NV12    ||
        format == FORMAT_NV21    ||
        format == FORMAT_NV16    ||
        format == FORMAT_NV61    ||
        format == FORMAT_NV24    ||
        format == FORMAT_GRAY) {
        unsigned char *out_ptr[1] = {output};
        bm_image_copy_device_to_host(img_o, (void **)out_ptr);
    } else {
        unsigned char *out_ptr[3] = {output, output + height * width, output + 2 * height * width};
        bm_image_copy_device_to_host(img_o, (void **)out_ptr);
    }
    bm_image_destroy(img_i);
    bm_image_destroy(img_o);
    return ret;
}

static int cmp_sobel(
        unsigned char* got,
        unsigned char* exp,
        int len) {
    for (int i = 0; i < len; i++) {
        if (abs(got[i] - exp[i]) > 1) {
            for (int j = 0; j < 5; j++)
                printf("cmp error: idx=%d  exp=%d  got=%d\n", i + j, exp[i + j], got[i + j]);
            return -1;
        }
    }
    return 0;
}

static void fill_image(unsigned char *input, int channel, int width, int height) {
    //int num = 0;
    for (int i = 0; i < channel; i++) {
        for (int j = 0; j < height; j++) {
            for (int k = 0; k < width; k++) {
                input[i * width * height + j * width + k] = rand() % 256;//num % 256;//
                //num++;
            }
        }
    }
}

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
        case FORMAT_YUV420P:
        case FORMAT_YUV422P:
        case FORMAT_YUV444P:
        case FORMAT_NV12:
        case FORMAT_NV21:
        case FORMAT_NV16:
        case FORMAT_NV61:
        case FORMAT_NV24:
            return width * height;
            break;
        default:
            printf("format error\n");
            return 0;
    }
}

static void read_bin(const char *input_path, unsigned char *input_data, int width, int height, int format) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("unable to open file %s\n", input_path);
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
        printf("unable to open file %s\n", output_path);
        return;
    }
    fwrite(output_data, sizeof(char), get_format_size(format, width, height), fp_dst);
    fclose(fp_dst);
    printf("write image success\n");
}

static void sobel_cpu(
        int width,
        int height,
        int format,
        int ksize,
        int dx,
        int dy,
        float scale,
        float delta,
        unsigned char* input_data,
        unsigned char* output_cpu) {
    int half_k = ksize / 2;
    int kw = (ksize == 1 && dx > 0) ? 3 : ksize;
    int kh = (ksize == 1 && dy > 0) ? 3 : ksize;
    if(kw == 3 || kh == 3){
        half_k = 1;
    }
    float* kernel = (float*)malloc(kw * kh * sizeof(float));
    if(ksize ==1 && dx ==1 &&dy ==1){
        ksize =3;
    }
    get_sobel_kernel(kernel, dx, dy, ksize, scale);
    int num_channels = 1;
    if (format == FORMAT_YUV420P ||
        format == FORMAT_YUV422P ||
        format == FORMAT_YUV444P ||
        format == FORMAT_NV12    ||
        format == FORMAT_NV21    ||
        format == FORMAT_NV16    ||
        format == FORMAT_NV61    ||
        format == FORMAT_NV24    ||
        format == FORMAT_GRAY) {
        num_channels = 1;
    } else {
        num_channels = 3;
    }
    for (int c = 0; c < num_channels; ++c) {
        int channel_offset = width * height * c;
        if(format == FORMAT_RGB_PACKED ||
           format == FORMAT_BGR_PACKED ) {
            channel_offset = c;
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float sum = 0.0;
                if (kw == 1 || kh == 1) {
                    if (kw == 1) {
                        for (int i = -half_k; i <= half_k; i++) {
                            int yy = y + i;
                            int xx = x;
                            if (yy < 0) yy = -yy;                         // Boundary reflection fill
                            if (yy >= height) yy = 2 * height - yy - 2;
                            if (xx < 0) xx = -xx;
                            if (xx >= width) xx = 2 * width - xx - 2;
                            int pixel_index = (format == FORMAT_RGB_PACKED || format == FORMAT_BGR_PACKED) ? channel_offset + (yy * width + xx) * 3 : channel_offset + yy * width + xx;
                            int pixel = input_data[pixel_index];
                            sum += pixel * kernel[i + half_k];
                        }
                    } else {
                        for (int j = -half_k; j <= half_k; j++) {
                            int yy = y;
                            int xx = x + j;
                            if (yy < 0) yy = -yy;
                            if (yy >= height) yy = 2 * height - yy - 2;
                            if (xx < 0) xx = -xx;
                            if (xx >= width) xx = 2 * width - xx - 2;
                            int pixel_index = (format == FORMAT_RGB_PACKED || format == FORMAT_BGR_PACKED) ? channel_offset + (yy * width + xx) * 3 : channel_offset + yy * width + xx;
                            int pixel = input_data[pixel_index];
                            sum += pixel * kernel[j + half_k];
                        }
                    }
                } else {
                    for (int i = -half_k; i <= half_k; i++) {
                        for (int j = -half_k; j <= half_k; j++) {
                            int yy = y + i;
                            int xx = x + j;
                            if (yy < 0) yy = -yy;
                            if (yy >= height) yy = 2 * height - yy - 2;
                            if (xx < 0) xx = -xx;
                            if (xx >= width) xx = 2 * width - xx - 2;
                            int pixel_index = (format == FORMAT_RGB_PACKED || format == FORMAT_BGR_PACKED) ? (channel_offset + (yy * width + xx) * 3) : (channel_offset + yy * width + xx);
                            int pixel = input_data[pixel_index];
                            sum += pixel * kernel[(i + half_k) * ksize + j + half_k];
                        }
                    }
                }
                sum = sum + delta;
                if (sum > 255) sum = 255;
                if (sum < 0) sum = 0;
                int output_index = (format == FORMAT_RGB_PACKED || format == FORMAT_BGR_PACKED) ? (channel_offset + (y * width + x) * 3) : (channel_offset + y * width + x);
                output_cpu[output_index] = (unsigned char)sum;
            }
        }
    }

    free(kernel);
}

static int test_sobel_random(
        int use_real_img,
        int width,
        int height,
        int format,
        int k_size,
        int dx,
        int dy,
        float scale,
        float delta,
        char* input_path,
        char* output_path,
        bm_handle_t handle) {
    printf("use_real_img = %d\n", use_real_img);
    printf("width        = %d\n", width);
    printf("height       = %d\n", height);
    printf("format       = %d\n", format);
    printf("k_size       = %d\n", k_size);
    printf("dx           = %d\n", dx);
    printf("dy           = %d\n", dy);
    printf("scale        = %f\n", scale);
    printf("delta        = %f\n", delta);
    int ret;
    unsigned char* input_data;
    unsigned char* output_cpu;
    unsigned char* output_tpu;
    if (format == FORMAT_YUV420P ||
        format == FORMAT_YUV422P ||
        format == FORMAT_YUV444P ||
        format == FORMAT_NV12    ||
        format == FORMAT_NV21    ||
        format == FORMAT_NV16    ||
        format == FORMAT_NV61    ||
        format == FORMAT_NV24    ||
        format == FORMAT_GRAY) {
        input_data = (unsigned char*)malloc(width * height * sizeof(unsigned char));
        output_cpu = (unsigned char*)malloc(width * height * sizeof(unsigned char));
        output_tpu = (unsigned char*)malloc(width * height * sizeof(unsigned char));
    } else {
        input_data = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
        output_cpu = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
        output_tpu = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
    }
    if (use_real_img) {
        read_bin(input_path, input_data, width, height, format);
    } else {
        if (format == FORMAT_YUV420P ||
            format == FORMAT_YUV422P ||
            format == FORMAT_YUV444P ||
            format == FORMAT_NV12    ||
            format == FORMAT_NV21    ||
            format == FORMAT_NV16    ||
            format == FORMAT_NV61    ||
            format == FORMAT_NV24    ||
            format == FORMAT_GRAY) {
            fill_image(input_data, 1, width, height);
        } else {
            fill_image(input_data, 3, width, height);

        }
    }
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    sobel_cpu(width, height, format, k_size, dx, dy, scale, delta, input_data, output_cpu);
    gettimeofday(&t2, NULL);
    printf("Sobel CPU using time: %ldus\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    ret = sobel_tpu(width, height, format, k_size, dx, dy, scale, delta, input_data, output_tpu, handle);
    if(ret != BM_SUCCESS) {
        free(input_data);
        free(output_cpu);
        free(output_tpu);
        return ret;
    }
    if (format == FORMAT_YUV420P ||
        format == FORMAT_YUV422P ||
        format == FORMAT_YUV444P ||
        format == FORMAT_NV12    ||
        format == FORMAT_NV21    ||
        format == FORMAT_NV16    ||
        format == FORMAT_NV61    ||
        format == FORMAT_NV24    ||
        format == FORMAT_GRAY) {
        ret = cmp_sobel(output_tpu, output_cpu, width * height);
    } else {
        ret = cmp_sobel(output_tpu, output_cpu, width * height * 3);
    }
    if (ret == 0) {
        printf("SOBEL compare TPU result with CPU result successfully!\n");
        if (use_real_img == 1) {
            if (format == FORMAT_YUV420P ||
                format == FORMAT_YUV422P ||
                format == FORMAT_YUV444P ||
                format == FORMAT_NV12    ||
                format == FORMAT_NV21    ||
                format == FORMAT_NV16    ||
                format == FORMAT_NV61    ||
                format == FORMAT_NV24    ||
                format == FORMAT_GRAY) {
                write_bin(output_path, output_tpu, width, height, 1);
            } else {
                write_bin(output_path, output_tpu, width, height, 3);
            }
        }
    } else {
        printf("cpu and tpu failed to compare \n");
    }
    free(input_data);
    free(output_cpu);
    free(output_tpu);
    return ret;
}

void* test_sobel(void* args) {
    cv_sobel_thread_arg_t* cv_sobel_thread_arg = (cv_sobel_thread_arg_t*)args;
    int loop_num = cv_sobel_thread_arg->loop_num;
    int use_real_img = cv_sobel_thread_arg->use_real_img;
    int width = cv_sobel_thread_arg->width;
    int height = cv_sobel_thread_arg->height;
    int format = cv_sobel_thread_arg->format;
    int k_size = cv_sobel_thread_arg->k_size;
    int dx = cv_sobel_thread_arg->dx;
    int dy = cv_sobel_thread_arg->dy;
    float scale = cv_sobel_thread_arg->scale;
    float delta = cv_sobel_thread_arg->delta;
    char* input_path = cv_sobel_thread_arg->input_path;
    char* output_path = cv_sobel_thread_arg->output_path;
    bm_handle_t handle = cv_sobel_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if(loop_num > 1) {
            int k_size_num[] = {1, 3, 5, 7};
            int size = sizeof(k_size_num) / sizeof(k_size_num[0]);
            int rand_num = rand() % size;
            k_size = k_size_num[rand_num];
            if(k_size == 1 || k_size == 3){
                width = 8 + rand() % 8185;
                height = 8 + rand() % 8185;
            } else if (k_size == 5) {
                width = 8 + rand() % 2041;
                height = 8 + rand() % 8192;
            } else if (k_size == 7) {
                width = 8 + rand() % 2041;
                height = 8 + rand() % 8192;
            }
            int format_num[] = {0,1,2,3,4,5,6,7,8,9,12,13,14};
            size = sizeof(format_num) / sizeof(format_num[0]);
            rand_num = rand() % size;
            format = format_num[rand_num];
            dx = rand() % 2 == 1 ? 1 : 0;
            dy = rand() % 2 == 1 ? 1 : 0;
            if(dx == 0 && dy == 0) {
                dx = 1;
            }
            scale = (float)rand() / RAND_MAX;
            delta = rand() % 256;
        }
        if (0 != test_sobel_random(use_real_img, width, height, format, k_size, dx, dy, scale, delta, input_path, output_path, handle)){
            printf("------TEST CV_SOBEL FAILED------\n");
            exit(-1);
        }
        printf("------TEST CV_SOBEL PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    printf("seed = %d\n", seed);
    int thread_num = 1;
    int loop = 1;
    int use_real_img = 0;
    int k_size_num[] = {1, 3, 5, 7};
    int size = sizeof(k_size_num) / sizeof(k_size_num[0]);
    int rand_num = rand() % size;
    int k_size = k_size_num[rand_num];
    int width = 8;
    int height = 8;
    if(k_size == 1 || k_size == 3){
        width = 8 + rand() % 8185;
        height = 8 + rand() % 8185;
    } else if (k_size == 5) {
        width = 8 + rand() % 4089;
        height = 8 + rand() % 8185;
    } else if (k_size == 7) {
        width = 8 + rand() % 2041;
        height = 8 + rand() % 8185;
    }
    int format_num[] = {0,1,2,3,4,5,6,7,8,9,12,13,14};
    size = sizeof(format_num) / sizeof(format_num[0]);
    rand_num = rand() % size;
    int format = format_num[rand_num];
    int dx = rand() % 2 == 1 ? 1 : 0;
    int dy = rand() % 2 == 1 ? 1 : 0;
    if(dx == 0 && dy == 0) {
        dx = 1;
    }
    float scale = (float)rand() / RAND_MAX;
    int delta = rand() % 256;
    char *input_path = NULL;
    char *output_path = NULL;
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s thread_num loop use_real_img width height format k_size dx dy scale delta input_path output_path(when use_real_img = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 1 0 512 512 \n", args[0]);
        printf("%s 1 1 1 1920 1080 res/1920x1080_rgbp.bin out/out_quantify.bin \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    if (argc > 5) height = atoi(args[5]);
    if (argc > 6) format = atoi(args[6]);
    if (argc > 7) k_size = atoi(args[7]);
    if (argc > 8) dx = atoi(args[8]);
    if (argc > 9) dy = atoi(args[9]);
    if (argc > 10) scale = atof(args[10]);
    if (argc > 11) delta = atof(args[11]);
    if (argc > 12) input_path = args[12];
    if (argc > 13) output_path = args[13];

    printf("thread_num   = %d\n", thread_num);
    printf("loop_num     = %d\n", loop);
    // test for multi-thread
    pthread_t pid[thread_num];
    cv_sobel_thread_arg_t cv_sobel_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_sobel_thread_arg[i].loop_num = loop;
        cv_sobel_thread_arg[i].use_real_img = use_real_img;
        cv_sobel_thread_arg[i].width = width;
        cv_sobel_thread_arg[i].height = height;
        cv_sobel_thread_arg[i].format = format;
        cv_sobel_thread_arg[i].k_size = k_size;
        cv_sobel_thread_arg[i].dx = dx;
        cv_sobel_thread_arg[i].dy = dy;
        cv_sobel_thread_arg[i].scale = scale;
        cv_sobel_thread_arg[i].delta = delta;
        cv_sobel_thread_arg[i].input_path = input_path;
        cv_sobel_thread_arg[i].output_path = output_path;
        cv_sobel_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_sobel, cv_sobel_thread_arg + i) != 0) {
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
    bm_dev_free(handle);
    return ret;
}
