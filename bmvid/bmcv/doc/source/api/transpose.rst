bmcv_image_transpose
====================

该接口可以实现图片宽和高的转置。

**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_transpose(
                bm_handle_t handle,
                bm_image input,
                bm_image output
        );

**传入参数说明:**

* bm_handle_t handle

输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* bm_image input

输入参数。输入图像的 bm_image 结构体。

* bm_image output

输出参数。输出图像的 bm_image 结构体。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败



**代码示例**

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
             bm_image_create(handle, image_h, image_w, FORMAT_RGB_PLANAR,
                     DATA_TYPE_EXT_1N_BYTE, &src);
             bm_image_create(handle, image_w, image_h, FORMAT_RGB_PLANAR,
                     DATA_TYPE_EXT_1N_BYTE, &dst);
             std::shared_ptr<u8*> src_ptr = std::make_shared<u8*>(
                     new u8[image_h * image_w * 3]);
             memset((void *)(*src_ptr.get()), 148, image_h * image_w * 3);
             u8 *host_ptr[] = {*src_ptr.get()};
             bm_image_copy_host_to_device(src, (void **)host_ptr);
             bmcv_image_transpose(handle, src, dst);
             bm_image_destroy(src);
             bm_image_destroy(dst);
             bm_dev_free(handle);
             return 0;
         }



**注意事项:**

1. 该 API 要求输入和输出的 bm_image 图像格式相同，支持以下格式：

        * FORMAT_RGB_PLANAR
        * FORMAT_BGR_PLANAR
        * FORMAT_GRAY

2. 该 API 要求输入和输出的 bm_image 数据类型相同，支持以下类型：

        * DATA_TYPE_EXT_FLOAT32,
        * DATA_TYPE_EXT_1N_BYTE,
        * DATA_TYPE_EXT_4N_BYTE,
        * DATA_TYPE_EXT_1N_BYTE_SIGNED,
        * DATA_TYPE_EXT_4N_BYTE_SIGNED,

3. 输出图像的 width 必须等于输入图像的 height，输出图像的 height 必须等于输入图像的 width ;

4. 输入图像支持带有 stride;

5. 输入输出 bm_image 结构必须提前创建，否则返回失败。

6. 输入 bm_image 必须 attach device memory，否则返回失败

7. 如果输出对象未 attach device memory，则会内部调用 bm_image_alloc_dev_mem 申请内部管理的 device memory，并将转置后的数据填充到 device memory 中。

