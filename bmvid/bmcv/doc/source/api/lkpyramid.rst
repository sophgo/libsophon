bmcv_image_lkpyramid
====================

LK金字塔光流算法。完整的使用步骤包括创建、执行、销毁三步。该算法前半部分使用TPU，而后半部分为串行运算需要使用CPU，因此对于PCIe模式，建议使能CPU进行加速，具体步骤参考第5章节。

创建
_____

由于该算法的内部实现需要一些缓存空间，为了避免重复申请释放空间，将一些准备工作封装在该创建接口中，只需要在启动前调用一次便可以多次调用execute接口（创建函数参数不变的情况下），接口形式如下：

    .. code-block:: c

        bm_status_t bmcv_image_lkpyramid_create_plan(
                bm_handle_t handle,
                void*& plan,
                int width,
                int height,
                int winW = 21,
                int winH = 21,
                int maxLevel = 3);

**输入参数说明：**

* bm_handle_t handle

输入参数。bm_handle 句柄

* void*& plan

输出参数。执行阶段所需要的句柄。

* int width

输入参数。待处理图像的宽度。

* int height

输入参数，待处理图像的高度。

* int winW

输入参数，算法处理窗口的宽度，默认值为21。

* int winH

输入参数，算法处理窗口的高度，默认值为21。

* int maxLevel

输入参数，金字塔处理的高度，默认值为3, 目前支持的最大值为5。该参数值越大，算法执行时间越长，建议根据实际效果选择可接受的最小值。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他：失败


执行
_____

使用上述接口创建后的plan就可以开始真正的执行阶段了，接口格式如下：

    .. code-block:: c

        typedef struct {
            float x;
            float y;
        } bmcv_point2f_t;

        typedef struct {
            int type;   // 1: maxCount   2: eps   3: both
            int max_count;
            double epsilon;
        } bmcv_term_criteria_t;

        bm_status_t bmcv_image_lkpyramid_execute(
                bm_handle_t handle,
                void* plan,
                bm_image prevImg,
                bm_image nextImg,
                int ptsNum,
                bmcv_point2f_t* prevPts,
                bmcv_point2f_t* nextPts,
                bool* status,
                bmcv_term_criteria_t criteria = {3, 30, 0.01});


**输入参数说明：**

* bm_handle_t handle

输入参数。bm_handle 句柄

* const void \*plan

输入参数。创建阶段所得到的句柄。

* bm_image prevImg

输入参数。前一幅图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image nextImg

输入参数。后一幅图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* int ptsNum

输入参数。需要追踪点的数量。

* bmcv_point2f_t* prevPts

输入参数。需要追踪点在前一幅图中的坐标指针，其指向的长度为ptsNum。

* bmcv_point2f_t* nextPts

输出参数。计算得到的追踪点在后一张图像中坐标指针，其指向的长度为ptsNum。

* bool* status

输出参数。nextPts中的各个追踪点是否有效，其指向的长度为ptsNum，与nextPts中的坐标一一对应，如果有效则为true，否则为false（表示没有在后一张图像中找到对应的跟踪点，可能超出图像范围）。

* bmcv_term_criteria_t criteria

输入参数。迭代结束标准，type表示以哪个参数作为结束判断条件：若为1则以迭代次数max_count为结束判断参数，若为2则以误差epsilon为结束判断参数，若为3则两者均需满足。该参数会影响执行时间，建议根据实际效果选择最优的停止迭代标准。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


销毁
______

当执行完成后需要销毁所创建的句柄。该接口必须和创建接口bmcv_image_lkpyramid_create_plan成对使用。

    .. code-block:: c

        void bmcv_image_lkpyramid_destroy_plan(bm_handle_t handle, void *plan);


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_GRAY            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+

示例代码
___________

    .. code-block:: c

        bm_handle_t handle;
        bm_status_t ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            return -1;
        }
        ret = bmcv_open_cpu_process(handle);
        if (ret != BM_SUCCESS) {
            printf("BMCV enable CPU failed. ret = %d\n", ret);
            bm_dev_free(handle);
            return -1;
        }
        bm_image_format_ext fmt = FORMAT_GRAY;
        bm_image prevImg;
        bm_image nextImg;
        bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &prevImg);
        bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &nextImg);
        bm_image_alloc_dev_mem(prevImg);
        bm_image_alloc_dev_mem(nextImg);
        bm_image_copy_host_to_device(prevImg, (void **)(&prevPtr));
        bm_image_copy_host_to_device(nextImg, (void **)(&nextPtr));
        void *plan = nullptr;
        bmcv_image_lkpyramid_create_plan(
                handle,
                plan,
                width,
                height,
                kw,
                kh,
                maxLevel);
        bmcv_image_lkpyramid_execute(
                handle,
                plan,
                prevImg,
                nextImg,
                ptsNum,
                prevPts,
                nextPts,
                status,
                criteria);
        bmcv_image_lkpyramid_destroy_plan(handle, plan);
        bm_image_destroy(prevImg);
        bm_image_destroy(nextImg);
        ret = bmcv_close_cpu_process(handle);
        if (ret != BM_SUCCESS) {
            printf("BMCV disable CPU failed. ret = %d\n", ret);
            bm_dev_free(handle);
            return -1;
        }
        bm_dev_free(handle);

