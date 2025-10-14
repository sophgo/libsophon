bmcv_faiss_indexPQ_ADC
======================

This interface calculates the distance table through query and centroids, looks up and sorts the base database encoding, and outputs the top K (sort_cnt) most matching vector indices and their corresponding distances.


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c++

        bm_status_t bmcv_faiss_indexPQ_ADC(
                    bm_handle_t handle,
                    bm_device_mem_t centroids_input_dev,
                    bm_device_mem_t nxquery_input_dev,
                    bm_device_mem_t nycodes_input_dev,
                    bm_device_mem_t distance_output_dev,
                    bm_device_mem_t index_output_dev,
                    int vec_dims,
                    int slice_num,
                    int centroids_num,
                    int database_num,
                    int query_num,
                    int sort_cnt,
                    int IP_metric);


**Input parameter description:**

* bm_handle_t handle

  Input parameters. The handle of bm_handle.

* bm_device_mem_t centroids_input_dev

  Input parameters.The deivce space for storing cluster center data.

* bm_device_mem_t nxquery_input_dev

  Input parameters.The deivce space that stores the matrix consisting of query vectors.

* bm_device_mem_t nycodes_input_dev

  Input parameters.The device space for storing the matrix composed of the base vector.

* bm_device_mem_t distance_output_dev

  output parameters.The device space for storing the output distance.

* bm_device_mem_t index_output_dev

  output parameters.The device space for storing output sorting.

* int vec_dims

  Input parameters.The dimensions of the original vector.

* int slice_num

  Input parameters.The number of original dimension splits.

* int centroids_num

  Input parameters.The number of cluster centers.

* int database_num

  Input parameters.The number of database bases.

* int query_num

  Input parameters.Retrieve the number of vectors.

* int sort_cnt

  Input parameters.Output the first sort_cnt most matching base vectors.

* int IP_metric

  Input parameters.0 means L2 distance calculation; 1 means IP distance calculation.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The data types of input data (query vector) and cluster center are float, and the data type of base database data (base database encoding) is uint8, which are all stored in device memory.

2. The data type of the sorted similarity result output is float, and the data type of the corresponding index is int, which is stored in device memory.

3. L2_metric calculates the square of L2 distance without square root (refer to the implementation of faiss source code).

4. The smaller the L2 distance between the query vector and the database vector, the higher the similarity between the two. The output L2 topk distance is sorted in ascending order.

5. The larger the IP distance between the query vector and the database vector, the higher the similarity between the two. The output IP topk distance is sorted in descending order.

6. The faiss series operators have multiple input parameters, each of which has a usage range limit. If the input parameter exceeds the range, the corresponding tpu output will fail. We selected three main parameters for testing, fixed two of the dimensions, and tested the maximum value of the third dimension. The test results are shown in the following table:

+-----------+--------------+-------------------+
| query_num | vec_dims     | max_database_num  |
+===========+==============+===================+
| 1         | 128          | 65million         |
+-----------+--------------+-------------------+
| 1         | 256          | 65million         |
+-----------+--------------+-------------------+
| 1         | 512          | 65million         |
+-----------+--------------+-------------------+
| 64        | 128          | 25million         |
+-----------+--------------+-------------------+
| 64        | 256          | 25million         |
+-----------+--------------+-------------------+
| 64        | 512          | 15million         |
+-----------+--------------+-------------------+
| 256       | 128          | 6million          |
+-----------+--------------+-------------------+
| 256       | 256          | 6million          |
+-----------+--------------+-------------------+
| 256       | 512          | 6million          |
+-----------+--------------+-------------------+

+--------------+--------------+----------------+
| database_num | vec_dims     | max_query_num  |
+==============+==============+================+
| 1000         | 128          | 1000           |
+--------------+--------------+----------------+
| 1000         | 256          | 1000           |
+--------------+--------------+----------------+
| 1000         | 512          | 1000           |
+--------------+--------------+----------------+
| 10k          | 128          | 1000           |
+--------------+--------------+----------------+
| 10k          | 256          | 1000           |
+--------------+--------------+----------------+
| 10k          | 512          | 1000           |
+--------------+--------------+----------------+
| 100k         | 128          | 100            |
+--------------+--------------+----------------+
| 100k         | 256          | 50             |
+--------------+--------------+----------------+
| 100k         | 512          | 50             |
+--------------+--------------+----------------+

+--------------+-----------------+--------------+
| database_num | query_num       | max_vec_dims |
+==============+=================+==============+
| 10k          | 1               | 2048         |
+--------------+-----------------+--------------+
| 10k          | 64              | 512          |
+--------------+-----------------+--------------+
| 10k          | 128             | 512          |
+--------------+-----------------+--------------+
| 10k          | 256             | 512          |
+--------------+-----------------+--------------+
| 100k         | 1               | 2048         |
+--------------+-----------------+--------------+
| 100k         | 32              | 512          |
+--------------+-----------------+--------------+
| 100k         | 64              | 512          |
+--------------+-----------------+--------------+
| 1million     | 1               | 128          |
+--------------+-----------------+--------------+


**Sample code**

    .. code-block:: c++

        #include "bmcv_api_ext.h"
        #include "test_misc.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <assert.h>

        int main()
        {
            int sort_cnt = 100;
            int vec_dims = 256;
            int query_num = 1;
            int slice_m = 32;
            int ksub = 256;
            int dsub = vec_dims / slice_m;
            int database_num = 2000000;
            int input_dtype = 5; // 5:float
            int output_dtype = 5;
            int IP_metric = 0;
            bm_handle_t handle;
            int round = 1;
            fp16 *centroids_input_sys_fp16 = (fp16*)malloc(slice_m * ksub * dsub * sizeof(fp16));
            fp16 *nxquery_input_sys_fp16 = (fp16*)malloc(query_num * vec_dims * sizeof(fp16));
            float *centroids_input_sys_fp32 = (float*)malloc(slice_m * ksub * dsub * sizeof(float));
            float *nxquery_input_sys_fp32 = (float*)malloc(query_num * vec_dims * sizeof(float));
            unsigned char *nycodes_input_sys = (unsigned char*)malloc(database_num * slice_m * sizeof(unsigned char));
            unsigned char *distance_output_sys = (unsigned char*)malloc(query_num * database_num * dtype_size((data_type_t)output_dtype));
            int *index_output_sys = (int*)malloc(query_num * database_num * sizeof(int));
            bm_device_mem_t centroids_input_dev, nxquery_input_dev, nycodes_input_dev, distance_output_dev, index_output_dev;
            int centroids_size = slice_m * ksub * dsub * dtype_size((data_type_t)input_dtype);
            int nxquery_size = query_num * vec_dims * dtype_size((data_type_t)input_dtype);
            int nycodes_size = database_num * slice_m * sizeof(char);
            int distance_size = query_num * database_num * dtype_size((data_type_t)output_dtype);
            int index_size = query_num * database_num * sizeof(int);

            for (int i = 0; i < slice_m; i++) {
                for (int j = 0; j < ksub; j++) {
                    for (int n = 0; n < dsub; n++) {
                        float value = (float)rand() / RAND_MAX * 20.0 - 10.0;
                        centroids_input_sys_fp32[i * dsub * ksub + j * dsub + n] = value;
                        centroids_input_sys_fp16[i * dsub * ksub + j * dsub + n] = fp32tofp16(value, round);
                    }
                }
            }
            for (int i = 0; i < query_num; i++) {
                for (int j = 0; j < vec_dims; j++) {
                    float value = (float)rand() / RAND_MAX * 20.0 - 10.0;
                    nxquery_input_sys_fp32[i * vec_dims + j] = value;
                    nxquery_input_sys_fp16[i * vec_dims + j] = fp32tofp16(value, round);
                }
            }
            for (int i = 0; i < database_num; i++) {
                for (int j = 0; j < slice_m; j++) {
                    nycodes_input_sys[i * slice_m + j] = rand() % 256;
                }
            }

            bm_dev_request(&handle, 0);
            bm_malloc_device_byte(handle, &centroids_input_dev, centroids_size);
            bm_malloc_device_byte(handle, &nxquery_input_dev, nxquery_size);
            bm_malloc_device_byte(handle, &nycodes_input_dev, nycodes_size);
            bm_malloc_device_byte(handle, &distance_output_dev, distance_size);
            bm_malloc_device_byte(handle, &index_output_dev, index_size);

            if (input_dtype == DT_FP16) {
                bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys_fp16);
                bm_memcpy_s2d(handle, nxquery_input_dev, nxquery_input_sys_fp16);
            } else {
                bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys_fp32);
                bm_memcpy_s2d(handle, nxquery_input_dev, nxquery_input_sys_fp32);
            }
            bm_memcpy_s2d(handle, nycodes_input_dev, nycodes_input_sys);

            bmcv_faiss_indexPQ_ADC_ext(handle, centroids_input_dev, nxquery_input_dev,
                                    nycodes_input_dev, distance_output_dev, index_output_dev,
                                    vec_dims, slice_m, ksub, database_num, query_num, sort_cnt,
                                    IP_metric, input_dtype, output_dtype);

            bm_memcpy_d2s(handle, distance_output_sys, distance_output_dev);
            bm_memcpy_d2s(handle, index_output_sys, index_output_dev);

            bm_free_device(handle, centroids_input_dev);
            bm_free_device(handle, nxquery_input_dev);
            bm_free_device(handle, nycodes_input_dev);
            bm_free_device(handle, distance_output_dev);
            bm_free_device(handle, index_output_dev);
            free(centroids_input_sys_fp32);
            free(centroids_input_sys_fp16);
            free(nxquery_input_sys_fp32);
            free(nxquery_input_sys_fp16);
            free(nycodes_input_sys);
            free(distance_output_sys);
            free(index_output_sys);
            bm_dev_free(handle);
            return 0;
        }