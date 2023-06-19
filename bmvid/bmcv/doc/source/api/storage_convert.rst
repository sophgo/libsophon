bmcv_image_storage_convert
==========================

该接口将源图像格式的对应的数据转换为目的图像的格式数据，并填充在目的图像关联的 device memory 中。


**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_storage_convert(
                bm_handle_t handle,
                int image_num,
                bm_image* input_image,
                bm_image* output_image
        );


**传入参数说明:**

* bm_handle_t handle

输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* int image_num

输入参数。输入/输出 image 数量。

* bm_image* input

输入参数。输入 bm_image 对象指针。

* bm_image* output

输出参数。输出 bm_image 对象指针。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项**

1. 该 API 支持以下所有格式的两两相互转换：

* (FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_NV21, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_NV16, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_NV61, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_YUV420P, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_YUV444P, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_RGB_PLANAR, DATA_TYPE_EXT_4N_BYTE)

* (FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32)

* (FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_BGR_PLANAR, DATA_TYPE_EXT_4N_BYTE)

* (FORMAT_BGR_PLANAR, DATA_TYPE_EXT_FLOAT32)

* (FORMAT_RGB_PACKED, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_RGB_PACKED, DATA_TYPE_EXT_4N_BYTE)

* (FORMAT_RGB_PACKED, DATA_TYPE_EXT_FLOAT32)

* (FORMAT_BGR_PACKED, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_BGR_PACKED, DATA_TYPE_EXT_4N_BYTE)

* (FORMAT_BGR_PACKED, DATA_TYPE_EXT_FLOAT32)

* (FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_4N_BYTE)

* (FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_FLOAT32)

* (FORMAT_BGRP_SEPARATE, DATA_TYPE_EXT_1N_BYTE)

* (FORMAT_BGRP_SEPARATE, DATA_TYPE_EXT_4N_BYTE)

* (FORMAT_BGRP_SEPARATE, DATA_TYPE_EXT_FLOAT32)。

如果输入输出 image 对象不在以上格式中，则返回失败。

2. 输入输出所有 bm_image 结构必须提前创建，否则返回失败。

3. 所有输入 bm_image 对象的 image_format，data_type，width，height 必须相等，所有输出 bm_image 对象的 image_format，data_type，width，height 必须相等，所有输入输出 bm_image 对象的 width，height 必须相等，否则返回失败。

4. image_num 表示输入图像个数，如果输入图像数据格式为 DATA_TYPE_EXT_4N_BYTE,则输入 bm_image 对象为 1 个，在 4N 中有 image_num 个有效图片。如果输入图像数据格式不是 DATA_TYPE_EXT_4N_BYTE，则输入 image_num 个 bm_image 对象。如果输出 bm_image 数据格式为 DATA_TYPE_EXT_4N_BYTE，则输出 1 个 bm_image 4N 对象，对象中有 bm_image 个有效图片。反之如果输出图像数据格式不是 DATA_TYPE_EXT_4N_BYTE，则输出 image_num 个对象。

5. image_num 必须大于等于 1，小于等于 4，否则返回失败。

6. 所有输入对象必须 attach device memory，否则返回失败。

7. 如果输出对象未 attach device memory，则会内部调用 bm_image_alloc_dev_mem 申请内部管理的 device memory,并将转化后的数据填充到 device memory 中。

8. 如果输入图像和输出图像格式相同，则直接返回成功，且不会将原数据拷贝到输出图像中。

9. 暂不支持 image_w > 8192 时的图像格式转换，如果 image_w > 8192 则返回失败。


**代码示例:**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "bmlib_utils.h"
        #include "common.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>

        int main(int argc, char *argv[]) {
            bm_handle_t handle;
            bm_dev_request(&handle, 0);

            int image_n = 1;
            int image_h = 1080;
            int image_w = 1920;
            bm_image src, dst;
            bm_image_create(handle, image_h, image_w, FORMAT_NV12, 
                    DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_create(handle, image_h, image_w, FORMAT_BGR_PLANAR, 
                    DATA_TYPE_EXT_1N_BYTE, &dst);
            std::shared_ptr<u8*> y_ptr = std::make_shared<u8*>(
                    new u8[image_h * image_w]);
            std::shared_ptr<u8*> uv_ptr = std::make_shared<u8*>(
                    new u8[image_h * image_w / 2]);
            memset((void *)(*y_ptr.get()), 148, image_h * image_w);
            memset((void *)(*uv_ptr.get()), 158, image_h * image_w / 2);
            u8 *host_ptr[] = {*y_ptr.get(), *uv_ptr.get()};
            bm_image_copy_host_to_device(src, (void **)host_ptr);
            bmcv_image_storage_convert(handle, image_n, &src, &dst);
            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_dev_free(handle);
            return 0;
        }
