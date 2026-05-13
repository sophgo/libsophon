#include <iostream>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <math.h>
#include <float.h>
#include <assert.h>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include<windows.h>
  #include "time.h"
#endif

using namespace std;

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

static void readBin(const char* path, unsigned char* input, int size)
{
    FILE *fp_src = fopen(path, "rb");

    if (fread((void *)input, 1, size, fp_src) < (unsigned int)size) {
        printf("file size is less than %d required bytes\n", size);
    }

    fclose(fp_src);
}

static void writeBin(const char * path, unsigned char* out, int size)
{
    FILE *fp_dst = fopen(path, "wb");

    if (fwrite((void *)out, 1, size, fp_dst) < (unsigned int)size) {
        printf("file size is less than %d required bytes\n", size);
    }

    fclose(fp_dst);
}

static int cmp_res(unsigned char* out_tpu, unsigned char* out_cpu, int height, int width)
{
    int i, j;

    for(i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
                if (abs(out_tpu[i * width + j] - out_cpu[i * width + j]) > 1) {
                        printf("the cpu_res[%d][%d] = %d, the tpu_res[%d][%d] = %d\n",
                                i, j, out_cpu[(i) * width + j],
                                i, j, out_tpu[(i) * width + j]);
                        return -1;
                }
            }
    }

    return 0;
}

static void fill( unsigned char* input, int width, int height)
{
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            input[i * width + j] = rand() % 256;;
        }
    }
}

static void get_sobel_kernel(int* kernel, int dx, int dy, int _ksize)
{
    if (_ksize == 3) {
        if ((dx == 1) && (dy == 0)) {
            kernel[0] = -1; kernel[1] = 0; kernel[2] = 1;
            kernel[3] = -2; kernel[4] = 0; kernel[5] = 2;
            kernel[6] = -1; kernel[7] = 0; kernel[8] = 1;
        } else if ((dx == 0) && (dy == 1)) {
            kernel[0] = -1; kernel[1] = -2; kernel[2] = -1;
            kernel[3] = 0; kernel[4] = 0; kernel[5] = 0;
            kernel[6] = 1; kernel[7] = 2; kernel[8] = 1;
        }
    }
}

static void sobel_cpu( int width, int height, int ksize, int dx, int dy, unsigned char* input_data, float* output_cpu)
{
    int half_k = ksize / 2;
    int* kernel = (int*)malloc(ksize * ksize * sizeof(int));

    get_sobel_kernel(kernel, dx, dy, ksize);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0;
                for (int i = -half_k; i <= half_k; i++) {
                    for (int j = -half_k; j <= half_k; j++) {
                        int yy = y + i;
                        int xx = x + j;
                        if (yy < 0) yy = -yy;
                        if (yy >= height) yy = 2 * height - yy - 2;
                        if (xx < 0) xx = -xx;
                        if (xx >= width) xx = 2 * width - xx - 2;
                        int pixel_index = yy * width + xx;
                        int pixel = input_data[pixel_index];
                        sum += pixel * kernel[(i + half_k) * ksize + j + half_k];
                    }
                }
            int output_index = y * width + x;
            output_cpu[output_index] = sum;
        }
    }

    free(kernel);
}

static void non_maximum_suppression(float* magnitude, float* direction, int height, int width) {

    float* suppressed = (float*)malloc(width * height * sizeof(float));
    int i, y, x, index;
    float angle;

    memset(suppressed, 0, width * height * sizeof(float));

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            index = y * width + x;
            angle = direction[index];

            if (((angle >= 0) && (angle < 22.5)) || ((angle >= 157.5) && (angle <= 180))) {
                if ((magnitude[index] >= magnitude[index - 1]) && (magnitude[index] >= magnitude[index + 1])) {
                    suppressed[index] = magnitude[index];
                }
            } else if ((angle >= 22.5) && (angle < 67.5)) {
                if ((magnitude[index] >= magnitude[index + width + 1]) && (magnitude[index] >= magnitude[index - width - 1])) {
                    suppressed[index] = magnitude[index];
                }
            } else if ((angle >= 67.5) && (angle < 112.5)) {
                if ((magnitude[index] >= magnitude[index - width]) && (magnitude[index] >= magnitude[index + width])) {
                    suppressed[index] = magnitude[index];
                }
            } else if ((angle >= 112.5) && (angle < 157.5)) {
                if ((magnitude[index] >= magnitude[index - width + 1]) && (magnitude[index] >= magnitude[index + width - 1])) {
                    suppressed[index] = magnitude[index];
                }
            }
        }
    }

    for (i = 0; i < width * height; i++) {
        magnitude[i] = suppressed[i];
    }

    free(suppressed);
}

static void double_threshold(float* magnitude, float low_threshold, float high_threshold, int height, int width)
{
    int x, y, index;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            index = y * width + x;

            if (magnitude[index] >= high_threshold) {
                magnitude[index] = 255;
            } else if (magnitude[index] >= low_threshold) {
                magnitude[index] = 128;
            } else {
                magnitude[index] = 0;
            }
        }
    }
}

static void edge_linking(float* edges, int height, int width)
{
    int x, y, index;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            index = y * width + x;

            if (edges[index] == 128) {
                if (edges[index - 1] == 255 || edges[index + 1] == 255 ||
                    edges[index - width - 1] == 255 || edges[index - width + 1] == 255 ||
                    edges[index - width] == 255 || edges[index + width] == 255 ||
                    edges[index + width - 1] == 255 || edges[index + width + 1] == 255) {
                    edges[index] = 255;
                } else {
                    edges[index] = 0;
                }
            }
        }
    }
}

static int canny_cpu(unsigned char* input, unsigned char* output, int height, int width, int ksize,
                    float low_thresh, float high_thresh, bool l2gradient)
{
    float* Gx = new float [width * height];
    float* Gy = new float [width * height];
    float* G = new float [width * height];
    float* direction = new float [width * height];
    unsigned char* res = new unsigned char [width * height];
    memset(direction, 0, width * height* sizeof(float));
    int i, j, index;
    float g;

    sobel_cpu(width, height, ksize, 1, 0, input, Gx);
    sobel_cpu(width, height, ksize, 0, 1, input, Gy);

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            index = i * width + j;

            if (l2gradient) {
                g = sqrt(Gx[index] * Gx[index] + Gy[index] * Gy[index]);
                if (g >= 255) {
                    G[index] = 255;
                } else {
                    G[index]= g;
                }
            }
            else {
                g = fabs(Gx[index]) + fabs(Gy[index]);
                if (g > 255) {
                    G[index] = 255;
                } else {
                    G[index]= g;
                }
            }

            direction[index] = atan2((Gy[index]), Gx[index]) * 180.0 / M_PI;
            if (direction[index] < 0) {
                direction[index] += 180;
            }
        }
    }

    non_maximum_suppression(G, direction, height, width);
    double_threshold(G, low_thresh, high_thresh, height, width);
    edge_linking(G, height, width);

    for (i = 0; i < width * height ; ++i) {
        res[i] = (unsigned char)round(G[i]);
    }

    memcpy(output, res, width * height * sizeof(unsigned char));
    return 0;
}

static bm_status_t canny_tpu(unsigned char* input, unsigned char* output, int height, int width, int ksize,
                    float low_thresh, float high_thresh, bool l2gradient, bm_handle_t handle)
{

    bm_image_format_ext fmt = FORMAT_GRAY;
    bm_image img_i;
    bm_image img_o;
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;

    ret = bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
    if (ret != BM_SUCCESS) {
        printf("bm_image_create img_i failed!\n");
        goto exit0;
    }
    ret = bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
    if (ret != BM_SUCCESS) {
        printf("bm_image_create img_o failed!\n");
        goto exit0;
    }
    ret = bm_image_alloc_dev_mem(img_i);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem img_i failed!\n");
        goto exit0;
    }
    ret = bm_image_alloc_dev_mem(img_o);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem img_o failed!\n");
        goto exit1;
    }
    ret = bm_image_copy_host_to_device(img_i, (void**)(&input));
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_host_to_device img_i failed!\n");
        goto exit2;
    }

    gettimeofday(&t1, NULL);
    ret = bmcv_image_canny(handle, img_i, img_o, low_thresh, high_thresh, ksize, l2gradient);
    gettimeofday(&t2, NULL);
    printf("Canny TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (ret != BM_SUCCESS) {
        printf("bmcv_image_cannyfailed!\n");
        goto exit2;
    }

    ret = bm_image_copy_device_to_host(img_o, (void **)(&output));
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_device_to_host img_o failed!\n");
        goto exit2;
    }

exit2:
    bm_image_destroy(img_o);
exit1:
    bm_image_destroy(img_i);
exit0:
    return ret;
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
    if(BM_SUCCESS != bmcv_image_gaussian_blur(handle, img_i, img_o, kw, kh, sigmaX, sigmaY)) {
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

static int test_canny_random(int height, int width, int ksize, bool l2gradient,
                            char* src_name, char* dst_name, char* gold_name, bm_handle_t handle)
{

    ksize = 3;
    float low_thresh = 100;
    float high_thresh = 200;
    unsigned char* origin_data = new unsigned char [width * height];
    unsigned char* output_tpu = new unsigned char [width * height];
    unsigned char* output_cpu = new unsigned char [width * height];
    int ret = 0;
    printf("canny param: height=%d, width=%d, ksize=%d, low_thresh=%f, high_thresh=%f, l2=%d\n",
            height, width, ksize, low_thresh, high_thresh, l2gradient);

    if (src_name != NULL) {
        readBin(src_name, origin_data, width * height);
    } else {
        fill(origin_data, width, height);
    }

    ret = canny_tpu(origin_data, output_tpu, height, width, ksize, low_thresh, high_thresh, l2gradient, handle);
    if (ret != 0) {
        printf("canny_tpu failed\n");
        goto exit0;
    }

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = canny_cpu(origin_data, output_cpu, height, width, ksize, low_thresh, high_thresh, l2gradient);
    gettimeofday(&t2, NULL);
    printf("Canny CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (ret != 0) {
        printf("canny_cpu failed\n");
        goto exit0;
    }

    if (dst_name != NULL) {
        writeBin(dst_name, output_tpu, width * height);
    }

    if (gold_name != NULL) {
        printf("get the gold output cpu!\n");
        readBin(gold_name, output_cpu, width * height);
    }

    ret = cmp_res(output_tpu, output_cpu, height, width);
    if (ret != 0) {
        printf("cmp_res failed\n");
        goto exit0;
    }

exit0:
    delete [] origin_data;
    delete [] output_tpu;
    delete [] output_cpu;
    return ret;
}

int main(int argc, char* args[])
{
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);

    int loop = 1;
    int height = 1080;
    int width = 1920;
    int ksize = 3;
    int l2gradient = 0;
    char* src_name = NULL;
    char* dst_name = NULL;
    char* gold_name = NULL;
    int ret = 0;
    int i;
    bm_handle_t handle;

    ret = (int)bm_dev_request(&handle, 0);
    if (ret != 0) {
        printf("bm_dev_request failed\n");
        bm_dev_free(handle);
        return ret;
    }

    if (argc > 1) loop = atoi(args[1]);
    if (argc > 2) l2gradient = atoi(args[2]);
    if (argc > 3) width = atoi(args[3]);
    if (argc > 4) height = atoi(args[4]);
    if (argc > 5) src_name = args[5];
    if (argc > 6) dst_name = args[6];
    if (argc > 7) gold_name = args[7];

    for (i = 0; i < loop; i++) {
        ret = test_canny_random(height, width, ksize, l2gradient, src_name, dst_name, gold_name, handle);
        if (ret) {
            printf("test canny failed\n");
            return ret;
        }
    }
    printf("Compare TPU result with OpenCV successfully!\n");
    bm_dev_free(handle);
    return ret;
}