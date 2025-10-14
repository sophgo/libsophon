bmcv_hm_distance
==================

计算两个向量中各个元素的汉明距离。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_hamming_distance(
                    bm_handle_t handle,
                    bm_device_mem_t input1,
                    bm_device_mem_t input2,
                    bm_device_mem_t output,
                    int bits_len,
                    int input1_num,
                    int input2_num);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input1

  输入参数。向量1数据的设备地址信息。

* bm_image input2

  输入参数。向量2数据的设备地址信息。

* bm_image output

  输出参数。output向量数据的设备地址信息。

* int bits_len

  输入参数。向量中的每个元素的长度

* int input1_num

  输入参数。向量1的数据个数

* int input2_num

  输入参数。向量2的数据个数


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**示例代码**

    .. code-block:: c

        #include <math.h>
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include "bmcv_api_ext.h"

        int main()
        {
            int bits_len = 8;
            int input1_num = 2;
            int input2_num = 2562;
            int* input1_data = new int[input1_num * bits_len];
            int* input2_data = new int[input2_num * bits_len];
            int* output_tpu  = new int[input1_num * input2_num];
            bm_device_mem_t input1_dev_mem;
            bm_device_mem_t input2_dev_mem;
            bm_device_mem_t output_dev_mem;
            bm_handle_t handle;

            memset(input1_data, 0, input1_num * bits_len * sizeof(int));
            memset(input2_data, 0, input2_num * bits_len * sizeof(int));
            memset(output_tpu,  0,  input1_num * input2_num * sizeof(int));

            // fill data
            for(int i = 0; i < input1_num * bits_len; i++) {
                input1_data[i] = rand() % 10;
            }
            for(int i = 0; i < input2_num * bits_len; i++) {
                input2_data[i] = rand() % 20 + 1;
            }

            bm_dev_request(&handle, 0);
            bm_malloc_device_byte(handle, &input1_dev_mem, input1_num * bits_len * sizeof(int));
            bm_malloc_device_byte(handle, &input2_dev_mem, input2_num * bits_len * sizeof(int));
            bm_malloc_device_byte(handle, &output_dev_mem, input1_num * input2_num * sizeof(int));
            bm_memcpy_s2d(handle, input1_dev_mem, input1_data);
            bm_memcpy_s2d(handle, input2_dev_mem, input2_data);
            bmcv_hamming_distance(handle, input1_dev_mem, input2_dev_mem, output_dev_mem,
                                bits_len, input1_num, input2_num);
            bm_memcpy_d2s(handle, output_tpu, output_dev_mem);

            delete[] input1_data;
            delete[] input2_data;
            delete[] output_tpu;
            bm_free_device(handle, input1_dev_mem);
            bm_free_device(handle, input2_dev_mem);
            bm_free_device(handle, output_dev_mem);
            bm_dev_free(handle);
            return 0;
        }