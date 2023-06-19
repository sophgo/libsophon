bmcv_faiss_indexflatIP
======================

计算查询向量与数据库向量的内积距离, 输出前 K （sort_cnt） 个最匹配的内积距离值及其对应的索引。

**接口形式：**

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


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t input_data_global_addr

  输入参数。存放查询向量组成的矩阵的 device 空间。

* bm_device_mem_t db_data_global_addr

  输入参数。存放底库向量组成的矩阵的 device 空间。

* bm_device_mem_t buffer_global_addr

  输入参数。存放计算出的内积值的缓存空间。

* bm_device_mem_t output_sorted_similarity_global_addr

  输出参数。存放排序后的最匹配的内积值的 device 空间。

* bm_device_mem_t output_sorted_index_global_add

  输出参数。存储输出内积值对应索引的 device 空间。

* int vec_dims

  输入参数。向量维数。

* int query_vecs_num

  输入参数。查询向量的个数。

* int database_vecs_num

  输入参数。底库向量的个数。

* int sort_cnt

  输入参数。输出的前 sort_cnt 个最匹配的内积值。

* int is_transpose

  输入参数。0 表示底库矩阵不转置; 1 表示底库矩阵转置。

* int input_dtype

  输入参数。输入数据类型，支持 float 和 char, 5 表示float, 1 表示char。

* int output_dtype

  输出参数。输出数据类型，支持 float 和 int, 5 表示float, 9 表示int。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项：**

1、输入数据(查询向量)和底库数据(底库向量)的数据类型为 float 或 char。

2、输出的排序后的相似度的数据类型为 float 或 int, 相对应的索引的数据类型为 int。

3、底库数据通常以 database_vecs_num * vec_dims 的形式排布在内存中。此时, 参数 is_transpose 需要设置为 1。

4、查询向量和数据库向量内积距离值越大, 表示两者的相似度越高。因此, 在 TopK 过程中对内积距离值按降序排序。

5、该接口用于 Faiss::IndexFlatIP.search(), 在 BM1684X 上实现。考虑 BM1684X 上 TPU 的连续内存, 针对 100W 底库, 可以在单芯片上一次查询最多约 512 个 256 维的输入。


**示例代码**


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