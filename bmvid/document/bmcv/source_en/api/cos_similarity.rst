bmcv_cos_similarity
====================

Normalize each row of the input matrix (divided by the l2 norm), then calculate the dot product of the normalized matrix and its transposed matrix to obtain the cosine similarity matrix, and finally adjust the similarity matrix elements to the range of 0 to 1.


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_cos_similarity(
                    bm_handle_t handle,
                    bm_device_mem_t input_data_global_addr,
                    bm_device_mem_t normalized_data_global_addr,
                    bm_device_mem_t output_data_global_addr,
                    int vec_num,
                    int vec_dims);


**Parameter description:**

* bm_handle_t handle

  Input parameter. bm_handle handle.

* bm_device_mem_t input_data_global_addr

  Input parameter. Device memory address for storing input data.

* bm_device_mem_t normalized_data_global_addr

  Input parameter. Device memory address for temporarily storing normalized matrix.

* bm_device_mem_t output_data_global_addr

  Output parameter. Device memory address for storing output data.

* int vec_num

  Input parameter. Number of data, corresponding to the number of rows of the input matrix.

* int vec_dims

  Input parameter. Number of data dimensions, corresponding to the number of columns of the input matrix.


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

2. The interface supports an input matrix with 256 columns and a matrix row range of 8-6000.


**Code example:**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include <stdlib.h>
        #include <string.h>
        #include <assert.h>
        #include <float.h>

        float random_float(float min, float max) {
            return min + ((float)rand() / RAND_MAX) * (max - min);
        }
        int main()
        {
            int vec_num = 2614;
            int vec_dims = 256;
            bm_handle_t handle;
            bm_dev_request(&handle, 0);
            float* output_tpu = (float*)malloc(vec_num * vec_num * sizeof(float));
            float* input_data = (float*)malloc(vec_num * vec_dims * sizeof(float));
            bm_device_mem_t in_global, norm_global, out_global;

            for (int i = 0; i < vec_num; ++i) {
                for (int j = 0; j < vec_dims; ++j) {
                    input_data[i * vec_dims + j] = random_float(-100.0f, 300.0f);
                }
            }

            bm_malloc_device_byte(handle, &in_global, sizeof(float) * vec_num * vec_dims);
            bm_malloc_device_byte(handle, &norm_global, sizeof(float) * vec_num * vec_dims);
            bm_malloc_device_byte(handle, &out_global, sizeof(float) * vec_num * vec_num);
            bm_memcpy_s2d(handle, in_global, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
            bmcv_cos_similarity(handle, in_global, norm_global, out_global, vec_num, vec_dims);
            bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(output_tpu)), out_global);

            bm_free_device(handle, in_global);
            bm_free_device(handle, norm_global);
            bm_free_device(handle, out_global);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }