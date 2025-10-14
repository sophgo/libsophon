bmcv_batch_topk
================

计算每个 db 中最大或最小的k个数，并返回index。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_batch_topk(
                    bm_handle_t handle,
                    bm_device_mem_t src_data_addr,
                    bm_device_mem_t src_index_addr,
                    bm_device_mem_t dst_data_addr,
                    bm_device_mem_t dst_index_addr,
                    bm_device_mem_t buffer_addr,
                    bool src_index_valid,
                    int k,
                    int batch,
                    int* per_batch_cnt,
                    bool same_batch_cnt,
                    int src_batch_stride,
                    bool descending);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t src_data_addr

  输入参数。input_data的设备地址信息。

* bm_device_mem_t src_index_addr

  输入参数。input_index的设备地址信息，当src_index_valid为true时，设置该参数。

* bm_device_mem_t dst_data_addr

  输出参数。output_data设备地址信息。

* bm_device_mem_t dst_index_addr

  输出参数。output_index设备信息

* bm_device_mem_t buffer_addr

  输入参数。缓冲区设备地址信息

* bool src_index_valid

  输入参数。如果为true， 则使用src_index，否则使用自动生成的index。

* int k

  输入参数。k的值。

* int batch

  输入参数。batch数量。

* int * per_batch_cnt

  输入参数。每个batch的数据数量。

* bool same_batch_cnt

  输入参数。判断每个batch数据是否相同。

* int src_batch_stride

  输入参数。两个batch之间的距离。

* bool descending

  输入参数。升序或者降序


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前仅支持float32类型数据。


**代码示例：**

    .. code-block:: c

        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include "bmcv_api_ext.h"

        int main()
        {
            int batch_num = 10000;
            int k = batch_num / 10;
            int descending = rand() % 2;
            int batch = rand() % 20 + 1;
            int batch_stride = batch_num;
            bool bottom_index_valid = true;
            bm_handle_t handle;
            float* bottom_data = new float[batch * batch_stride * sizeof(float)];
            int* bottom_index = new int[batch * batch_stride];
            float* top_data = new float[batch * batch_stride * sizeof(float)];
            int* top_index = new int[batch * batch_stride];
            float* buffer = new float[3 * batch_stride * sizeof(float)];

            bm_dev_request(&handle, 0);

            for(int i = 0; i < batch; i++){
                for(int j = 0; j < batch_num; j++){
                    bottom_data[i * batch_stride + j] = rand() % 10000 * 1.0f;
                    bottom_index[i * batch_stride + j] = i * batch_stride + j;
                }
            }

            bmcv_batch_topk(handle, bm_mem_from_system((void*)bottom_data),
                            bm_mem_from_system((void*)bottom_index),
                            bm_mem_from_system((void*)top_data),
                            bm_mem_from_system((void*)top_index),
                            bm_mem_from_system((void*)buffer),
                            bottom_index_valid, k, batch,
                            &batch_num, true, batch_stride,
                            descending);

            delete [] bottom_data;
            delete [] bottom_index;
            delete [] top_data;
            delete [] top_index;
            bm_dev_free(handle);
            return 0;
        }