#include "bmcv_api_ext.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "string.h"
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#define ERR_MAX 1e-3
#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

enum Win_mode {
    HANN = 0,
    HAMM = 1,
};

enum Pad_mode {
    CONSTANT = 0,
    REFLECT = 1,
};

typedef struct {
    int loop_num;
    int length;
    int batch;
    bool real_input;
    int pad_mode;
    int win_mode;
    char* src_name;
    char* dst_name;
    char* gold_name;
    bool norm;
    int n_fft;
    int hop_len;
} cv_istft_thread_arg_t;

typedef struct {
    float real;
    float imag;
} Complex;

static void readBin_4b(const char* path, float* input_data, int size)
{
    FILE *fp_src = fopen(path, "rb");
    if (fread((void *)input_data, 4, size, fp_src) < (unsigned int)size){
        printf("file size is less than %d required bytes\n", size);
    };

    fclose(fp_src);
}

static void readBin_8b(const char* path, float* Host_R, float* Host_I, int size)
{
    FILE *fp_src = fopen(path, "rb");
    Complex* last_data = (Complex*)malloc(size * sizeof(Complex));
    int i;

    if (fread((void *)last_data, 8, size, fp_src) < (unsigned int)size){
        printf("file size is less than %d required bytes\n", size);
    };

    for (i = 0; i < size; ++i) {
        Host_R[i] = last_data[i].real;
        Host_I[i] = last_data[i].imag;
    }

    free(last_data);
    fclose(fp_src);
}

static void writeBin_8b(const char * path, float* input_XR, float* input_XI, int size)
{
    FILE *fp_dst = fopen(path, "wb");
    Complex* last_data = (Complex*)malloc(size * sizeof(Complex));
    int i;

    for (i = 0; i < size; ++i) {
        last_data[i].real = input_XR[i];
        last_data[i].imag = input_XI[i];
    }

    if (fwrite((void *)last_data, 8, size, fp_dst) < (unsigned int)size){
        printf("file size is less than %d required bytes\n", size);
    };

    free(last_data);
    fclose(fp_dst);
}

static int cmp_res(float* tpu, float* cpu, int L, int batch)
{
    int i, j, index;

    for(i = 0; i < batch; ++i) {
        for (j = 0; j < L; ++j) {
            index = i * L + j;
            if (fabs(cpu[index] - tpu[index]) > ERR_MAX) {
                    printf("the cpu_res[%d].real = %f, the tpu_res[%d].real = %f\n",
                            index, cpu[index], index, tpu[index]);
                    return -1;
            }
        }
    }
    return 0;
}
void fft(float* in_real, float* in_imag, float* output_real, float* output_imag, int length, int step, bool forward)
{
    int i;

    if (length == 1) {
        output_real[0] = in_real[0];
        output_imag[0] = in_imag[0];
        return;
    }

    fft(in_real, in_imag, output_real, output_imag, length / 2, 2 * step, forward);
    fft(in_real + step, in_imag + step, output_real + length / 2, output_imag + length / 2, length / 2, 2 * step, forward);

    for (i = 0; i < length / 2; ++i) {
        double angle = forward ? -2 * M_PI * i / length : 2 * M_PI * i / length;
        double wr = cos(angle);
        double wi = sin(angle);

        float tr = wr * output_real[i + length / 2] - wi * output_imag[i + length / 2];
        float ti = wr * output_imag[i + length / 2] + wi * output_real[i + length / 2];

        output_real[i + length / 2] = output_real[i] - tr;
        output_imag[i + length / 2] = output_imag[i] - ti;
        output_real[i] += tr;
        output_imag[i] += ti;
    }
}

void transpose(int rows, int cols, float* input, float* output)
{
    int i, j;

    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            output[j * rows + i] = input[i * cols + j];
        }
    }
}

void apply_window(float* window, int n, int win_mode)
{
    int i;

    if (win_mode == HANN) {
        for (i = 0; i < n; i++) {
            window[i] *= 0.5 * (1 - cos(2 * M_PI * (float)i / n));
        }
    } else if (win_mode == HAMM) {
        for (i = 0; i < n; i++) {
            window[i] *= 0.54 - 0.46 * cos(2 * M_PI * (float)i / n);
        }
    }
}

void normalize_stft(float* res_XR, float* res_XI, int frm_len, int num)
{
    float norm_fac;
    int i;

    norm_fac = 1.0f / sqrtf((float)frm_len);

    for (i = 0; i < num; ++i) {
        res_XR[i] *= norm_fac;
        res_XI[i] *= norm_fac;
    }
}

static bm_status_t tpu_istft(float* XRHost, float* XIHost, float* YRHost, float* YIHost, int L,
                            int batch, bool realInput, int pad_mode,
                            int win_mode, int n_fft, int hop_len, bool norm, bm_handle_t handle)
{
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
    ret = bmcv_istft(handle, XRHost, XIHost, YRHost, YIHost, batch, L, realInput,
                    pad_mode, n_fft, win_mode, hop_len, norm);
    if (ret != BM_SUCCESS) {
        printf("bmcv_istft failed!\n");
        return ret;
    }
    gettimeofday(&t2, NULL);
    printf("ISTFT TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    return ret;
}

int cpu_istft(float* input_R, float* input_I, float* out_R, float* out_I, int L,
                    int batch, int n_fft, bool real_input, int win_mode, int hop_length, bool norm)
{
    int hop, i, j;
    int pad_length = n_fft / 2;
    int pad_signal_len = L + 2 * pad_length;
    int row_num = n_fft / 2 + 1;
    // int hop_length = n_fft / 4;
    int num_frames = 1 + L / hop_length;
    float* input_XR = (float*)malloc(n_fft * sizeof(float));
    float* input_XI = (float*)malloc(n_fft * sizeof(float));
    float* output_XR = (float*)malloc(n_fft * sizeof(float));
    float* output_XI = (float*)malloc(n_fft * sizeof(float));
    float* output_XR_tmp = (float*)malloc(batch * pad_signal_len * sizeof(float));
    float* output_XI_tmp = (float*)malloc(batch * pad_signal_len * sizeof(float));
    float* win_squares = (float*)malloc(n_fft * sizeof(float));
    float* cumul_win_squares = (float*)malloc(pad_signal_len * sizeof(float));
    float* input_r = (float*)malloc(batch * num_frames * row_num * sizeof(float));
    float* input_i = (float*)malloc(batch * num_frames * row_num * sizeof(float));

    memset(cumul_win_squares, 0, pad_signal_len * sizeof(float));
    memset(output_XR_tmp, 0, batch * pad_signal_len * sizeof(float));
    if (!real_input) {
        memset(output_XI_tmp, 0, batch * pad_signal_len * sizeof(float));
    }

    /* Compute the window squares */
    if (win_mode == HANN) {
        for (i = 0; i < n_fft; i++) {
            win_squares[i] = 0.5 * (1 - cos(2 * M_PI * (float)i / n_fft));
            win_squares[i] = win_squares[i] * win_squares[i];
        }
    } else if (win_mode == HAMM) {
        for (i = 0; i < n_fft; i++) {
            win_squares[i] = 0.54 - 0.46 * cos(2 * M_PI * (float)i / n_fft);
            win_squares[i] = win_squares[i] * win_squares[i];
        }
    }

    for (hop = 0; hop < num_frames; ++hop) {
        for (i = 0; i < n_fft; ++i) {
            cumul_win_squares[hop * hop_length + i] += win_squares[i];
        }
    }

    for (j = 0; j < batch; j++) {
        transpose(row_num, num_frames, &input_R[j * row_num * num_frames], &input_r[j * row_num * num_frames]);
        transpose(row_num, num_frames, &input_I[j * row_num * num_frames], &input_i[j * row_num * num_frames]);

        /* calculate window & idft */
        for (hop = 0; hop < num_frames; ++hop) {
            memcpy(input_XR, &input_r[j * row_num * num_frames + hop * row_num], row_num * sizeof(float));
            memcpy(input_XI, &input_i[j * row_num * num_frames + hop * row_num], row_num * sizeof(float));

            /* Expand single-side spectrum to full spectrum */
            for (i = 1; i < row_num - 1; ++i) {
                input_XR[n_fft - i] = input_r[j * row_num * num_frames + hop * row_num + i];
                input_XI[n_fft - i] = -1 * input_i[j * row_num * num_frames + hop * row_num + i];
            }

            fft(input_XR, input_XI, output_XR, output_XI, n_fft, 1, false);

            if (norm) {
                normalize_stft(output_XR, output_XI, n_fft, n_fft);
            } else {
                normalize_stft(output_XR, output_XI, n_fft, n_fft);
                normalize_stft(output_XR, output_XI, n_fft, n_fft);
            }

            apply_window(output_XR, n_fft, win_mode);
            if (!real_input) {
                apply_window(output_XI, n_fft, win_mode);
            }

            for (i = 0; i < n_fft; ++i) {
                if (!real_input) {
                    output_XI_tmp[j * pad_signal_len + hop * hop_length + i] += output_XI[i];
                }
                output_XR_tmp[j * pad_signal_len + hop * hop_length + i] += output_XR[i];
            }
        }

        for (i = 0; i < pad_signal_len; ++i) {
            output_XR_tmp[j * pad_signal_len + i] = output_XR_tmp[j * pad_signal_len + i] / cumul_win_squares[i];
            if (!real_input) {
                output_XI_tmp[j * pad_signal_len + i] = output_XI_tmp[j * pad_signal_len + i] / cumul_win_squares[i];
            }
        }

        memcpy(&out_R[j * L], &output_XR_tmp[j * pad_signal_len + n_fft / 2], L * sizeof(float));
        if (!real_input) {
            memcpy(&out_I[j * L], &output_XI_tmp[j * pad_signal_len + n_fft / 2], L * sizeof(float));
        }
    }

    free(input_XR);
    free(input_XI);
    free(output_XR);
    free(output_XI);
    free(output_XR_tmp);
    free(output_XI_tmp);
    free(win_squares);
    free(cumul_win_squares);
    free(input_r);
    free(input_i);
    return 0;
}

static int test_istft(int L, int batch, bool real_input, int pad_mode, int win_mode, bm_handle_t handle,
                        char* src_name, char* dst_name, char* gold_name, bool norm, int n_fft, int hop_length)
{
    int ret = 0;
    int i;
    struct timeval t1, t2;
    int num_frames = 1 + L / hop_length;
    int row_num = n_fft / 2 + 1;

    float* input_R = (float*)malloc(batch * row_num * num_frames * sizeof(float));
    float* input_I = (float*)malloc(batch * row_num * num_frames * sizeof(float));
    float* YRHost_cpu = (float*)malloc(L * batch * sizeof(float));
    float* YRHost_tpu = (float*)malloc(L * batch * sizeof(float));
    float* YIHost_cpu = (float*)malloc(L * batch * sizeof(float));
    float* YIHost_tpu = (float*)malloc(L * batch * sizeof(float));

    memset(input_R, 0, batch * row_num * num_frames * sizeof(float));
    memset(input_I, 0, batch * row_num * num_frames * sizeof(float));

    printf("the L=%d, the batch=%d, the n_fft=%d, the hop_len = %d, \
            the pad_mode=%d, the win_mode=%d, the noram = %d\n",
            L, batch, n_fft, hop_length, pad_mode, win_mode, norm);

    if (src_name != NULL) {
        readBin_8b(src_name, input_R, input_I, batch * row_num * num_frames);
    } else {
        for (i = 0; i < batch * row_num * num_frames; i++) {
            input_R[i] = (float)rand() / RAND_MAX;
            input_I[i] = (float)rand() / RAND_MAX;
        }
    }

    gettimeofday(&t1, NULL);
    ret = cpu_istft(input_R, input_I, YRHost_cpu, YIHost_cpu, L, batch,
                    n_fft, real_input, win_mode, hop_length, norm);
    if (ret != 0) {
        printf("cpu_istft fail!\n");
        goto exit;
    }
    gettimeofday(&t2, NULL);
    printf("ISTFT CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    if (dst_name != NULL) {
        writeBin_8b(dst_name, YRHost_cpu, YIHost_cpu, L * batch);
    }

    ret = tpu_istft(input_R, input_I, YRHost_tpu, YIHost_tpu, L, batch, real_input,
                    pad_mode, win_mode, n_fft, hop_length, norm, handle);
    if (ret != 0) {
        printf("tpu_istft failed!\n");
        goto exit;
    }

    if (gold_name != NULL) {
        printf("get the gold output tpu!\n");
        readBin_4b(gold_name, YRHost_cpu, L * batch);
    }

    ret = cmp_res(YRHost_tpu, YRHost_cpu, L, batch);
    if (ret != 0) {
        printf("cmp_res real result fail!\n");
        goto exit;
    }

    if (!real_input) {
        ret = cmp_res(YIHost_tpu, YIHost_cpu, L, batch);
        if (ret != 0) {
            printf("cmp_res img result fail!\n");
            goto exit;
        }
    }

exit:
    free(input_R);
    free(input_I);
    free(YRHost_cpu);
    free(YIHost_cpu);
    free(YRHost_tpu);
    free(YIHost_tpu);
    return ret;
}

void* test_thread_istft(void* args) {
    cv_istft_thread_arg_t* cv_istft_thread_arg = (cv_istft_thread_arg_t*)args;
    int loop = cv_istft_thread_arg->loop_num;
    int length = cv_istft_thread_arg->length;
    int batch = cv_istft_thread_arg->batch;
    bool real_input = cv_istft_thread_arg->real_input;
    int pad_mode = cv_istft_thread_arg->pad_mode;
    int win_mode = cv_istft_thread_arg->win_mode;
    int n_fft = cv_istft_thread_arg->n_fft;
    int hop_len = cv_istft_thread_arg->hop_len;
    bool norm = cv_istft_thread_arg->norm;
    char *src_name = cv_istft_thread_arg->src_name;
    char *dst_name = cv_istft_thread_arg->dst_name;
    char *gold_name = cv_istft_thread_arg->gold_name;
    int i, ret = 0;
    bm_handle_t handle;

    ret = (int)bm_dev_request(&handle, 0);
    if (ret) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }

    for (i = 0; i < loop; ++i) {
        ret = test_istft(length, batch, real_input, pad_mode, win_mode, handle, src_name, dst_name, gold_name, norm, n_fft, hop_len);
        if(ret) {
            printf("test_istft failed\n");
            bm_dev_free(handle);
            exit(-1);
        }
    }
    bm_dev_free(handle);
    return (void*)0;
}

int main(int argc, char* args[])
{
    struct timespec tp;
    clock_gettime(0, &tp);
    srand(tp.tv_nsec);

    int ret = 0;
    int i;
    int length = 4096;
    int batch = 1;
    int loop = 1;
    bool real_input = true;
    int pad_mode = REFLECT;
    int win_mode = HANN;
    int thread_num = 1;
    int n_fft = 4096;
    int hop_len = n_fft / 4;
    bool norm = true;
    char* src_name = NULL;
    char* dst_name = NULL;
    char* gold_name = NULL;

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: %d\n", argc);
        printf("%s thread_num loop length batch real_input pad_mode win_mode norm n_fft hop_len src_name dst_name gold_name) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 1 2\n", args[0]);
        printf("%s 1 1 4096 1 1 1 1 1 4096 1024\n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) length = atoi(args[3]);
    if (argc > 4) batch = atoi(args[4]);
    if (argc > 5) real_input = atoi(args[5]);
    if (argc > 6) pad_mode = atoi(args[6]);
    if (argc > 7) win_mode = atoi(args[7]);
    if (argc > 8) norm = atoi(args[8]);
    if (argc > 9) n_fft = atoi(args[9]);
    if (argc > 10) hop_len = atoi(args[10]);
    if (argc > 11) src_name = args[11];
    if (argc > 12) dst_name = args[12];
    if (argc > 13) gold_name = args[13];

    pthread_t pid[thread_num];
    cv_istft_thread_arg_t cv_istft_thread_arg[thread_num];

    for (i = 0; i < thread_num; i++) {
        cv_istft_thread_arg[i].loop_num = loop;
        cv_istft_thread_arg[i].length = length;
        cv_istft_thread_arg[i].batch = batch;
        cv_istft_thread_arg[i].real_input = real_input;
        cv_istft_thread_arg[i].pad_mode = pad_mode;
        cv_istft_thread_arg[i].win_mode = win_mode;
        cv_istft_thread_arg[i].norm = norm;
        cv_istft_thread_arg[i].n_fft = n_fft;
        cv_istft_thread_arg[i].hop_len = hop_len;
        cv_istft_thread_arg[i].src_name = src_name;
        cv_istft_thread_arg[i].dst_name = dst_name;
        cv_istft_thread_arg[i].gold_name = gold_name;
        if (pthread_create(&pid[i], NULL, test_thread_istft, &cv_istft_thread_arg[i]) != 0) {
            printf("create thread failed\n");
            return -1;
        }
    }
    for (i = 0; i < thread_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            exit(-1);
        }
    }

    printf("------Test ISTFT All Success!------\n");
    return ret;
}