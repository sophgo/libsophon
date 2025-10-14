#include <stdio.h>
#include "bmcv_api_ext.h"
#include "stdlib.h"
#include "string.h"
#include <assert.h>
#include <float.h>
#include <sys/time.h>
#include <pthread.h>
#include "test_misc.h"
#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

typedef struct {
    int loop_num;
    int vec_dims;
    int vec_num;
    int use_real_matrix;
    char* input_path;
    char* output_path;
    bm_handle_t handle;
} cv_cos_similarity_thread_arg_t;

static int parameters_check(int vec_dims, int vec_num) {
    if (vec_dims > 256 || vec_num > 6000) {
        printf("Unsupported size : max_vec_dims: 256  max_vec_num:6000\n");
        return 1;
    }
    return 0;
}

float random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

static void fill_matrix(
        float** input_matrix,
        int vec_num,
        int vec_dims) {
    for (int j = 0; j < vec_num; j++) {
        for(int k = 0; k < vec_dims; k++){
            float num = random_float(-100.0f, 300.0f);
            input_matrix[j][k] = num;
        }
    }
}

static void normalize_matrix(float **input_matrix, int rows, int cols, float** normalized_matrix) {
    for (int i = 0; i < rows; i++) {
        float norm = 0.0;
        for (int j = 0; j < cols; j++) {
            norm += input_matrix[i][j] * input_matrix[i][j];
        }
        norm = sqrt(norm);
        for (int j = 0; j < cols; j++) {
            normalized_matrix[i][j] = input_matrix[i][j] / norm;
        }
    }
}

static void matrix_multiply_with_transpose(float **normalized_matrix, float **output_matrix_cpu, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < rows; j++) {
            float dot_product = 0.0;
            for (int k = 0; k < cols; k++) {
                dot_product += normalized_matrix[i][k] * normalized_matrix[j][k];
            }
            output_matrix_cpu[i][j] = 0.5 * (1.0 + dot_product);
        }
    }
}

static void print_matrix(float **M, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%f ", M[i][j]);
        }
        printf("\n");
    }
}

static void cos_similarity_cpu(float** input_matrix, float** output_matrix_cpu, int vec_num, int vec_dims) {
    float** normalized_matrix = (float**)malloc(vec_num * sizeof(float*));
    for (int i = 0; i < vec_num; i++) {
        normalized_matrix[i] = (float*)malloc(vec_dims * sizeof(float));
    }
    normalize_matrix(input_matrix, vec_num, vec_dims, normalized_matrix);
    matrix_multiply_with_transpose(normalized_matrix, output_matrix_cpu, vec_num, vec_dims);
    for (int i = 0; i < vec_num; ++i) {
        free(normalized_matrix[i]);
    }
    free(normalized_matrix);
}

static void cos_similarity_tpu(bm_handle_t handle, float** input_matrix, float* output_tpu, int vec_num, int vec_dims) {
    bm_status_t ret;
    float* input_data = (float*)malloc(vec_num * vec_dims * sizeof(float));
    for (int i = 0; i < vec_num; ++i) {
        for (int j = 0; j < vec_dims; ++j) {
            input_data[i * vec_dims + j] = input_matrix[i][j];
        }
    }

    bm_device_mem_t input_data_global_addr, normalized_data_global_addr, output_data_global_addr;
    bm_malloc_device_byte(handle, &input_data_global_addr, sizeof(float) * vec_num * vec_dims);
    bm_malloc_device_byte(handle, &normalized_data_global_addr, sizeof(float) * vec_num * vec_dims);
    bm_malloc_device_byte(handle, &output_data_global_addr, sizeof(float) * vec_num * vec_num);
    bm_memcpy_s2d(handle, input_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_cos_similarity(handle,
                              input_data_global_addr,
                              normalized_data_global_addr,
                              output_data_global_addr,
                              vec_num,
                              vec_dims);
    gettimeofday(&t2, NULL);
    printf("cos_similarity TPU1 using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if(ret != BM_SUCCESS) {
        printf("bmcv_cos_similarity failed!\n");
        bm_free_device(handle, input_data_global_addr);
        bm_free_device(handle, normalized_data_global_addr);
        bm_free_device(handle, output_data_global_addr);
        free(input_data);
        exit(-1);
    }
    gettimeofday(&t1, NULL);
    ret = bmcv_cos_similarity(handle,
                              input_data_global_addr,
                              normalized_data_global_addr,
                              output_data_global_addr,
                              vec_num,
                              vec_dims);
    gettimeofday(&t2, NULL);
    printf("cos_similarity TPU2 using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if(ret != BM_SUCCESS) {
        printf("bmcv_cos_similarity failed!\n");
        bm_free_device(handle, input_data_global_addr);
        bm_free_device(handle, normalized_data_global_addr);
        bm_free_device(handle, output_data_global_addr);
        free(input_data);
        exit(-1);
    }
    bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(output_tpu)), output_data_global_addr);
    bm_free_device(handle, input_data_global_addr);
    bm_free_device(handle, normalized_data_global_addr);
    bm_free_device(handle, output_data_global_addr);
    free(input_data);
}

static bool result_compare(float* output_cpu, float* output_tpu, int vec_num) {
    for(int i = 0; i < vec_num; i++) {
        for(int j = 0; j < vec_num; j++) {
            if(fabs(output_cpu[i * vec_num + j] - output_tpu[i * vec_num + j]) > 0.1) {
                printf("cos_similarity cmp failed\n");
                printf("error index: i = %d, j = %d, cpu_output: %f, tpu_output: %f\n", i, j, output_cpu[i * vec_num + j], output_tpu[i * vec_num + j]);
                return false;
            }
        }
    }
    return true;
}

static int test_cos_similarity_random(
        int vec_dims,
        int vec_num,
        int use_real_matrix,
        char *input_path,
        char *output_path,
        bm_handle_t handle) {
    printf("vec_num = %d, vec_dims = %d\n", vec_num, vec_dims);
    struct timeval t1, t2;
    int i;
    float* output_cpu = (float*)malloc(vec_num * vec_num * sizeof(float));
    float* output_tpu = (float*)malloc(vec_num * vec_num * sizeof(float));
    float** input_matrix = (float**)malloc(vec_num * sizeof(float*));
    float** output_matrix_cpu = (float**)malloc(vec_num * sizeof(float*));
    for (i = 0; i < vec_num; i++) {
        input_matrix[i] = (float*)malloc(vec_dims * sizeof(float));
        output_matrix_cpu[i] = (float*)malloc(vec_num * sizeof(float));
        if (input_matrix[i] == NULL || output_matrix_cpu[i] == NULL) {
            perror("Memory allocation for matrix failed");
            exit(EXIT_FAILURE);
        }
    }
    if(use_real_matrix) {
        FILE* file = fopen(input_path, "r");
        if (file == NULL) {
            perror("Error opening file");
            return 1;
        }
        for (i = 0; i < vec_num; i++) {
            for (int j = 0; j < vec_dims; j++) {
                if (fscanf(file, "%f", &input_matrix[i][j]) != 1) {
                    perror("Error or end of file reached while reading the matrix");
                    fclose(file);
                    for (int k = 0; k <= i; k++) {
                        free(input_matrix[k]);
                    }
                    free(input_matrix);
                    return 1;
                }
            }
        }
        printf("read input_matrix success\n");
        fclose(file);
    } else {
        fill_matrix(input_matrix, vec_num, vec_dims);
    }
    gettimeofday(&t1, NULL);
    cos_similarity_cpu(input_matrix, output_matrix_cpu, vec_num, vec_dims);
    gettimeofday(&t2, NULL);
    printf("cos_similarity CPU using time: %ld(us)\n", TIME_COST_US(t1, t2));
    cos_similarity_tpu(handle, input_matrix, output_tpu, vec_num, vec_dims);
    for (i = 0; i < vec_num; ++i) {
        for (int j = 0; j < vec_num; ++j) {
            output_cpu[i * vec_num + j] = output_matrix_cpu[i][j];
        }
    }
    if (use_real_matrix) {
        FILE* file = fopen(output_path, "w");
        if (file == NULL) {
            perror("Error opening file");
            return 1;
        }

        for (i = 0; i < vec_num; i++) {
            for (int j = 0; j < vec_num; j++) {
                int index = i * vec_num + j;
                fprintf(file, "%f ", output_tpu[index]);
            }
            fprintf(file, "\n");
        }
        printf("write output_matrix success\n");
        fclose(file);

    }
    if (false == result_compare(output_cpu, output_tpu, vec_num)) {
        printf("-------------cos_similarity compare failed!-------\n");
        exit(-1);
    }
    printf("-------------cos_similarity compare succeed!-----------\n");

    free(output_cpu);
    free(output_tpu);
    for (i = 0; i < vec_num; ++i) {
        free(input_matrix[i]);
        free(output_matrix_cpu[i]);
    }
    free(input_matrix);
    free(output_matrix_cpu);
    return BM_SUCCESS;
}

void* test_cos_similarity(void* args) {
    cv_cos_similarity_thread_arg_t* cv_cos_similarity_thread_arg = (cv_cos_similarity_thread_arg_t*)args;
    int loop_num = cv_cos_similarity_thread_arg->loop_num;
    int vec_dims = cv_cos_similarity_thread_arg->vec_dims;
    int vec_num = cv_cos_similarity_thread_arg->vec_num;
    int use_real_matrix = cv_cos_similarity_thread_arg->use_real_matrix;
    char* input_path = cv_cos_similarity_thread_arg->input_path;
    char* output_path = cv_cos_similarity_thread_arg->output_path;
    bm_handle_t handle = cv_cos_similarity_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if(loop_num > 1) {
            vec_num += 1;
            vec_dims = 256;
        }
        if (0 != test_cos_similarity_random(vec_dims, vec_num, use_real_matrix, input_path, output_path, handle)){
            printf("------TEST CV_cos_similarity FAILED------\n");
            exit(-1);
        }
        printf("------TEST CV_cos_similarity PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    int loop = 1;
    int use_real_matrix = 0;
    int vec_dims = 256;
    int vec_num = 2164;
    int thread_num = 1;
    int check = 0;
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
        printf("%s thread_num loop vec_dims vec_num use_real_matrix input_path output_path(when use_real_matrix = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) vec_dims = atoi(args[3]);
    if (argc > 4) vec_num = atoi(args[4]);
    if (argc > 5) use_real_matrix = atoi(args[5]);
    if (argc > 6) input_path = args[6];
    if (argc > 7) output_path = args[7];
    check = parameters_check(vec_dims, vec_num);
    if (check) {
        printf("Parameters Failed! \n");
        return check;
    }

    // test for multi-thread
    pthread_t pid[thread_num];
    cv_cos_similarity_thread_arg_t cv_cos_similarity_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_cos_similarity_thread_arg[i].loop_num = loop;
        cv_cos_similarity_thread_arg[i].vec_dims = vec_dims;
        cv_cos_similarity_thread_arg[i].vec_num = vec_num;
        cv_cos_similarity_thread_arg[i].use_real_matrix = use_real_matrix;
        cv_cos_similarity_thread_arg[i].input_path = input_path;
        cv_cos_similarity_thread_arg[i].output_path = output_path;
        cv_cos_similarity_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_cos_similarity, cv_cos_similarity_thread_arg + i) != 0) {
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
