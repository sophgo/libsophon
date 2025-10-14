bmcv_as_strided
================

This interface can create a view matrix based on the existing matrix and the given step size.


**Processor model support**

This interface only supports BM1684X.


**Interface form:**

     .. code-block:: c

         bm_status_t bmcv_as_strided(
                     bm_handle_t handle,
                     bm_device_mem_t input,
                     bm_device_mem_t output,
                     int input_row,
                     int input_col,
                     int output_row,
                     int output_col,
                     int row_stride,
                     int col_stride);


**Input parameter description:**

* bm_handle_t handle

   Input parameter. bm_handle Handle.

* bm_device_mem_t input

   Input parameter. The device memory address where the input matrix data is stored.

* bm_device_mem_t output

   Input parameter. The device memory address where the output matrix data is stored.

* int input_row

   Input parameter. The number of rows of the input matrix.

* int input_col

   Input parameter. The number of columns of the input matrix.

* int output_row

   Input parameter. The number of rows of the output matrix.

* int output_col

   Input parameter. The number of columns of the output matrix.

* int row_stride

   Input parameter. The step size between the rows of the output matrix.

* int col_stride

   Input parameter. The step size between the columns of the output matrix.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Example Code**

     .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>

        int main()
        {
            int input_row = 5;
            int input_col = 5;
            int output_row = 3;
            int output_col = 3;
            int row_stride = 1;
            int col_stride = 2;
            bm_handle_t handle;
            int len = input_row * input_col;
            bm_device_mem_t input_dev_mem, output_dev_mem;
            bm_dev_request(&handle, 0);

            float* input_data = new float[input_row * input_col];
            float* output_data = new float[output_row * output_col];

            for (int i = 0; i < len; i++) {
                input_data[i] = (float)rand() / (float)RAND_MAX * 100;
            }

            bm_malloc_device_byte(handle, &input_dev_mem, input_row * input_col * sizeof(float));
            bm_malloc_device_byte(handle, &output_dev_mem, output_row * output_col * sizeof(float));
            bm_memcpy_s2d(handle, input_dev_mem, input_data);
            bmcv_as_strided(handle, input_dev_mem, output_dev_mem, input_row, input_col,
                            output_row, output_col, row_stride, col_stride);
            bm_memcpy_d2s(handle, output_data, output_dev_mem);

            bm_free_device(handle, input_dev_mem);
            bm_free_device(handle, output_dev_mem);
            delete[] output_data;
            delete[] input_data;
            bm_dev_free(handle);
            return 0;
        }