.. vim: syntax=rst

proc文件系统介绍
---------------
proc文件系统接口在/proc节点下创建设备信息节点，用户通过cat或者编程的方式读取相关节点获取设备温度、版本等信息。

bm-smi侧重于以界面的形式直观显示设备信息，proc文件系统接口侧重于为用户提供编程获取设备信息的接口。
下表列举了proc文件系统可以获取的设备信息以及在PCIe和SOC模式下的支持情况：

.. list-table::
   :widths: 40 30 30
   :header-rows: 0


   * - **设备信息**
     - **PCIe模式**
     - **SOC模式**

   * - card_num
     - 支持
     - 不支持

   * - chip_num
     - 支持
     - 不支持

   * - chip_num_on_card
     - 支持
     - 不支持

   * - board_power
     - 支持
     - 不支持

   * - board_temp
     - 支持
     - 不支持

   * - chipid
     - 支持
     - 不支持

   * - chip_temp
     - 支持
     - 不支持

   * - dbdf
     - 支持
     - 不支持

   * - dynfreq
     - 支持
     - 不支持

   * - ecc
     - 支持
     - 不支持

   * - maxboardp
     - 支持
     - 不支持

   * - mode
     - 支持
     - 不支持

   * - pcie_cap_speed
     - 支持
     - 不支持

   * - pcie_cap_width
     - 支持
     - 不支持

   * - pcie_link_speed
     - 支持
     - 不支持

   * - pcie_link_width
     - 支持
     - 不支持

   * - pcie_region
     - 支持
     - 不支持

   * - tpuid
     - 支持
     - 不支持

   * - tpu_maxclk
     - 支持
     - 不支持

   * - tpu_minclk
     - 支持
     - 不支持

   * - tpu_freq
     - 支持
     - 不支持

   * - tpu_power
     - 支持
     - 不支持

   * - firmware_info
     - 支持
     - 不支持

   * - sn
     - 支持
     - 不支持

   * - boot_loader_version
     - 支持
     - 不支持

   * - board_type
     - 支持
     - 不支持

   * - driver_version
     - 支持
     - 不支持

   * - board_version
     - 支持
     - 不支持

   * - mcu_version
     - 支持
     - 不支持

   * - versions
     - 支持
     - 不支持

   * - cdma_in_time
     - 支持
     - 不支持

   * - cdma_in_counter
     - 支持
     - 不支持

   * - cdma_out_time
     - 支持
     - 不支持

   * - cdma_out_counter
     - 支持
     - 不支持

   * - tpu_process_time
     - 支持
     - 不支持

   * - completed_api_counter
     - 支持
     - 不支持

   * - send_api_counter
     - 支持
     - 不支持

   * - tpu_volt
     - 支持
     - 不支持

   * - tpu_cur
     - 支持
     - 不支持

   * - fan_speed
     - 支持
     - 不支持

   * - media
     - 支持
     - 不支持

   * - a53_enable
     - 支持
     - 不支持

   * - arm9_cache
     - 支持
     - 不支持

   * - bmcpu_status
     - 支持
     - 不支持

   * - bom_version
     - 支持
     - 不支持

   * - boot_mode
     - 支持
     - 不支持

   * - clk
     - 支持
     - 不支持

   * - ddr_capacity
     - 支持
     - 不支持

   * - dumpreg
     - 支持
     - 不支持

   * - heap
     - 支持
     - 不支持

   * - location
     - 支持
     - 不支持

   * - pcb_version
     - 支持
     - 不支持

   * - pmu_infos
     - 支持
     - 不支持

   * - status
     - 支持
     - 不支持

   * - vddc_power
     - 支持
     - 不支持

   * - vddphy_power
     - 支持
     - 不支持

   * - jpu
     - 不支持
     - 支持

   * - vpu
     - 不支持
     - 支持


驱动安装时系统根据板卡数量依次创建/proc/bmsophon/card0…n目录，其中card0目录对应存放第0张板卡的信息，card1目录对应存放第1张板卡信息，依次类推。

在板卡目录下根据当前板卡上设备数量依次创建/proc/bmsophon/cardn/bmsophon0...x目录，其中bmsophonx中的x对应板卡n下设备id为x的设备信息，例如机器上只插了一张SC5+，安装驱动后对应生成/proc/bmsophon/card0，card0目录下会生成bmsophon0/1/2目录，分别存放设备0/1/2的信息。

SOC模式只有JPU和VPU支持 proc文件系统接口，节点分别是/proc/jpuinfo和/proc/vpuinfo。

各项参数的含义
-------

SOC模式只有/proc/jpuinfo和/proc/vpuinfo; PCIe 模式proc文件系统中目录和文件节点安排如下：

::

   bitmain@weiqiao-MS-7B46:~/work/bm168x$ ls /proc/bmsophon/ -l

   total 0

   -r--r--r--.
   1 root root 0 5月 6 23:06 card_num

   -r--r--r--.
   1 root root 0 5月 6 23:06 chip_num

   -r--r--r--.
   1 root root 0 5月 6 23:06 driver_version

   dr-xr-xr-x 2 root root 0 5月 6 13:46 card0 //文件夹下面有板卡0的信息，如下：

   bitmain@weiqiao-MS-7B46:~/work/bm168x$ ls /proc/bmsophon/card0/ -l

   total 0

   -r--r--r--.
   1 root root 0 5月 6 23:06 board_power

   -r--r--r--.
   1 root root 0 5月 6 23:06 board_temp

   -r--r--r--.
   1 root root 0 5月 6 23:06 board_type

   -r--r--r--.
   1 root root 0 5月 6 23:06 board_version

   -r--r--r--.
   1 root root 0 5月 6 23:06 bom_version

   -r--r--r--.
   1 root root 0 5月 6 23:06 chipid

   -r--r--r--.
   1 root root 0 5月 6 23:06 chip_num_on_card

   -rw-r--r--.
   1 root root 0 5月 6 23:06 fan_speed

   -r--r--r--.
   1 root root 0 5月 6 23:06 maxboardp

   -r--r--r--.
   1 root root 0 5月 6 23:06 mode

   -r--r--r--.
   1 root root 0 5月 6 23:06 pcb_version

   -r--r--r--.
   1 root root 0 5月 6 23:06 sn

   -r--r--r--.
   1 root root 0 5月 6 23:06 tpu_maxclk

   -r--r--r--.
   1 root root 0 5月 6 23:06 tpu_minclk

   -r--r--r--.
   1 root root 0 5月 6 23:06 versions

   dr-xr-xr-x.
   2 root root 0 5月 6 23:06 bmsophon0//文件夹下有设备0的信息，如下：

   bitmain@weiqiao-MS-7B46:~/work/bm168x$ ls /proc/bmsophon/card0/bmsophon0 -l

   total 0

   -r--r--r--.
   1 root root 0 5月 6 23:11 a53_enable

   -r--r--r--.
   1 root root 0 5月 6 23:11 arm9_cache

   -r--r--r--.
   1 root root 0 5月 6 23:11 bmcpu_status

   -r--r--r--.
   1 root root 0 5月 6 23:11 boot_loader_version

   -r--r--r--.
   1 root root 0 5月 6 23:11 boot_mode

   -r--r--r--.
   1 root root 0 5月 6 23:11 cdma_in_counter

   -r--r--r--.
   1 root root 0 5月 6 23:11 cdma_in_time

   -r--r--r--.
   1 root root 0 5月 6 23:11 cdma_out_counter

   -r--r--r--.
   1 root root 0 5月 6 23:11 cdma_out_time

   -r--r--r--.
   1 root root 0 5月 6 23:11 chip_temp

   -r--r--r--.
   1 root root 0 5月 6 23:11 clk

   -r--r--r--.
   1 root root 0 5月 6 23:11 completed_api_counter

   -r--r--r--.
   1 root root 0 5月 6 23:11 dbdf

   -r--r--r--.
   1 root root 0 5月 6 23:11 ddr_capacity

   -rw-r--r--.
   1 root root 0 5月 6 23:11 dumpreg

   -rw-r--r--.
   1 root root 0 5月 6 23:11 dynfreq

   -r--r--r--.
   1 root root 0 5月 6 23:11 ecc

   -r--r--r--.
   1 root root 0 5月 6 23:11 heap

   -rw-r--r--.
   1 root root 0 5月 6 23:11 jpu

   -r--r--r--.
   1 root root 0 5月 6 23:11 location

   -r--r--r--.
   1 root root 0 5月 6 23:11 mcu_version

   -rw-r--r--.
   1 root root 0 5月 6 23:11 media

   -r--r--r--.
   1 root root 0 5月 6 23:11 pcie_cap_speed

   -r--r--r--.
   1 root root 0 5月 6 23:11 pcie_cap_width

   -r--r--r--.
   1 root root 0 5月 6 23:11 pcie_link_speed

   -r--r--r--.
   1 root root 0 5月 6 23:11 pcie_link_width

   -r--r--r--.
   1 root root 0 5月 6 23:11 pcie_region

   -r--r--r--.
   1 root root 0 5月 6 23:11 pmu_infos

   -r--r--r--.
   1 root root 0 5月 6 23:11 sent_api_counter

   -r--r--r--.
   1 root root 0 5月 6 23:11 status

   -r--r--r--.
   1 root root 0 5月 6 23:11 tpu_cur

   -rw-r--r--.
   1 root root 0 5月 6 23:06 tpu_freq

   -r--r--r--.
   1 root root 0 5月 6 23:11 tpuid

   -r--r--r--.
   1 root root 0 5月 6 23:11 tpu_power

   -r--r--r--.
   1 root root 0 5月 6 23:11 firmware_info

   -r--r--r--.
   1 root root 0 5月 6 23:11 tpu_process_time

   -rw-r--r--.
   1 root root 0 5月 6 23:11 tpu_volt

   -rw-r--r--.
   1 root root 0 5月 6 23:11 vddc_power

   -rw-r--r--.
   1 root root 0 5月 6 23:11 vddphy_power

注：如果PCIe模式使用SC5P，则mcu_version会创建在/proc/bmsophon/card/板卡目录下。

如果使用其他类型板卡，则mcu_version会创建在/proc/bmsophon/card/bmsophon/设备目录下。

各项参数的含义和使用方法
------------

PCIe模式各个设备的详细信息
~~~~~~~~~~~~~~~~~~~

-  card_num

..

   读写属性：只读；

   含义：系统板卡数量

-  chip_num

..

   读写属性：只读；

   含义：系统设备数量

-  chip_num_on_card

..

   读写属性：只读；

   含义：对应板卡上设备数量

-  board_power

..

   读写属性：只读；

   含义：板级功耗

-  board_temp

..

   读写属性：只读；

   含义：板级温度

-  chipid

..

   读写属性：只读；

   含义：芯片id（0x1684x/0x1684/0x1682）

-  chip_temp

..

   读写属性：只读

   含义：芯片温度

-  dbdf

..

   读写属性：只读

   含义：domain:bus:dev.function

-  dynfreq

..

   读写属性：读写

   含义：使能或者禁止动态tpu调频功能；0/1有效，其他值无效

-  ecc

..

   读写属性：只读

   含义：打开或者关闭ECC功能

-  maxboardp

..

   读写属性：只读

   含义：最大板级功耗

-  mode

..

   读写属性：只读

   含义：工作模式，PCIe/SOC

-  pcie_cap_speed

..

   读写属性：只读

   含义：设备支持的PCIe最大速度

-  pcie_cap_width

..

   读写属性：只读

   含义：设备支持的PCIe接口最大lane的宽度

-  pcie_link_speed

..

   读写属性：只读

   含义：设备的PCIe接口速度

-  pcie_link_width

..

   读写属性：只读

   含义：设备的PCIe接口lane宽度

-  pcie_region

..

   读写属性：只读

   含义：设备PCIe bar的大小

-  tpuid

..

   读写属性：只读

   含义：tpu的ID（0/1/2/3……）

-  tpu_maxclk

..

   读写属性：只读

   含义：tpu的最大工作频率

-  tpu_minclk

..

   读写属性：只读

   含义：tpu的最小工作频率

-  tpu_freq

..

   读写属性：读写

   含义：tpu的工作频率，可通过写入参数来改变频率，写入前应向dynfreq写入0来关闭动态tpu调频

-  tpu_power

..

   读写属性：只读

   含义：tpu的瞬时功率

-  firmware_info

..

   读写属性：只读

   含义：firmware的版本信息，包括commit id和编译时间

-  sn

..

   读写属性：只读

   含义：板卡产品编号

-  boot_loader_version

..

   读写属性：只读

   含义：spi flash 中的bootloader 版本号

-  board_type

..

   读写属性：只读

   含义：板卡类型

-  driver_version

..

   读写属性：只读

   含义：驱动的版本号

-  board_version

..

   读写属性：只读

   含义：板卡硬件的版本号

-  mcu_version

..

   读写属性：只读

   含义：mcu软件版本号

-  versions

..

   读写属性：只读

   含义：板卡软硬件版本的集合

-  cdma_in_time

..

   读写属性：只读

   含义：cdma 从host搬数据到板卡消耗的总时间

-  cdma_in_counter

..

   读写属性：只读

   含义：cdma 从host搬数据到板卡的总次数

-  cdma_out_time

..

   读写属性：只读

   含义：cdma 从板卡搬数据到host消耗的总时间

-  cdma_out_counter

..

   读写属性：只读

   含义：cdma 从板卡搬数据到host的总次数

-  tpu_process_time

..

   读写属性：只读

   含义：tpu 处理过程中消耗的时间

-  completed_api_counter

..

   读写属性：只读

   含义：已完成api的次数

-  send_api_counter

..

   读写属性：只读

   含义：已发送api的次数

-  tpu_volt

..

   读写属性：读写

   含义：tpu的电压，可通过写入参数来改变电压

-  tpu_cur

..

   读写属性：只读

   含义：tpu 电流

-  fan_speed

..

   读写属性：只读

   含义：duty 风扇调速pwm 占空比，fan_speed 风扇实际转速

-  media

..

   读写属性：只读

   total_mem_size :vpu和jpu使用内存总大小

   used_mem_size :vpu和jpu正在使用的内存

   free_mem_size :空闲内存

   id :vpu core的编号

   link_num :编/解码路数

-  a53_enable

..

   读写属性：只读

   含义：a53使能状态

-  arm9_cache

..

   读写属性：只读

   含义：arm9的cache的使能状态

-  bmcpu_status

..

   读写属性：只读

   含义：bmcpu的状态

-  bom_version

..

   读写属性：只读

   含义：bom的版本号

-  boot_mode

..

   读写属性：只读

   含义：启动方式

-  clk

..

   读写属性：只读

   含义：各个模块的时钟

-  ddr_capacity

..

   读写属性：只读

   含义：ddr的容量

-  dumpreg

..

   读写属性：读写

   含义：转存寄存器，输入1转存到tpu寄存器，输入2转存到gdma寄存器

-  heap

..

   读写属性：只读

   含义：显示各个heap的大小

-  location

..

   读写属性：只读

   含义：显示当前位于哪个设备之上

-  pcb_version

..

   读写属性：只读

   含义：pcb的版本号

-  pmu_infos

..

   读写属性：只读

   含义：更详细的电流电压信息

-  status

..

   读写属性：只读

   含义：板卡状态

-  vddc_power

..

   读写属性：只读

   含义：vddc功率

-  vddphy_power

..

   读写属性：只读

   含义：vddphy功率

SOC模式各个设备的详细信息
~~~~~~~~~~~~~~~~~~

SOC模式只有JPU和VPU支持proc接口，对应的proc节点为/proc/jpuinfo和/proc/vpuinfo。

-  jpuinfo

..

   读写属性：只读

   JPU loadbalance : 记录JPU0-JPU1(1684x),JPU0-JPU3(1684)编码/解码次数，JPU*为芯片内部的JPEG编解码器, 取值范围：0~ 2147483647


-  vpuinfo

..

   读写属性：只读

   id: vpu core的编号，取值范围: 0~2(1684x), 0-4(1684)

   link_num: 编/解码路数，取值范围：0~32
