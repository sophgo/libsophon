bmcv_debug_savedata
====================

该接口用于将bm_image对象输出至内部定义的二进制文件方便debug，二进制文件格式以及解析方式在示例代码中给出。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_debug_savedata(
                    bm_image input,
                    const char *name);


**参数说明:**

* bm_image input

  输入参数。输入 bm_image。

* const char\* name

  输入参数。保存的二进制文件路径以及文件名称。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项:**

1. 在调用 bmcv_debug_savedata()之前必须确保输入的 image 已被正确创建并保证is_attached，否则该函数将返回失败。


**代码示例以及二进制文件解析方法:**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "test_misc.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include <string.h>
        #include <memory>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        int main()
        {
            bm_handle_t handle;
            bm_image input;
            int height = 1080;
            int width = 1920;
            unsigned char* input_data = (unsigned char*)malloc(height * width * 3 / 2);
            const char *input_path = "path/to/input";

            bm_dev_request(&handle, 0);
            bm_image_create(handle, height, width, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem_heap_mask(input, 6);
            readBin(input_path, input_data, height * width * 3 / 2);
            bm_image_copy_host_to_device(input, (void**)&input_data);
            bmcv_debug_savedata(input, "input.bin");

            bm_image_destroy(input);
            bm_dev_free(handle);
            free(input_data);
            return 0;
        }