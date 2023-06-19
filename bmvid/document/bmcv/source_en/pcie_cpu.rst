PCIe CPU
==========

For operations that are inconvenient to use TPU acceleration, the cooperation of CPU is required.

If it is SoC mode, the host side is the on-chip ARM A53 processor, which completes the CPU operation.

In case of PCIe mode, the host side is the user’s host, and the CPU operation can be completed at the host side or by using the on-chip ARM A53 processor. The two implementation methods have their own advantages and disadvantages: the former needs to carry input and output data between device and host, but the operation performance may be better than ARM, so users can choose the better method according to their own host processor performance, load and other actual conditions. It is the former by default. If you need to use an on-chip processor, you can turn it on in the following way.


Preparatory Work
________________

If you want to enable the on-chip processor, you need the following two files:

* ramboot_rootfs.itb

* fip.bin

You need to set the path where these two files are located to the environment variable BMCV_CPU_KERNEL_PATH where the program runs, as follows:

$ export BMCV_CPU_KERNEL_PATH=/path/to/kernel_fils/

All implementations of BMCV that require CPU operations are in the library libbmcv_cpu_func.so, you need to add the path of the file to the environment variable BMCV_CPU_KERNEL_PATH where the program runs, as follows:

$ export BMCV_CPU_LIB_PATH=/path/to/lib/

At present, the APIs that require CPU participation are as follows. If the following APIs are not used, this function can be ignored.

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

Opening and Closing
_____________________


Users can use the following two interfaces at the beginning and end of the program to turn on and off the function respectively.

    .. code-block:: c

        bm_status_t bmcv_open_cpu_process(bm_handle_t handle);

        bm_status_t bmcv_close_cpu_process(bm_handle_t handle);


**Input parameters description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.


**Return value description:**

* BM_SUCCESS: success

* Other: failed

