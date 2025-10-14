bmcv_min_max
============

For a group of data stored in continuous space in device memory, the interface can obtain the maximum and minimum values in the data group.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_min_max(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    float *minVal,
                    float *maxVal,
                    int len);


**Processor model support**

This interface supports BM1684/BM1684X.


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_device_mem_t input

  Input parameter. Input the device address of the data.

* float \*minVal

  Input parameter. The minimum value obtained after operation. If it is NULL, the minimum value will not be calculated.

* float \*maxVal

  Output parameter. The maximum value obtained after operation. If it is NULL, the maximum value will not be calculated.

* int len

  Input parameter. The length of the input data.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Sample code**

    .. code-block:: c

        #include "bmcv_api_ext_c.h"
        #include <stdio.h>
        #include <stdlib.h>

        int main()
        {
            bm_handle_t handle;
            int len = 1000;
            float max = 0;
            float min = 0;
            bm_device_mem_t input_mem;
            float *input = new float[len];
            for (int i = 0; i < 1000; i++) {
                input[i] = (float)(rand() % 1000) / 10.0;
            }

            bm_dev_request(&handle, 0);
            bm_malloc_device_byte(handle, &input_mem, len * sizeof(float));
            bm_memcpy_s2d(handle, input_mem, input);
            bmcv_min_max(handle, input_mem, &min, &max, len);

            bm_free_device(handle, input_mem);
            bm_dev_free(handle);
            delete[] input;
            return 0;
        }