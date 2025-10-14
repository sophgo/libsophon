bmcv_debug_savedata
====================

This interface is used to input bm_image object to the internally defined binary file for debugging. The binary file format and parsing method are given in the example code.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_debug_savedata(
                    bm_image input,
                    const char *name);


**Parameter Description:**

* bm_image input

  Input parameter. Input bm_image.

* const char\* name

  Input parameter. The saved binary file path and name.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. Before calling bmcv_debug_savedata(), users must ensure that the input image has been created correctly and guaranteed is_attached, otherwise the function will return a failure.


**Code example and binary file parsing method::**

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