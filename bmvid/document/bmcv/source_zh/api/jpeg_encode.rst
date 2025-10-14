bmcv_image_jpeg_enc
===================

该接口可以实现对多张 bm_image 的 JPEG 编码过程。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_enc(
                    bm_handle_t handle,
                    int image_num,
                    bm_image* src,
                    void* p_jpeg_data[],
                    size_t* out_size,
                    int quality_factor = 85);


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* int  image_num

  输入参数。输入图片数量，最多支持 4。

* bm_image\* src

  输入参数。输入 bm_image的指针。每个 bm_image 需要外部调用 bmcv_image_create 创建，image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存,或者使用 bmcv_image_attach 来 attach 已有的内存。

* void \*  p_jpeg_data,

  输出参数。编码后图片的数据指针，由于该接口支持对多张图片的编码，因此为指针数组，数组的大小即为 image_num。用户可以选择不为其申请空间（即数组每个元素均为NULL），在 api 内部会根据编码后数据的大小自动分配空间，但当不再使用时需要用户手动释放该空间。当然用户也可以选择自己申请足够的空间。

* size_t \*out_size,

  输出参数。完成编码后各张图片的大小（以 byte 为单位）存放在该指针中。

* int quality_factor = 85

  输入参数。编码后图片的质量因子。取值 0～100 之间，值越大表示图片质量越高，但数据量也就越大，反之值越小图片质量越低，数据量也就越少。该参数为可选参数，默认值为85。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


.. note::

    目前编码支持的图片格式包括以下几种：
     | FORMAT_YUV420P
     | FORMAT_YUV422P
     | FORMAT_YUV444P
     | FORMAT_NV12
     | FORMAT_NV21
     | FORMAT_NV16
     | FORMAT_NV61
     | FORMAT_GRAY

    目前编码支持的数据格式如下：
     | DATA_TYPE_EXT_1N_BYTE


**示例代码**

    .. code-block:: c

        #include <stdio.h>
        #include <stdint.h>
        #include <stdlib.h>
        #include <memory.h>
        #include "bmcv_api_ext.h"
        #include <assert.h>
        #include <math.h>

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
            int format = FORMAT_YUV420P;
            int image_h = 1080;
            int image_w = 1920;
            bm_image src;
            bm_handle_t handle;
            size_t byte_size = image_w * image_h * 3 / 2;
            unsigned char* input_data = (unsigned char*)malloc(byte_size);
            unsigned char* in_ptr[3] = {input_data, input_data + image_h * image_w, input_data + 2 * image_h * image_w};
            void* jpeg_data[4] = {NULL, NULL, NULL, NULL};
            const char *src_name = "path/to/src";

            readBin(src_name, input_data, byte_size);
            bm_dev_request(&handle, 0);
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_alloc_dev_mem(src, BMCV_HEAP1_ID);
            bm_image_copy_host_to_device(src, (void**)in_ptr);
            bmcv_image_jpeg_enc(handle, 1, &src, jpeg_data, &byte_size, 95);

            bm_image_destroy(src);
            free(input_data);
            bm_dev_free(handle);
            return 0;
        }