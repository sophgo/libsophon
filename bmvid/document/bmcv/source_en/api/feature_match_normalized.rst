bmcv_feature_match_normalized
==================================

The interface is used to compare the feature points obtained from the network (float format) with the feature points in the database (float format),and output the best matching top-k.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_feature_match_normalized(
                    bm_handle_t handle,
                    bm_device_mem_t input_data_global_addr,
                    bm_device_mem_t db_data_global_addr,
                    bm_device_mem_t db_feature_global_addr,
                    bm_device_mem_t output_similarity_global_addr,
                    bm_device_mem_t output_index_global_addr,
                    int batch_size,
                    int feature_size,
                    int db_size);


**参数说明：**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input_data_global_addr

  Input parameter. The address of the feature point data storage to be compared. The data is arranged based on the data format of batch_size * feature_size. The specific meanings of batch_size and feature_size will be introduced below. bm_device_mem_t is the built-in data type representing the address, and the function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the usage in the example code.

* bm_device_mem_t db_data_global_addr

  Input parameter. The address of the feature point data storage of the database. The data is arranged based on the data format of feature_size * db_size. The specific meanings of feature_size and db_size will be introduced below. bm_device_mem_t is the built-in data type representing the address, and the function bm_mem_from_system (addr) can be used to convert the pointer or address used by ordinary users to this type. Users can refer to the usage in the example code.

* bm_device_mem_t db_feature_global_addr

  Input parameter: The address of the inverse of the magnitude in the feature_size direction of the feature points in the database. This data is arranged according to the data format of db_size.

* bm_device_mem_t output_similarity_global_addr

  Output parameter: The storage address of the maximum value among the comparison results obtained for each batch. This data is arranged according to the batch_size data format. The specific meaning of batch_size will be explained below. bm_device_mem_t is the built-in data type for representing addresses, and the function bm_mem_from_system(addr) can be used to convert a regular user pointer or address into this type. Users can refer to the usage in the example code.

* bm_device_mem_t output_index_global_addr

  Output parameter: The storage address of the index of the comparison results for each batch in the database. For example, for batch 0, if the data in output_sorted_similarity_global_addr for batch 0 corresponds to the comparison between the input data and the 800th feature set in the database, then the data at the address pointed to by output_sorted_index_global_addr for batch 0 will be 800.

* int batch_size

  Input parameter: The number of batches of input data. For example, if there are 4 sets of feature points in the input data, then the batch_size is 4. The maximum value of batch_size should not exceed 10.

* int feature_size

  Input parameter: The number of feature points in each data set. The maximum value of feature_size should not exceed 1000.

* int db_size

  Input parameter: The number of feature point sets in the database. The maximum value of db_size should not exceed 90,000.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note：**

1. The data type of input data and data in the database is float.

2. The output comparison result data type is float, and the output sequence number type is int.

3. The data in the database is arranged in the memory as feature_size * db_size. Therefore, it is necessary to transpose a group of feature points before putting them into the database.

4. The calculation method for the inverse of db_feature_global_addr is: 1 / sqrt(y1 * y1 + y2 * y2 + ...... + yn * yn);


**Sample code**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>

        static int calc_sqrt_transposed(float** feature, int rows, int cols, float* db_feature)
        {
            int i, j;
            float tmp;
            float result;

            for (i = 0; i < cols; ++i) {
                tmp = 0.f;
                for (j = 0; j < rows; ++j) {
                    tmp += feature[j][i] * feature[j][i];
                }
                result = 1.f / sqrt(tmp);
                db_feature[i] = result;
            }
            return 0;
        }

        int main()
        {
            int batch_size = rand() % 8 + 1;
            int feature_size = rand() % 1000 + 1;
            int db_size = (rand() % 90 + 1) * 1000;
            bm_handle_t handle;
            int ret = 0;

            ret = (int)bm_dev_request(&handle, 0);
            if (ret) {
                printf("Create bm handle failed. ret = %d\n", ret);
                return ret;
            }

            float* input_data = (float*)malloc(sizeof(float) * batch_size * feature_size);
            float* db_data = (float*)malloc(sizeof(float) * db_size * feature_size);
            float* db_feature = (float*)malloc(sizeof(float) * db_size);
            float* output_similarity = (float*)malloc(sizeof(float) * batch_size); /*float*/
            int* output_index = (int*)malloc(sizeof(int) * batch_size);
            int i, j;
            float** db_content_vec = (float**)malloc(feature_size * sizeof(float*)); /*row = feature_size col = db_size*/
            float** input_content_vec = (float**)malloc(batch_size * sizeof(float*)); /*row = batch_size col = feature_size*/
            float** ref_res = (float**)malloc(sizeof(float*) * batch_size); /* row = batch_size col = db_size */

            for (i = 0; i < feature_size; ++i) {
                db_content_vec[i] = (float*)malloc(db_size * sizeof(float));
                for (j = 0; j < db_size; ++j) {
                    db_content_vec[i][j] = rand() % 20 -10;
                }
            }

            for (i = 0; i < batch_size; ++i) {
                input_content_vec[i] = (float*)malloc(feature_size * sizeof(float));
                for (j = 0; j < feature_size; ++j) {
                    input_content_vec[i][j] = rand() % 20 -10;
                }
            }

            for (i = 0; i < batch_size; ++i) {
                ref_res[i] = (float*)malloc(db_size * sizeof(float));
            }

            for (i = 0; i < feature_size; ++i) {
                for (j = 0; j < db_size; ++j) {
                    db_data[i * db_size + j] = db_content_vec[i][j];
                }
            }

            ret = calc_sqrt_transposed(db_content_vec, feature_size, db_size, db_feature);

            for (i = 0; i < batch_size; i++) {
                for (j = 0; j < feature_size; j++) {
                    input_data[i * feature_size + j] = input_content_vec[i][j];
                }
            }

            ret = bmcv_feature_match_normalized(handle, bm_mem_from_system(input_data), bm_mem_from_system(db_data),
                                            bm_mem_from_system(db_feature), bm_mem_from_system(output_similarity),
                                            bm_mem_from_system(output_index), batch_size, feature_size, db_size);


            free(input_data);
            free(db_data);
            free(db_feature);
            free(output_similarity);
            free(output_index);
            for(i = 0; i < batch_size; i++) {
                free(input_content_vec[i]);
                free(ref_res[i]);
            }
            for(i = 0; i < feature_size; i++) {
                free(db_content_vec[i]);
            }
            free(input_content_vec);
            free(db_content_vec);
            free(ref_res);

            bm_dev_free(handle);
            return ret;
        }