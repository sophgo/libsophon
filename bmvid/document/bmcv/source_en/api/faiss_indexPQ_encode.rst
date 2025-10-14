bmcv_faiss_indexPQ_encode
===========================

This interface inputs vectors and centroids, calculates the distance table and sorts it, and outputs the quantized code of vectors.


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c++

        bm_status_t bmcv_faiss_indexPQ_encode(
                    bm_handle_t handle,
                    bm_device_mem_t vector_input_dev,
                    bm_device_mem_t centroids_input_dev,
                    bm_device_mem_t buffer_table_dev,
                    bm_device_mem_t codes_output_dev,
                    int encode_vec_num,
                    int vec_dims,
                    int slice_num,
                    int centroids_num,
                    int IP_metric);


**Input parameter description:**

* bm_handle_t handle

  Input parameters. The handle of bm_handle.

* bm_device_mem_t vector_input_dev

  Input parameters.The device space for storing the vector to be encoded.

* bm_device_mem_t centroids_input_dev

  Input parameters.The deivce space for storing cluster center data.

* bm_device_mem_t buffer_table_dev

  Input parameters.Cache space for storing calculated distance tables.

* bm_device_mem_t codes_output_dev

  Output parameters.The device space for storing vector encoding results.

* int encode_vec_num

  Input parameters.The number of vectors to be encoded.

* int vec_dims

  Input parameters.The dimensions of the original vector.

* int slice_num

  Input parameters.The number of original dimension splits.

* int centroids_num

  Input parameters.The number of cluster centers.

* int IP_metric

  Input parameters.0 means L2 distance calculation; 1 means IP distance calculation.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The data type of input data (query vector) and cluster center is float, and the data type of output vector encoding is uint8, which is stored in device memory.

2. The size of buffer_table is slice_num * centroids_num, and the data type is float.


**Sample code**

    .. code-block:: c++

      #include "bmcv_api_ext.h"
      #include "test_misc.h"
      #include <stdio.h>
      #include <stdlib.h>
      #include <assert.h>

        int main()
        {
            int vec_dims = 256;
            int encode_vec_num = 1;
            int slice_m = 32;
            int ksub = 256;
            int dsub = vec_dims / slice_m;
            int input_dtype = 5; // 5: float
            int IP_metric = 0;
            bm_handle_t handle;
            int centroids_size = slice_m * ksub * dsub * dtype_size((data_type_t)input_dtype);
            int nxcodes_size = encode_vec_num * vec_dims * dtype_size((data_type_t)input_dtype);;
            int buffer_table_size = slice_m * ksub * dtype_size((data_type_t)input_dtype);;
            int output_codes_size = encode_vec_num * slice_m;
            bm_device_mem_t centroids_input_dev, nxcodes_input_dev, buffer_table_dev, codes_output_dev;
            float* centroids_input_sys_fp32 = (float*)malloc(slice_m * ksub * dsub * sizeof(float));
            unsigned char* nxcodes_input_sys = (unsigned char*)malloc(encode_vec_num * vec_dims);
            unsigned char* output_codes_sys = (unsigned char*)malloc(encode_vec_num * slice_m);

            for (int i = 0; i < slice_m; i++) {
                for (int j = 0; j < ksub; j++) {
                    for (int n = 0; n < dsub; n++) {
                        float value = (float)rand() / RAND_MAX * 20.0 - 10.0;
                        centroids_input_sys_fp32[i * dsub * ksub + j * dsub + n] = value;
                    }
                }
            }
            for (int i = 0; i < encode_vec_num; i++) {
                for (int j = 0; j < slice_m; j++) {
                    nxcodes_input_sys[i * slice_m + j] = rand() % 256;
                }
            }

            bm_dev_request(&handle, 0);
            bm_malloc_device_byte(handle, &centroids_input_dev, centroids_size);
            bm_malloc_device_byte(handle, &nxcodes_input_dev, nxcodes_size);
            bm_malloc_device_byte(handle, &buffer_table_dev, buffer_table_size);
            bm_malloc_device_byte(handle, &codes_output_dev, output_codes_size);
            bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys_fp32);
            bm_memcpy_s2d(handle, nxcodes_input_dev, nxcodes_input_sys);

            bmcv_faiss_indexPQ_encode(handle, nxcodes_input_dev, centroids_input_dev, buffer_table_dev,
                        codes_output_dev, encode_vec_num, vec_dims, slice_m, ksub, IP_metric);
            bm_memcpy_d2s(handle, output_codes_sys, codes_output_dev);

            bm_free_device(handle, centroids_input_dev);
            bm_free_device(handle, nxcodes_input_dev);
            bm_free_device(handle, buffer_table_dev);
            bm_free_device(handle, codes_output_dev);
            free(centroids_input_sys_fp32);
            free(nxcodes_input_sys);
            free(output_codes_sys);
            bm_dev_free(handle);
            return 0;
        }