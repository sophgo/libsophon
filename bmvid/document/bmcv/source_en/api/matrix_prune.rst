bmcv_matrix_prune
====================

The matrix is ​​sparsely processed, and the parameter p is used to control the proportion of edges retained.

1. Different strategies are selected according to the number of data points to determine the number of neighboring points retained for each point.

2. For each point, the similarity with the rest of the points is sorted, the most similar part is retained, and the rest is set to 0, so that the matrix becomes sparse.

3. The sparsed symmetric similarity matrix is ​​returned (symmetrization is achieved by taking the average of the original matrix and its transpose).


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_matrix_prune(
                    bm_handle_t handle,
                    bm_device_mem_t input_data_global_addr,
                    bm_device_mem_t output_data_global_addr,
                    bm_device_mem_t sort_index_global_addr,
                    int matrix_dims,
                    float p);


**Parameter description:**

* bm_handle_t handle

  Input parameter. bm_handle handle.

* bm_device_mem_t input_data_global_addr

  Input parameter. Device memory address for storing input data.

* bm_device_mem_t output_data_global_addr

  Output parameter. Device memory address for storing output data.

* bm_device_mem_t sort_index_global_addr

  Input parameter. Device memory address where the sorted index is temporarily stored.

* int matrix_dims

  Input parameters. Matrix dimensions, controlling both the rows and columns of the matrix.

* float p

  Input parameter. Controls the proportion of edges that are preserved.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Data type support:**

Input data currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

Output data currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+


**Notes:**

1. Before calling this interface, you must ensure that the device memory to be used has been applied for.

2. The interface supports input matrix dimensions ranging from 8 to 6000, and parameter p ranging from 0.0 to 1.0.


**Code example:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include "test_misc.h"

        int main()
        {
            int matrix_dims = 2164;
            float p = 0.5;
            bm_handle_t handle;
            float* output_tpu = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
            float* input_data = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
            bm_device_mem_t input_data_global_addr, output_data_global_addr, sort_index_global_addr;
            bm_dev_request(&handle, 0);

            for (int i = 0; i < matrix_dims; ++i) {
                for (int j = 0; j < matrix_dims; ++j) {
                    input_data[i * matrix_dims + j] = (float)rand() / RAND_MAX;
                }
            }

            bm_malloc_device_byte(handle, &input_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
            bm_malloc_device_byte(handle, &output_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
            bm_malloc_device_byte(handle, &sort_index_global_addr, sizeof(DT_INT32) * matrix_dims * matrix_dims);
            bm_memcpy_s2d(handle, input_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
            bmcv_matrix_prune(handle, input_data_global_addr, output_data_global_addr,
                            sort_index_global_addr, matrix_dims, p);
            bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(output_tpu)), output_data_global_addr);

            bm_free_device(handle, input_data_global_addr);
            bm_free_device(handle, sort_index_global_addr);
            bm_free_device(handle, output_data_global_addr);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }