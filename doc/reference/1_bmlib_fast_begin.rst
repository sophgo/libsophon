快速开始
========

术语解释 
--------

.. list-table::
   :widths: 30 70
   :header-rows: 0


   * - **术语**
     - **说明**

   * - BM1684
     - 算能面向深度学习领域推出的第三代张量处理器

   * - BM1684X
     - 算能面向深度学习领域推出的第四代张量处理器

   * - TPU
     - BM1684内部神经网络处理单元

   * - SOC Mode
     - 一种产品形态，SDK运行于A53 AARCH64平台，TPU作为平台总线设备。

   * - PCIE Mode
     - 一种产品形态，SDK运行于host平台（可以是X86/AARCH64服务器），BM1684作为PCIE接口的深度学习计算加速卡存在

   * - Driver
     - Driver是API接口访问硬件的通道

   * - Gmem
     - 卡上用于NPU加速的DDR内存

   * - Handle
     - 一个用户进程(线程)使用设备的句柄，一切操作都要通过handle

