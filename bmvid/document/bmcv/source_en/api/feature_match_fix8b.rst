bmcv_feature_match
==========================

The interface is used to compare the feature points obtained from the network (int8 format) with the feature points in the database (int8 format),and output the best matching top-k.

**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_feature_match(
            bm_handle_t     handle,
            bm_device_mem_t input_data_global_addr,
            bm_device_mem_t db_data_global_addr,
            bm_device_mem_t output_sorted_similarity_global_addr,
            bm_device_mem_t output_sorted_index_global_addr,
            int             batch_size,
            int             feature_size,
            int             db_size,
            int             sort_cnt = 1,
            int             rshiftbits = 0);


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t  input_data_global_addr

  Input parameter. The address of the feature point data storage to be compared. The data is arranged based on the data format of batch_size * feature_size. The specific meanings of batch_size and feature_size will be introduced below. bm_device_mem_t is the built-in data type representing the address, and the function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the usage in the example code.

* bm_device_mem_t db_data_global_addr

  Input parameter. The address of the feature point data storage of the database. The data is arranged based on the data format of feature_size * db_size. The specific meanings of feature_size and db_size will be introduced below. bm_device_mem_t is the built-in data type representing the address, and the function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the usage in the example code.

* bm_device_mem_t output_sorted_similarity_global_addr

  Output parameter. The storage address of the maximum values (in descending order) of the comparison results obtained by each batch. The specific number of values is determined by sort_cnt. The data is arranged based on the data format of batch_size * sort_cnt. The specific meaning of batch_size will be introduced below. bm_device_mem_t is the built-in data representing the address type, you can use the function bm_mem_from_system(addr) to convert the pointer or address used by ordinary users to For this type, users can refer to the usage in the sample code.

* bm_device_mem_t output_sorted_index_global_addr

  Output parameter. The storage address of the serial number in the database of the comparison result obtained by each batch. For example, for batch 0, if output_sorted_similarity_global_addr is obtained by comparing the input data with the 800th group of feature points in the database, then the data of batch 0 corresponding to the address of output_sorted_index_global_addr is 800. The data in output_sorted_similarity_global_addr is arranged in the data format of batch_size * sort_cnt. The specific meaning of batch_size will be introduced below.  bm_device_mem_t is the built-in data type representing the address, and the function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the usage in the example code.

* int  batch_size

  Input parameter. The number of batch whose data is to be input. If the input data has 4 groups of feature points, the batch_size of the data is 4. The maximum batch_size should not exceed 8.

* int  feature_size

  Input parameter. The number of feature points of each data group. The maximum feature_size should not exceed 4096.

* int  db_size

  Input parameter. The number of groups of data feature points in the database. The maximum db_size should not exceed 500000.

* int  sort_cnt

  Input parameter. The number to be sorted in each batch comparison result, that is, the number of output results. If the maximum three comparison results are required, set sort_cnt to 3. The defaulted value is 1. The maximum sort_cnt should not exceed 30.

* int  rshiftbits

  Input parameter. The number of digits of shifting the result to the right, which uses round to round the decimal. This parameter defaults to 0.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The data type of input data and data in the database is char.

2. The output comparison result data type is short, and the output sequence number type is int.

3. The data in the database is arranged in the memory as feature_size * db_size. Therefore, it is necessary to transpose a group of feature points before putting them into the database.

4. The value range of sort_cnt is 1 ~ 30.


**Sample code**


    .. code-block:: c

         int batch_size = 4;
         int feature_size = 512;
         int db_size = 1000;
         int sort_cnt = 1;
         unsigned char src_data_p[4 * 512];
         unsigned char db_data_p[512 * 1000];
         short output_val[4];
         int output_index[4];
         for (int i = 0; i < 4 * 512; i++) {
             src_data_p[i] = rand() % 1000;
         }
         for (int i = 0; i < 512 * 1000; i++) {
             db_data_p[i] = rand() % 1000;
         }
         bmcv_feature_match(handle,
             bm_mem_from_system(src_data_p),
             bm_mem_from_system(db_data_p),
             bm_mem_from_system(output_val),
             bm_mem_from_system(output_index),
             batch_size,
             feature_size,
             db_size,
             sort_cnt, 8);


