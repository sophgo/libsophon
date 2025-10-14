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


**Processor model support**

This interface supports BM1684/BM1684X.


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle

* int batch

  Input parameter. Number of batches.

* int len

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

        #include "bmcv_api_ext.h"
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>

        int main()
        {
            bm_handle_t handle;
            int ret = 0;
            int i;
            int L = 100;
            int batch = 100;
            bool forward = true;
            bool realInput = false;
            bm_device_mem_t XRDev, XIDev, YRDev, YIDev;
            void* plan = NULL;

            ret = (int)bm_dev_request(&handle, 0);
            if (ret) {
                printf("Create bm handle failed. ret = %d\n", ret);
                return ret;
            }

            float* XRHost = (float*)malloc(L * batch * sizeof(float));
            float* XIHost = (float*)malloc(L * batch * sizeof(float));
            float* YRHost_tpu = (float*)malloc(L * batch * sizeof(float));
            float* YIHost_tpu = (float*)malloc(L * batch * sizeof(float));

            for (i = 0; i < L * batch; ++i) {
                XRHost[i] = (float)rand() / RAND_MAX;
                XIHost[i] = realInput ? 0 : ((float)rand() / RAND_MAX);
            }

            ret = bm_malloc_device_byte(handle, &XRDev, L * batch * sizeof(float));
            ret = bm_malloc_device_byte(handle, &XIDev, L * batch * sizeof(float));
            ret = bm_malloc_device_byte(handle, &YRDev, L * batch * sizeof(float));
            ret = bm_malloc_device_byte(handle, &YIDev, L * batch * sizeof(float));

            ret = bm_memcpy_s2d(handle, XRDev, XRHost);
            ret = bm_memcpy_s2d(handle, XIDev, XIHost);

            ret = bmcv_fft_2d_create_plan(handle, L, batch, forward, plan);
            if (realInput) {
                bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan);
            } else {
                bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan);
            }

            ret = bm_memcpy_d2s(handle, (void*)YRHost_tpu, YRDev);
            ret = bm_memcpy_d2s(handle, (void*)YIHost_tpu, YIDev);

            if (plan != NULL) {
                bmcv_fft_destroy_plan(handle, plan);
            }
            free(XRHost);
            free(XIHost);
            free(YRHost_tpu);
            free(YIHost_tpu);
            bm_free_device(handle, XRDev);
            bm_free_device(handle, XIDev);
            bm_free_device(handle, YRDev);
            bm_free_device(handle, YIDev);
            bm_dev_free(handle);
            return ret;
        }