bmcv_image_yuv2bgr_ext
========================

该接口实现YUV格式到RGB格式的转换。

**接口形式:**

    .. code-block:: c

        bm_status_t bmcv_image_yuv2bgr_ext(
                bm_handle_t handle,
                int image_num,
                bm_image* input,
                bm_image* output
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

* 其他: 失败



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
             bmcv_image_yuv2bgr_ext(handle, image_n, &src, &dst);
             bm_image_destroy(src);
             bm_image_destroy(dst);
             bm_dev_free(handle);
             return 0;
         }



**注意事项:**

1. 该 API 输入 NV12/NV21/NV16/NV61/YUV420P 格式的 image 对象，并将转化后的 RGB 数据结果填充到 output image 对象所关联的 device memory 中。

2. 目前该 API 仅支持

- 输入 bm_image 图像格式为：

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_NV12                   |
+-----+-------------------------------+
|  2  | FORMAT_NV21                   |
+-----+-------------------------------+
|  3  | FORMAT_NV16                   |
+-----+-------------------------------+
|  4  | FORMAT_NV61                   |
+-----+-------------------------------+
|  5  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  6  | FORMAT_YUV422P                |
+-----+-------------------------------+

- 支持输出 bm_image 图像格式为:

+-----+-------------------------------+
| num | output image_format           |
+=====+===============================+
|  1  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  2  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+

- bm1684支持 bm_image 数据格式为：

+-----+------------------------+-------------------------------+
| num | input data type        | output data type              |
+=====+========================+===============================+
|  1  |                        | DATA_TYPE_EXT_FLOAT32         |
+-----+                        +-------------------------------+
|  2  | DATA_TYPE_EXT_1N_BYTE  | DATA_TYPE_EXT_1N_BYTE         |
+-----+                        +-------------------------------+
|  3  |                        | DATA_TYPE_EXT_4N_BYTE         |
+-----+------------------------+-------------------------------+

- bm1684x支持 bm_image 数据格式为：

+-----+------------------------+-------------------------------+
| num | input data type        | output data type              |
+=====+========================+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE  | DATA_TYPE_EXT_FLOAT32         |
+-----+                        +-------------------------------+
|  2  |                        | DATA_TYPE_EXT_1N_BYTE         |
+-----+------------------------+-------------------------------+

如果不满足输入输出格式要求，则返回失败。

3. 输入输出所有 bm_image 结构必须提前创建，否则返回失败。

4. 所有输入 bm_image 对象的 image_format、data_type、width、height 必须相等，所有输出 bm_image 对象的 image_format、data_type、width、height 必须相等，所有输入输出 bm_image 对象的 width、height 必须相等，否则返回失败。

5. image_num 表示输入对象的个数，如果输出 bm_image 数据格式为 DATA_TYPE_EXT_4N_BYTE，则仅输出 1 个 bm_image 4N 对象，反之则输出 image_num 个对象。

6. image_num 必须大于等于 1，小于等于 4，否则返回失败。

7. 所有输入对象必须 attach device memory，否则返回失败

8. 如果输出对象未 attach device memory，则会内部调用 bm_image_alloc_dev_mem 申请内部管理的 device memory，并将转化后的 RGB 数据填充到 device memory 中。

