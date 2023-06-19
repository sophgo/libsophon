PCIe CPU
==========

对于不方便使用 TPU 加速的操作，需要 CPU 配合来完成。

如果是 SoC 模式，host端即为片上的ARM A53处理器，由它来完成CPU操作。

如果是 PCIe 模式，host端为用户的主机，CPU 操作可以选择在host端完成，也可以使用片上的ARM A53处理器来完成。两种实现方式各有优缺点：前者需要在device和host之间搬运输入输出数据，但运算性能可能优于ARM，所以用户可以根据自身host处理器性能、负载等实际情况选择最优的方式。默认情况下为前者，如果需要使用片上处理器可按照以下方式开启。


准备工作
________

如果要使能片上处理器，那么需要以下两个文件：

* ramboot_rootfs.itb

* fip.bin

需要将这两个文件所在的路径设置到程序运行的环境变量 BMCV_CPU_KERNEL_PATH 中， 如下：

$ export BMCV_CPU_KERNEL_PATH=/path/to/kernel_fils/

BMCV所有需要CPU操作的实现均在库 libbmcv_cpu_func.so 中，需要将该文件所在路径添加到程序运行的环境变量 BMCV_CPU_LIB_PATH 中，如下：

$ export BMCV_CPU_LIB_PATH=/path/to/lib/

目前需要CPU参与实现的API如下所示，如果没有使用以下API可忽略该功能。

+-----+-----------------------------------+
| num |       API                         |
+=====+===================================+
| 1   | bmcv_image_draw_lines             |
+-----+-----------------------------------+
| 2   | bmcv_image_erode                  |
+-----+-----------------------------------+
| 3   | bmcv_image_dilate                 |
+-----+-----------------------------------+
| 4   | bmcv_image_lkpyramid_execute      |
+-----+-----------------------------------+
| 5   | bmcv_image_morph                  |
+-----+-----------------------------------+

开启和关闭
___________


用户可以在程序的开始结束处分别使用以下两个接口，即可分别实现该功能的开启和关闭。

    .. code-block:: c

        bm_status_t bmcv_open_cpu_process(bm_handle_t handle);

        bm_status_t bmcv_close_cpu_process(bm_handle_t handle);


**传入参数说明:**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败

