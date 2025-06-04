.. vim: syntax=rst

Introduction of proc file system
------------------------------------
The proc file system interface creates a device information node under the /proc node. Users can read the relevant nodes such as device temperature and version, through cat command or programming to obtain information.

Bm-smi focuses on displaying device information visually using interface, and the proc file system interface focuses on providing users with an interface for programming to obtain device information. The table below lists the device information that can be obtained by the proc file system and its support information in PCIe and SOC modes:

.. list-table::
   :widths: 40 30 30
   :header-rows: 0


   * - **Device Information**
     - **PCIe Mode**
     - **SOC Mode**

   * - card_num
     - Supported
     - Not supported

   * - chip_num
     - Supported
     - Not supported

   * - chip_num_on_card
     - Supported
     - Not supported

   * - board_power
     - Supported
     - Not supported

   * - board_temp
     - Supported
     - Not supported

   * - chipid
     - Supported
     - Not supported

   * - chip_temp
     - Supported
     - Not supported

   * - dbdf
     - Supported
     - Not supported

   * - dynfreq
     - Supported
     - Not supported

   * - ecc
     - Supported
     - Not supported

   * - maxboardp
     - Supported
     - Not supported

   * - mode
     - Supported
     - Not supported

   * - pcie_cap_speed
     - Supported
     - Not supported

   * - pcie_cap_width
     - Supported
     - Not supported

   * - pcie_link_speed
     - Supported
     - Not supported

   * - pcie_link_width
     - Supported
     - Not supported

   * - pcie_region
     - Supported
     - Not supported

   * - tpuid
     - Supported
     - Not supported

   * - tpu_maxclk
     - Supported
     - Not supported

   * - tpu_minclk
     - Supported
     - Not supported

   * - tpu_freq
     - Supported
     - Not supported

   * - tpu_power
     - Supported
     - Not supported

   * - firmware_info
     - Supported
     - Not supported

   * - sn
     - Supported
     - Not supported

   * - boot_loader_version
     - Supported
     - Not supported

   * - board_type
     - Supported
     - Not supported

   * - driver_version
     - Supported
     - Not supported

   * - board_version
     - Supported
     - Not supported

   * - mcu_version
     - Supported
     - Not supported

   * - versions
     - Supported
     - Not supported

   * - cdma_in_time
     - Supported
     - Not supported

   * - cdma_in_counter
     - Supported
     - Not supported

   * - cdma_out_time
     - Supported
     - Not supported

   * - cdma_out_counter
     - Supported
     - Not supported

   * - tpu_process_time
     - Supported
     - Not supported

   * - completed_api_counter
     - Supported
     - Not supported

   * - send_api_counter
     - Supported
     - Not supported

   * - tpu_volt
     - Supported
     - Not supported

   * - tpu_cur
     - Supported
     - Not supported

   * - fan_speed
     - Supported
     - Not supported

   * - media
     - Supported
     - Not supported

   * - a53_enable
     - Supported
     - Not supported

   * - arm9_cache
     - Supported
     - Not supported

   * - bmcpu_status
     - Supported
     - Not supported

   * - bom_version
     - Supported
     - Not supported

   * - boot_mode
     - Supported
     - Not supported

   * - clk
     - Supported
     - Not supported

   * - ddr_capacity
     - Supported
     - Not supported

   * - dumpreg
     - Supported
     - Not supported

   * - heap
     - Supported
     - Not supported

   * - location
     - Supported
     - Not supported

   * - pcb_version
     - Supported
     - Not supported

   * - pmu_infos
     - Supported
     - Not supported

   * - status
     - Supported
     - Not supported

   * - vddc_power
     - Supported
     - Not supported

   * - vddphy_power
     - Supported
     - Not supported

   * - jpu
     - Not supported
     - Supported

   * - vpu
     - Not supported
     - Supported


When the driver is installed, the system creates the /proc/bmsophon/card0⋯n directory according to the number of board, among which the card0 directory corresponds to the information of the 0th board card, the card1 directory corresponds to the information of the first card, and so on.

The /proc/bmsophon/cardn/bmsophon0⋯x directories are created in sequence under the board directory according to the number of devices on the current board, where x in bmsophonx corresponds to the device information with device id x of board n. For example, only SC5+ is inserted in the device, /proc/bmsophon/card0 will be generated accordingly after the driver is installed, and the bmsophon0/1/2 directory will be generated under the card0 directory to store the information of device 0/1/2, respectively.

In SOC mode, only JPU and VPU support the proc file system interface, and the nodes are /proc/jpuinfo and /proc/vpuinfo.

Meaning of parameters
------------------------

In SOC mode, the parameters are only /proc/jpuinfo and /proc/vpuinfo; and PCIe mode, the directories and file nodes in the proc file system are structured as follows:

::

   bitmain@weiqiao-MS-7B46:~/work/bm168x$ ls /proc/bmsophon/ -l

   total 0

   -r--r--r--.
   1 root root 0 May 6 23:06 card_num

   -r--r--r--.
   1 root root 0 May 6 23:06 chip_num

   -r--r--r--.
   1 root root 0 May 6 23:06 driver_version

   dr-xr-xr-x 2 root root 0 May 6 13:46 card0 //there are the information of board 0 under the folder, as follows:

   bitmain@weiqiao-MS-7B46:~/work/bm168x$ ls /proc/bmsophon/card0/ -l

   total 0

   -r--r--r--.
   1 root root 0 May 6 23:06 board_power

   -r--r--r--.
   1 root root 0 May 6 23:06 board_temp

   -r--r--r--.
   1 root root 0 May 6 23:06 board_type

   -r--r--r--.
   1 root root 0 May 6 23:06 board_version

   -r--r--r--.
   1 root root 0 May 6 23:06 bom_version

   -r--r--r--.
   1 root root 0 May 6 23:06 chipid

   -r--r--r--.
   1 root root 0 May 6 23:06 chip_num_on_card

   -rw-r--r--.
   1 root root 0 May 6 23:06 fan_speed

   -r--r--r--.
   1 root root 0 May 6 23:06 maxboardp

   -r--r--r--.
   1 root root 0 May 6 23:06 mode

   -r--r--r--.
   1 root root 0 May 6 23:06 pcb_version

   -r--r--r--.
   1 root root 0 May 6 23:06 sn

   -r--r--r--.
   1 root root 0 May 6 23:06 tpu_maxclk

   -r--r--r--.
   1 root root 0 May 6 23:06 tpu_minclk

   -r--r--r--.
   1 root root 0 May 6 23:06 versions

   dr-xr-xr-x.
   2 root root 0 May 6 23:06 bmsophon0//there are the information of board 0 under the folder, as follows:

   bitmain@weiqiao-MS-7B46:~/work/bm168x$ ls /proc/bmsophon/card0/bmsophon0 -l

   total 0

   -r--r--r--.
   1 root root 0 May 6 23:11 a53_enable

   -r--r--r--.
   1 root root 0 May 6 23:11 arm9_cache

   -r--r--r--.
   1 root root 0 May 6 23:11 bmcpu_status

   -r--r--r--.
   1 root root 0 May 6 23:11 boot_loader_version

   -r--r--r--.
   1 root root 0 May 6 23:11 boot_mode

   -r--r--r--.
   1 root root 0 May 6 23:11 cdma_in_counter

   -r--r--r--.
   1 root root 0 May 6 23:11 cdma_in_time

   -r--r--r--.
   1 root root 0 May 6 23:11 cdma_out_counter

   -r--r--r--.
   1 root root 0 May 6 23:11 cdma_out_time

   -r--r--r--.
   1 root root 0 May 6 23:11 chip_temp

   -r--r--r--.
   1 root root 0 May 6 23:11 clk

   -r--r--r--.
   1 root root 0 May 6 23:11 completed_api_counter

   -r--r--r--.
   1 root root 0 May 6 23:11 dbdf

   -r--r--r--.
   1 root root 0 May 6 23:11 ddr_capacity

   -rw-r--r--.
   1 root root 0 May 6 23:11 dumpreg

   -rw-r--r--.
   1 root root 0 May 6 23:11 dynfreq

   -r--r--r--.
   1 root root 0 May 6 23:11 ecc

   -r--r--r--.
   1 root root 0 May 6 23:11 heap

   -rw-r--r--.
   1 root root 0 May 6 23:11 jpu

   -r--r--r--.
   1 root root 0 May 6 23:11 location

   -r--r--r--.
   1 root root 0 May 6 23:11 mcu_version

   -rw-r--r--.
   1 root root 0 May 6 23:11 media

   -r--r--r--.
   1 root root 0 May 6 23:11 pcie_cap_speed

   -r--r--r--.
   1 root root 0 May 6 23:11 pcie_cap_width

   -r--r--r--.
   1 root root 0 May 6 23:11 pcie_link_speed

   -r--r--r--.
   1 root root 0 May 6 23:11 pcie_link_width

   -r--r--r--.
   1 root root 0 May 6 23:11 pcie_region

   -r--r--r--.
   1 root root 0 May 6 23:11 pmu_infos

   -r--r--r--.
   1 root root 0 May 6 23:11 sent_api_counter

   -r--r--r--.
   1 root root 0 May 6 23:11 status

   -r--r--r--.
   1 root root 0 May 6 23:11 tpu_cur

   -rw-r--r--.
   1 root root 0 May 6 23:06 tpu_freq

   -r--r--r--.
   1 root root 0 May 6 23:11 tpuid

   -r--r--r--.
   1 root root 0 May 6 23:11 tpu_power

   -r--r--r--.
   1 root root 0 May 6 23:11 firmware_info

   -r--r--r--.
   1 root root 0 May 6 23:11 tpu_process_time

   -rw-r--r--.
   1 root root 0 May 6 23:11 tpu_volt

   -rw-r--r--.
   1 root root 0 May 6 23:11 vddc_power

   -rw-r--r--.
   1 root root 0 May 6 23:11 vddphy_power

Note: If the PCIe mode uses SC5P, the mcu_version will be created under the /proc/bmsophon/card/ board directory.

If other type of board is used, mcu_version will be created under the /proc/bmsophon/card/bmsophon/ device directory.

Meanings and Operation Methods of Parameters
-----------------------------------------------

Detailed Information of Devices in PCIe Mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  card_num

..

   Readwrite property: read only.

   Meaning: number of system boards.

-  chip_num

..

   Readwrite property: read only.

   Meaning: number of system devices.

-  chip_num_on_card

..

   Readwrite property: read only.

   Meaning: number of devices on the corresponding board.

-  board_power

..

   Readwrite property: read only.

   Meaning: board power consumption.

-  board_temp

..

   Readwrite property: read only.

   Meaning: board temperature.

-  chipid

..

   Readwrite property: read only.

   Meaning: chip id (0x1684x/0x1684/0x1682).

-  chip_temp

..

   Readwrite property: read only.

   Meaning: chip temperature.

-  dbdf

..

   Readwrite property: read only.

   Meaning: domain:bus:dev.function.

-  dynfreq

..

   Readwrite property: read and write.

   Meaning: Enable or disable the dynamic tpu frequency modulation function; 0/1 is valid, other values are invalid.

-  ecc

..

   Readwrite property: read only.

   Meaning: Enable or disable ECC function.

-  maxboardp

..

   Readwrite property: read only.

   Meaning: maximum board power consumption.

-  mode

..

   Readwrite property: read only.

   Meaning: work mode, PCIe/SOC.

-  pcie_cap_speed

..

   Readwrite property: read only.

   Meaning: The maximum speed of PCIe supported by the device.

-  pcie_cap_width

..

   Readwrite property: read only.

   Meaning: PCIe interface's maximum lane width supported by the device.

-  pcie_link_speed

..

   Readwrite property: read only.

   Meaning: PCIe interface's speed of the device.

-  pcie_link_width

..

   Readwrite property: read only.

   Meaning: lane width of PCIe interface of the device.

-  pcie_region

..

   Readwrite property: read only.

   Meaning: size of PCIe bar.

-  tpuid

..

   Readwrite property: read only.

   Meaning: tpu ID (0/1/2/3⋯⋯).

-  tpu_maxclk

..

   Readwrite property: read only.

   Meaning: maximum work frequency of tpu.

-  tpu_minclk

..

   Readwrite property: read only.

   Meaning: minimum work frequency of tpu.

-  tpu_freq

..

   Readwrite property: read and write.

   Meaning: work frequency of the tpu, which can be changed by writing parameters. 0 should be written into dynfreq to turn off the dynamic TPU frequency modulation before writing parameters.

-  tpu_power

..

   Readwrite property: read only.

   Meaning: instantaneous power of tpu.

-  firmware_info

..

   Readwrite property: read only.

   Meaning: version information of firmware, including commit id and compiling time.

-  sn

..

   Readwrite property: read only.

   Meaning: number of board product.

-  boot_loader_version

..

   Readwrite property: read only.

   Meaning: bootloader version number in spi flash.

-  board_type

..

   Readwrite property: read only.

   Meaning:  type of board.

-  driver_version

..

   Readwrite property: read only.

   Meaning: version number of drive.

-  board_version

..

   Readwrite property: read only.

   Meaning: version number of board hardware.

-  mcu_version

..

   Readwrite property: read only.

   Meaning: version number of mcu software.

-  versions

..

   Readwrite property: read only.

   Meaning: a collection of board software and hardware versions.

-  cdma_in_time

..

   Readwrite property: read only.

   Meaning: the total time spent by cdma to move data from the host to the board.

-  cdma_in_counter

..

   Readwrite property: read only.

   Meaning: the total number of times that cdma moves data from the host to the board.

-  cdma_out_time

..

   Readwrite property: read only.

   Meaning: the total time spent by cdma to move data from the board to the host.

-  cdma_out_counter

..

   Readwrite property: read only.

   Meaning: the total number of times that cdma moves data from the board to the host.

-  tpu_process_time

..

   Readwrite property: read only.

   Meaning: the time spent in tpu processing.

-  completed_api_counter

..

   Readwrite property: read only.

   Meaning: the number of times the api has been completed.

-  send_api_counter

..

   Readwrite property: read only.

   Meaning: the number of times the api has been sent.

-  tpu_volt

..

   Readwrite property: read and write.

   Meaning: tpu voltage, the voltage can be changed by writing a parameter.

-  tpu_cur

..

   Readwrite property: read only.

   Meaning: tpu current.

-  fan_speed

..

   Readwrite property: read only.

   Meaning: fan actual speed.

-  media

..

   Readwrite property: read only.

   total_mem_size : total size of memory used by vpu and jpu.

   used_mem_size :  memory being used by vpu and jpu.

   free_mem_size : free memory.

   id : number of vpu core.

   link_num : number of encoding/decoding channels.

-  a53_enable

..

   Readwrite property: read only.

   Meaning: a53 enable status.

-  arm9_cache

..

   Readwrite property: read only.

   Meaning: cache enable status of arm9.

-  bmcpu_status

..

   Readwrite property: read only. 

   Meaning: bmcpu status.

-  bom_version

..

   Readwrite property: read only. 

   Meaning: bom version number.

-  boot_mode

..

   Readwrite property: read only. 

   Meaning: start mode.

-  clk

..

   Readwrite property: read only. 

   Meaning: clocks of modules.

-  ddr_capacity

..

   Readwrite property: read only.

   Meaning: ddr capacity.

-  dumpreg

..

   Readwrite property: read and write.

   Meaning: Dump register, input 1 is dumped to tpu register, input 2 is dumped to gdma register.

-  heap

..

   Readwrite property: read only. 

   Meaning: displaying the size of each heap.

-  location

..

   Readwrite property: read only. 

   Meaning: showing it is on which device currently.

-  pcb_version

..

   Readwrite property: read only.

   Meaning: pcb version number.

-  pmu_infos

..

   Readwrite property: read only.

   Meaning: More detailed current and voltage information.

-  status

..

   Readwrite property: read only.  

   Meaning: board status.

-  vddc_power

..

   Readwrite property: read only.

   Meaning: vddc power.

-  vddphy_power

..

   Readwrite property: read only.

   Meaning: vddphy power.

Detailed information of devices in SOC mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In SOC mode, only JPU  and  VPU  supports  proc  interface, and the corresponding  proc  nodes are /proc/jpuinfo and /proc/vpuinfo.

-  jpuinfo

..

   Readwrite property: read only.

   JPU loadbalance : recording JPU0-JPU1(1684x),JPU0-JPU3(1684) encoding/decoding times, JPU* is JPEG encoder/decoder inside chip, value range: 0~2147483647


-  vpuinfo

..

   Readwrite property: read only.

   id: vpu core  number, value range: 0~2(1684x), 0~4(1684).

   link_num: number of encoding/decoding channels; value range: 0~32.
