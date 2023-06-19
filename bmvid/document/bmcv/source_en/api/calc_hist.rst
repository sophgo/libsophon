bmcv_calc_hist
==================

Histogram
_________


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_calc_hist(
                bm_handle_t handle,
                bm_device_mem_t input,
                bm_device_mem_t output,
                int C,
                int H,
                int W,
                const int *channels,
                int dims,
                const int *histSizes,
                const float *ranges,
                int inputDtype);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input

  Input parameter. The device memory space stores the input data. The type can be float32 or uint8, which is determined by the parameter inputDtype. Its size is C*H*W*sizeof (Dtype).

* bm_device_mem_t output

  Output parameter. The device memory space stores the output results. The type is float and its size is histSizes[0]*histSizes[1]*...*histSizes[n]*sizeof (float).

* int C

  Input parameter. Number of channels for input data.

* int H

  Input parameter. The height of each channel of the input data.

* int W

  Input parameter. The width of each channel of the input data.

* const int \*channels

  Input parameter. The channel list of histogram needs to be calculated. Its length is dims, and the value of each element must be less than C.

* int dims

  Input parameter. The output histogram dimension, which shall not be greater than 3.

* const int \*histSizes

  Input parameter. Corresponding to the number of copies of each channel statistical histogram. Its length is dims.

* const float \*ranges

  Input parameter. The range of each channel participating in statistics, with a length of 2*dims.

* int inputDtype

  Input parameter. Type of input data: 0 means float, 1 means uint8.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Code example:**

    .. code-block:: c

        int H = 1024;
        int W = 1024;
        int C = 3;
        int dim = 3;
        int channels[3] = {0, 1, 2};
        int histSizes[] = {15000, 32, 32};
        float ranges[] = {0, 1000000, 0, 256, 0, 256};
        int totalHists = 1;
        for (int i = 0; i < dim; ++i)
            totalHists *= histSizes[i];
        bm_handle_t handle = nullptr;
        bm_status_t ret = bm_dev_request(&handle, 0);
        float *inputHost = new float[C * H * W];
        float *outputHost = new float[totalHists];
        for (int i = 0; i < C; ++i)
            for (int j = 0; j < H * W; ++j)
                inputHost[i * H * W + j] = static_cast<float>(rand() % 1000000);
        if (ret != BM_SUCCESS) {
            printf("bm_dev_request failed. ret = %d\n", ret);
            exit(-1);
        }
        bm_device_mem_t input, output;
        ret = bm_malloc_device_byte(handle, &input, C * H * W * 4);
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed. ret = %d\n", ret);
            exit(-1);
        }
        ret = bm_memcpy_s2d(handle, input, inputHost);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed. ret = %d\n", ret);
            exit(-1);
        }
        ret = bm_malloc_device_byte(handle, &output, totalHists * 4);
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed. ret = %d\n", ret);
            exit(-1);
        }
        ret = bmcv_calc_hist(handle,
                             input,
                             output,
                             C,
                             H,
                             W,
                             channels,
                             dim,
                             histSizes,
                             ranges,
                             0);
        if (ret != BM_SUCCESS) {
            printf("bmcv_calc_hist failed. ret = %d\n", ret);
            exit(-1);
        }
        ret = bm_memcpy_d2s(handle, outputHost, output);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s failed. ret = %d\n", ret);
            exit(-1);
        }
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        bm_dev_free(handle);
        delete [] inputHost;
        delete [] outputHost;


Weighted Histogram
__________________


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_calc_hist_with_weight(
                bm_handle_t handle,
                bm_device_mem_t input,
                bm_device_mem_t output,
                const float *weight,
                int C,
                int H,
                int W,
                const int *channels,
                int dims,
                const int *histSizes,
                const float *ranges,
                int inputDtype);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input

  Input parameter. The device memory space stores the input data, and its size is C*H*W* sizeof (Dtype).

* bm_device_mem_t output

  Output parameter. The device memory space stores the output results. The type is float, and its size is histSizes[0]* histSizes[1]*...*histSizes[n]*sizeof (float).

* const float \*weight

  Input parameter. The weight of each element in the channel during histogram statistics. Its size is H*W*sizeof (float). If all values are 1, it has the same function as the ordinary histogram.

* int C

  Input parameter. Number of channels for input data.

* int H

  Input parameter. The height of each channel of the input data

* int W

  Input parameter. The width of each channel of the input data.

* const int \*channels

  Input parameter. The channel list of histogram needs to be calculated. Its length is dims, and the value of each element must be less than C.

* int dims

  Input parameter. The output histogram dimension shall not be greater than 3.

* const int \*histSizes

  Input parameter. Corresponding to the number of copies of each channel statistical histogram. Its length is dims.

* const float \*ranges

  Input parameter. The range of each channel participating in statistics, with a length of 2*dims.

* int inputDtype

  Input parameter. Type of input data: 0 means float, 1 means uint8.


**Return value description:**

* BM_SUCCESS: success

* Other: failed

