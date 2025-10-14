bmcv_lap_matrix
====================

拉普拉斯矩阵是图论中一个非常重要的概念，用于分析图的性质。


**处理器型号支持：**

该接口支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_lap_matrix(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    bm_device_mem_t output,
                    int row,
                    int col);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t input

  输入参数。输入矩阵的 device 空间。其大小为 row * col * sizeof(float32)。

* bm_device_mem_t output

  输出参数。输出矩阵的 device 空间。其大小为 row * col * sizeof(float32)。

* int row

  输入参数。矩阵的行数。

* int col

  输入参数。矩阵的列数。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项：**

1、目前该接口只支持矩阵的数据类型为float。


**代码示例：**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>
        int main()
        {
            int row = 1024;
            int col = 1024;
            float* input_addr = (float*)malloc(row * col * sizeof(float));
            float* output_addr = (float*)malloc(row * col * sizeof(float));
            bm_handle_t handle;
            bm_status_t ret = BM_SUCCESS;
            bm_device_mem_t input, output;
            int i;

            for (i = 0; i < row * col; ++i) {
                input_addr[i] = (float)rand() / RAND_MAX;
            }

            ret = bm_dev_request(&handle, 0);
            if (ret != BM_SUCCESS) {
                printf("bm_dev_request failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_malloc_device_byte(handle, &input, row * col * sizeof(float));
            if (ret != BM_SUCCESS) {
                printf("bm_malloc_device_byte failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_malloc_device_byte(handle, &output, row * col * sizeof(float));
            if (ret != BM_SUCCESS) {
                printf("bm_malloc_device_byte failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_memcpy_s2d(handle, input, input_addr);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_s2d failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bmcv_lap_matrix(handle, input, output, row, col);
            if (ret != BM_SUCCESS) {
                printf("bmcv_lap_matrix failed. ret = %d\n", ret);
                exit(-1);
            }

            ret = bm_memcpy_d2s(handle, output_addr, output);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_d2s failed. ret = %d\n", ret);
                exit(-1);
            }

            free(input_addr);
            free(output_addr);
            bm_free_device(handle, input);
            bm_free_device(handle, output);
            bm_dev_free(handle);
            return 0;
        }