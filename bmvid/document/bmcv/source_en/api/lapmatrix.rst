bmcv_lap_matrix
====================

The Laplace matrix is ​a very important concept in graph theory and is used to analyze the properties of graphs.


**Processor model support**

This interface supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_lap_matrix(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    bm_device_mem_t output,
                    int row,
                    int col);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input

  Input parameter. The device memory space stores the input data. Its size is row * col * sizeof(float32)。

* bm_device_mem_t output

  Output parameter. The device memory space stores the input data. Its size is row * col * sizeof(float32)。

* int row

  Input parameter. The row of the input matrix.

* int col

  Input parameter. The col of the input matrix.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note**

1. Currently, this interface only supports matrix data type of float.


**Code example:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>
        int main()
        {
            int row = 1024;
            int col = 1024;
            float* input_addr = (float*)malloc(row * col * sizeof(float));
            float* output_addr = (float*)malloc(row * col * sizeof(float));
            bm_handle_t handle;
            bm_status_t ret = BM_SUCCESS;
            bm_device_mem_t input, output;
            int i;

            for (i = 0; i < row * col; ++i) {
                input_addr[i] = (float)rand() / RAND_MAX;
            }

            ret = bm_dev_request(&handle, 0);
            if (ret != BM_SUCCESS) {
                printf("bm_dev_request failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_malloc_device_byte(handle, &input, row * col * sizeof(float));
            if (ret != BM_SUCCESS) {
                printf("bm_malloc_device_byte failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_malloc_device_byte(handle, &output, row * col * sizeof(float));
            if (ret != BM_SUCCESS) {
                printf("bm_malloc_device_byte failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_memcpy_s2d(handle, input, input_addr);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_s2d failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bmcv_lap_matrix(handle, input, output, row, col);
            if (ret != BM_SUCCESS) {
                printf("bmcv_lap_matrix failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_memcpy_d2s(handle, output_addr, output);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_d2s failed. ret = %d\n", ret);
                exit(-1);
            }

            free(input_addr);
            free(output_addr);
            bm_free_device(handle, input);
            bm_free_device(handle, output);
            bm_dev_free(handle);
            return 0;
        }