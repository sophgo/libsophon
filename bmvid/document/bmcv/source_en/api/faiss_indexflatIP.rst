bmcv_faiss_indexflatIP
======================

This interface is used to calculate inner product distance between query vectors and database vectors, output the top K (sort_cnt)  IP-values and the corresponding indices, return BM_SUCCESS if succeed.


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c++

        bm_status_t bmcv_faiss_indexflatIP(
                    bm_handle_t handle,
                    bm_device_mem_t input_data_global_addr,
                    bm_device_mem_t db_data_global_addr,
                    bm_device_mem_t buffer_global_addr,
                    bm_device_mem_t output_sorted_similarity_global_addr,
                    bm_device_mem_t output_sorted_index_global_addr,
                    int vec_dims,
                    int query_vecs_num,
                    int database_vecs_num,
                    int sort_cnt,
                    int is_transpose,
                    int input_dtype,
                    int output_dtype);


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input_data_global_addr

  Input parameter. Device addr information of the query matrix.

* bm_device_mem_t db_data_global_addr

  Input parameter. Device addr information of the database matrix.

* bm_device_mem_t buffer_global_addr

  Input parameter. Inner product values stored in the buffer.

* bm_device_mem_t output_sorted_similarity_global_addr

  Output parameter. The IP-values matrix.

* bm_device_mem_t output_sorted_index_global_addr

  Output parameter. The result indices matrix.

* int vec_dims

  Input parameter. Vector dimension.

* int query_vecs_num

  Input parameter. The num of query vectors.

* int database_vecs_num

  Input parameter. The num of database vectors.

* int sort_cnt

  Input parameter. Get top sort_cnt values.

* int is_transpose

  Input parameter. db_matrix 0: NO_TRNAS; 1: TRANS.

* int input_dtype

  Input parameters. Input data types. Supports fp32, fp16, and char. 5 indicates fp32, 3 indicates fp16, and 1 indicates char.

* int output_dtype

  Output parameters. Output data type. When the input data type is fp32, the output data type supports fp32 and fp16; when the input data type is fp16, the output data type supports fp32 and fp16; when the input data type is char, the output data type supports int. Here, 5 represents fp32, 3 represents fp16, and 9 represents int.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The input data type (query vectors) and data in the database (database vectors) are fp32, fp16 or char.

2. The data type of the output sorted similarity result is fp32, fp16 or int, and that of the corresponding indices is int.

3. Usually, the data in the database is arranged in the memory as database_vecs_num * vec_dims. Therefore, the is_transpose needs to be set to 1.

4. The larger the inner product values of the query vector and the database vector, the higher the similarity of the two vectors. Therefore, the inner product values are sorted in descending order in the process of TopK.

5. The interface is used for Faiss::IndexFlatIP.search() and implemented on BM1684X. According to the continuous memory of Tensor Computing Processor on BM1684X, we can query about 512 inputs of 256 dimensions at a time on a single processor if the database is about 100W.

6. If the device memory requested in a single application exceeds 4GB, an interface with the suffix u64 must be used to request, transfer, and release device memory.

7. The Faiss series operators have multiple input parameters, each of which has a usage range limit. If the input parameter exceeds the range, the corresponding TPU output will fail. We selected three main parameters for testing, fixed two of the dimensions, and tested the maximum value of the third dimension. The test results are shown in the following table:

+-----------+--------------+-------------------+
| query_num | vec_dims     | max_database_num  |
+===========+==============+===================+
| 1         | 128          | 8million          |
+-----------+--------------+-------------------+
| 1         | 256          | 4million          |
+-----------+--------------+-------------------+
| 1         | 512          | 2million          |
+-----------+--------------+-------------------+
| 64        | 128          | 8million          |
+-----------+--------------+-------------------+
| 64        | 256          | 4million          |
+-----------+--------------+-------------------+
| 64        | 512          | 2million          |
+-----------+--------------+-------------------+
| 128       | 128          | 4million          |
+-----------+--------------+-------------------+
| 128       | 256          | 2.5million        |
+-----------+--------------+-------------------+
| 128       | 512          | 2million          |
+-----------+--------------+-------------------+
| 256       | 128          | 5.5million        |
+-----------+--------------+-------------------+
| 256       | 256          | 4million          |
+-----------+--------------+-------------------+
| 256       | 512          | 2million          |
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
| 100k         | 128          | 1000           |
+--------------+--------------+----------------+
| 100k         | 256          | 1000           |
+--------------+--------------+----------------+
| 100k         | 512          | 1000           |
+--------------+--------------+----------------+
| 1million     | 128          | 100            |
+--------------+--------------+----------------+
| 1million     | 256          | 100            |
+--------------+--------------+----------------+
| 1million     | 512          | 100            |
+--------------+--------------+----------------+
| 4million     | 128          | 100            |
+--------------+--------------+----------------+
| 4million     | 256          | 100            |
+--------------+--------------+----------------+
| 4million     | 512          | 100            |
+--------------+--------------+----------------+

+--------------+-----------------+--------------+
| database_num | query_num       | max_vec_dims |
+==============+=================+==============+
| 10k          | 1               | 512          |
+--------------+-----------------+--------------+
| 10k          | 64              | 512          |
+--------------+-----------------+--------------+
| 10k          | 128             | 512          |
+--------------+-----------------+--------------+
| 10k          | 256             | 512          |
+--------------+-----------------+--------------+
| 100k         | 1               | 512          |
+--------------+-----------------+--------------+
| 100k         | 32              | 512          |
+--------------+-----------------+--------------+
| 100k         | 64              | 512          |
+--------------+-----------------+--------------+
| 1million     | 1               | 512          |
+--------------+-----------------+--------------+
| 1million     | 16              | 512          |
+--------------+-----------------+--------------+
| 4million     | 1               | 128          |
+--------------+-----------------+--------------+


**Sample code**

    .. code-block:: c++

        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <stdint.h>
        #include "bmcv_api_ext.h"
        int main()
        {
            int sort_cnt = 100;
            int vec_dims = 256;
            int query_vecs_num = 1;
            int database_vecs_num = 10000;
            int is_transpose = 1;
            int input_dtype = 5; // 5: float
            int output_dtype = 5;
            float* input_data = new float[query_vecs_num * vec_dims];
            float* db_data = new float[database_vecs_num * vec_dims];
            bm_handle_t handle;
            bm_device_mem_t query_data_dev_mem;
            bm_device_mem_t db_data_dev_mem;
            float *output_dis = new float[query_vecs_num * sort_cnt];
            int *output_inx = new int[query_vecs_num * sort_cnt];
            bm_device_mem_t buffer_dev_mem;
            bm_device_mem_t sorted_similarity_dev_mem;
            bm_device_mem_t sorted_index_dev_mem;

            bm_dev_request(&handle, 0);
            for (int i = 0; i < query_vecs_num * vec_dims; i++) {
                input_data[i] = ((float)rand() / (float)RAND_MAX) * 3.3;
            }
            for (int i = 0; i < vec_dims * database_vecs_num; i++) {
                db_data[i] = ((float)rand() / (float)RAND_MAX) * 3.3;
            }

            bm_malloc_device_byte(handle, &query_data_dev_mem, query_vecs_num * vec_dims * sizeof(float));
            bm_malloc_device_byte(handle, &db_data_dev_mem, database_vecs_num * vec_dims * sizeof(float));
            bm_memcpy_s2d(handle, query_data_dev_mem, input_data);
            bm_memcpy_s2d(handle, db_data_dev_mem, db_data);

            bm_malloc_device_byte(handle, &buffer_dev_mem, query_vecs_num * database_vecs_num * sizeof(float));
            bm_malloc_device_byte(handle, &sorted_similarity_dev_mem, query_vecs_num * sort_cnt * sizeof(float));
            bm_malloc_device_byte(handle, &sorted_index_dev_mem, query_vecs_num * sort_cnt * sizeof(int));

            bmcv_faiss_indexflatIP(handle, query_data_dev_mem, db_data_dev_mem, buffer_dev_mem,
                                sorted_similarity_dev_mem, sorted_index_dev_mem, vec_dims,
                                query_vecs_num, database_vecs_num, sort_cnt, is_transpose,
                                input_dtype, output_dtype);

            bm_memcpy_d2s(handle, output_dis, sorted_similarity_dev_mem);
            bm_memcpy_d2s(handle, output_inx, sorted_index_dev_mem);

            delete[] input_data;
            delete[] db_data;
            delete[] output_dis;
            delete[] output_inx;
            bm_free_device(handle, query_data_dev_mem);
            bm_free_device(handle, db_data_dev_mem);
            bm_free_device(handle, buffer_dev_mem);
            bm_free_device(handle, sorted_similarity_dev_mem);
            bm_free_device(handle, sorted_index_dev_mem);
            bm_dev_free(handle);
            return 0;
        }
        //Requesting more than 4GB of device memory
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <stdint.h>
        #include <cstdint>
        #include "bmcv_api_ext.h"
        int main()
        {
            int sort_cnt = 100;
            int vec_dims = 256;
            int query_vecs_num = 1;
            int database_vecs_num = 10000;
            int is_transpose = 1;
            int input_dtype = 5; // 5: float
            int output_dtype = 5;
            float* input_data = new float[query_vecs_num * vec_dims];
            float* db_data = new float[database_vecs_num * vec_dims];
            float *output_dis = new float[query_vecs_num * sort_cnt];
            int *output_inx = new int[query_vecs_num * sort_cnt];
            bm_handle_t handle;
            bm_device_mem_u64_t query_data_dev_mem;
            bm_device_mem_u64_t db_data_dev_mem;
            bm_device_mem_u64_t buffer_dev_mem;
            bm_device_mem_u64_t sorted_similarity_dev_mem;
            bm_device_mem_u64_t sorted_index_dev_mem;

            bm_dev_request(&handle, 0);
            for (int i = 0; i < query_vecs_num * vec_dims; i++) {
                input_data[i] = ((float)rand() / (float)RAND_MAX) * 3.3;
            }
            for (int i = 0; i < vec_dims * database_vecs_num; i++) {
                db_data[i] = ((float)rand() / (float)RAND_MAX) * 3.3;
            }

            bm_malloc_device_byte_u64(handle, &query_data_dev_mem, query_vecs_num * vec_dims * sizeof(float));
            bm_malloc_device_byte_u64(handle, &db_data_dev_mem, (uint64_t)database_vecs_num * (uint64_t)vec_dims * (uint64_t)sizeof(float));
            bm_malloc_device_byte_u64(handle, &buffer_dev_mem, (uint64_t)query_vecs_num * (uint64_t)database_vecs_num * (uint64_t)sizeof(float));
            bm_malloc_device_byte_u64(handle, &sorted_similarity_dev_mem, query_vecs_num * sort_cnt * sizeof(float));
            bm_malloc_device_byte_u64(handle, &sorted_index_dev_mem, query_vecs_num * sort_cnt * sizeof(int));
            bm_memcpy_s2d_u64(handle, query_data_dev_mem, input_data);
            bm_memcpy_s2d_u64(handle, db_data_dev_mem, db_data);

            bmcv_faiss_indexflatIP_u64(handle, query_data_dev_mem, db_data_dev_mem, buffer_dev_mem,
                                sorted_similarity_dev_mem, sorted_index_dev_mem, vec_dims,
                                query_vecs_num, database_vecs_num, sort_cnt, is_transpose,
                                input_dtype, output_dtype);

            bm_memcpy_d2s_u64(handle, output_dis, sorted_similarity_dev_mem);
            bm_memcpy_d2s_u64(handle, output_inx, sorted_index_dev_mem);

            delete[] input_data;
            delete[] db_data;
            delete[] output_dis;
            delete[] output_inx;
            bm_free_device_u64(handle, query_data_dev_mem);
            bm_free_device_u64(handle, db_data_dev_mem);
            bm_free_device_u64(handle, buffer_dev_mem);
            bm_free_device_u64(handle, sorted_similarity_dev_mem);
            bm_free_device_u64(handle, sorted_index_dev_mem);
            bm_dev_free(handle);
            return 0;
        }