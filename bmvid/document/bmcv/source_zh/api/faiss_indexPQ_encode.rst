bmcv_faiss_indexPQ_encode
==========================

该接口输入 vectors 和 centroids 计算距离表并排序，输出 vectors 的量化编码。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c++

        bm_status_t bmcv_faiss_indexPQ_encode(
                    bm_handle_t handle,
                    bm_device_mem_t vector_input_dev,
                    bm_device_mem_t centroids_input_dev,
                    bm_device_mem_t buffer_table_dev,
                    bm_device_mem_t codes_output_dev,
                    int encode_vec_num,
                    int vec_dims,
                    int slice_num,
                    int centroids_num,
                    int IP_metric);


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t vector_input_dev

  输入参数。存放待编码向量的 device 空间。

* bm_device_mem_t centroids_input_dev

  输入参数。存储聚类中心数据的 device 空间。

* bm_device_mem_t buffer_table_dev

  输入参数。存放计算出的距离表的缓存空间。

* bm_device_mem_t codes_output_dev

  输出参数。存放向量编码结果的 device 空间。

* int encode_vec_num

  输入参数。待编码向量的个数。

* int vec_dims

  输入参数。原始向量的维度。

* int slice_num

  输入参数。原始维度切分数量。

* int centroids_num

  输入参数。聚类中心的数量。

* int IP_metric

  输入参数。0 表示L2距离计算; 1 表示IP距离计算。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1、输入数据 (查询向量) 和聚类中心的数据类型为 float，输出向量编码的数据类型为 uint8，存储在设备内存上。

2、buffer_table 的大小为 slice_num * centroids_num，数据类型为float。


**示例代码**

    .. code-block:: c++

      #include "bmcv_api_ext.h"
      #include "test_misc.h"
      #include <stdio.h>
      #include <stdlib.h>
      #include <assert.h>

        int main()
        {
            int vec_dims = 256;
            int encode_vec_num = 1;
            int slice_m = 32;
            int ksub = 256;
            int dsub = vec_dims / slice_m;
            int input_dtype = 5; // 5: float
            int IP_metric = 0;
            bm_handle_t handle;
            int centroids_size = slice_m * ksub * dsub * dtype_size((data_type_t)input_dtype);
            int nxcodes_size = encode_vec_num * vec_dims * dtype_size((data_type_t)input_dtype);;
            int buffer_table_size = slice_m * ksub * dtype_size((data_type_t)input_dtype);;
            int output_codes_size = encode_vec_num * slice_m;
            bm_device_mem_t centroids_input_dev, nxcodes_input_dev, buffer_table_dev, codes_output_dev;
            float* centroids_input_sys_fp32 = (float*)malloc(slice_m * ksub * dsub * sizeof(float));
            unsigned char* nxcodes_input_sys = (unsigned char*)malloc(encode_vec_num * vec_dims);
            unsigned char* output_codes_sys = (unsigned char*)malloc(encode_vec_num * slice_m);

            for (int i = 0; i < slice_m; i++) {
                for (int j = 0; j < ksub; j++) {
                    for (int n = 0; n < dsub; n++) {
                        float value = (float)rand() / RAND_MAX * 20.0 - 10.0;
                        centroids_input_sys_fp32[i * dsub * ksub + j * dsub + n] = value;
                    }
                }
            }
            for (int i = 0; i < encode_vec_num; i++) {
                for (int j = 0; j < slice_m; j++) {
                    nxcodes_input_sys[i * slice_m + j] = rand() % 256;
                }
            }

            bm_dev_request(&handle, 0);
            bm_malloc_device_byte(handle, &centroids_input_dev, centroids_size);
            bm_malloc_device_byte(handle, &nxcodes_input_dev, nxcodes_size);
            bm_malloc_device_byte(handle, &buffer_table_dev, buffer_table_size);
            bm_malloc_device_byte(handle, &codes_output_dev, output_codes_size);
            bm_memcpy_s2d(handle, centroids_input_dev, centroids_input_sys_fp32);
            bm_memcpy_s2d(handle, nxcodes_input_dev, nxcodes_input_sys);

            bmcv_faiss_indexPQ_encode(handle, nxcodes_input_dev, centroids_input_dev, buffer_table_dev,
                        codes_output_dev, encode_vec_num, vec_dims, slice_m, ksub, IP_metric);
            bm_memcpy_d2s(handle, output_codes_sys, codes_output_dev);

            bm_free_device(handle, centroids_input_dev);
            bm_free_device(handle, nxcodes_input_dev);
            bm_free_device(handle, buffer_table_dev);
            bm_free_device(handle, codes_output_dev);
            free(centroids_input_sys_fp32);
            free(nxcodes_input_sys);
            free(output_codes_sys);
            bm_dev_free(handle);
            return 0;
        }