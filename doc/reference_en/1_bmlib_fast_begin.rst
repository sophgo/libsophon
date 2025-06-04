Quick Start
========

Term Interpretation 
--------

.. list-table::
   :widths: 30 70
   :header-rows: 0


   * - **Term**
     - **Description**

   * - BM1684
     - The third-generation tensor processor unit for deep learning developed by SOPHGO

   * - BM1684X
     - The fourth-generation tensor processor unit for deep learning developed by SOPHGO

   * - TPU
     - Neural network processing unit in BM1684

   * - SOC Mode
     - A product form, the SDK runs on A53 AARCH64 platform, and TPU is used as the platform bus device

   * - PCIE Mode
     - A product form, SDK runs on the host platform ( it can be X86 or AARCH64 server), BM1684 serves as deep learning computing accelerator card in PCIe interface

   * - Driver
     - Driver is the channel for API to access the hardware

   * - Gmem
     - DDR memory on card for NPU acceleration

   * - Handle
     - A user process (thread) handle of the device, and all operations must be done through the handle

