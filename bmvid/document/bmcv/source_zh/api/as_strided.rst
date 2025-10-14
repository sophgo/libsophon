bmcv_as_strided
=================

该接口可以根据现有矩阵以及给定的步长来创建一个视图矩阵。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_as_strided(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    bm_device_mem_t output,
                    int input_row,
                    int input_col,
                    int output_row,
                    int output_col,
                    int row_stride,
                    int col_stride);


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t input

  输入参数。存放输入矩阵 input 数据的设备内存地址。

* bm_device_mem_t output

  输入参数。存放输出矩阵 output 数据的设备内存地址。

* int input_row

  输入参数。输入矩阵 input 的行数。

* int input_col

  输入参数。输入矩阵 input 的列数。

* int output_row

  输入参数。输出矩阵 output 的行数。

* int output_col

  输入参数。输出矩阵 output 的列数。

* int row_stride

  输入参数。输出矩阵行之间的步长。

* int col_stride

  输入参数。输出矩阵列之间的步长。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**示例代码**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>

        int main()
        {
            int input_row = 5;
            int input_col = 5;
            int output_row = 3;
            int output_col = 3;
            int row_stride = 1;
            int col_stride = 2;
            bm_handle_t handle;
            int len = input_row * input_col;
            bm_device_mem_t input_dev_mem, output_dev_mem;
            bm_dev_request(&handle, 0);

            float* input_data = new float[input_row * input_col];
            float* output_data = new float[output_row * output_col];

            for (int i = 0; i < len; i++) {
                input_data[i] = (float)rand() / (float)RAND_MAX * 100;
            }

            bm_malloc_device_byte(handle, &input_dev_mem, input_row * input_col * sizeof(float));
            bm_malloc_device_byte(handle, &output_dev_mem, output_row * output_col * sizeof(float));
            bm_memcpy_s2d(handle, input_dev_mem, input_data);
            bmcv_as_strided(handle, input_dev_mem, output_dev_mem, input_row, input_col,
                            output_row, output_col, row_stride, col_stride);
            bm_memcpy_d2s(handle, output_data, output_dev_mem);

            bm_free_device(handle, input_dev_mem);
            bm_free_device(handle, output_dev_mem);
            delete[] output_data;
            delete[] input_data;
            bm_dev_free(handle);
            return 0;
        }