bmcv_gemm
============

该接口可以实现 float32 类型矩阵的通用乘法计算，如下公式：

  .. math::

      C = \alpha\times A\times B + \beta\times C

其中，A、B、C均为矩阵，:math:`\alpha` 和 :math:`\beta` 均为常系数

接口的格式如下：

    .. code-block:: c

         bm_status_t bmcv_gemm(bm_handle_t     handle,
                               bool            is_A_trans,
                               bool            is_B_trans,
                               int             M,
                               int             N,
                               int             K,
                               float           alpha,
                               bm_device_mem_t A,
                               int             lda,
                               bm_device_mem_t B,
                               int             ldb,
                               float           beta,
                               bm_device_mem_t C,
                               int             ldc);


**输入参数说明：**

* bm_handle_t handle

输入参数。bm_handle 句柄

* bool is_A_trans

输入参数。设定矩阵 A 是否转置

* bool is_B_trans

输入参数。设定矩阵 B 是否转置

* int M

输入参数。矩阵 A 和矩阵 C 的行数

* int N

输入参数。矩阵 B 和矩阵 C 的列数

* int K

输入参数。矩阵 A 的列数和矩阵 B 的行数

* float alpha

输入参数。数乘系数

* bm_device_mem_t A

输入参数。根据数据存放位置保存左矩阵 A 数据的 device 地址或者 host 地址。如果数据存放于 host 空间则内部会自动完成 s2d 的搬运

* int lda

输入参数。矩阵 A 的 leading dimension, 即第一维度的大小，在行与行之间没有stride的情况下即为 A 的列数（不做转置）或行数（做转置） 

* bm_device_mem_t B

输入参数。根据数据存放位置保存右矩阵 B 数据的 device 地址或者 host 地址。如果数据存放于 host 空间则内部会自动完成 s2d 的搬运。

* int ldb

输入参数。矩阵 C 的 leading dimension, 即第一维度的大小，在行与行之间没有stride的情况下即为 B 的列数（不做转置）或行数（做转置。

* float beta

输入参数。数乘系数。

* bm_device_mem_t C

输出参数。根据数据存放位置保存矩阵 C 数据的 device 地址或者 host 地址。如果是 host 地址，则当beta不为0时，计算前内部会自动完成 s2d 的搬运，计算后再自动完成 d2s 的搬运。

* int ldc

输入参数。矩阵 C 的 leading dimension, 即第一维度的大小，在行与行之间没有stride的情况下即为 C 的列数。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败



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
    
        bmcv_gemm(handle,
                  is_A_trans,
                  is_B_trans,
                  M,
                  N,
                  K,
                  alpha,
                  bm_mem_from_system((void *)A),   
                  is_A_trans ? M : K,              
                  bm_mem_from_system((void *)B),   
                  is_B_trans ? K : N,              
                  beta,
                  bm_mem_from_system((void *)C),   
                  N);
        delete A;
        delete B;
        delete C;

