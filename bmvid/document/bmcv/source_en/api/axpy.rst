bmcv_axpy
==========

This interface implements F = A * X + Y, where A is a constant of size n * c , and F , X , Y are all matrices of size n * c * h * w.


**Interface form:**

    .. code-block:: c

         bm_status_t bmcv_image_axpy(
                 bm_handle_t handle,
                 bm_device_mem_t tensor_A,
                 bm_device_mem_t tensor_X,
                 bm_device_mem_t tensor_Y,
                 bm_device_mem_t tensor_F,
                 int input_n,
                 int input_c,
                 int input_h,
                 int input_w);


**Description of parameters:**

* bm_handle_t handle

  Input parameters. bm_handle handle.

* bm_device_mem_t tensor_A

  Input parameters. The device memory address where the scalar A is stored.

* bm_device_mem_t tensor_X

  Input parameters. The device memory address where matrix X is stored.

* bm_device_mem_t tensor_Y

  Input parameters. The device memory address where matrix Y is stored.

* bm_device_mem_t tensor_F

  Output parameters. The device memory address where the result matrix F is stored.

* int input_n

  Input parameters. The size of n dimension.

* int input_c

  Input parameters. The size of c dimension.

* int input_h

  Input parameters. The size of h dimension.

* int input_w

  Input parameters. The size of w dimension.

**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Code example:**

    .. code-block:: c

        #define N (10)
        #define C 256 //(64 * 2 + (64 >> 1))
        #define H 8
        #define W 8
        #define TENSOR_SIZE (N * C * H * W)

        bm_handle_t handle;
        bm_status_t ret = BM_SUCCESS;

        bm_dev_request(&handle, 0);
        int trials = 0;
        if (argc == 1) {
            trials = 5;
        }else if(argc == 2){
            trials  = atoi(argv[1]);
        }else{
            std::cout << "command input error, please follow this "
                     "order:test_cv_axpy loop_num "
                  << std::endl;
            return -1;
        }

        float* tensor_X = new float[TENSOR_SIZE];
        float* tensor_A = new float[N*C];
        float* tensor_Y = new float[TENSOR_SIZE];
        float* tensor_F = new float[TENSOR_SIZE];

        for (int idx_trial = 0; idx_trial < trials; idx_trial++) {

            for (int idx = 0; idx < TENSOR_SIZE; idx++) {
                 tensor_X[idx] = (float)idx - 5.0f;
                 tensor_Y[idx] = (float)idx/3.0f - 8.2f;  //y
            }

            for (int idx = 0; idx < N*C; idx++) {
            tensor_A[idx] = (float)idx * 1.5f + 1.0f;
            }

            struct timeval t1, t2;
            gettimeofday_(&t1);
            ret = bmcv_image_axpy(handle,
                                  bm_mem_from_system((void *)tensor_A),
                                  bm_mem_from_system((void *)tensor_X),
                                  bm_mem_from_system((void *)tensor_Y),
                                  bm_mem_from_system((void *)tensor_F),
                                  N, C, H, W);
            gettimeofday_(&t2);
            std::cout << "The "<< idx_trial <<" loop "<< " axpy using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec)  << "us" << std::endl;
        }
        delete []tensor_A;
        delete []tensor_X;
        delete []tensor_Y;
        delete []tensor_F;
        delete []tensor_F_cmp;
        bm_dev_free(handle);