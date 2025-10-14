bmcv_feature_match
==========================

该接口用于将网络得到特征点（int8格式）与数据库中特征点（int8格式）进行比对，输出最佳匹配的top-k。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

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


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t  input_data_global_addr

  输入参数。所要比对的特征点数据存储的地址。该数据按照 batch_size * feature_size 的数据格式进行排列。batch_size，feature _size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t db_data_global_addr

  输入参数。数据库的特征点数据存储的地址。该数据按照 feature_size * db_size 的数据格式进行排列。feature_size，db_size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t output_sorted_similarity_global_addr

  输出参数。每个batch得到的比对结果的值中最大几个值（降序排列）存储地址，具体取多少个值由sort_cnt决定。该数据按照 batch_size * sort_cnt 的数据格式进行排列。batch_size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t output_sorted_index_global_addr

  输出参数。每个batch得到的比对结果的在数据库中的序号的存储地址。如对于 batch 0 ，如果 output_sorted_similarity_global_addr 中 bacth 0 的数据是由输入数据与数据库的第800组特征点进行比对得到的，那么 output_sorted_index_global_addr 所在地址对应 batch 0 的数据为800. output_sorted_similarity_global_addr 中的数据按照 batch_size * sort_cnt 的数据格式进行排列。batch_size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* int  batch_size

  输入参数。待输入数据的 batch 个数，如输入数据有4组特征点，则该数据的 batch_size 为4。batch_size最大值不应超过8。

* int  feature_size

  输入参数。每组数据的特征点个数。feature_size最大值不应该超过4096。

* int  db_size

  输入参数。数据库中数据特征点的组数。db_size最大值不应该超过500000。

* int  sort_cnt

  输入参数。每个 batch 对比结果中所要排序个数，也就是输出结果个数，如需要最大的3个比对结果，则sort_cnt设置为3。该值默认为1。sort_cnt最大值不应该超过30。

* int  rshiftbits

  输入参数。对结果进行右移处理的位数，右移采用round对小数进行取整处理。该参数默认为0。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1、输入数据和数据库中数据的数据类型为 char。

2、输出的比对结果数据类型为 short，输出的序号类型为 int。

3、数据库中的数据在内存的排布为 feature_size * db_size，因此需要将一组特征点进行转置之后再放入数据库中。

4、sort_cnt的取值范围为 1 ~ 30。


**示例代码**

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