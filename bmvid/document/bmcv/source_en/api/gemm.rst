bmcv_gemm
============

This interface is used to implement the general multiplication calculation of float32 type matrix, as shown in the following formula:

  .. math::

      C = \alpha\times A\times B + \beta\times C

  Among them, A, B and C are matrices,and :math:`\alpha` and :math:`\beta` are constant coefficients.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_gemm(
                    bm_handle_t handle,
                    bool is_A_trans,
                    bool is_B_trans,
                    int M,
                    int N,
                    int K,
                    float alpha,
                    bm_device_mem_t A,
                    int lda,
                    bm_device_mem_t B,
                    int ldb,
                    float beta,
                    bm_device_mem_t C,
                    int ldc);


**Processor model support**

This interface supports BM1684/BM1684X.


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bool is_A_trans

  Input parameter. Set whether matrix A is transposed

* bool is_B_trans

  Input parameter. Set whether matrix B is transposed

* int M

  Input parameter. The number of rows of matrix A and matrix C

* int N

  Input parameter. The number of columns of matrix B and matrix C

* int K

  Input parameter. The number of columns of matrix A and the number of rows of matrix B

* float alpha

  Input parameter. Number multiplication coefficient

* bm_device_mem_t A

  Input parameter. Save the device address or host address of the left matrix A data according to the data storage location. If the data is stored in the host space, it will automatically complete the handling of s2d.

* int lda

  Input parameter. The leading dimension of matrix A, that is, the size of the first dimension, is the number of columns (no transpose) or rows (transpose) of A when there is no stride between rows.

* bm_device_mem_t B

  Input parameter. Save the device address or host address of the right matrix B data according to the data storage location. If the data is stored in the host space, it will automatically complete the handling of s2d.

* int ldb

  Input parameter. The leading dimension of matrix C, that is, the size of the first dimension, is the number of columns (no transpose) or rows (transpose) of B when there is no stride between rows.

* float beta

  Input parameter. Number multiplication factor.

* bm_device_mem_t C

  Output parameter. Save the device address or host address of matrix C data according to the data storage location. If it is the host address, when the beta is not 0, the transportation of s2d will be completed automatically before calculation, and then the transportation of d2s will be completed automatically after calculation.

* int ldc

  Input parameter. The leading dimension of matrix C, that is, the size of the first dimension, is the number of columns of C when there is no stride between rows.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Sample code**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include "bmcv_api.h"
        #include <stdio.h>
        #include <stdint.h>
        #include <stdlib.h>
        #include <string.h>
        #include <math.h>

        int main()
        {
            int M = 3, N = 4, K = 5;
            float alpha = 0.4, beta = 0.6;
            bool if_A_trans = false;
            bool if_B_trans = false;
            float* A = new float[M * K];
            float* B = new float[K * N];
            float* C = new float[M * N];
            bm_handle_t handle;
            int lda = if_A_trans ? M : K;
            int ldb = if_B_trans ? K : N;

            for (int i = 0; i < M * K; ++i) {
                A[i] = 1.0f;
            }

            for (int i = 0; i < N * K; ++i) {
                B[i] = 2.0f;
            }

            for (int i = 0; i < M * N; ++i) {
                C[i] = 3.0f;
            }

            bm_dev_request(&handle, 0);
            bmcv_gemm(handle, if_A_trans, if_B_trans, M, N, K, alpha, bm_mem_from_system((void *)A),
                    lda, bm_mem_from_system((void *)B), ldb, beta, bm_mem_from_system((void *)C), N);

            delete[] A;
            delete[] B;
            delete[] C;
            bm_dev_free(handle);
            return 0;
        }