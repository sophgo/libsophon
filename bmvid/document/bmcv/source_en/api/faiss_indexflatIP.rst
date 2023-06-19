bmcv_faiss_indexflatIP
======================

This interface is used to calculate inner product distance between query vectors and database vectors, output the top K (sort_cnt)  IP-values and the corresponding indices, return BM_SUCCESS if succeed.

**Interface form:**

    .. code-block:: c++

        bm_status_t bmcv_faiss_indexflatIP(
                bm_handle_t     handle,
                bm_device_mem_t input_data_global_addr,
                bm_device_mem_t db_data_global_addr,
                bm_device_mem_t buffer_global_addr,
                bm_device_mem_t output_sorted_similarity_global_addr,
                bm_device_mem_t output_sorted_index_global_addr,
                int             vec_dims,
                int             query_vecs_num,
                int             database_vecs_num,
                int             sort_cnt,
                int             is_transpose,
                int             input_dtype,
                int             output_dtype);


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

  Input parameter. Support float and char, 5 means float, 1 means char.

* int output_dtype

  Output parameter. Support float and int, 5 means float, 9 means int.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The input data type (query vectors) and data in the database (database vectors) are float or char.

2. The data type of the output sorted similarity result is float or int, and that of the corresponding indices is int.

3. Usually, the data in the database is arranged in the memory as database_vecs_num * vec_dims. Therefore, the is_transpose needs to be set to 1.

4. The larger the inner product values of the query vector and the database vector, the higher the similarity of the two vectors. Therefore, the inner product values are sorted in descending order in the process of TopK.

5. The interface is used for Faiss::IndexFlatIP.search() and implemented on BM1684X. According to the continuous memory of TPU on BM1684X, we can query about 512 inputs of 256 dimensions at a time on a single chip if the database is about 100W.


**Sample code**


    .. code-block:: c++

        int sort_cnt = 100;
        int vec_dims = 256;
        int query_vecs_num = 1;
        int database_vecs_num = 2000000;
        int is_transpose = 1;
        int input_dtype = 5; // 5: float
        int output_dtype = 5;

        float *input_data = new float[query_vecs_num * vec_dims];
        float *db_data = new float[database_vecs_num * vec_dims];

        void matrix_gen_data(float* data, u32 len) {
            for (u32 i = 0; i < len; i++) {
                data[i] = ((float)rand() / (float)RAND_MAX) * 3.3;
            }
        }

        matrix_gen_data(input_data, query_vecs_num * vec_dims);
        matrix_gen_data(db_data, vec_dims * database_vecs_num);

        bm_handle_t handle = nullptr;
        bm_dev_request(&handle, 0);
        bm_device_mem_t query_data_dev_mem;
        bm_device_mem_t db_data_dev_mem;
        bm_malloc_device_byte(handle, &query_data_dev_mem,
                query_vecs_num * vec_dims * sizeof(float));
        bm_malloc_device_byte(handle, &db_data_dev_mem,
                database_vecs_num * vec_dims * sizeof(float));
        bm_memcpy_s2d(handle, query_data_dev_mem, input_data);
        bm_memcpy_s2d(handle, db_data_dev_mem, db_data);

        float *output_dis = new float[query_vecs_num * sort_cnt];
        int *output_inx = new int[query_vecs_num * sort_cnt];
        bm_device_mem_t buffer_dev_mem;
        bm_device_mem_t sorted_similarity_dev_mem;
        bm_device_mem_t sorted_index_dev_mem;
        bm_malloc_device_byte(handle, &buffer_dev_mem,
                query_vecs_num * database_vecs_num * sizeof(float));
        bm_malloc_device_byte(handle, &sorted_similarity_dev_mem,
                query_vecs_num * sort_cnt * sizeof(float));
        bm_malloc_device_byte(handle, &sorted_index_dev_mem,
                query_vecs_num * sort_cnt * sizeof(int));

        bmcv_faiss_indexflatIP(handle,
                               query_data_dev_mem,
                               db_data_dev_mem,
                               buffer_dev_mem,
                               sorted_similarity_dev_mem,
                               sorted_index_dev_mem,
                               vec_dims,
                               query_vecs_num,
                               database_vecs_num,
                               sort_cnt,
                               is_transpose,
                               input_dtype,
                               output_dtype);
        bm_memcpy_d2s(handle, output_dis, sorted_similarity_dev_mem);
        bm_memcpy_d2s(handle, output_inx, sorted_index_dev_mem);
        delete[] input_data;
        delete[] db_data;
        delete[] output_similarity;
        delete[] output_index;
        bm_free_device(handle, query_data_dev_mem);
        bm_free_device(handle, db_data_dev_mem);
        bm_free_device(handle, buffer_dev_mem);
        bm_free_device(handle, sorted_similarity_dev_mem);
        bm_free_device(handle, sorted_index_dev_mem);
        bm_dev_free(handle);