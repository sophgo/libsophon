#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <stdint.h>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#if defined(__linux__) && (USING_OPENBLAS)
#define GLOBAL_PRINT 0

#define DUMP_SINGAL_FLIP_EIGENVECTOR 0

typedef int (*L_fp)();
extern "C" {
    void sgees_ (char *jobvs, char *sort, L_fp select, int *n, float *a, int *lda, int *sdim, float *wr, float *wi, float *vs, int *ldvs, float *work, int *lwork, int *bwork, int *info);
    void ssyevr_(char *jobz, char *range, char *uplo, int *n, float *a, int *lda,
                 float *vl, float *vu, int *il, int *iu, float *abstol,
                 int *m, float *w, float *z, int *ldz, int *ISUPPZ, float *work,
                 int *lwork, int *iwork, int *liwork, int *info);
    void ssyevd_ (char* jobz, char* uplo, int* n, float* a, int* lda, float* w, float* work, int *lwork, int *iwork, int *liwork, int *info);

    void ssyev_ (char* jobz, char* uplo, int* n, float* a, int* lda, float* w, float* work, int* lwork, int* info);

    void sgeevx_(char* balanc, char* jobvl, char* jobvr, char* sense, int* n, float* a, int* lda, float* wr, float* wi,
      float* vl, int* ldvl, float* vr, int* ldvr,
      int* 	ilo, int* ihi, float* scale, float* abnrm, float* rconde, float* rcondv, float*	work, int*	lwork,
      int* 	iwork, int* info);
}


void print_matrix(float *matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%f ", matrix[i * cols + j]);
        }
        printf("\n");
    }
}

void select(float* re, float* im) {
    *re = 1.0; // Example selection criterion
    *im = 0.0;
}

struct sort_helper
{
    float value;
    int index;
};

int cmp(const void *a, const void *b)
{
    struct sort_helper *a1 = (struct sort_helper *)a;
    struct sort_helper *a2 = (struct sort_helper *)b;
    if ((*a1).value > (*a2).value)
        return 1;
    else if ((*a1).value < (*a2).value)
        return -1;
    else
        return 0;
}

// Function to perform argsort on A
void argsort(float *A, int *indices, int n) {
    struct sort_helper objects[n];
    for (int i = 0; i < n; i++)
    {
        objects[i].value = A[i];
        objects[i].index = i;
    }
    qsort(objects, n, sizeof(objects[0]), cmp);
    for (int i = 0; i < n; i++)
    {
        A[i]       = objects[i].value;
        indices[i] = objects[i].index;
    }
}

// Function to sort the array A
void sort_corresponding(float *A, int *indices, int n) {
    float *tempA = (float *)malloc(n * sizeof(float));
    memcpy(tempA, A, n * sizeof(float));

    argsort(A, indices, n);
    // if(GLOBAL_PRINT) {
    //         printf("indices \n");
    //         for (int j = 0; j < n; j++) {
    //             printf("%d \n", indices[j]);
    //         }
    //         printf("\n");
    // }
    float *sortedA = (float *)malloc(n * sizeof(float));
    std::transform(indices, indices + n, sortedA, [&tempA](int index) { return tempA[index]; });
    memcpy(A, sortedA, n * sizeof(float));

    free(tempA);
    free(sortedA);
}

void bmcv_qr_cpu_sgees(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks) {
    printf("[Info] Using sgees_!\n");
    float *A = (float *)malloc(n * n * sizeof(float));
    memcpy(A, input_A, n * n * sizeof(float));
    char jobvs = 'V'; // Compute both eigenvalues and eigenvectors
    char sort  = 'N'; // Sort eigenvalues
    float *wr = (float *)malloc(n * sizeof(float));
    float *wi = (float *)malloc(n * sizeof(float));
    float *vs = (float *)malloc(n * n * sizeof(float));
    int lda  = n;
    int ldvs = n;
    int sdim;
    int lwork = 4 * n;
    float *work = (float *)malloc(lwork * sizeof(float));
    int *bwork = (int *)malloc(n * sizeof(int));
    int info;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    L_fp select = NULL; // Use default selection function

    sgees_(&jobvs, &sort, select, &n, A, &lda, &sdim, wr, wi, vs, &ldvs, work, &lwork, bwork, &info);
    gettimeofday(&end, NULL);
    double elapsed_time_QR = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    if (info == 0) {
        if(GLOBAL_PRINT){
            printf("Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     printf("Eigenvectors %d:\n",i);
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", vs[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        printf("[Computation Time] Time QR: %f seconds\n", elapsed_time_QR);
        gettimeofday(&start, NULL);

        // Sort A and get the sorted indices
        int *indices = (int *)malloc(n * sizeof(int));
        sort_corresponding(wr, indices, n);

        // num_spks = num_spks if num_spks is not None else np.argmax(np.diff(eig_values[:max_num_spks + 1])) + 1
        // num_spks = max(num_spks, min_num_spks)
        // return eig_vectors[:, :num_spks]
        *num_spks_output =  user_num_spks;
        if(user_num_spks <= 0) {
                float *diff_v = (float *)malloc((max_num_spks) * sizeof(float));
                assert(max_num_spks + 1 <= n);
                for (int i = 0; i < max_num_spks ; i++) {
                        diff_v[i]  = wr[i+1] - wr[i];
                }
                // float max_value = diff_v[0];
                // *num_spks_output = 1;
                // for (int ii = 1; ii < max_num_spks ; ii++) {
                //         if(diff_v[ii] > max_value) {
                //             max_value = diff_v[ii];
                //             *num_spks_output = ii + 1;
                //         }
                // }
                auto maxIterator =std::max_element(diff_v, diff_v + max_num_spks);
                *num_spks_output= std::distance(diff_v, maxIterator) + 1;
                *num_spks_output = *num_spks_output >  min_num_spks ? *num_spks_output  : min_num_spks;
                // if(GLOBAL_PRINT) {
                //     printf("diff_v: \n");
                //     for (int i = 0; i < max_num_spks ; i++) {
                //         printf("%f \n",diff_v[i]);
                //     }
                // }
                free(diff_v);
        }

        // Reorder B based on the sorted indices
        float *sortedB = (float *)malloc(n * n * sizeof(float));
        // int twice_flag = 0;
        // int last_index = indices[0];
        for (int i = 0; i < n; i++) {
            // last_index = indices[i];
            for (int j = 0; j < n; j++) {
                // printf("%d, %d,:,%d,%d,:%f %d\n",j,i,indices[i],j,sign,twice_flag);
                sortedB[i + j*n] =  vs[indices[i] * n + j];
            }
        }
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < (*num_spks_output); j++) {
                arm_select_egin_vector[i * (*num_spks_output) + j] = sortedB[i * n + j];
            }
        }
        gettimeofday(&end, NULL);
        double elapsed_time_postprocess = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        if(GLOBAL_PRINT){
            printf("----------------------------------------\n");
            printf("Sorted Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Sorted Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", sortedB[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        memcpy(arm_sorted_eign_value, wr, n * sizeof(float));

        printf("[Computation Time] QR PostProcess: %f seconds\n", elapsed_time_postprocess);
        printf("[Computation Time] QR Total: %f seconds\n", elapsed_time_QR +elapsed_time_postprocess);

    } else {
        printf("Eigenvalue computation failed with error code %d\n", info);
    }

    free(A);
    free(wr);
    free(wi);
    free(vs);
    free(work);
    free(bwork);
}

void bmcv_qr_cpu_ssyev(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks) {
    printf("[Info] Using ssyev_!\n");
    float *A = (float *)malloc(n * n * sizeof(float));
    memcpy(A, input_A, n * n * sizeof(float));
    float *wr  = (float *)malloc(n * sizeof(float));
    int lda    = n;
    int lwork  = 3 * n -1; // Workspace size query
    int *bwork = (int *)malloc(( 3 * n - 1) * sizeof(int));
    int info;
    char jobz  = 'V';
    char uplo  = 'U';
    // float optimal_work_size;
    float *optimal_work_size  = (float *)malloc(lwork * sizeof(float));

    struct timeval start, end;
    gettimeofday(&start, NULL);

    ssyev_(&jobz, &uplo, &n, A, &lda, wr, optimal_work_size, &lwork, &info);
    gettimeofday(&end, NULL);
    double elapsed_time_QR = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    if (info == 0) {
        if(GLOBAL_PRINT){
            printf("Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     printf("Eigenvectors %d:\n",i);
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", vs[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        printf("[Computation Time] Time QR: %f seconds\n", elapsed_time_QR);
        gettimeofday(&start, NULL);

        // num_spks = num_spks if num_spks is not None else np.argmax(np.diff(eig_values[:max_num_spks + 1])) + 1
        // num_spks = max(num_spks, min_num_spks)
        // return eig_vectors[:, :num_spks]
        *num_spks_output =  user_num_spks;
        if(user_num_spks <= 0) {
                float *diff_v = (float *)malloc((max_num_spks) * sizeof(float));
                assert(max_num_spks + 1 <= n);
                for (int i = 0; i < max_num_spks ; i++) {
                        diff_v[i]  = wr[i+1] - wr[i];
                }
                // float max_value = diff_v[0];
                // *num_spks_output = 1;
                // for (int ii = 1; ii < max_num_spks ; ii++) {
                //         if(diff_v[ii] > max_value) {
                //             max_value = diff_v[ii];
                //             *num_spks_output = ii + 1;
                //         }
                // }
                auto maxIterator =std::max_element(diff_v, diff_v + max_num_spks);
                *num_spks_output= std::distance(diff_v, maxIterator) + 1;
                *num_spks_output = *num_spks_output >  min_num_spks ? *num_spks_output  : min_num_spks;
                // if(GLOBAL_PRINT) {
                //     printf("diff_v: \n");
                //     for (int i = 0; i < max_num_spks ; i++) {
                //         printf("%f \n",diff_v[i]);
                //     }
                // }
                free(diff_v);
        }

        // Reorder eigenvecotr as A is column-priority
        float *sortedB = (float *)malloc(n * n * sizeof(float));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                sortedB[i + j*n] =  A[i * n + j];
            }
        }

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < (*num_spks_output); j++) {
                arm_select_egin_vector[i * (*num_spks_output) + j] = sortedB[i * n + j];
            }
        }
        if(DUMP_SINGAL_FLIP_EIGENVECTOR) {
            FILE*  file= fopen("bmvid_qr_eigin_vector_selected.txt", "w");
            if (file == NULL) {
                perror("Error opening file");
            }
            for (int i = 0; i < n * (*num_spks_output); i++) {
                fprintf(file, "%f ", arm_select_egin_vector[i]);
            }
            fclose(file);
        }
        gettimeofday(&end, NULL);
        double elapsed_time_postprocess = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        if(GLOBAL_PRINT){
            printf("----------------------------------------\n");
            printf("Sorted Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Sorted Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", sortedB[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        memcpy(arm_sorted_eign_value, wr, n * sizeof(float));

        printf("[Computation Time] QR PostProcess: %f seconds\n", elapsed_time_postprocess);
        printf("[Computation Time] QR Total: %f seconds\n", elapsed_time_QR +elapsed_time_postprocess);

    } else {
        printf("Eigenvalue computation failed with error code %d\n", info);
    }

    free(A);
    free(wr);
    free(bwork);
    free(optimal_work_size);
}

void bmcv_qr_cpu_ssyevr(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks) {
    printf("[Info] Using ssyevr_!\n");
    float *A = (float *)malloc(n * n * sizeof(float));
    memcpy(A, input_A, n * n * sizeof(float));
    float *wr  = (float *)malloc(n * sizeof(float));
    memset(wr, 0, n * sizeof(float));
    int lda    = n;
    int lwork  = 3 * n -1; // Workspace size query
    int *bwork = (int *)malloc(( 3 * n - 1) * sizeof(int));
    memset(bwork, 0, ( 3 * n - 1) * sizeof(int));
    int info;
    char jobz  = 'V';
    char uplo  = 'U';
    float *optimal_work_size  = (float *)malloc(lwork * sizeof(float));
    memset(optimal_work_size, 0, lwork * sizeof(float));

    struct timeval start, end;
    gettimeofday(&start, NULL);

    int LDZ = n; // Number of eigenvectors to store

    // Set parameters for ssyevr
    char RANGE = 'I'; // Index range
    float VL = 0.0f; // Not used for 'I'
    float VU = 1.0f; // Not used for 'I'

    int IL = 1; // Start index for the smallest 10 eigenvalues
    int IU = max_num_spks + 1; // End index for the smallest 10 eigenvalues
    int M  = IU - IL + 1; // Number of eigenvalues found

    float ABSTOL = 1e-10f; // Absolute tolerance
    float *Z  = (float *)malloc(LDZ * M * sizeof(float));
    int   *ISUPPZ = (int *)malloc(2 * M * sizeof(int));
    // Workspace sizes
    int  LWORK  = 26 * n; // Query for optimal workspace size
    float *WORK_QUERY  = (float *)malloc(LWORK * sizeof(float));
    int  LIWORK = 10 * n; // Query for optimal integer workspace size
    int *IWORK         = (int *)malloc(LIWORK * sizeof(int));

    // Query for optimal workspace size
    ssyevr_(&jobz, &RANGE, &uplo, &n, A, &lda, &VL, &VU, &IL, &IU, &ABSTOL,
            &M, wr, Z, &LDZ, ISUPPZ, WORK_QUERY, &LWORK, IWORK, &LIWORK, &info);
    gettimeofday(&end, NULL);
    double elapsed_time_QR = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    if (info == 0) {
        if(GLOBAL_PRINT){
            printf("Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     printf("Eigenvectors %d:\n",i);
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", vs[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        printf("[Computation Time] Time QR: %f seconds\n", elapsed_time_QR);
        gettimeofday(&start, NULL);

        // num_spks = num_spks if num_spks is not None else np.argmax(np.diff(eig_values[:max_num_spks + 1])) + 1
        // num_spks = max(num_spks, min_num_spks)
        // return eig_vectors[:, :num_spks]
        *num_spks_output =  user_num_spks;

        if(user_num_spks <= 0) {
                std::vector<float> diff_v(max_num_spks, 0.0f);
                assert(max_num_spks + 1 <= n);
                for (int i = 0; i < max_num_spks ; i++) {
                        diff_v[i]  = wr[i+1] - wr[i];
                }
                auto maxIterator =std::max_element(diff_v.begin(), diff_v.begin() + max_num_spks);
                *num_spks_output= std::distance(diff_v.begin(), maxIterator) + 1;
                *num_spks_output = *num_spks_output >  min_num_spks ? *num_spks_output  : min_num_spks;
                // if(GLOBAL_PRINT) {
                //     printf("diff_v: \n");
                //     for (int i = 0; i < max_num_spks ; i++) {
                //         printf("%f \n",diff_v[i]);
                //     }
                // }
        }

        // Reorder eigenvecotr as A is column-priority
         std::vector<float> sortedB(n  * n, 0.0f);
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < n; j++) {
                sortedB[i + j*n] =  Z[i * n + j];
            }
        }

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < (*num_spks_output); j++) {
                arm_select_egin_vector[i * (*num_spks_output) + j] = sortedB[i * n + j];
            }
        }
        if(DUMP_SINGAL_FLIP_EIGENVECTOR) {
            FILE*  file= fopen("bmvid_qr_eigin_vector_selected.txt", "w");
            if (file == NULL) {
                perror("Error opening file");
            }
            for (int i = 0; i < n * (*num_spks_output); i++) {
                fprintf(file, "%f ", arm_select_egin_vector[i]);
            }
            fclose(file);
        }
        gettimeofday(&end, NULL);
        double elapsed_time_postprocess = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        if(GLOBAL_PRINT){
            printf("----------------------------------------\n");
            printf("Sorted Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Sorted Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", sortedB[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        memcpy(arm_sorted_eign_value, wr, n * sizeof(float));

        printf("[Computation Time] QR PostProcess: %f seconds\n", elapsed_time_postprocess);
        printf("[Computation Time] QR Total: %f seconds\n", elapsed_time_QR +elapsed_time_postprocess);

    } else {
        printf("Eigenvalue computation failed with error code %d\n", info);
    }
    free(Z);
    free(WORK_QUERY);
    free(IWORK);
    free(A);
    free(wr);
    free(bwork);
    free(optimal_work_size);
    free(ISUPPZ);
}


void bmcv_qr_cpu_sgeevd(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks) {
    printf("[Info] Using ssyevd_!\n");
    float *A = (float *)malloc(n * n * sizeof(float));
    memcpy(A, input_A, n * n * sizeof(float));
    float *wr  = (float *)malloc(n * sizeof(float));
    int lda    = n;
    int lwork  = 3 * n -1; // Workspace size query
    int *bwork = (int *)malloc(( 3 * n - 1) * sizeof(int));
    int info;
    char jobz  = 'V';
    char uplo  = 'U';
    float *optimal_work_size  = (float *)malloc(lwork * sizeof(float));

    struct timeval start, end;
    gettimeofday(&start, NULL);


    // Workspace sizes
    int  LWORK  = 1 + 6 * n + 2 * n * n; // Query for optimal workspace size
    float *WORK_QUERY  = (float *)malloc(LWORK * sizeof(float));
    int  LIWORK = 3 + 5 * n; // Query for optimal integer workspace size
    int *IWORK         = (int *)malloc(LIWORK * sizeof(int));

    // Query for optimal workspace size
    ssyevd_(&jobz, &uplo, &n, A, &lda,
            wr,  WORK_QUERY, &LWORK, IWORK, &LIWORK, &info);
    gettimeofday(&end, NULL);
    double elapsed_time_QR = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    if (info == 0) {
        if(GLOBAL_PRINT){
            printf("Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     printf("Eigenvectors %d:\n",i);
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", vs[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        printf("[Computation Time] Time QR: %f seconds\n", elapsed_time_QR);
        gettimeofday(&start, NULL);

        // num_spks = num_spks if num_spks is not None else np.argmax(np.diff(eig_values[:max_num_spks + 1])) + 1
        // num_spks = max(num_spks, min_num_spks)
        // return eig_vectors[:, :num_spks]
        *num_spks_output =  user_num_spks;
        if(user_num_spks <= 0) {
                float *diff_v = (float *)malloc((max_num_spks) * sizeof(float));
                assert(max_num_spks + 1 <= n);
                for (int i = 0; i < max_num_spks ; i++) {
                        diff_v[i]  = wr[i+1] - wr[i];
                }
                // float max_value = diff_v[0];
                // *num_spks_output = 1;
                // for (int ii = 1; ii < max_num_spks ; ii++) {
                //         if(diff_v[ii] > max_value) {
                //             max_value = diff_v[ii];
                //             *num_spks_output = ii + 1;
                //         }
                // }
                auto maxIterator =std::max_element(diff_v, diff_v + max_num_spks);
                *num_spks_output= std::distance(diff_v, maxIterator) + 1;
                *num_spks_output = *num_spks_output >  min_num_spks ? *num_spks_output  : min_num_spks;
                // if(GLOBAL_PRINT) {
                //     printf("diff_v: \n");
                //     for (int i = 0; i < max_num_spks ; i++) {
                //         printf("%f \n",diff_v[i]);
                //     }
                // }
                free(diff_v);
        }

        // Reorder eigenvecotr as A is column-priority
        float *sortedB = (float *)malloc(n * n * sizeof(float));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                sortedB[i + j*n] =  A[i * n + j];
            }
        }

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < (*num_spks_output); j++) {
                arm_select_egin_vector[i * (*num_spks_output) + j] = sortedB[i * n + j];
            }
        }
        if(DUMP_SINGAL_FLIP_EIGENVECTOR) {
            FILE*  file= fopen("bmvid_qr_eigin_vector_selected.txt", "w");
            if (file == NULL) {
                perror("Error opening file");
            }
            for (int i = 0; i < n * (*num_spks_output); i++) {
                fprintf(file, "%f ", arm_select_egin_vector[i]);
            }
            fclose(file);
        }
        gettimeofday(&end, NULL);
        double elapsed_time_postprocess = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        if(GLOBAL_PRINT){
            printf("----------------------------------------\n");
            printf("Sorted Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Sorted Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", sortedB[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        memcpy(arm_sorted_eign_value, wr, n * sizeof(float));

        printf("[Computation Time] QR PostProcess: %f seconds\n", elapsed_time_postprocess);
        printf("[Computation Time] QR Total: %f seconds\n", elapsed_time_QR +elapsed_time_postprocess);

    } else {
        printf("Eigenvalue computation failed with error code %d\n", info);
    }
    free(WORK_QUERY);
    free(IWORK);
    free(A);
    free(wr);
    free(bwork);
    free(optimal_work_size);
}


void bmcv_qr_cpu_sgeevx(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks) {
    printf("[Info] Using sgeevx_!\n");
    float *A = (float *)malloc(n * n * sizeof(float));
    memcpy(A, input_A, n * n * sizeof(float));
    float *wr  = (float *)malloc(n * sizeof(float));
    float *wi  = (float *)malloc(n * sizeof(float));

    int lda    = n;
    int lwork  = 3 * n -1; // Workspace size query
    int *bwork = (int *)malloc(( 3 * n - 1) * sizeof(int));
    int info;
    float *optimal_work_size  = (float *)malloc(lwork * sizeof(float));

    struct timeval start, end;
    gettimeofday(&start, NULL);

    // Workspace sizes
    int  LWORK  = n * (n + 6); // Query for optimal workspace size
    float *WORK_QUERY  = (float *)malloc(LWORK * sizeof(float));
    int  LIWORK = 2 * n - 2; // Query for optimal integer workspace size
    int *IWORK         = (int *)malloc(LIWORK * sizeof(int));

    char balanc  = 'P';
    char jobvl   = 'V';
    char jobvr   = 'V';
    char sense   = 'B';

    int ldvl  = n;
    float *vl = (float *)malloc(n * ldvl * sizeof(float));

    int ldvr  = n;
    float *vr = (float *)malloc(n * ldvr * sizeof(float));

    int ilo   = std::min(3,n);
    int ihi   = std::min(int(n/2),n-1);

    float *rconde         = (float *)malloc(n * sizeof(float));
    float *rcondv         = (float *)malloc(n * sizeof(float));
    float *scale          = (float *)malloc(n * sizeof(float));

    float abnrm = 0.0;
    sgeevx_(&balanc, &jobvl, &jobvr, &sense, &n, A, &lda, wr, wi,
        vl, &ldvl, vr, &ldvr,
        &ilo, &ihi, scale, &abnrm, rconde, rcondv, WORK_QUERY, &LWORK,
        IWORK, &info);
    gettimeofday(&end, NULL);
    double elapsed_time_QR = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    if (info == 0) {
        if(1){//GLOBAL_PRINT
            printf("Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     printf("Eigenvectors %d:\n",i);
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", vs[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        printf("[Computation Time] Time QR: %f seconds\n", elapsed_time_QR);
        gettimeofday(&start, NULL);

        // num_spks = num_spks if num_spks is not None else np.argmax(np.diff(eig_values[:max_num_spks + 1])) + 1
        // num_spks = max(num_spks, min_num_spks)
        // return eig_vectors[:, :num_spks]
        *num_spks_output =  user_num_spks;
        if(user_num_spks <= 0) {
                float *diff_v = (float *)malloc((max_num_spks) * sizeof(float));
                assert(max_num_spks + 1 <= n);
                for (int i = 0; i < max_num_spks ; i++) {
                        diff_v[i]  = wr[i+1] - wr[i];
                }
                // float max_value = diff_v[0];
                // *num_spks_output = 1;
                // for (int ii = 1; ii < max_num_spks ; ii++) {
                //         if(diff_v[ii] > max_value) {
                //             max_value = diff_v[ii];
                //             *num_spks_output = ii + 1;
                //         }
                // }
                auto maxIterator =std::max_element(diff_v, diff_v + max_num_spks);
                *num_spks_output= std::distance(diff_v, maxIterator) + 1;
                *num_spks_output = *num_spks_output >  min_num_spks ? *num_spks_output  : min_num_spks;
                // if(GLOBAL_PRINT) {
                //     printf("diff_v: \n");
                //     for (int i = 0; i < max_num_spks ; i++) {
                //         printf("%f \n",diff_v[i]);
                //     }
                // }
                free(diff_v);
        }

        // Reorder eigenvecotr as A is column-priority
        float *sortedB = (float *)malloc(n * n * sizeof(float));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                sortedB[i + j*n] =  vr[i * n + j];
            }
        }

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < (*num_spks_output); j++) {
                arm_select_egin_vector[i * (*num_spks_output) + j] = sortedB[i * n + j];
            }
        }
        if(DUMP_SINGAL_FLIP_EIGENVECTOR) {
            FILE*  file= fopen("bmvid_qr_eigin_vector_selected.txt", "w");
            if (file == NULL) {
                perror("Error opening file");
            }
            for (int i = 0; i < n * (*num_spks_output); i++) {
                fprintf(file, "%f ", arm_select_egin_vector[i]);
            }
            fclose(file);
        }
        gettimeofday(&end, NULL);
        double elapsed_time_postprocess = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        if(GLOBAL_PRINT){
            printf("----------------------------------------\n");
            printf("Sorted Eigenvalues:\n");
            int practical_N = n > 10 ? 10 : n;
            for (int i = 0; i < practical_N; i++) {
                printf("%f\n", wr[i]);
            }
            // printf("Sorted Eigenvectors:\n");
            // for (int i = 0; i < n; i++) {
            //     for (int j = 0; j < n; j++) {
            //         printf("%f ", sortedB[i * n + j]);
            //     }
            //     printf("\n");
            // }
        }
        memcpy(arm_sorted_eign_value, wr, n * sizeof(float));

        printf("[Computation Time] QR PostProcess: %f seconds\n", elapsed_time_postprocess);
        printf("[Computation Time] QR Total: %f seconds\n", elapsed_time_QR +elapsed_time_postprocess);

    } else {
        printf("Eigenvalue computation failed with error code %d\n", info);
        assert(0);
    }
    assert(0);
    free(WORK_QUERY);
    free(IWORK);
    free(A);
    free(wr);
    free(wi);
    free(bwork);
    free(optimal_work_size);
    free(rconde);
    free(rcondv);
    free(scale);
}


/*
[Note]multi-thread is auto optimized to num_cores
  function    eigen nums               x86  c906
--------------------------------------------------
 ssyevr_   first max_num_spks eigen   0.38s  4.6s
 ssyevr_   N eigen                    2.6s   42s
 ssyevd_   N eigen                    3.2s   6.82s
 sgees     N eigen                    4.68    >120
 sgeevx     N eigen                   large
*/
void bmcv_qr_cpu(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks) {
    // bmcv_qr_cpu_sgees(
    //         arm_select_egin_vector,
    //         num_spks_output,
    //         input_A,
    //         arm_sorted_eign_value,
    //         n,
    //         user_num_spks,
    //         max_num_spks,
    //         min_num_spks);
    // bmcv_qr_cpu_ssyev(
    //         arm_select_egin_vector,
    //         num_spks_output,
    //         input_A,
    //         arm_sorted_eign_value,
    //         n,
    //         user_num_spks,
    //         max_num_spks,
            // min_num_spks);
    bmcv_qr_cpu_ssyevr(
            arm_select_egin_vector,
            num_spks_output,
            input_A,
            arm_sorted_eign_value,
            n,
            user_num_spks,
            max_num_spks,
            min_num_spks);
    // bmcv_qr_cpu_sgeevd(
    //         arm_select_egin_vector,
    //         num_spks_output,
    //         input_A,
    //         arm_sorted_eign_value,
    //         n,
    //         user_num_spks,
    //         max_num_spks,
    //         min_num_spks);
    // bmcv_qr_cpu_sgeevx(
    //     arm_select_egin_vector,
    //     num_spks_output,
    //     input_A,
    //     arm_sorted_eign_value,
    //     n,
    //     user_num_spks,
    //     max_num_spks,
    //     min_num_spks);

}
#endif