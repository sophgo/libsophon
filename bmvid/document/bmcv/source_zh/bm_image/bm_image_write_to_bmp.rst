bm_image_write_to_bmp
=====================

该接口用于将bm_image对象输出为位图（.bmp）。


**接口形式:**

    .. code-block:: c

        bm_status_t bm_image_write_to_bmp(
                bm_image    input,
                const char* filename);

**参数说明:**

* bm_image input

输入参数。输入 bm_image。

* const char\* filename

输入参数。保存的位图文件路径以及文件名称。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败

**注意事项:**

1. 在调用 bm_image_write_to_bmp()之前必须确保输入的 image 已被正确创建并保证is_attached，否则该函数将返回失败。
