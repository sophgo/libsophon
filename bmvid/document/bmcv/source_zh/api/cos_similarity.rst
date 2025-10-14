bmcv_cos_similarity
====================

对输入矩阵的每行元素进行规范化（除以l2范数），随后计算规范化矩阵和其转置矩阵的点积，得到余弦相似度矩阵，最后将相似度矩阵元素调整至0～1的范围内。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_cos_similarity(
                    bm_handle_t handle,
                    bm_device_mem_t in_global,
                    bm_device_mem_t norm_global,
                    bm_device_mem_t out_global,
                    int vec_num,
                    int vec_dims);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t in_global

  输入参数。存放输入数据的设备内存地址。

* bm_device_mem_t norm_global

  输入参数。暂存规范化矩阵的设备内存地址。

* bm_device_mem_t out_global

  输出参数。存放输出数据的设备内存地址。

* int vec_num

  输入参数。数据数量，对应输入矩阵的行数。

* int vec_dims

  输入参数。数据维度数，对应输入矩阵的列数。

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

2. 该接口支持的输入矩阵列数为256，矩阵行数范围为8-6000。


**代码示例：**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include <stdlib.h>
        #include <string.h>
        #include <assert.h>
        #include <float.h>

        float random_float(float min, float max) {
            return min + ((float)rand() / RAND_MAX) * (max - min);
        }
        int main()
        {
            int vec_num = 2614;
            int vec_dims = 256;
            bm_handle_t handle;
            bm_dev_request(&handle, 0);
            float* output_tpu = (float*)malloc(vec_num * vec_num * sizeof(float));
            float* input_data = (float*)malloc(vec_num * vec_dims * sizeof(float));
            bm_device_mem_t in_global, norm_global, out_global;

            for (int i = 0; i < vec_num; ++i) {
                for (int j = 0; j < vec_dims; ++j) {
                    input_data[i * vec_dims + j] = random_float(-100.0f, 300.0f);
                }
            }

            bm_malloc_device_byte(handle, &in_global, sizeof(float) * vec_num * vec_dims);
            bm_malloc_device_byte(handle, &norm_global, sizeof(float) * vec_num * vec_dims);
            bm_malloc_device_byte(handle, &out_global, sizeof(float) * vec_num * vec_num);
            bm_memcpy_s2d(handle, in_global, bm_mem_get_system_addr(bm_mem_from_system(input_data)));
            bmcv_cos_similarity(handle, in_global, norm_global, out_global, vec_num, vec_dims);
            bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(output_tpu)), out_global);

            bm_free_device(handle, in_global);
            bm_free_device(handle, norm_global);
            bm_free_device(handle, out_global);
            free(input_data);
            free(output_tpu);
            bm_dev_free(handle);
            return 0;
        }