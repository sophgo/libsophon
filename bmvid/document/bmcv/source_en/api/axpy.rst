bmcv_axpy
==========

This interface implements F = A * X + Y, where A is a constant of size n * c , and F , X , Y are all matrices of size n * c * h * w.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_axpy(
                    bm_handle_t handle,
                    bm_device_mem_t tensor_A,
                    bm_device_mem_t tensor_X,
                    bm_device_mem_t tensor_Y,
                    bm_device_mem_t tensor_F,
                    int input_n,
                    int input_c,
                    int input_h,
                    int input_w);


**Description of parameters:**

* bm_handle_t handle

  Input parameters. bm_handle handle.

* bm_device_mem_t tensor_A

  Input parameters. The device memory address where the scalar A is stored.

* bm_device_mem_t tensor_X

  Input parameters. The device memory address where matrix X is stored.

* bm_device_mem_t tensor_Y

  Input parameters. The device memory address where matrix Y is stored.

* bm_device_mem_t tensor_F

  Output parameters. The device memory address where the result matrix F is stored.

* int input_n

  Input parameters. The size of n dimension.

* int input_c

  Input parameters. The size of c dimension.

* int input_h

  Input parameters. The size of h dimension.

* int input_w

  Input parameters. The size of w dimension.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Code example:**

    .. code-block:: c

        #include <stdlib.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <string.h>
        #include <math.h>
        #include "bmcv_api_ext.h"

        #define N (10)
        #define C 256 //(64 * 2 + (64 >> 1))
        #define H 8
        #define W 8
        #define TENSOR_SIZE (N * C * H * W)

        int main()
        {
            bm_handle_t handle;
            bm_dev_request(&handle, 0);

            float* tensor_X = new float[TENSOR_SIZE];
            float* tensor_A = new float[N*C];
            float* tensor_Y = new float[TENSOR_SIZE];
            float* tensor_F = new float[TENSOR_SIZE];

            for (int idx = 0; idx < TENSOR_SIZE; idx++) {
                tensor_X[idx] = (float)idx - 5.0f;
                tensor_Y[idx] = (float)idx/3.0f - 8.2f;  //y
            }

            for (int idx = 0; idx < N * C; idx++) {
            tensor_A[idx] = (float)idx * 1.5f + 1.0f;
            }

            bmcv_image_axpy(handle, bm_mem_from_system((void *)tensor_A),
                             bm_mem_from_system((void *)tensor_X),
                            bm_mem_from_system((void *)tensor_Y),
                            bm_mem_from_system((void *)tensor_F),
                            N, C, H, W);

            delete []tensor_A;
            delete []tensor_X;
            delete []tensor_Y;
            delete []tensor_F;
            bm_dev_free(handle);
            return 0;
        }