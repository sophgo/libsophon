bmcv_feature_match_normalized
==================================

该接口用于将网络得到特征点（float格式）与数据库中特征点（float格式）进行比对，输出最佳匹配。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

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

  输入参数。bm_handle 句柄。

* bm_device_mem_t input_data_global_addr

  输入参数。所要比对的特征点数据存储的地址。该数据按照 batch_size * feature_size 的数据格式进行排列。batch_size，feature _size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t db_data_global_addr

  输入参数。数据库的特征点数据存储的地址。该数据按照 feature_size * db_size 的数据格式进行排列。feature_size，db_size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t db_feature_global_addr

  输入参数。数据库的特征点的 feature_size 方向模的倒数的地址。该数据按照 db_size 的数据格式进行排列。

* bm_device_mem_t output_similarity_global_addr

  输出参数。每个batch得到的比对结果的值中最大值存储地址。该数据按照 batch_size 的数据格式进行排列。batch_size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* bm_device_mem_t output_index_global_addr

  输出参数。每个batch得到的比对结果的在数据库中的序号的存储地址。如对于 batch 0 ，如果 output_sorted_similarity_global_addr 中 bacth 0 的数据是由输入数据与数据库的第800组特征点进行比对得到的，那么 output_sorted_index_global_addr 所在地址对应 batch 0 的数据为800. output_sorted_similarity_global_addr 中的数据按照 batch_size c的数据格式进行排列。batch_size 具体含义将在下面进行介绍。bm_device_mem_t 为内置表示地址的数据类型，可以使用函数 bm_mem_from_system(addr) 将普通用户使用的指针或地址转为该类型，用户可参考示例代码中的使用方式。

* int batch_size

  输入参数。待输入数据的 batch 个数，如输入数据有4组特征点，则该数据的 batch_size 为4。batch_size最大值不应超过 10。

* int feature_size

  输入参数。每组数据的特征点个数。feature_size最大值不应该超过1000。

* int db_size

  输入参数。数据库中数据特征点的组数。db_size最大值不应该超过90000。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1. 输入数据 和 数据库中数据的数据类型为 float 类型。

2. 输出的比对结果数据类型为 float，输出的序号类型为 int。

3. 数据库中的数据在内存的排布为 feature_size * db_size，因此需要将一组特征点进行转置之后再放入数据库中。

4. db_feature_global_addr 模的倒数计算方法为：1 / sqrt(y1 * y1 + y2 * y2 + ...... + yn * yn);


**示例代码**

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