bm_image device memory 管理
============================


bm_image 结构需要关联相关 device memory，并且 device memory 中有你所需要的数据时，才能够调用之后的 bmcv API。无论是调用 bm_image_alloc_dev_mem 内部申请，还是调用 bm_image_attach 关联外部内存，均能够使得 bm_image 对象关联 device memory。


判断 bm_image 对象是否已经关联了，可以调用以下 API：

    .. code-block:: c

        bool bm_image_is_attached(
                bm_image image
        );


**传入参数说明:**

* bm_image image

  输入参数。待判断的 bm_image 对象


**返回值说明：**

1. 如果 bm_image 对象未创建，则返回 false;

2. 该函数返回 bm_image 对象是否关联了一块 device memory，如果已关联，则返回 true，否则返回 false


**注意事项:**

1. 一般情况而言，调用 bmcv api 要求输入 bm_image 对象关联 device memory，否则返回失败。而输出 bm_image 对象如果未关联 device memory，则会在内部调用 bm_image_alloc_dev_mem 函数，内部申请内存。

2. bm_image 调用 bm_image_alloc_dev_mem 所申请的内存都由内部自动管理，在调用 bm_image_destroy、 bm_image_detach 或者 bm_image_attach 其他 device memory 时自动释放，无需调用者管理。相反，如果 bm_image_attach 一块 device memory 时，表示这块 memory 将由调用者自己管理。无论是 bm_image_destroy、bm_image_detach，或者再调用 bm_image_attach 其他 device memory，均不会释放，需要调用者手动释放。

3. 目前 device memory 分为三块内存空间：heap0、heap1和heap2。三者的区别在于bm1684 芯片的硬件 VPP 模块是否有读取权限，其他完全相同，因此如果某一 API 需要指定使用bm1684 硬件VPP模块来实现，则必须保证该 API 的输入 bm_image 保存在 heap1 或者 heap2 空间上。  bm1684x vpp无此限制。

+------------------+------------------+------------------+
|    heap id       |   bm1684 VPP     |   bm1684x VPP    |
+==================+==================+==================+
|    heap0         |     不可读       |     可读         |
+------------------+------------------+------------------+
|    heap1         |     可读         |     可读         |
+------------------+------------------+------------------+
|    heap2         |     可读         |     可读         |
+------------------+------------------+------------------+
