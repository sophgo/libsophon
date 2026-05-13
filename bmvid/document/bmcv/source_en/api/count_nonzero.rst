bmcv_count_nonzero
====================

Count the number of non-zero elements in the input image data and output the corresponding idx values.


**Processor model support**

This interface is only supported by the BM1684X.


**Interface Form**

    .. code-block:: c

        bm_status_t bmcv_count_nonzero(
                bm_handle_t handle,
                bm_device_mem_t input_addr,
                bm_device_mem_t nonzero_idx_addr,
                int width,
                int height,
                int channel,
                int* nonzero_count);


**Parameter description:**

* bm_handle_t handle

  input parameter. device environment handle.

* bm_device_mem_t input_addr

  Input parameter. The device memory address where the input image data is stored.

* bm_device_mem_t nonzero_idx_addr

  Output parameter. Device memory address storing non-zero index data.

* int width

  Input parameter. Represents the width of the input image data.

* int height

  Input parameters. Represents the height of the input image data.

* int channel

  Input parameters. Indicates the number of channels in the input image data.

* int* nonzero_count

  Output parameter. A pointer storing the number of non-zero data items.

**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Notes**

1. Before calling this interface, ensure that memory has been allocated on the device. The output index data address requires pre-allocating a space of width*height*channel*sizeof(int).

2. The data type for input image data only supports unsigned char.

3. The input image data width and height range is 1x1 to 8192x8192, with support for 1 or 3 channels.

4. The data type for output index data is int, where an invalid index value is -1.


**Code Example**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext_c.h"
        #include "stdlib.h"
        #include <sys/time.h>

        #define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))

        static void read_bin(const char *input_path, unsigned char *input_data, int width, int height) {
            FILE *fp_src = fopen(input_path, "rb");
            if (fp_src == NULL)
            {
                printf("Unable to open input file %s\n", input_path);
                return;
            }
            if(fread(input_data, sizeof(unsigned char), width * height, fp_src) != 0)
                printf("read image success\n");
            fclose(fp_src);
        }

        int main() {
            int height = 1 + rand() % 8192;
            int width = 1 + rand() % 8192;
            int channel = 1 + rand() % 2 * 2;
            const char *input_path = "count_nonzero_input.bin";
            bm_handle_t handle;
            bm_status_t ret = bm_dev_request(&handle, 0);
            if (ret != BM_SUCCESS) {
                printf("Create bm handle failed. ret = %d\n", ret);
                return -1;
            }
            int nonzero_count_tpu = 0;
            struct timeval t1, t2;
            bm_device_mem_t input_addr, nonzero_idx_addr;
            unsigned char* input_data = (unsigned char*)malloc(width * height * channel * sizeof(unsigned char));
            int* output_idx_tpu_addr = (int*)malloc(width * height * channel * sizeof(int));
            read_bin(input_path, input_data, width, height);
            bm_malloc_device_byte(handle, &input_addr, width * height * channel * sizeof(unsigned char));
            bm_malloc_device_byte(handle, &nonzero_idx_addr, width * height * channel * sizeof(int));
            bm_memcpy_s2d(handle, input_addr, input_data);
            gettimeofday(&t1, NULL);
            bmcv_count_nonzero(handle, input_addr, nonzero_idx_addr, width, height, channel, &nonzero_count_tpu);
            gettimeofday(&t2, NULL);
            printf("Count nonzero TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
            bm_memcpy_d2s(handle, output_idx_tpu_addr, nonzero_idx_addr);
            bm_free_device(handle, input_addr);
            bm_free_device(handle, nonzero_idx_addr);
            free(input_data);
            free(output_idx_tpu_addr);
            bm_dev_free(handle);
            return ret;
        }

