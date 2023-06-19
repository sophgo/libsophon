安装libsophon
--------------

.. |ver| replace:: 0.4.8

libsophon在不同的Linux发行版上提供不同类型的安装方式。请根据您的系统选择对应的方式，不要在一台机器上混用多种安装方式。
以下描述中“|ver|”仅为示例，视当前实际安装版本会有变化。

**请先卸载旧的BM1684 SDK的驱动：**

.. parsed-literal::

  进入SDK的安装目录，执行：
  sudo ./remove_driver_pcie.sh
  或者：
  sudo rmmod bmsophon
  sudo rm -f /lib/modules/$(uname -r)/kernel/drivers/pci/bmsophon.ko


**如果使用Debian/Ubuntu系统：**


安装包由三个文件构成，其中“$arch”为当前机器的硬件架构，使用以下命令可以获取当前服务器的arch：

.. parsed-literal::

  uname -m

通常x86_64机器对应的硬件架构为amd64，arm64机器对应的硬件架构为arm64：
 - sophon-driver\_\ |ver|\ _$arch.deb
 - sophon-libsophon\_\ |ver|\ _$arch.deb
 - sophon-libsophon-dev\_\ |ver|\ _$arch.deb

其中：sophon-driver包含了PCIe加速卡驱动；sophon-libsophon包含了运行时环境（库文件、工具等）；sophon-libsophon-dev包含了开发环境（头文件等）。
如果只是在部署环境上安装，则不需要安装sophon-libsophon-dev。

可以通过如下步骤安装：

.. parsed-literal::

  安装依赖库，只需要执行一次：
  sudo apt install dkms libncurses5
  安装libsophon：
  sudo dpkg -i sophon-*.deb
  在终端执行如下命令，或者登出再登入当前用户后即可使用bm-smi等命令：
  source /etc/profile

安装位置为：

.. parsed-literal::

  /opt/sophon/
  ├── driver-|ver|
  ├── libsophon-|ver|
  │    ├── bin
  │    ├── data
  │    ├── include
  │    └── lib
  └── libsophon-current -> /opt/sophon/libsophon-|ver|

deb包安装方式并不允许您安装同一个包的多个不同版本，但您可能用其它方式在/opt/sophon下放置了若干不同版本。
在使用deb包安装时，/opt/sophon/libsophon-current会指向最后安装的那个版本。在卸载后，它会指向余下的最新版本（如果有的话）。

在使用deb包安装时，如果出现了下面的提示信息

.. parsed-literal::

  modprobe: FATAL: Module bmsophon is in use.

需要您手动停止正在使用驱动的程序后，手动执行下面的命令来安装新的deb包里的驱动。

.. parsed-literal::

  sudo modprobe -r bmsophon
  sudo modprobe bmsophon


卸载方式：

注意：如果安装了sophon-mw及sophon-rpc，因为它们对libsophon有依赖关系，请先卸载它们。

.. parsed-literal::

  sudo apt remove sophon-driver sophon-libsophon
  或者:
  sudo dpkg -r sophon-driver
  sudo dpkg -r sophon-libsophon-dev
  sudo dpkg -r sophon-libsophon

如果卸载时遇到问题，可以尝试如下方法：

.. parsed-literal::

  手工卸载驱动：
  dkms status
  检查输出结果，通常如下：
  bmsophon, |ver|, 5.15.0-41-generic, x86_64: installed
  记下前两个字段，套用到如下命令中：
  sudo dkms remove -m bmsophon -v |ver| --all
  然后再次卸载驱动：
  sudo apt remove sophon-driver
  sudo dpkg --purge sophon-driver
  彻底清除libsophon：
  sudo apt purge sophon-libsophon


**如果使用Centos系统, 当前版本仅支持x86_64：**


安装包由三个文件构成，其中“$arch”为当前机器的硬件架构，使用以下命令可以获取当前服务器的arch：

.. parsed-literal::

  uname -m

x86_64机器对应的安装包名称为：
 - sophon-driver-\ |ver|\ -1.$arch.rpm
 - sophon-libsophon-\ |ver|\ -1.$arch.rpm
 - sophon-libsophon-dev-\ |ver|\ -1.$arch.rpm


安装前需要通过后面“卸载方式”中的步骤卸载旧版本libsophon，可以通过如下步骤安装：

.. parsed-literal::

  安装依赖库，只需要执行一次:
  sudo yum install -y epel-release
  sudo yum install -y dkms
  sudo yum install -y ncurses*
  安装libsophon：
  sudo  rpm -ivh sophon-driver-\ |ver|\ -1.x86_64.rpm
  sudo  rpm -ivh sophon-libsophon-\ |ver|\ -1.x86_64.rpm
  sudo  rpm -ivh --force sophon-libsophon-dev-\ |ver|\ -1.x86_64.rpm
  在终端执行如下命令，或者登出再登入当前用户后即可使用bm-smi等命令：
  source /etc/profile

卸载方式：

.. parsed-literal::

  sudo rpm -e sophon-driver
  sudo rpm -e sophon-libsophon-dev
  sudo rpm -e sophon-libsophon

**如果使用其它Linux系统：**

安装包由一个文件构成，其中“$arch”为当前机器的硬件架构，使用以下命令可以获取当前服务器的arch：

.. parsed-literal::

  uname -m

通常x86_64机器对应的硬件架构为x86_64，arm64机器对应的硬件架构为aarch64：
 - libsophon\_\ |ver|\ _$arch.tar.gz

可以通过如下步骤安装：

注意：如果有旧版本，先参考下面的卸载方式步骤卸载旧版本。

.. parsed-literal::

  tar -xzvf libsophon\_\ |ver|\ _$arch.tar.gz
  sudo cp -r libsophon\_\ |ver|\ _$arch/* /
  sudo ln -s /opt/sophon/libsophon-|ver| /opt/sophon/libsophon-current


接下来请先按照您所使用Linux发行版的要求搭建驱动编译环境，然后做如下操作：

.. parsed-literal::

  sudo ln -s /opt/sophon/driver-\ |ver|\ /$bin /lib/firmware/bm1684x_firmware.bin
  sudo ln -s /opt/sophon/driver-\ |ver|\ /$bin /lib/firmware/bm1684_ddr_firmware.bin
  sudo ln -s /opt/sophon/driver-\ |ver|\ /$bin /lib/firmware/bm1684_tcm_firmware.bin
  cd /opt/sophon/driver-\ |ver|


此处“$bin”是bin文件全名, 对于bm1684x板卡，为a53lite_pkg.bin, 对于bm1684板卡，如bm1684_ddr.bin_v3.1.2-3dfbe057-221128和bm1684_tcm.bin_v3.1.2-3dfbe057-221128。

之后就可以编译驱动了（这里不依赖于dkms）：

::

  sudo make SOC_MODE=0 PLATFORM=asic SYNC_API_INT_MODE=1 \
            TARGET_PROJECT=sg_pcie_device FW_SIMPLE=0 \
            PCIE_MODE_ENABLE_CPU=1
  sudo cp ./bmsophon.ko /lib/modules/$(uname -r)/kernel/
  sudo depmod
  sudo modprobe bmsophon

最后是一些配置工作：

.. parsed-literal::

  添加库和可执行文件路径：
  sudo cp /opt/sophon/libsophon-current/data/libsophon.conf /etc/ld.so.conf.d/
  sudo ldconfig
  sudo cp /opt/sophon/libsophon-current/data/libsophon-bin-path.sh /etc/profile.d/
  在终端执行如下命令，或者登出再登入当前用户后即可使用bm-smi等命令：
  source /etc/profile

  添加cmake config文件：
  sudo mkdir -p /usr/lib/cmake/libsophon
  sudo cp /opt/sophon/libsophon-current/data/libsophon-config.cmake /usr/lib/cmake/libsophon/

卸载方式：

.. parsed-literal::

  sudo rm -f /etc/ld.so.conf.d/libsophon.conf
  sudo ldconfig
  sudo rm -f /etc/profile.d/libsophon-bin-path.sh
  sudo rm -rf /usr/lib/cmake/libsophon
  sudo rmmod bmsophon
  sudo rm -f /lib/modules/$(uname -r)/kernel/bmsophon.ko
  sudo depmod
  sudo rm -f /lib/firmware/bm1684x_firmware.bin
  sudo rm -f /lib/firmware/bm1684_ddr_firmware.bin
  sudo rm -f /lib/firmware/bm1684_tcm_firmware.bin
  sudo rm -f /opt/sophon/libsophon-current
  sudo rm -rf /opt/sophon/libsophon-|ver|
  sudo rm -rf /opt/sophon/driver-|ver|
