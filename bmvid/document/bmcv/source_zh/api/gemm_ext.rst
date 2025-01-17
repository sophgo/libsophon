bmcv_gemm_ext
=============

该接口可以实现 fp32/fp16 类型矩阵的通用乘法计算，如下公式：

  .. math::

      Y = \alpha\times A\times B + \beta\times C

其中，A、B、C、Y均为矩阵，:math:`\alpha` 和 :math:`\beta` 均为常系数

接口的格式如下：

    .. code-block:: c

         bm_status_t bmcv_gemm_ext(bm_handle_t     handle,
                               bool            is_A_trans,
                               bool            is_B_trans,
                               int             M,
                               int             N,
                               int             K,
                               float           alpha,
                               bm_device_mem_t A,
                               bm_device_mem_t B,
                               float           beta,
                               bm_device_mem_t C,
                               bm_device_mem_t Y,
                               bm_image_data_format_ext input_dtype,
                               bm_image_data_format_ext output_dtype);

**处理器型号支持：**

该接口仅支持BM1684X。


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄

* bool is_A_trans

  输入参数。设定矩阵 A 是否转置

* bool is_B_trans

  输入参数。设定矩阵 B 是否转置

* int M

  输入参数。矩阵 A、C、Y 的行数

* int N

  输入参数。矩阵 B、C、Y 的列数

* int K

  输入参数。矩阵 A 的列数和矩阵 B 的行数

* float alpha

  输入参数。数乘系数

* bm_device_mem_t A

  输入参数。根据数据存放位置保存左矩阵 A 数据的 device 地址，需在使用前完成数据s2d搬运。

* bm_device_mem_t B

  输入参数。根据数据存放位置保存右矩阵 B 数据的 device 地址，需在使用前完成数据s2d搬运。

* float beta

  输入参数。数乘系数。

* bm_device_mem_t C

  输入参数。根据数据存放位置保存矩阵 C 数据的 device 地址，需在使用前完成数据s2d搬运。

* bm_device_mem_t Y

  输出参数。矩阵 Y 数据的 device 地址，保存输出结果。

* bm_image_data_format_ext input_dtype

  输入参数。输入矩阵A、B、C的数据类型。支持输入FP16-输出FP16或FP32，输入FP32-输出FP32。

* bm_image_data_format_ext output_dtype

  输入参数。输出矩阵Y的数据类型。

**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败

**注意：**

1. 该接口在FP16输入、A矩阵转置的情况下，M仅支持小于等于64的取值。

2. 该接口不支持FP32输入且FP16输出。

**示例代码**


    .. code-block:: c

        int M = 3, N = 4, K = 5;
        float alpha = 0.4, beta = 0.6;
        bool is_A_trans = false;
        bool is_B_trans = false;
        float *A     = new float[M * K];
        float *B     = new float[N * K];
        float *C     = new float[M * N];
        memset(A, 0x11, M * K * sizeof(float));
        memset(B, 0x22, N * K * sizeof(float));
        memset(C, 0x33, M * N * sizeof(float));
        bm_device_mem_t input_dev_buffer[3];
        bm_device_mem_t output_dev_buffer[1];
        bm_malloc_device_byte(handle, &input_dev_buffer[0], M * K * sizeof(float));
        bm_malloc_device_byte(handle, &input_dev_buffer[1], N * K * sizeof(float));
        bm_malloc_device_byte(handle, &input_dev_buffer[2], M * N * sizeof(float));
        bm_memcpy_s2d(handle, input_dev_buffer[0], (void *)A);
        bm_memcpy_s2d(handle, input_dev_buffer[1], (void *)B);
        bm_memcpy_s2d(handle, input_dev_buffer[2], (void *)C);
        bm_malloc_device_byte(handle, &output_dev_buffer[0], M * N * sizeof(float));
        bm_image_data_format_ext in_dtype = DATA_TYPE_EXT_FLOAT32;
        bm_image_data_format_ext out_dtype = DATA_TYPE_EXT_FLOAT32;
        bmcv_gemm_ext(handle,
                is_A_trans,
                is_B_trans,
                M,
                N,
                K,
                alpha,
                input_dev_buffer[0],
                input_dev_buffer[1],
                beta,
                input_dev_buffer[2],
                output_dev_buffer[0],
                in_dtype,
                out_dtype);
        delete A;
        delete B;
        delete C;
        delete Y;
        for (int i = 0; i < 3; i++)
        {
          bm_free_device(handle, input_dev_buffer[i]);
        }
        bm_free_device(handle, output_dev_buffer[0]);
