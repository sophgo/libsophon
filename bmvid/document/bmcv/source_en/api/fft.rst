bmcv_fft
============

FFT operation. The complete operation includes creation, execution and destruction.

Create
______

It supports one-dimensional or two-dimensional FFT calculation. The difference is that in the creation process, the later execution and destruction use the same interface.

For one-dimensional FFT, multi-batch operation is supported. The interface form is as follows:

    .. code-block:: c

        bm_status_t bmcv_fft_1d_create_plan(
                bm_handle_t handle,
                int batch,
                int len,
                bool forward,
                void *&plan);

**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle

* int batch

  Input parameter. Number of batches.

* int int

  Input parameters. The length of each batch.

* bool forward

  Input parameter. Whether it is a forward transformation. False indicates an inverse transformation.

* void \*\&plan

  Output parameter. The handle required for execution.

**Return value description:**

* BM_SUCCESS: success

* Other: failed


For two-dimensional M*N FFT, the inerface form is as follows:

    .. code-block:: c

        bm_status_t bmcv_fft_2d_create_plan(
                bm_handle_t handle,
                int M,
                int N,
                bool forward,
                void *&plan);

**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle

* int M

  Input parameter. The size of the first dimension.

* int N

  Input parameter. The size of the second dimension.

* bool forward

  Input parameter. Whether it is a forward transformation. False indicates an inverse transformation.

* void \*\&plan

  Output parameter. The handle required for execution.

**Return value Description:**

* BM_SUCCESS: success

* Other: failed


Execute
_______

Use the plan created above to start the real execution phase. It supports two interfaces: complex input and real input. Their formats are as follows:

    .. code-block:: c

        bm_status_t bmcv_fft_execute(
                bm_handle_t handle,
                bm_device_mem_t inputReal,
                bm_device_mem_t inputImag,
                bm_device_mem_t outputReal,
                bm_device_mem_t outputImag,
                const void *plan);

        bm_status_t bmcv_fft_execute_real_input(
                bm_handle_t handle,
                bm_device_mem_t inputReal,
                bm_device_mem_t outputReal,
                bm_device_mem_t outputImag,
                const void *plan);


**Input parameter description:**

* bm_handle_t handle

  Input parameters. The handle of bm_handle

* bm_device_mem_t inputReal

  Input parameter. The device memory space storing the real number of the input data is batch*len*sizeof (float) for one-dimensional FFT and M*N*sizeof (float) for two-dimensional FFT.

* bm_device_mem_t inputImag

  Input parameter. The device memory space storing the imaginary number of the input data. For one-dimensional FFT, its size is batch*len*sizeof (float) and M*N*sizeof (float) for two-dimensional FFT.

* bm_device_mem_t outputReal

  Output parameter. The device memory space storing the real number of the output result is batch*len*sizeof (float) for one-dimensional FFT and M*N*sizeof (float) for two-dimensional FFT.

* bm_device_mem_t outputImag

  Output parameter. The device memory space storing the imaginary number of the output result is batch*len*sizeof (float) for one-dimensional FFT and M*N*sizeof (float) for two-dimensional FFT.

* const void \*plan

  Input parameter. The handle obtained during the creation phase.

**Return value description:**

* BM_SUCCESS: success

* Other: failed


Destruct
________

When the execution is completed, the created handle needs to be destructed.

    .. code-block:: c

        void bmcv_fft_destroy_plan(bm_handle_t handle, void *plan);




Sample code:
____________

    .. code-block:: c

        bool realInput = false;
        float *XRHost = new float[M * N];
        float *XIHost = new float[M * N];
        float *YRHost = new float[M * N];
        float *YIHost = new float[M * N];
        for (int i = 0; i < M * N; ++i) {
            XRHost[i] = rand() % 5 - 2;
            XIHost[i] = realInput ? 0 : rand() % 5 - 2;
        }
        bm_handle_t handle = nullptr;
        bm_dev_request(&handle, 0);
        bm_device_mem_t XRDev, XIDev, YRDev, YIDev;
        bm_malloc_device_byte(handle, &XRDev, M * N * 4);
        bm_malloc_device_byte(handle, &XIDev, M * N * 4);
        bm_malloc_device_byte(handle, &YRDev, M * N * 4);
        bm_malloc_device_byte(handle, &YIDev, M * N * 4);
        bm_memcpy_s2d(handle, XRDev, XRHost);
        bm_memcpy_s2d(handle, XIDev, XIHost);
        void *plan = nullptr;
        bmcv_fft_2d_create_plan(handle, M, N, forward, plan);
        if (realInput)
            bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan);
        else
            bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan);
        bmcv_fft_destroy_plan(handle, plan);
        bm_memcpy_d2s(handle, YRHost, YRDev);
        bm_memcpy_d2s(handle, YIHost, YIDev);
        bm_free_device(handle, XRDev);
        bm_free_device(handle, XIDev);
        bm_free_device(handle, YRDev);
        bm_free_device(handle, YIDev);
        bm_dev_free(handle);

