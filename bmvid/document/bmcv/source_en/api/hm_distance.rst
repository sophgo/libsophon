bmcv_hm_distance
==================

Calculates the Hamming distance of each element in two vectors.


**Processor model support**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_hamming_distance(
                    bm_handle_t handle,
                    bm_device_mem_t input1,
                    bm_device_mem_t input2,
                    bm_device_mem_t output,
                    int bits_len,
                    int input1_num,
                    int input2_num);


**Description of parameters:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image input1

  Input parameters. Device address information for vector 1 data.

* bm_image input2

  Input parameters. Device address information for vector 2 data.

* bm_image output

  Output parameters. Device address information for output vector data.

* int bits_len

  Input parameters. The length of each element in the vector.

* int input1_num

  Input parameters. The num of vector 1 data.

* int input2_num

  Input parameters. The num of vector 2 data.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Code example:**

    .. code-block:: c

        #include <math.h>
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include "bmcv_api_ext.h"

        int main()
        {
            int bits_len = 8;
            int input1_num = 2;
            int input2_num = 2562;
            int* input1_data = new int[input1_num * bits_len];
            int* input2_data = new int[input2_num * bits_len];
            int* output_tpu  = new int[input1_num * input2_num];
            bm_device_mem_t input1_dev_mem;
            bm_device_mem_t input2_dev_mem;
            bm_device_mem_t output_dev_mem;
            bm_handle_t handle;

            memset(input1_data, 0, input1_num * bits_len * sizeof(int));
            memset(input2_data, 0, input2_num * bits_len * sizeof(int));
            memset(output_tpu,  0,  input1_num * input2_num * sizeof(int));

            // fill data
            for(int i = 0; i < input1_num * bits_len; i++) {
                input1_data[i] = rand() % 10;
            }
            for(int i = 0; i < input2_num * bits_len; i++) {
                input2_data[i] = rand() % 20 + 1;
            }

            bm_dev_request(&handle, 0);
            bm_malloc_device_byte(handle, &input1_dev_mem, input1_num * bits_len * sizeof(int));
            bm_malloc_device_byte(handle, &input2_dev_mem, input2_num * bits_len * sizeof(int));
            bm_malloc_device_byte(handle, &output_dev_mem, input1_num * input2_num * sizeof(int));
            bm_memcpy_s2d(handle, input1_dev_mem, input1_data);
            bm_memcpy_s2d(handle, input2_dev_mem, input2_data);
            bmcv_hamming_distance(handle, input1_dev_mem, input2_dev_mem, output_dev_mem,
                                bits_len, input1_num, input2_num);
            bm_memcpy_d2s(handle, output_tpu, output_dev_mem);

            delete[] input1_data;
            delete[] input2_data;
            delete[] output_tpu;
            bm_free_device(handle, input1_dev_mem);
            bm_free_device(handle, input2_dev_mem);
            bm_free_device(handle, output_dev_mem);
            bm_dev_free(handle);
            return 0;
        }