.. vim: syntax=rst

Introduction of TPU Drive Sysfs File System
------------------------------------------------

The sysfs file system interface is used to obtain information such as the usage of TPU. 
The table below lists the device information that can be obtained by TPU Drive Sysfs File System and the support information in PCIe and SOC modes:

.. list-table::
   :widths: 40 30 30
   :header-rows: 0


   * - **Device Information**
     - **PCIe Mode**
     - **SOC Mode**

   * - npu_usage
     - Supported
     - Supported

   * - npu_usage_enable
     - Supported
     - Supported

   * - npu_usage_interval
     - Supported
     - Supported

After the driver is installed, some properties are created for each device to view and modify some parameters. these properties are located in :

PCIe mode: /sys/class/bm-sophon/bm-sophon0/device

SOC mode: /sys/class/bm-tpu/bm-tpu0/device

Meanings of Parameters
-----------------------

The meaning of each part is introduced below.

-  npu_usage, the percentage of time TPU (NPU) is working over a period of time (window width)

-  npu_usage_enable, npu_usage_enable, whether to enable statistics on NPU usage, enabled by default.

-  npu_usage_interval, the time window width of statistics on NPU usage (in ms). The default is 500ms and the value range is [200,2000].

Specific use method of TPU Drive Sysfs File System interface
-------------------------------------------------------------

Examples are as follows:

Change the time window width (only under superuser):

::

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_interval

    "interval": 600

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# echo 500 > npu_usage_interval

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_interval

    "interval": 500

Turn off the enable function of statistics on the usage of NPU:

::

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_enable

    "enable": 1

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# echo 0 > npu_usage_enable

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_enable

    "enable": 0

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage

    Please, set [Usage enable] to 1

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# echo 1 > npu_usage_enable

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_enable

    "enable": 1

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage

    "usage": 0, "avusage": 0

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device#

View the usage of NPU:

::

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage

    "usage": 0, "avusage": 0

Usage indicates the usage of NPU in a past time window.

Avusage indicates the usage of NPU since the drive was installed.
