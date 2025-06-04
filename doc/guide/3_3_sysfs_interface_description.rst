.. vim: syntax=rst

Tpu驱动sysfs文件系统介绍
----------------------

sysfs文件系统接口用来获取TPU的利用率等信息。
下表列举了Tpu驱动sysfs文件系统可以获取的设备信息以及在PCIe和SOC模式下的支持情况：

.. list-table::
   :widths: 40 30 30
   :header-rows: 0


   * - **设备信息**
     - **PCIe模式**
     - **SOC模式**

   * - npu_usage
     - 支持
     - 支持

   * - npu_usage_enable
     - 支持
     - 支持

   * - npu_usage_interval
     - 支持
     - 支持

驱动安装后，会为每个设备创建一些属性，用于查看和修改一些参数。这些属性所在的位置：

PCIe模式：/sys/class/bm-sophon/bm-sophon0/device

SOC模式：/sys/class/bm-tpu/bm-tpu0/device

各项参数的含义
-------

下面逐一介绍每个部分代表的含义。

-  npu_usage，Tpu（npu）在一段时间内（窗口宽度）处于工作状态的百分比。

-  npu_usage_enable，是否使能统计npu利用率，默认使能。

-  npu_usage_interval，统计npu利用率的时间窗口宽度，单位ms，默认500ms。取值范围[200,2000]。

Tpu驱动sysfs文件系统接口的具体使用方法
-----------------------

使用例子如下：

更改时间窗口宽度（只能在超级用户下）：

::

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_interval

    "interval": 600

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# echo 500 > npu_usage_interval

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage_interval

    "interval": 500

使能关闭对npu利用率的统计：

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

查看npu利用率：

::

    root@bitmain:/sys/class/bm-sophon/bm-sophon0/device# cat npu_usage

    "usage": 0, "avusage": 0

Usage表示过去一个时间窗口内的npu利用率。

Avusage表示自安装驱动以来npu的利用率。
