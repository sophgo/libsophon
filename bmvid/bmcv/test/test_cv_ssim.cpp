#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <float.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#ifdef __linux__
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

typedef struct {
    int loop;
    int use_real_img;
    int width;
    int height;
    int win_size;
    bool gradient;
    float data_range;
    int format;
    bool gaussian_weights;
    bool full;
    char* im1_path;
    char* im2_path;
    char* output_path;
    bm_handle_t handle;
} cv_ssim_thread_arg_t;

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

// Gaussian filter(reflective boundary)
void gaussian_filter(float* src, float* dst, int width, int height, float sigma) {
    int r = (int)(3.5f * sigma + 0.5f);
    int win_size = 2 * r + 1;
    float *kernel = (float*)malloc(win_size * win_size * sizeof(float));

    create_gaussian_kernel(kernel, win_size, win_size, sigma, sigma);

    // convolution(reflective boundary)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float val = 0.0f;
            for (int ky = 0; ky < win_size; ky++) {
                for (int kx = 0; kx < win_size; kx++) {
                    int iy = y + ky - r;
                    int ix = x + kx - r;

                    // reflective boundary
                    if (iy < 0) iy = -iy;
                    if (iy >= height) iy = 2 * height - 2 - iy;
                    if (ix < 0) ix = -ix;
                    if (ix >= width) ix = 2 * width - 2 - ix;

                    val += src[iy * width + ix] * kernel[ky * win_size + kx];
                }
            }
            dst[y * width + x] = val;
        }
    }
    free(kernel);
}

// uniform filter(reflective boundary)
void uniform_filter(float* src, float* dst, int width, int height, int win_size) {
    int pad = win_size / 2;
    float norm = 1.0f / (win_size * win_size);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (int dy = -pad; dy <= pad; dy++) {
                for (int dx = -pad; dx <= pad; dx++) {
                    int iy = y + dy;
                    int ix = x + dx;
                    // reflective boundary
                    if (iy < 0) iy = -iy;
                    if (iy >= height) iy = 2 * height - 2 - iy;
                    if (ix < 0) ix = -ix;
                    if (ix >= width) ix = 2 * width - 2 - ix;
                    sum += src[iy * width + ix];
                }
            }
            dst[y * width + x] = sum * norm;
        }
    }
}

// single channel SSIM
float structural_similarity_channel(
    float* im1,
    float* im2,
    int width, int height,
    int win_size,
    // bool gradient,
    float data_range,
    bool gaussian_weights,
    bool full,
    float** diff_map
) {
    if (win_size % 2 == 0) {
        fprintf(stderr, "Error: win_size must be odd.\n");
        return -1.0f;
    }

    float K1 = 0.01f;
    float K2 = 0.03f;
    float sigma = 1.5f;  // default Gaussian sigma

    // allocate temporary buffer
    float* ux = (float*)calloc(width * height, sizeof(float));
    float* uy = (float*)calloc(width * height, sizeof(float));
    float* uxx = (float*)calloc(width * height, sizeof(float));
    float* uyy = (float*)calloc(width * height, sizeof(float));
    float* uxy = (float*)calloc(width * height, sizeof(float));
    float* vx = (float*)calloc(width * height, sizeof(float));
    float* vy = (float*)calloc(width * height, sizeof(float));
    float* vxy = (float*)calloc(width * height, sizeof(float));
    float* S = (float*)calloc(width * height, sizeof(float));

    // calculate mean value
    if (gaussian_weights) {
        gaussian_filter(im1, ux, width, height, sigma);
        gaussian_filter(im2, uy, width, height, sigma);

        float* im1_sq = (float*)malloc(width * height * sizeof(float));
        for (int i = 0; i < width * height; i++) im1_sq[i] = im1[i] * im1[i];
        gaussian_filter(im1_sq, uxx, width, height, sigma);
        free(im1_sq);

        float* im2_sq = (float*)malloc(width * height * sizeof(float));
        for (int i = 0; i < width * height; i++) im2_sq[i] = im2[i] * im2[i];
        gaussian_filter(im2_sq, uyy, width, height, sigma);
        free(im2_sq);

        float* im1_im2 = (float*)malloc(width * height * sizeof(float));
        for (int i = 0; i < width * height; i++) im1_im2[i] = im1[i] * im2[i];
        gaussian_filter(im1_im2, uxy, width, height, sigma);
        free(im1_im2);
    } else {
        uniform_filter(im1, ux, width, height, win_size);
        uniform_filter(im2, uy, width, height, win_size);

        float* im1_sq = (float*)malloc(width * height * sizeof(float));
        for (int i = 0; i < width * height; i++) im1_sq[i] = im1[i] * im1[i];
        uniform_filter(im1_sq, uxx, width, height, win_size);
        free(im1_sq);

        float* im2_sq = (float*)malloc(width * height * sizeof(float));
        for (int i = 0; i < width * height; i++) im2_sq[i] = im2[i] * im2[i];
        uniform_filter(im2_sq, uyy, width, height, win_size);
        free(im2_sq);

        float* im1_im2 = (float*)malloc(width * height * sizeof(float));
        for (int i = 0; i < width * height; i++) im1_im2[i] = im1[i] * im2[i];
        uniform_filter(im1_im2, uxy, width, height, win_size);
        free(im1_im2);
    }

    // calculate variance and covariance
    int np = win_size * win_size;
    float cov_norm = np / (np - 1.0f);  // unbiased estimator
    // float cov_norm = 1.0f;                 // biased estimator

    for (int i = 0; i < width * height; i++) {
        vx[i] = cov_norm * (uxx[i] - ux[i] * ux[i]);
        vy[i] = cov_norm * (uyy[i] - uy[i] * uy[i]);
        vxy[i] = cov_norm * (uxy[i] - ux[i] * uy[i]);
    }

    // calculate SSIM
    float C1 = (K1 * data_range) * (K1 * data_range);
    float C2 = (K2 * data_range) * (K2 * data_range);

    for (int i = 0; i < width * height; i++) {
        float A1 = 2.0f * ux[i] * uy[i] + C1;
        float A2 = 2.0f * vxy[i] + C2;
        float B1 = ux[i] * ux[i] + uy[i] * uy[i] + C1;
        float B2 = vx[i] + vy[i] + C2;
        float D = B1 * B2;

        if (D == 0.0f) {
            S[i] = 1.0f;
        } else {
            S[i] = (A1 * A2) / D;
        }
    }

    // calculate mean SSIM(ignore boundary)
    int pad = (win_size - 1) / 2;
    double mssim = 0.0f;
    int count = 0;
    float *pad_S = (float*)malloc((width-2*pad) * (height-2*pad) * sizeof(float));
    for (int y = pad; y < height - pad; y++) {
        for (int x = pad; x < width - pad; x++) {
            mssim += S[y * width + x];
            pad_S[(y - pad) * (width - 2*pad) + (x - pad)] = S[y * width + x];
            count++;
        }
    }
    mssim /= count;

    // output complete SSIM diffmap
    if (full && diff_map) {
        *diff_map = (float*)malloc(width * height * sizeof(float));
        memcpy(*diff_map, S, width * height * sizeof(float));
    }

    free(ux); free(uy); free(uxx); free(uyy); free(uxy);
    free(vx); free(vy); free(vxy); free(S);

    return mssim;
}

static int get_format_size(int format, int width, int height) {
    switch (format) {
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
            return width * height * 3;
        case FORMAT_GRAY:
            return width * height;
        default:
            printf("format error\n");
            return -1;
    }
}


// read binary img(raw)
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

static void fill_image(unsigned char* input, int img_size) {
    for (int i = 0; i < img_size; ++i) {
        input[i] = rand() % 256;
    }
}

static float ssim_cpu(
    int width,
    int height,
    int win_size,
    // bool gradient,
    float data_range,
    int format,
    bool gaussian_weights,
    bool full,
    unsigned char* im1_data,
    unsigned char* im2_data,
    float*** diff_map_cpu
) {
    int channels = 1;
    if (format != FORMAT_GRAY) channels = 3;

    float total_ssim = 0.0f;

    if (full && diff_map_cpu) {
        *diff_map_cpu = (float**)malloc(channels * sizeof(float*));
        if (*diff_map_cpu == NULL) {
            fprintf(stderr, "Error: Failed to allocate diff_map array. \n");
            return -1.0f;
        }
        for (int c = 0; c < channels; c++) {
            (*diff_map_cpu)[c] = NULL;
        }
    }

    for (int c = 0; c < channels; c++) {
        float* tmp1 = (float*)malloc(width * height * sizeof(float));
        float* tmp2 = (float*)malloc(width * height * sizeof(float));

        for (int i = 0; i < width * height; i++) {
            tmp1[i] = im1_data[c * width * height + i];
            tmp2[i] = im2_data[c * width * height + i];
        }

        float *local_diff_map = NULL;

        float ssim_ch = structural_similarity_channel(
            tmp1, tmp2,
            width, height,
            win_size,
            // gradient,
            data_range,
            gaussian_weights,
            full,
            full ? &local_diff_map : NULL
        );

        if (full && diff_map_cpu && local_diff_map) {
            (*diff_map_cpu)[c] = local_diff_map;
        }

        total_ssim += ssim_ch;
    }

    return total_ssim / channels;
}

static int ssim_tpu(
    int width,
    int height,
    int win_size,
    // bool gradient,
    float data_range,
    int format,
    bool gaussian_weights,
    bool full,
    unsigned char* im1_data,
    unsigned char* im2_data,
    float** diff_map_tpu,
    float* mssim_tpu,
    bm_handle_t handle
) {
    int ret = 0;
    struct timeval t1, t2;
    bm_image bm_im1;
    bm_image bm_im2;

    bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &bm_im1);
    bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &bm_im2);
    bm_image_alloc_dev_mem(bm_im1);
    bm_image_alloc_dev_mem(bm_im2);

    int im1_byte_size[4] = {0};
    int im2_byte_size[4] = {0};
    bm_image_get_byte_size(bm_im1, im1_byte_size);
    bm_image_get_byte_size(bm_im2, im2_byte_size);
    void* im1_addr[4] = {(void *)im1_data,
                         (void *)(im1_data + im1_byte_size[0]),
                         (void *)(im1_data + im1_byte_size[0] + im1_byte_size[1]),
                         (void *)(im1_data + im1_byte_size[0] + im1_byte_size[1] + im1_byte_size[2])};
    void* im2_addr[4] = {(void *)im2_data,
                         (void *)(im2_data + im2_byte_size[0]),
                         (void *)(im2_data + im2_byte_size[0] + im2_byte_size[1]),
                         (void *)(im2_data + im2_byte_size[0] + im2_byte_size[1] + im2_byte_size[2])};
    bm_image_copy_host_to_device(bm_im1, (void **)im1_addr);
    bm_image_copy_host_to_device(bm_im2, (void **)im2_addr);

    gettimeofday(&t1, NULL);
    // ret = bmcv_image_ssim(handle, win_size, gradient, data_range, gaussian_weights, full, bm_im1, bm_im2, diff_map_tpu, mssim_tpu);
    ret = bmcv_image_ssim(handle, win_size, data_range, gaussian_weights, full, bm_im1, bm_im2, diff_map_tpu, mssim_tpu);
    gettimeofday(&t2, NULL);
    printf("ssim TPU using time: %ld us\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (ret != BM_SUCCESS) {
        printf("bmcv_image_ssim error \n");
    }

    return ret;
}

int cmp_res(float* diff_map_cpu, float* diff_map_tpu, int width, int height) {
    int ret = 0;
    for (int i = 0; i < width * height; i++) {
        if (fabs(diff_map_cpu[i] - diff_map_tpu[i]) > 0.1) {
            printf("diff_map_cpu = %.3f, diff_map_tpu = %.3f", diff_map_cpu[i], diff_map_tpu[i]);
            ret = -1;
            break;
        }
    }
    return ret;
}

static int test_ssim_random(
    int use_real_img,
    int width,
    int height,
    int win_size,
    bool gradient,
    float data_range,
    int format,
    bool gaussian_weights,
    bool full,
    char* im1_path,
    char* im2_path,
    char* output_path,
    bm_handle_t handle
) {
    printf("use_real_img: %d, width: %d, height: %d, "
           "win_size: %d, gradient: %s, data_range: %f, format: %d, "
           "gaussian_weights: %s, full: %s\n",
           use_real_img, width, height, win_size,
           gradient ? "true" : "false", data_range, format,
           gaussian_weights ? "true" : "false", full ? "true" : "false");
    int ret = 0;
    int channel = 1;
    unsigned char* im1_data;
    unsigned char* im2_data;
    float** diff_map_cpu = NULL;
    float* diff_map_tpu = NULL;

    if  (format == FORMAT_BGR_PLANAR || format == FORMAT_RGB_PLANAR) {
        channel = 3;
    } else if (format == FORMAT_GRAY) {
        channel = 1;
    } else {
        printf("Not supported format!\n");
        return -1;
    }
    im1_data = (unsigned char*)malloc(width * height * channel * sizeof(unsigned char));
    im2_data = (unsigned char*)malloc(width * height * channel * sizeof(unsigned char));

    if (use_real_img) {
        read_bin(im1_path, im1_data, width, height, format);
        read_bin(im2_path, im2_data, width, height, format);
    } else {
        fill_image(im1_data, width * height * channel);
        fill_image(im2_data, width * height * channel);
    }

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    // float mssim_cpu = ssim_cpu(width, height, win_size, gradient, data_range, format, gaussian_weights, full, im1_data, im2_data, &diff_map_cpu);
    float mssim_cpu = ssim_cpu(width, height, win_size, data_range, format, gaussian_weights, full, im1_data, im2_data, &diff_map_cpu);
    gettimeofday(&t2, NULL);
    printf("Mean SSIM (CPU): %.6lf\n", mssim_cpu);
    printf("ssim CPU using time: %ld us \n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    float mssim_tpu = 0.0f;
    // ret = ssim_tpu(width, height, win_size, gradient, data_range, format, gaussian_weights, full, im1_data, im2_data, &diff_map_tpu, &mssim_tpu);
     ret = ssim_tpu(width, height, win_size, data_range, format, gaussian_weights, full, im1_data, im2_data, &diff_map_tpu, &mssim_tpu, handle);
    if (ret != BM_SUCCESS) {
        goto exit;
        return ret;
    }
    printf("Mean SSIM (TPU): %.6lf\n", mssim_tpu);
    if (full && diff_map_cpu && diff_map_tpu) {
        for (int i = 0; i < channel; i++) {
            ret = cmp_res(diff_map_cpu[i], &diff_map_tpu[i * width * height], width, height);
            if (ret != 0) {
                printf("cpu diff_map compare with tpu diff_map failed!\n");
                break;
            }
        }
    }
    if (mssim_cpu - mssim_tpu > 0.001) {
        ret = -1;
        printf("cpu ssim compare with tpu ssim failed!\n");
    }

    if (output_path != NULL) {
        printf("output_path: %s", output_path);
    }

exit:
    free(im1_data);
    free(im2_data);
    free(diff_map_tpu);
    free(diff_map_cpu);

    return ret;
}

void* test_ssim(void* args) {
    cv_ssim_thread_arg_t* cv_ssim_thread_arg = (cv_ssim_thread_arg_t*)args;
    int loop = cv_ssim_thread_arg->loop;
    int use_real_img = cv_ssim_thread_arg->use_real_img;
    int width = cv_ssim_thread_arg->width;
    int height = cv_ssim_thread_arg->height;
    int win_size = cv_ssim_thread_arg->win_size;
    bool gradient = cv_ssim_thread_arg->gradient;
    float data_range = cv_ssim_thread_arg->data_range;
    int format = cv_ssim_thread_arg->format;
    bool gaussian_weights = cv_ssim_thread_arg->gaussian_weights;
    bool full = cv_ssim_thread_arg->full;
    char *im1_path = cv_ssim_thread_arg->im1_path;
    char *im2_path = cv_ssim_thread_arg->im2_path;
    char *output_path = cv_ssim_thread_arg->output_path;
    bm_handle_t handle = cv_ssim_thread_arg->handle;
    for (int i = 0; i < loop; i++) {
        if (loop > 1) {
            use_real_img = 0;
            width = 3840;
            height = 2160;
            int rand_num = rand() % 5 + 1;
            win_size = rand_num * 2 + 1;
            gradient = false;
            data_range = 255.0f;

            int fmt[3] = {FORMAT_GRAY, FORMAT_BGR_PLANAR, FORMAT_RGB_PLANAR};
            int size = sizeof(fmt) / sizeof(fmt[0]);
            rand_num = rand() % size;
            format = fmt[rand_num];

            gaussian_weights = rand() % 2;
            full = rand() % 2;
            im1_path = NULL;
            im2_path = NULL;
            output_path = NULL;
        }
        if (0 != test_ssim_random(use_real_img, width, height, win_size, gradient, data_range, format, gaussian_weights, full, im1_path, im2_path, output_path, handle)) {
            printf("------TEST CV_SSIM FAILED------\n");
            exit(-1);
        }
        printf("------TEST CV_SSIM PASSED!------\n");
    }
    return NULL;
}


int main(int argc, char* argv[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);

    static bm_handle_t handle;
    int thread_num = 1;
    int loop = 1;
    int use_real_img = 0;
    // int width1 = 8 + rand() % 8185;
    // int height1 = 8 + rand() % 8185;
    int width = 3840;
    int height = 2160;
    int rand_num = rand() % 5 + 1;
    int win_size = rand_num * 2 + 1;
    bool gradient = false;
    float data_range = 255.0f;
    int fmt[3] = {FORMAT_GRAY, FORMAT_BGR_PLANAR, FORMAT_RGB_PLANAR};
    int size = sizeof(fmt) / sizeof(fmt[0]);
    rand_num = rand() % size;
    int format = fmt[rand_num];

    bool gaussian_weights = rand() % 2;
    bool full = rand() % 2;

    char *im1_path = NULL;
    char *im2_path = NULL;
    char *output_path = NULL;

    if (argc == 2 && atoi(argv[1]) == -1) {
        printf("usage: \n");
        printf("%s thread_num loop use_real_img width height win_size gradient data_range format gaussian_weights full im1_path im2_path output_path\n", argv[0]);
        printf("example: \n");
        printf("%s \n", argv[0]);
        printf("%s 2 \n", argv[0]);
        printf("%s 2 1 0 3840 2160 \n", argv[0]);
        printf("%s 2 1 1 3840 2160 11 0 255 14 1 1 gray1.bin gray2.bin diff_map.bin\n", argv[0]);
        return 0;
    }

    if(argc > 1) thread_num= atoi(argv[1]);
    if(argc > 2) loop = atoi(argv[2]);
    if(argc > 3) use_real_img = atoi(argv[3]);
    if(argc > 4) width = atoi(argv[4]);
    if(argc > 5) height = atoi(argv[5]);
    if(argc > 6) win_size = atoi(argv[6]);
    if(argc > 7) gradient = atoi(argv[7]);
    if(argc > 8) data_range= atof(argv[8]);
    if(argc > 9) format = atoi(argv[9]);
    if(argc > 10) gaussian_weights = atoi(argv[10]);
    if(argc > 11) full = atoi(argv[11]);
    if(argc > 12) im1_path = argv[12];
    if(argc > 13) im2_path = argv[13];
    if(argc > 14) output_path = argv[14];

    int dev_id = 0;
    bm_status_t status = bm_dev_request(&handle, dev_id);
    if (status != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", status);
        exit(-1);
    }

    pthread_t pid[thread_num];
    cv_ssim_thread_arg_t cv_ssim_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_ssim_thread_arg[i].loop = loop;
        cv_ssim_thread_arg[i].use_real_img = use_real_img;
        cv_ssim_thread_arg[i].width = width;
        cv_ssim_thread_arg[i].height = height;
        cv_ssim_thread_arg[i].win_size = win_size;
        cv_ssim_thread_arg[i].gradient = gradient;
        cv_ssim_thread_arg[i].data_range = data_range;
        cv_ssim_thread_arg[i].format = format;
        cv_ssim_thread_arg[i].gaussian_weights = gaussian_weights;
        cv_ssim_thread_arg[i].full = full;
        cv_ssim_thread_arg[i].im1_path = im1_path;
        cv_ssim_thread_arg[i].im2_path = im2_path;
        cv_ssim_thread_arg[i].output_path = output_path;
        cv_ssim_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_ssim, cv_ssim_thread_arg + i) != 0) {
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
    return status;

    return 0;
}
