bmcv_gemm_ext
=============

This interface is used to implement the general multiplication calculation of float32 or float16 type matrix, as shown in the following formula:

  .. math::

      Y = \alpha\times A\times B + \beta\times C

  Among them, A, B, C and Y are matrices,and :math:`\alpha` and :math:`\beta` are constant coefficients.

The format of the interface is as follows:

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

**Processor model support**

This interface only supports BM1684X.


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bool is_A_trans

  Input parameter. Set whether matrix A is transposed

* bool is_B_trans

  Input parameter. Set whether matrix B is transposed

* int M

  Input parameter. The number of rows of matrix A, matrix C and matrix Y

* int N

  Input parameter. The number of columns of matrix B, matrix C and matrix Y

* int K

  Input parameter. The number of columns of matrix A and the number of rows of matrix B

* float alpha

  Input parameter. Number multiplication coefficient

* bm_device_mem_t A

  Input parameter. The device address of the left matrix A data is stored according to the data storage location, and the data s2d handling needs to be completed before use.

* bm_device_mem_t B

  Input parameter. The device address of the right matrix B data is stored according to the data storage location, and the data s2d handling needs to be completed before use.

* float beta

  Input parameter. Number multiplication factor.

* bm_device_mem_t C

  Input parameters. The device address of the matrix C data is stored according to the data storage location, and the data s2d handling needs to be done before use.

* bm_device_mem_t Y

  Output parameters. The device address of the matrix Y data, which holds the output result.

* bm_image_data_format_ext input_dtype

  Input parameters. Data type of input matrix A, B, C. Support input FP16-output FP16 or FP32, input FP32-output FP32.

* bm_image_data_format_ext output_dtype

  Input parameters. The data type of the output matrix Y.

**Return value description:**

* BM_SUCCESS: success

* Other: failed

**Note**

1. In the case of FP16 input and A matrix transpose, M only supports values less than or equal to 64.

2. This interface does not support FP32 input and FP16 output.

**Sample code**


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
