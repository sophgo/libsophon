bmcv_faiss_indexPQ_ADC
======================

该接口通过 query 和 centroids 计算距离表，对底库编码查表并排序，输出前 K (sort_cnt) 个最匹配的向量索引及其对应的距离。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

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


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t centroids_input_dev

  输入参数。存储聚类中心数据的 device 空间。

* bm_device_mem_t nxquery_input_dev

  输入参数。存储查询向量组成的矩阵的 device 空间。

* bm_device_mem_t nycodes_input_dev

  输入参数。存放底库向量组成的矩阵的 device 空间。

* bm_device_mem_t distance_output_dev

  输出参数。存放输出距离的 device 空间。

* bm_device_mem_t index_output_dev

  输出参数。存放输出排序的 device 空间。

* int vec_dims

  输入参数。原始向量的维度。

* int slice_num

  输入参数。原始维度切分数量。

* int centroids_num

  输入参数。聚类中心的数量。

* int database_num

  输入参数。数据底库的数量。

* int query_num

  输入参数。检索向量的数量。

* int sort_cnt

  输入参数。输出前sort_cnt个最匹配的底库向量。

* int IP_metric

  输入参数。0 表示L2距离计算; 1 表示IP距离计算。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项：**

1、输入数据 (查询向量) 和聚类中心的数据类型为 float，底库数据 (底库编码)的数据类型为uint8，都存储在设备内存上。

2、输出的排序后的相似度结果的数据类型为 float, 相对应的索引的数据类型为 int，存储在设备内存上。

3、L2_metric 计算的是 L2 距离平方，没有进行开方 (参考faiss源码的实现)。

4、查询向量和数据库向量 L2 距离越小, 表示两者的相似度越高。输出 L2 topk距离按升序排序。

5、查询向量和数据库向量 IP 距离越大, 表示两者的相似度越高。输出 IP topk距离按降序排序。

6、faiss系列算子有多个输入参数，每个参数都有一个使用范围限制，超过该范围的入参对应tpu输出会出错，我们选择了三个主要参数做了测试，固定其中两个维度，测试了第三个维度的最大值，测试结果如下表格所示：

+-----------+--------------+-------------------+
| query_num | vec_dims     | max_database_num  |
+===========+==============+===================+
| 1         | 128          | 6500万            |
+-----------+--------------+-------------------+
| 1         | 256          | 6500万            |
+-----------+--------------+-------------------+
| 1         | 512          | 6500万            |
+-----------+--------------+-------------------+
| 64        | 128          | 2500万            |
+-----------+--------------+-------------------+
| 64        | 256          | 2500万            |
+-----------+--------------+-------------------+
| 64        | 512          | 1500万            |
+-----------+--------------+-------------------+
| 256       | 128          | 600万             |
+-----------+--------------+-------------------+
| 256       | 256          | 600万             |
+-----------+--------------+-------------------+
| 256       | 512          | 600万             |
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
| 1万          | 128          | 1000           |
+--------------+--------------+----------------+
| 1万          | 256          | 1000           |
+--------------+--------------+----------------+
| 1万          | 512          | 1000           |
+--------------+--------------+----------------+
| 10万         | 128          | 100            |
+--------------+--------------+----------------+
| 10万         | 256          | 50             |
+--------------+--------------+----------------+
| 10万         | 512          | 50             |
+--------------+--------------+----------------+

+--------------+-----------------+--------------+
| database_num | query_num       | max_vec_dims |
+==============+=================+==============+
| 1万          | 1               | 2048         |
+--------------+-----------------+--------------+
| 1万          | 64              | 512          |
+--------------+-----------------+--------------+
| 1万          | 128             | 512          |
+--------------+-----------------+--------------+
| 1万          | 256             | 512          |
+--------------+-----------------+--------------+
| 10万         | 1               | 2048         |
+--------------+-----------------+--------------+
| 10万         | 32              | 512          |
+--------------+-----------------+--------------+
| 10万         | 64              | 512          |
+--------------+-----------------+--------------+
| 100万        | 1               | 128          |
+--------------+-----------------+--------------+


**示例代码**

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