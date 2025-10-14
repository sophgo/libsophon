bmcv_feature_match
==========================

The interface is used to compare the feature points obtained from the network (int8 format) with the feature points in the database (int8 format),and output the best matching top-k.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_feature_match(
                    bm_handle_t handle,
                    bm_device_mem_t input_data_global_addr,
                    bm_device_mem_t db_data_global_addr,
                    bm_device_mem_t output_sorted_similarity_global_addr,
                    bm_device_mem_t output_sorted_index_global_addr,
                    int batch_size,
                    int feature_size,
                    int db_size,
                    int sort_cnt = 1,
                    int rshiftbits = 0);


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

      #include "bmcv_api_ext.h"
      #include <stdint.h>
      #include <stdio.h>
      #include <stdlib.h>
      #include <math.h>
      #include <string.h>

      int main()
      {
          int batch_size = rand() % 10 + 1;
          int feature_size = rand() % 1000 + 1;
          int rshiftbits = rand() % 3;
          int sort_cnt = rand() % 30 + 1;
          int db_size = (rand() % 90 + 1) * 1000;
          bm_handle_t handle;
          int ret = 0;

          ret = (int)bm_dev_request(&handle, 0);

          int8_t* input_data = (int8_t*)malloc(sizeof(int8_t) * batch_size * feature_size);
          int8_t* db_data = (int8_t*)malloc(sizeof(int8_t) * db_size * feature_size);
          short* output_similarity = (short*)malloc(sizeof(short) * batch_size * db_size);
          int* output_index = (int*)malloc(sizeof(int) * batch_size * db_size);
          int i, j;
          int8_t temp_val;

          int8_t** db_content_vec = (int8_t**)malloc(sizeof(int8_t*) * feature_size);
          for (i = 0; i < feature_size; ++i) {
              db_content_vec[i] = (int8_t*)malloc(sizeof(int8_t) * db_size);
          }
          int8_t** input_content_vec = (int8_t**)malloc(sizeof(int8_t*) * batch_size);
          for (i = 0; i < batch_size; ++i) {
              input_content_vec[i] = (int8_t*)malloc(sizeof(int8_t) * feature_size);
          }

          short** ref_res = (short**)malloc(sizeof(short*) * batch_size);
          for (i = 0; i < batch_size; ++i) {
              ref_res[i] = (short*)malloc(sizeof(short) * db_size);
          }

          for (i = 0; i < feature_size; ++i) {
              for (j = 0; j < db_size; ++j) {
                  temp_val = rand() % 20 - 10;
                  db_content_vec[i][j] = temp_val;
              }
          }

          for (i = 0; i < batch_size; ++i) {
              for (j = 0; j < feature_size; ++j) {
                  temp_val = rand() % 20 - 10;
                  input_content_vec[i][j] = temp_val;
              }
          }

          for (i = 0; i < feature_size; ++i) {
              for (j = 0; j < db_size; ++j) {
                  db_data[i * db_size + j] = db_content_vec[i][j];
              }
          }

          for (i = 0; i < batch_size; ++i) {
              for (j = 0; j < feature_size; ++j) {
                  input_data[i * feature_size + j] = input_content_vec[i][j];
              }
          }

          ret = bmcv_feature_match(handle, bm_mem_from_system(input_data), bm_mem_from_system(db_data),
                                bm_mem_from_system(output_similarity), bm_mem_from_system(output_index),
                                batch_size, feature_size, db_size, sort_cnt, rshiftbits);

          free(input_data);
          free(db_data);
          free(output_similarity);
          free(output_index);
          for(i = 0; i < batch_size; ++i) {
              free(input_content_vec[i]);
              free(ref_res[i]);
          }
          for(i = 0; i < feature_size; ++i) {
              free(db_content_vec[i]);
          }
          free(input_content_vec);
          free(db_content_vec);
          free(ref_res);

          bm_dev_free(handle);
          return ret;
      }