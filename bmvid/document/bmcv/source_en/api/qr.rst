bmcv_qr_cpu
==================

  matrix decomposition


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

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


**Parameter description:**

* float* arm_select_egin_vector

  Output parameter. Save eignvectors via customized postprocess, , whose logic shape is [n,num_spks_global_addr[0]] and physical shape is [n,n].

* int* num_spks_global_addr

  Output parameter. Save dynamic num_spks in postprocess, which is exactly the n_feat for later KNN block.

* float* input_A

  Input parameter.  Save input matrix, whose shape is [n,n].

* float* arm_sorted_eign_value

  Output parameter. Save origin eignvectors, whose shape is [n,n].

* int  n

  Input parameter. Save Input shapes.

* int   user_num_spks

  Input parameter. Gather first num_spks-th eign_vectors, -1 when not used.

* int   max_num_spks

  Input parameter. Gather first max_num_spks-th eign_vectors at most.

* int   min_num_spks

  Input parameter. Gather first min_num_spks-th eign_vectors at least.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Data type support:**

Input data currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

Output data currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+


**Notes:**

1. The results might be different from other implementations,  as other implementations might use different BLAS functions.

   However, the output eginvectors X_i and eignvalues Diag_i still satisfied with A@X_i = X@Diag_i.