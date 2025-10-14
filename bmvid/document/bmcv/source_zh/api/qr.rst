bmcv_qr_cpu
==================

 矩阵分解


**处理器型号支持：**

该接口支持BM1684X。


**接口形式：**

    .. code-block:: c

        bmcv_qr_cpu(
            float* arm_select_egin_vector,
            int* num_spks_output,
            float* input_A,
            float* arm_sorted_eign_value,
            int n,
            int user_num_spks,
            int max_num_spks,
            int min_num_spks);


**参数说明：**

* float* arm_select_egin_vector

  输出参数。存放按指定方式处理后的特征根。逻辑形状为[n,num_spks_global_addr[0]]。物理形状为[n,n]。

* int* num_spks_global_addr

  输出参数。存放在后处理计算的动态num_spks, 是后续KNN Op的n_feat。

* float* input_A

  输入参数。存放输入矩阵。形状为[n,n]。

* float* arm_sorted_eign_value

  输出参数。存放特征根。 形状为[n,n]。

* int  n

  输入参数。存放形参。

* int   user_num_spks

  输入参数。取num_spks前有效特征根。 不使用时为 -1。

* int   max_num_spks

  输入参数。最大取max_num_spks前有效特征根。

* int   min_num_spks

  输入参数。最小取max_num_spks前有效特征根。


**返回值说明：**

* 无


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

1、因具体使用的BLAS函数不同, 本函数结果可能不同于其他实现,

   但仍符合特征根定义 A@X_i = X@Diag_i, 其中特征根X_i, 特征值Diag_i, 输入矩阵A。