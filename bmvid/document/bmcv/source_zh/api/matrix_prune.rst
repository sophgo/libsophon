bmcv_matrix_prune
====================

对矩阵进行稀疏化处理，参数p用于控制保留的边的比例。

1.根据数据点的数量选择不同的策略来确定每个点保留的邻接点数量。

2.对每个点，将与其余点的相似度进行排序，保留相似度最高的部分，并将其余的设置为0，从而使得矩阵变得稀疏。

3.返回稀疏化后的对称相似度矩阵（通过取原矩阵与其转置的平均实现对称化）。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_matrix_prune(
                    bm_handle_t handle,
                    bm_device_mem_t input_data_global_addr,
                    bm_device_mem_t output_data_global_addr,
                    bm_device_mem_t sort_index_global_addr,
                    int matrix_dims,
                    float p);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t input_data_global_addr

  输入参数。存放输入数据的设备内存地址。

* bm_device_mem_t output_data_global_addr

  输出参数。存放输出数据的设备内存地址。

* bm_device_mem_t sort_index_global_addr

  输入参数。暂存排序后索引的设备内存地址。

* int matrix_dims

  输入参数。矩阵维度，同时控制矩阵的行和列。

* float p

  输入参数。控制保留的边的比例。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型支持：**

输入数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

输出数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+


**注意事项：**

1. 在调用该接口之前必须确保所用设备内存已经申请。

2. 该接口支持的输入矩阵维度范围为8-6000，参数p的范围为0.0-1.0。


**代码示例：**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include "test_misc.h"

        int main()
        {
            int matrix_dims = 2164;
            float p = 0.5;
            bm_handle_t handle;
            float* output_tpu = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
            float* input_data = (float*)malloc(matrix_dims * matrix_dims * sizeof(float));
            bm_device_mem_t input_data_global_addr, output_data_global_addr, sort_index_global_addr;
            bm_dev_request(&handle, 0);

            for (int i = 0; i < matrix_dims; ++i) {
                for (int j = 0; j < matrix_dims; ++j) {
                    input_data[i * matrix_dims + j] = (float)rand() / RAND_MAX;
                }
            }

            bm_malloc_device_byte(handle, &input_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
            bm_malloc_device_byte(handle, &output_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
            bm_malloc_device_byte(handle, &sort_index_global_addr, sizeof(DT_INT32) * matrix_dims * matrix_dims);
            bm_memcpy_s2d(handle, input_data_global_addr, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
            bmcv_matrix_prune(handle, input_data_global_addr, output_data_global_addr,
                            sort_index_global_addr, matrix_dims, p);
            bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(output_tpu)), output_data_global_addr);

            bm_free_device(handle, input_data_global_addr);
            bm_free_device(handle, sort_index_global_addr);
            bm_free_device(handle, output_data_global_addr);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }