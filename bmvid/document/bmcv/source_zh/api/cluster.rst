bmcv_cluster
=============

谱聚类。内部通过依次调用：bmcv_cos_similarity、bmcv_matrix_prune、bmcv_lap_matrix、bmcv_qr、bmcv_knn 接口实现谱聚类。


**处理器型号支持：**

该接口支持 BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_cluster(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    bm_device_mem_t output,
                    int row,
                    int col,
                    float p,
                    int min_num_spks,
                    int max_num_spks,
                    int user_num_spks,
                    int weight_mode_KNN,
                    int num_iter_KNN);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。存放输入矩阵，其大小为 row * col * sizeof(float32)。

* bm_image output

  输出参数。存放 KNN 结果标签，其大小为 row * sizeof(int)。

* int row

  输入参数。输入矩阵的行数。

* int col

  输入参数。输入矩阵的列数。

* float p

  输入参数。用于剪枝步骤中的比例参数，控制如何减少相似性矩阵中的连接。

* int min_num_spks

  输入参数。最小的聚类数。

* int max_num_spks

  输入参数。最大的聚类数。

* int user_num_spks

  输入参数。指定要使用的特征向量数量，可用于直接控制输出的聚类数。如果未指定，则根据数据动态计算。

* int weight_mode_KNN

  在SciPy库中，K-means算法的质心初始化方法, 0 表示 CONST_WEIGHT，1 表示 POISSON_CPP，2 表示 MT19937_CPP。默认使用 CONST_WEIGHT。

* int num_iter_KNN

  输入参数。KNN 算法的迭代次数。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

1、目前该接口只支持矩阵的数据类型为float。


**代码示例：**

    .. code-block:: c

        #include "bmcv_api.h"
        #include "test_misc.h"
        #include <assert.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <string.h>

        int main()
        {
            int row = 128;
            int col = 128;
            float p = 0.01;
            int min_num_spks = 2;
            int max_num_spks = 8;
            int num_iter_KNN = 2;
            int weight_mode_KNN = 0;
            int user_num_spks = -1;
            float* input_data = (float *)malloc(row * col * sizeof(float));
            int* output_data = (int *)malloc(row * max_num_spks * sizeof(int));
            bm_handle_t handle;
            bm_device_mem_t input, output;

            bm_dev_request(&handle, 0);

            for (int i = 0; i < row * col; ++i) {
                input_data[i] = (float)rand() / RAND_MAX;
            }

            bm_malloc_device_byte(handle, &input, sizeof(float) * row * col);
            bm_malloc_device_byte(handle, &output, sizeof(float) * row * max_num_spks);
            bm_memcpy_s2d(handle, input, input_data);
            bmcv_cluster(handle, input, output, row, col, p, min_num_spks, max_num_spks,
                        user_num_spks, weight_mode_KNN, num_iter_KNN);
            bm_memcpy_d2s(handle, output_data, output);

            bm_free_device(handle, input);
            bm_free_device(handle, output);
            free(input_data);
            free(output_data);
            bm_dev_free(handle);
            return 0;
        }