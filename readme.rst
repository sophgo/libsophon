libsophon概述
-------------

**libsophon目前包含如下组件：**


1. TPU这个模块的Linux驱动
2. 运行时库
    1. bmlib
    2. tpu runtime
3. 运行时库的开发环境（头文件等）
4. 辅助工具

**如何从源码编译文档：**

libsophon目录下已经提供了编译好的bmlib接口文档《BMLIB开发参考手册》。

如果想自己编译的话，请参考如下的步骤：

.. **安装依赖包：**

.. ::

..     sudo -E apt update
..     sudo -E apt-get install -y \
..             bsdmainutils \
..             build-essential \
..             cmake \
..             ninja-build \
..             pkg-config \
..             libncurses5-dev \
..             libgflags-dev \
..             libgtest-dev \
..             dkms \
..             gcc-aarch64-linux-gnu \
..             g++-aarch64-linux-gnu \
..             git

.. **获取git submodules：**

.. ::

..     git submodule update --init

.. **编译：**

.. 使用Ninja：

.. ::

..     mkdir build && cd build

..     cmake -G Ninja ..
..     # or, to install to certian destination, eg. $PWD/../install
..     cmake -G Ninja -DCMAKE_INSTALL_PREFIX=$PWD/../install ..

..     # build
..     cmake --build .

..     # build driver
..     cmake --build . --target driver

..     # install to the dest dir
..     cmake --build . --target install

..     # packing .tar.gz and .deb
..     cmake --build . --target package

..     # test bm-smi
..     sudo ln -s xxx/a53lite_pkg.bin /lib/firmware/bm1684x_firmware.bin.bin
..     # where xxx is the path of firmware, if firmware is modify, ln new firmware to /lib/firmware/bm1684x_firmware.bin.bin
..     insmod ./sg_x86_pcie_device/bmsophon.ko
..     ./bm-smi/bm-smi

..     # test bmrt_test
..     ./tpu-runtime/bmrt_test --context static.int8
..     ./tpu-runtime/bmrt_test --context static.int8.b4

.. 使用Make：

.. ::

..     mkdir build && cd build
..     cmake -DPLATFORM=soc -DCMAKE_INSTALL_PREFIX=$PWD/../install ..
..     make -j4
..     make driver
..     make install -j4
..     make package -j4

..     sudo ln -s xxx/a53lite_pkg.bin /lib/firmware/bm1684x_firmware.bin.bin
..     # where xxx is the path of firmware, if firmware is modify, ln new firmware to /lib/firmware/bm1684x_firmware.bin

..     sudo dpkg -i sophon-*.deb
..     source /etc/profile
..     ./bm-smi/bm-smi
..     ./tpu-runtime/bmrt_test --context static.int8
..     ./tpu-runtime/bmrt_test --context static.int8.b4

.. 编译arm64架构的模式libsophon安装包，推荐使用交叉编译工具链在x86_64的服务器上编译，需要准备aarch64架构用的交叉编译工具链gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu，按以下命令配置交叉编译工具链的目录，然后使用cmake进行编译。

.. ::

..     mkdir build && cd build
..     cmake -DPLATFORM=soc -DCROSS_COMPILE_PATH=/absolute_path_to-gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu \
..           -DCMAKE_TOOLCHAIN_FILE=/absolute_path_to_libsophon/toolchain-aarch64-linux.cmake \
..           -DLIB_DIR=/absolute_path_to_libsophon/3rdparty/arm64/soc \
.. 					-DBUILD_STATIC_LIB=ON \
..           -DCMAKE_INSTALL_PREFIX=$PWD/../install ..
..     make
..     make driver
..     make install
..     make package

..     sudo ln -s xxx/a53lite_pkg.bin /lib/firmware/bm1684x_firmware.bin
..     # where xxx is the path of firmware, if firmware is modify, ln new firmware to /lib/firmware/bm1684x_firmware.bin

..     sudo dpkg -i sophon-*.deb
..     source /etc/profile
..     ./bm-smi/bm-smi
..     ./tpu-runtime/bmrt_test --context static.int8
..     ./tpu-runtime/bmrt_test --context static.int8.b4

.. 编译 loongarch64 架构的 pcie 模式 libsophon 安装包，使用交叉编译工具链在 x86_64 的服务器上编译，需要准备 loongarch64 架构用的交叉编译工具链 loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.1，按以下命令配置交叉编译工具链的目录，然后使用cmake进行编译。

.. ::

..     mkdir build && cd build
..     cmake \
..         -DPLATFORM=pcie_loongarch64 \
..         -DCROSS_COMPILE_PATH=/absolute_path_to-loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.1 \
..         -DCMAKE_TOOLCHAIN_FILE=/absolute_path_to_libsophon/toolchain-loongarch64-linux.cmake \
..         -DLIB_DIR=/absolute_path_to_libsophon/3rdparty/loongarch64 \
..         -DCMAKE_INSTALL_PREFIX=$PWD/../install ..

..     PATH=/absolute_path_to-loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.1/bin:$PATH
..     make
..     make driver
..     make install
..     make package

.. **编译文档：**

1.安装编译文档需要的依赖包：

.. code-block:: bash

    # 安装latex：
    sudo apt install texlive-xetex texlive-latex-recommended
    # 安装sphinx：
    sudo apt install python-is-python3
    pip install sphinx sphinx-autobuild sphinx_rtd_theme rst2pdf
    # 安装结巴中文分词库，以支持中文搜索：
    pip install jieba3k
    # 安装中文字体：
    wget http://mirrors.ctan.org/fonts/fandol.zip
    unzip fandol.zip
    sudo cp -r fandol /usr/share/fonts/
    cp -r fandol ~/.fonts

2.编译文档：

.. code-block:: bash

    # 如果没有进行区域设置（locale），请先设置区域：
    export LC_ALL=en_US.UTF-8
    export LANG=en_US.UTF-8
    # 再运行编译：
    cmake --build . --target doc
    # 或者：
    make doc
    # 在doc/reference/build下即可以看到bmlib开发html和pdf格式的文档。


**如何从源码编译SoC版本：**


首先您需要编译SoC BSP，请参考BSP的编译指导。


.. 我们提供2种方式编译soc版本


**交叉编译环境方式：**


1.环境准备：

.. code-block:: bash

    # 参考步骤是在docker里编译，如果已经有Ubuntu18.04以上环境，也可以直接在本机编译。
    # 拷贝交叉编译工具链：
    mkdir -p ${path_to_local_workspace}
    cp -r ${path_to_gcc}/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu ${path_to_local_workspace}/

    # 拷贝libsophon源码：
    cp -r ${path_to_libsophon_repo}/libsophon ${path_to_local_workspace}/

    # 从SoC BSP编译目录拷贝linux-headers安装包：
    cp ${path_to_soc_bsp}/install/soc_*/bsp-debs/linux-headers-*.deb ${path_to_local_workspace}/
    mkdir -p ${path_to_local_workspace}/soc_kernel
    dpkg -x ${path_to_local_workspace}/linux-headers-*.deb ${path_to_local_workspace}/soc_kernel

    # 进入docker：
    docker pull ubuntu:focal
    sudo docker run -v ${path_to_local_workspace}:/workspace -it ubuntu:focal bash
    # 此时您应该得到了如下层级的目录，请注意如果您的路径与下面不同，请同步修改toolchain-aarch64-linux.cmake文件中的路径：
    /workspace
         |----gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu
         |----libsophon
         |----linux-headers-*.deb
         |----soc_kernel


2.编译来libsophon：

.. code-block:: bash

    # 安装依赖包。
    # 手动修改aarch64-linux-gnu-成6.3.1版本：
    cd /usr/bin/ && mkdir aarch64-bak
    mv aarch64-linux-gnu-* aarch64-bak
    ln -s /workspace/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-* .

    # 假定前面拷贝的linux-headers安装包名叫linux-headers-5.4.207-bm1684-ga2f7484bf21a.deb，设置环境变量：
    export header="linux-headers-5.4.207-bm1684-ga2f7484bf21a"

    cd /workspace/libsophon
    mkdir build && cd build
    cmake -DPLATFORM=soc -DSOC_LINUX_DIR=/workspace/soc_kernel/usr/src/${header}/ -DLIB_DIR=/workspace/libsophon/3rdparty/arm64/soc/ \
          -DCROSS_COMPILE_PATH=/absolute_path_to-gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu \
          -DCMAKE_TOOLCHAIN_FILE=/absolute_path_to_libsophon/toolchain-aarch64-linux.cmake \
          -DCMAKE_INSTALL_PREFIX=$PWD/../install ..

    make -j8
    make driver -j8
    make install -j8
.. make vpu_driver -j8
.. make jpu_driver
.. make package -j8

.. 过程中如果遇到下面的问题，按照提示操作执行即可：
..     cd /workspace/libsophon/bmvid/jpeg/driver/bmjpulite && /usr/bin/cmake -E chdir .. git checkout -- include/version.h
..     fatal: detected dubious ownership in repository at '/workspace/bmvid'
..     To add an exception for this directory, call:

..         git config --global --add safe.directory /workspace/bmvid


3.脚本安装:

.. code-block:: bash

  chmod +x libsophon_install.sh
  ./temp_install.sh {/path/to/libsophon-0.4.9}
.. ./install_libsophon.sh


**模型运行：**

.. code-block:: bash

  # bmodel模式下运行模型：
  .{/path/to/libsophon-0.4.9}/bin/bmrt_test --context_dir {/model_to_path/} --devid 0  &

把/path/to/libsophon-0.4.9，替换成自己的安装路径；把model_to_path替换成自己的bmodel模型路径。



.. **qemu环境编译方式：**


.. 从网络抓取构建Debian 9，进入qemu环境编译：

.. ::

..     sudo apt update
..     sudo apt-get install qemu-user-static debootstrap
..     mkdir debian-rootfs
..     cd debian-rootfs
..     sudo qemu-debootstrap --arch=arm64 stretch .

..     从SoC BSP编译目录copy linux-headers安装包：
..     sudo cp ${path_to_soc_bsp}/install/soc_bm1684/bsp-debs/linux-headers-*.deb .
..     由于我们编译需要拉取libsophon代码，请再copy您的如下文件：
..     sudo cp ~/.gitconfig ./root/
..     sudo cp -r ~/.ssh ./root/

..     sudo chroot . /bin/bash

.. 此时应该看到"/#"提示符了，之后的步骤都在这个qemu环境里进行，所有的文件操作都会保留在磁盘上。请务必确认chroot成功，以免后续操作对您的本机系统造成损坏。

.. 在qemu环境里继续安装依赖包：

.. ::

..     apt update
..     apt-get install -y \
..             build-essential \
..             git bc bison flex \
..             ninja-build \
..             pkg-config \
..             libncurses5-dev \
..             libgflags-dev \
..             libgtest-dev \
..             libssl-dev

..     把cmake 升级到3.13:
..     wget https://cmake.org/files/v3.13/cmake-3.13.2.tar.gz
..     tar xvf cmake-3.13.2.tar.gz
..     cd cmake-3.13.2
..     ./bootstrap --prefix=/usr
..     make
..     make install

.. 以上步骤只需要进行一次，以后再用到时只要chroot进来就可以了。

.. 接下来安装最开始时copy进来的linux-headers deb包（编译SoC版驱动需要）:

.. ::

..     假定前面拷贝到当前目录的linux-headers安装包名叫linux-headers-5.4.207-bm1684-ga2f7484bf21a.deb：
..     cd /
..     header="linux-headers-5.4.207-bm1684-ga2f7484bf21a"
..     dpkg -i ${header}.deb
..     cd /usr/src/${header}
..     rm ./scripts/mod/modpost
..     make prepare0
..     make scripts

.. 上面这个步骤只有第一次，或当kernel发生了不向前兼容的改动时才需要进行，记得更新linux-headers安装包。

.. 编译libsophon：

.. ::

..     cd /
..     git clone ssh://${your_name}@gerrit-ai.sophgo.vip:29418/libsophon
..     cd libsophon
..     git submodule update --init
..     mkdir build && cd build
..     cmake -DPLATFORM=soc -DSOC_LINUX_DIR=/usr/src/${header}/  -DCMAKE_INSTALL_PREFIX=$PWD/../install ..
..     make
..     make driver
..     make vpu_driver
..     make jpu_driver
..     make package

.. 最后用exit命令就可以退出qemu环境了。

.. **在 windows 下编译**

.. 1. 安装 Visual Studio 并将其加入 PATH;

.. 2. 安装 xxd 并将其加入 PATH;

.. 3. 拉取 bm_prebuilt_toolchains_win 至 libsophon 同级目录下，目录结构如下:

.. ::
..     .
..     ├── bm_prebuilt_toolchains_win
..     └── libsophon

.. 3. 在 CMD 中进入 libsophon 目录下进行编译:

.. ::
..     call build.bat release MT

.. 生成文件在 libsophon/release/libsophon 下，可使用以下命令对其进行打包:

.. ::
..     call build.bat release MT pack

.. 最后在 libsophon/release 下可获取 libsophon_win_x.x.x_arch.zip 形式的压缩包文件。
