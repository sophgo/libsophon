bmcv_image_add_weighted
=======================

Fusion of two images of the same size by weighted, as follows:

.. math::
    \begin{array}{c}
    output = alpha * input1 + beta * input2 + gamma
    \end{array}


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_add_weighted(
                    bm_handle_t handle,
                    bm_image input1,
                    float alpha,
                    bm_image input2,
                    float beta,
                    float gamma,
                    bm_image output);


**Description of parameters:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image input1

  Input parameter. The bm_image of the first input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* float alpha

  The weight of the first image.

* bm_image input2

  Input parameter. The bm_image of the second input image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* float beta

  The weight of the second image.

* float gamma

  Offset after fusion.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to create new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be allocated automatically within the API.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following images_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_BGR_PACKED      |
+-----+------------------------+
| 2   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 3   | FORMAT_RGB_PACKED      |
+-----+------------------------+
| 4   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 5   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 6   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 7   | FORMAT_GRAY            |
+-----+------------------------+
| 8   | FORMAT_YUV420P         |
+-----+------------------------+
| 9   | FORMAT_YUV422P         |
+-----+------------------------+
| 10  | FORMAT_YUV444P         |
+-----+------------------------+
| 11  | FORMAT_NV12            |
+-----+------------------------+
| 12  | FORMAT_NV21            |
+-----+------------------------+
| 13  | FORMAT_NV16            |
+-----+------------------------+
| 14  | FORMAT_NV61            |
+-----+------------------------+
| 15  | FORMAT_NV24            |
+-----+------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.

2. The data_type and image_format of input and output must be the same.


**Code example:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <math.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");
            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            int channel = 3;
            int width = 1920;
            int height = 1080;
            int dev_id = 0;
            bm_handle_t handle;
            bm_image input1, input2, output;
            unsigned char* input1_data = (unsigned char*)malloc(width * height * channel);
            unsigned char* input2_data = (unsigned char*)malloc(width * height * channel);
            unsigned char* output_tpu = (unsigned char*)malloc(width * height * channel);
            const char *src1_name = "path/to/src1";
            const char *src2_name = "path/to/src2";
            const char *dst_name = "path/to/dst";
            unsigned char* in1_ptr[3] = {input1_data, input1_data + height * width, input1_data + 2 * height * width};
            unsigned char* in2_ptr[3] = {input2_data, input2_data + height * width, input2_data + 2 * height * width};
            unsigned char* out_ptr[3] = {output_tpu, output_tpu + height * width, output_tpu + 2 * height * width};
            int img_size = width * height * channel;

            readBin(src1_name, input1_data, img_size);
            readBin(src2_name, input2_data, img_size);

            bm_dev_request(&handle, dev_id);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &input1);
            bm_image_alloc_dev_mem(input1);
            bm_image_copy_host_to_device(input1, (void **)&in1_ptr);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &input2);
            bm_image_alloc_dev_mem(input2);
            bm_image_copy_host_to_device(input2, (void **)&in2_ptr);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_add_weighted(handle, input1, 0.5, input2, 0.5, 0, output);
            bm_image_copy_device_to_host(output, (void **)&out_ptr);
            writeBin(dst_name, output_tpu, img_size);

            bm_image_destroy(input1);
            bm_image_destroy(input2);
            bm_image_destroy(output);
            bm_dev_free(handle);
            free(input1_data);
            free(input2_data);
            free(output_tpu);
            return 0;
        }