bmcv_matmul
============

This interface used to implment the multiplication calculation of 8-bit data type matrix, as shown in the following formula:

  .. math:: C = (A\times B) >> rshift\_bit
    :label: eq:pfx

or

  .. math:: C = alpha \times (A\times B) + beta
    :label: eq:pfy


Among them,

* A is the input left matrix, and its data type can be unsigned char or 8-bit data of signed char, with the size of (M, K);

* B is the input right matrix, and its data type can be unsigned char or 8-bit data of signed char, with the size of (K, N);

* C is the output result matrix. Its data type length can be int8, int16 or float32, which is determined by user configuration.

  When C is int8 or int16, execute the function of the above formula :eq:`eq:pfx` and its symbol depends on A and B. when A and B are both unsigned, C is an unsigned number, otherwise it is signed;

  When C is float32, execute the function of the above formula :eq:`eq:pfy` .

* rshift_bit is the right shift number of the matrix product, which is valid only when C is int8 or int16. Since the matrix product may exceed the range of 8-bit or 16-bit, users can configure a certain right shift number to prevent overflow by discarding some accuracy.

* alpha and beta are constant coefficients of float32, which is valid only when C is float32.


The format of the interface is as follows:

    .. code-block:: c

         bm_status_t bmcv_matmul(bm_handle_t      handle,
                                 int              M,
                                 int              N,
                                 int              K,
                                 bm_device_mem_t  A,
                                 bm_device_mem_t  B,
                                 bm_device_mem_t  C,
                                 int              A_sign,
                                 int              B_sign,
                                 int              rshift_bit,
                                 int              result_type,
                                 bool             is_B_trans,
                                 float            alpha = 1,
                                 float            beta = 0);



**输入参数说明：**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* int M

  Input parameter. The number of rows of matrix A and matrix C

* int N

  Input parameter. The number of columns of matrix B and matrix C

* int K

  Input parameter. The number of columns of matrix A and the number of rows of matrix B

* bm_device_mem_t A

  Enter parameters. Save its device address or host address according to the data storage location of left matrix A. If the data is stored in the host space, it will automatically complete the handling of S2D.

* bm_device_mem_t B

  Input parameter. Save its device address or host address according to the data storage location of right matrix B. If the data is stored in the host space, it will automatically complete the handling of s2d.

* bm_device_mem_t C

  Output parameter. Save its device address or host address according to the data storage location of matrix C. If it is the host address, when the beta is not 0, the transportation of s2d will be completed automatically before calculation, and then the transportation of d2s will be completed automatically after calculation.

* int A_sign

  Input parameter. The sign of left matrix A, 1 means signed and 0 means unsigned.

* int B_sign

  Input parameter. The sign of right matrix B, 1 means signed and 0 means unsigned.

* int rshift_bit

  Input parameter. The right shift number of matrix product is non-negative. Valid only when result_type is equal to 0 or 1.

* int result_type

  Input parameter. The data type of the output result matrix. 0 means int8, 1 means int16, and 2 means float32.

* bool is_B_trans

  Input parameter. Whether the input right matrix B needs to be transposed before calculation.

* float alpha

  Constant coefficient, which is multiplied by input matrices A and B and then multiplied by this coefficient. Only valid when result_type is equal to 2. The default value is 1.

* float beta

  Constant coefficient, add the offset before the output result matrix C. Only valid when result_type is equal to 2. The default value is 0.


**Return value description:**

* BM_SUCCESS: success

* Other: failed



**Sample code**


    .. code-block:: c

        int M = 3, N = 4, K = 5;
        int result_type = 1;
        bool is_B_trans = false;
        int rshift_bit = 0;
        char *A     = new char[M * K];
        char *B     = new char[N * K];
        short *C     = new short[M * N];
        memset(A, 0x11, M * K * sizeof(char));
        memset(B, 0x22, N * K * sizeof(char));

        bmcv_matmul(handle,
                    M,
                    N,
                    K,
                    bm_mem_from_system((void *)A),
                    bm_mem_from_system((void *)B),
                    bm_mem_from_system((void *)C),
                    1,
                    1,
                    rshift_bit,
                    result_type,
                    is_B_trans);

        delete A;
        delete B;
        delete C;

