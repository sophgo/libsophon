bmcv_count_nonzero
====================

统计输入图像数据的非零个数，并输出相应idx值。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_count_nonzero(
                bm_handle_t handle,
                bm_device_mem_t input_addr,
                bm_device_mem_t nonzero_idx_addr,
                int width,
                int height,
                int channel,
                int* nonzero_count);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_device_mem_t input_addr

  输入参数。存放输入图像数据的设备内存地址。

* bm_device_mem_t nonzero_idx_addr

  输出参数。存放非零索引数据的设备内存地址。

* int width

  输入参数。表示输入图像数据的宽。

* int height

  输入参数。表示输入图像数据的高。

* int channel

  输入参数。表示输入图像数据的通道数。

* int* nonzero_count

  输出参数。存放非零数据个数的指针。

**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1. 在调用该接口之前必须确保所用设备内存已经申请，其中输出索引数据地址需要预先申请width*height*channel*sizeof(int)空间大小。

2. 输入图像数据的数据类型仅支持unsigned char。

3. 输入图像数据的宽高范围为1x1～8192x8192,通道数支持1或3。

4. 输出索引数据的数据类型为int，其中无效索引值为-1。


**代码示例：**

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

