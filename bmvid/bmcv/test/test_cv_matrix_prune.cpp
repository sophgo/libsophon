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
    int matrix_dims;
    float p;
    int use_real_matrix;
    char* input_path;
    char* output_path;
    bm_handle_t handle;
} cv_matrix_prune_thread_arg_t;

static int parameters_check(int matrix_dims, float p) {
    if (matrix_dims > 6000) {
        printf("Unsupported size : max_matrix_dims: 6000\n");
        return 1;
    }
    if (matrix_dims < 8) {
        printf("Unsupported size : min_matrix_dims: 8\n");
        return 1;
    }
    if (p < 0.0 || p > 1.0) {
        printf("Unsupported size : p range: 0.0f~1.0f \n");
        return 1;
    }
    return 0;
}

float random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

static void fill_matrix(
        float** input_matrix,
        int matrix_dims) {
    for (int j = 0; j < matrix_dims; j++) {
        for(int k = 0; k < matrix_dims; k++){
            float num = random_float(-100.0f, 300.0f);
            input_matrix[j][k] = num;
        }
    }
}

static void matrix_transpose(float** src, float** dest, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            dest[j][i] = src[i][j];
        }
    }
}

typedef struct {
    float value;
    int index;
} IndexedValue;

int compareIndexedValues(const void* a, const void* b) {
    float diff = ((IndexedValue*)a)->value - ((IndexedValue*)b)->value;
    return (diff > 0) - (diff < 0);
}

static void matrix_prune_cpu(float** input_matrix, float** output_matrix_cpu, int matrix_dims, float p) {
    int n;
    if (matrix_dims < 1000) {
        n = (matrix_dims - 10 > 2) ? matrix_dims - 10 : 2;
    } else {
        n = (int)((1.0 - p) * matrix_dims);
    }
    IndexedValue* row = (IndexedValue*)malloc(matrix_dims * sizeof(IndexedValue));

    for (int i = 0; i < matrix_dims; i++) {
        for (int j = 0; j < matrix_dims; j++) {
            output_matrix_cpu[i][j] = input_matrix[i][j];
        }
        for (int j = 0; j < matrix_dims; j++) {
            row[j].value = input_matrix[i][j];
            row[j].index = j;
        }

        qsort(row, matrix_dims, sizeof(IndexedValue), compareIndexedValues);

        for (int j = 0; j < n; j++) {
            output_matrix_cpu[i][row[j].index] = 0.0f;
        }
        for (int j = n; j < matrix_dims; j++) {
            output_matrix_cpu[i][row[j].index] = 1.0f;
        }
    }

    float** transposed_matrix = (float**)malloc(matrix_dims * sizeof(float*));
    for (int i = 0; i < matrix_dims; ++i) {
        transposed_matrix[i] = (float*)malloc(matrix_dims * sizeof(float));
    }
    matrix_transpose(output_matrix_cpu, transposed_matrix, matrix_dims);
    for (int i = 0; i < matrix_dims; ++i) {
        for (int j = 0; j < matrix_dims; ++j) {
            output_matrix_cpu[i][j] = 0.5f * (output_matrix_cpu[i][j] + transposed_matrix[i][j]);
        }
        free(transposed_matrix[i]);
    }
    free(transposed_matrix);
    free(row);
}

static int matrix_prune_tpu(bm_handle_t handle, float** input_matrix, float* output_tpu, int matrix_dims, float p) {
    bm_status_t ret;
    float* input_data = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
    for (int i = 0; i < matrix_dims; ++i) {
        for (int j = 0; j < matrix_dims; ++j) {
            input_data[i * matrix_dims + j] = input_matrix[i][j];
        }
    }
    bm_device_mem_t input_data_global_addr, output_data_global_addr, sort_index_global_addr;
    bm_malloc_device_byte(handle, &input_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
    bm_malloc_device_byte(handle, &output_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
    bm_malloc_device_byte(handle, &sort_index_global_addr, sizeof(DT_INT32) * matrix_dims * matrix_dims);
    bm_memcpy_s2d(handle, input_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_matrix_prune(handle,
                      input_data_global_addr,
                      output_data_global_addr,
                      sort_index_global_addr,
                      matrix_dims,
                      p);
    gettimeofday(&t2, NULL);
    if (ret != BM_SUCCESS) {
        printf("bmcv_matrix_prune error!");
        bm_free_device(handle, input_data_global_addr);
        bm_free_device(handle, output_data_global_addr);
        bm_free_device(handle, sort_index_global_addr);
        free(input_data);
        return -1;
    }
    printf("cv_matrix_prune TPU1 using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    gettimeofday(&t1, NULL);
    ret = bmcv_matrix_prune(handle,
                      input_data_global_addr,
                      output_data_global_addr,
                      sort_index_global_addr,
                      matrix_dims,
                      p);
    gettimeofday(&t2, NULL);
    if (ret != BM_SUCCESS) {
        printf("bmcv_matrix_prune error!");
        bm_free_device(handle, input_data_global_addr);
        bm_free_device(handle, output_data_global_addr);
        bm_free_device(handle, sort_index_global_addr);
        free(input_data);
        return -1;
    }
    printf("cv_matrix_prune TPU2 using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(output_tpu)), output_data_global_addr);
    bm_free_device(handle, input_data_global_addr);
    bm_free_device(handle, output_data_global_addr);
    bm_free_device(handle, sort_index_global_addr);
    free(input_data);
    return 0;
}

static bool result_compare(float* output_cpu, float* output_tpu, int matrix_dims) {
    for(int i = 0; i < matrix_dims; i++) {
        for(int j = 0; j < matrix_dims; j++) {
            if(fabs(output_cpu[i * matrix_dims + j] - output_tpu[i * matrix_dims + j]) > 0.1) {
                printf("matrix_prune cmp failed\n");
                printf("error index: i = %d, j = %d, cpu_output: %f, tpu_output: %f\n", i, j, output_cpu[i * matrix_dims + j], output_tpu[i * matrix_dims + j]);
                return false;
            }
        }
    }
    return true;
}

static int test_matrix_prune_random(
        int matrix_dims,
        float p,
        int use_real_matrix,
        char *input_path,
        char *output_path,
        bm_handle_t handle) {
    printf("matrix_dims: %d ,  p = %f\n", matrix_dims, p);
    struct timeval t1, t2;
    float* output_cpu = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
    float* output_tpu = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
    float** input_matrix = (float**)malloc(matrix_dims * matrix_dims * sizeof(float*));
    float** output_matrix_cpu = (float**)malloc(matrix_dims * sizeof(float*));
    for (int i = 0; i < matrix_dims; i++) {
        input_matrix[i] = (float*)malloc(matrix_dims * sizeof(float));
        output_matrix_cpu[i] = (float*)malloc(matrix_dims * sizeof(float));
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
        for (int i = 0; i < matrix_dims; i++) {
            for (int j = 0; j < matrix_dims; j++) {
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
        fill_matrix(input_matrix, matrix_dims);
    }
    gettimeofday(&t1, NULL);
    matrix_prune_cpu(input_matrix, output_matrix_cpu, matrix_dims, p);
    gettimeofday(&t2, NULL);
    printf("cv_matrix_prune CPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if(0 != matrix_prune_tpu(handle, input_matrix, output_tpu, matrix_dims, p)){
        free(output_cpu);
        free(output_tpu);
        for (int i = 0; i < matrix_dims; ++i) {
            free(input_matrix[i]);
            free(output_matrix_cpu[i]);
        }
        free(input_matrix);
        free(output_matrix_cpu);
        return -1;
    };
    for (int i = 0; i < matrix_dims; ++i) {
        for (int j = 0; j < matrix_dims; ++j) {
            output_cpu[i * matrix_dims + j] = output_matrix_cpu[i][j];
        }
    }

    if (use_real_matrix) {
        FILE* file = fopen(output_path, "w");
        if (file == NULL) {
            perror("Error opening file");
            return 1;
        }

        for (int i = 0; i < matrix_dims; i++) {
            for (int j = 0; j < matrix_dims; j++) {
                int index = i * matrix_dims + j;
                fprintf(file, "%f ", output_tpu[index]);
            }
            fprintf(file, "\n");
        }
        printf("write output_matrix success\n");
        fclose(file);
    }
    if (false == result_compare(output_cpu, output_tpu, matrix_dims)) {
        printf("-------------matrix_prune compare failed-------\n");
        exit(-1);
    }

    printf("-------------matrix_prune compare succeed-----------\n");

    free(output_cpu);
    free(output_tpu);
    for (int i = 0; i < matrix_dims; ++i) {
        free(input_matrix[i]);
        free(output_matrix_cpu[i]);
    }
    free(input_matrix);
    free(output_matrix_cpu);
    return BM_SUCCESS;
}

void* test_matrix_prune(void* args) {
    cv_matrix_prune_thread_arg_t* cv_matrix_prune_thread_arg = (cv_matrix_prune_thread_arg_t*)args;
    int loop_num = cv_matrix_prune_thread_arg->loop_num;
    int matrix_dims = cv_matrix_prune_thread_arg->matrix_dims;
    float p = cv_matrix_prune_thread_arg->p;
    int use_real_matrix = cv_matrix_prune_thread_arg->use_real_matrix;
    char* input_path = cv_matrix_prune_thread_arg->input_path;
    char* output_path = cv_matrix_prune_thread_arg->output_path;
    bm_handle_t handle = cv_matrix_prune_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
        if(loop_num > 1) {
            matrix_dims = 8 + rand() % 5993;
            p = (float)rand() / (float)RAND_MAX;
        }
        if (0 != test_matrix_prune_random(matrix_dims, p, use_real_matrix, input_path, output_path, handle)){
            printf("------TEST CV_MATRIX_PRUNE FAILED------\n");
            exit(-1);
        }
        printf("------TEST CV_MATRIX_PRUNE PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    int loop = 1;
    int matrix_dims = 2164;
    float p = 0.5;
    int use_real_matrix = 0;
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
        printf("%s thread_num loop matrix_dims p use_real_matrix input_path output_path(when use_real_matrix = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        return 0;
    }

    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) matrix_dims = atoi(args[3]);
    if (argc > 4) p = atof(args[4]);
    if (argc > 5) use_real_matrix = atoi(args[5]);
    if (argc > 6) input_path = args[6];
    if (argc > 7) output_path = args[7];

    check = parameters_check(matrix_dims, p);
    if (check) {
        printf("Parameters Failed! \n");
        return check;
    }

    // test for multi-thread
    pthread_t pid[thread_num];
    cv_matrix_prune_thread_arg_t cv_matrix_prune_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        cv_matrix_prune_thread_arg[i].loop_num = loop;
        cv_matrix_prune_thread_arg[i].matrix_dims = matrix_dims;
        cv_matrix_prune_thread_arg[i].p = p;
        cv_matrix_prune_thread_arg[i].use_real_matrix = use_real_matrix;
        cv_matrix_prune_thread_arg[i].input_path = input_path;
        cv_matrix_prune_thread_arg[i].output_path = output_path;
        cv_matrix_prune_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_matrix_prune, cv_matrix_prune_thread_arg + i) != 0) {
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
