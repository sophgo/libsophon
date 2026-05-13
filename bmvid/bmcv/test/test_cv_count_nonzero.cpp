#include <stdio.h>
#include <stdint.h>
#include "bmcv_api_ext_c.h"
#include "stdlib.h"
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

typedef struct {
    int loop_num;
    int height;
    int width;
    int channel;
    int use_real_img;
    char* input_path;
    bm_handle_t handle;
} cv_count_nonzero_thread_arg_t;

static void read_bin(const char *input_path, unsigned char *input_data, int width, int height) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL)
    {
        printf("Unable to open input file %s\n", input_path);
        return;
    }
    if(fread(input_data, sizeof(unsigned char), width * height, fp_src) != 0)
        printf("read image success\n");
    fclose(fp_src);
}

static void fill_image(unsigned char* input, int channel, int width, int height) {
    for (int i = 0; i < channel; i++) {
        for (int j = 0; j < height; j++) {
            for(int k = 0; k < width; k++){
                unsigned char num = random() % 256;
                input[i * width * height + j * width + k] = num;
            }
        }
    }
}

static void count_nonzero_cpu(unsigned char* input_data, int* output_idx_cpu, int height, int width, int channel, int* nonzero_count) {
    const int totalPixels = width * height * channel;
    int count = 0;
    for (int i = 0; i < totalPixels; ++i) {
        output_idx_cpu[i] = -1;
    }
    for (int i = 0; i < totalPixels; ++i) {
        if (input_data[i] != 0) {
            output_idx_cpu[count++] = i;
        }
    }
    *nonzero_count = count;
}

static int count_nonzero_tpu(unsigned char* input, int* output_idx_addr, int height, int width, int channel,
                             bm_handle_t handle, int* nonzero_count) {
    bm_device_mem_t input_addr, nonzero_idx_addr;
    struct timeval t1, t2;
    bm_malloc_device_byte(handle, &input_addr, width * height * channel * sizeof(unsigned char));
    bm_malloc_device_byte(handle, &nonzero_idx_addr, width * height * channel * sizeof(int));
    bm_memcpy_s2d(handle, input_addr, input);
    gettimeofday(&t1, NULL);
    bmcv_count_nonzero(handle, input_addr, nonzero_idx_addr, width, height, channel, nonzero_count);
    gettimeofday(&t2, NULL);
    printf("Count nonzero TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    bm_memcpy_d2s(handle, output_idx_addr, nonzero_idx_addr);
    bm_free_device(handle, input_addr);
    bm_free_device(handle, nonzero_idx_addr);
    return 0;
}

static int cmp(int* output_tpu, int* output_cpu, int nonzero_count_cpu, int nonzero_count_tpu) {
    if(nonzero_count_cpu != nonzero_count_tpu) {
        printf("nonzero_count_cpu != nonzero_count_tpu\n");
        return -1;
    }
    for (int i = 0; i < nonzero_count_cpu; i++) {
        if (output_tpu[i] != output_cpu[i]) {
            printf("cmp error: idx=%d  output_cpu=%d  output_tpu=%d\n", i, output_cpu[i], output_tpu[i]);
            return -1;
        }
    }
    return 0;
}

static int test_count_nonzero_random(
        int channel,
        int height,
        int width,
        int use_real_img,
        char *input_path,
        bm_handle_t handle) {
    printf("channel = %d, width: %d , height: %d\n", channel, width, height);
    int ret;
    struct timeval t1, t2;
    int nonzero_count_cpu = 0;
    int nonzero_count_tpu = 0;
    unsigned char* input_data = (unsigned char*)malloc(width * height * channel * sizeof(unsigned char));
    int* output_idx_cpu = (int*)malloc(width * height * channel * sizeof(int));
    int* output_idx_tpu = (int*)malloc(width * height * channel * sizeof(int));
    if(use_real_img == 1){
        read_bin(input_path, input_data, width, height);
    } else {
        fill_image(input_data, channel, width, height);
    }
    gettimeofday(&t1, NULL);
    count_nonzero_cpu(input_data, output_idx_cpu, height, width, channel, &nonzero_count_cpu);
    gettimeofday(&t2, NULL);
    printf("Count_nonzero CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    ret = count_nonzero_tpu(input_data, output_idx_tpu, height, width, channel, handle, &nonzero_count_tpu);
    if(ret != 0){
        free(input_data);
        free(output_idx_cpu);
        free(output_idx_tpu);
        return ret;
    }
    ret = cmp(output_idx_tpu, output_idx_cpu, nonzero_count_cpu, nonzero_count_tpu);
    if (ret == 0) {
        printf("TPU and CPU results comparison successful!\n");
    } else {
        printf("TPU and CPU results comparison failed!\n");
    }
    free(input_data);
    free(output_idx_cpu);
    free(output_idx_tpu);
    return ret;
}

void* test_count_nonzero(void* args) {
    cv_count_nonzero_thread_arg_t* cv_count_nonzero_thread_arg = (cv_count_nonzero_thread_arg_t*)args;
    int loop_num = cv_count_nonzero_thread_arg->loop_num;
    int use_real_img = cv_count_nonzero_thread_arg->use_real_img;
    int height = cv_count_nonzero_thread_arg->height;
    int width = cv_count_nonzero_thread_arg->width;
    int channel = cv_count_nonzero_thread_arg->channel;
    char* input_path = cv_count_nonzero_thread_arg->input_path;
    bm_handle_t handle = cv_count_nonzero_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if(loop_num > 1) {
            height = 1 + rand() % 8192;
            width = 1 + rand() % 8192;
            channel = 1 + rand() % 2 * 2;
        }
        if (0 != test_count_nonzero_random(channel, height, width, use_real_img, input_path, handle)){
            printf("------TEST CV_COUNT_NONZERO FAILED------\n");
            exit(-1);
        }
        printf("------TEST CV_COUNT_NONZERO PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    int use_real_img = 0;
    int loop = 1;
    int height = 1 + rand() % 8192;
    int width = 1 + rand() % 8192;
    int channel = 1 + rand() % 2 * 2;
    int thread_num = 1;
    char *input_path = NULL;
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s thread_num loop use_real_img height width channel input_path(when use_real_img = 1,need to set input_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 2 1 0 512 512 8 \n", args[0]);
        printf("%s 1 1 1 1920 1080 8 res/1920x1080_rgbp.bin\n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) use_real_img = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    if (argc > 5) height = atoi(args[5]);
    if (argc > 6) channel = atoi(args[6]);
    if (argc > 7) input_path = args[7];

    // test for multi-thread
    pthread_t pid[thread_num];
    cv_count_nonzero_thread_arg_t cv_count_nonzero_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_count_nonzero_thread_arg[i].loop_num = loop;
        cv_count_nonzero_thread_arg[i].height = height;
        cv_count_nonzero_thread_arg[i].width = width;
        cv_count_nonzero_thread_arg[i].channel = channel;
        cv_count_nonzero_thread_arg[i].use_real_img = use_real_img;
        cv_count_nonzero_thread_arg[i].input_path = input_path;
        cv_count_nonzero_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_count_nonzero, cv_count_nonzero_thread_arg + i) != 0) {
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
