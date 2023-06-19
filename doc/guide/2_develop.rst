使用libsophon开发
------------------

.. |ver| replace:: 0.4.8

PCIE MODE
~~~~~~~~~~~~~~

在安装完libsophon后，推荐您使用cmake来将libsophon中的库链接到自己的程序中，可以在您程序的CMakeLists.txt中添加如下段落：

.. parsed-literal::

    find_package(libsophon REQUIRED)
    include_directories(${LIBSOPHON_INCLUDE_DIRS})

    add_executable(${YOUR_TARGET_NAME} ${YOUR_SOURCE_FILES})

    target_link_libraries(${YOUR_TARGET_NAME} ${the_libbmlib.so})
    上面${the_libbmlib.so}即表示要链接libbmlib.so这个库，其它库同理。


在您的代码中即可以调用libbmlib中的函数：

.. parsed-literal::

    #include <bmlib_runtime.h>
    #include <stdio.h>

    int main(int argc, char const *argv[])
    {
        bm_status_t ret;
        unsigned int card_num = 0;

        ret = bm_get_card_num(&card_num);
        printf("card number: %d/%d\n", ret, card_num);

        return 0;
    }


SOC MODE
~~~~~~~~~~~~~~

SOC 板卡上编译程序
^^^^^^^^^^^^^^^^

如果您希望在SOC模式的板子上运行的Linux环境下进行开发，则需要安装sophon-soc-libsophon-dev\_\ |ver|\ _arm64.deb工具包，使用以下命令安装。

.. parsed-literal::
    sudo dpkg -i sophon-soc-libsophon-dev\_\ |ver|\ _arm64.deb

安装完成后，您可以参考PCIE MODE的开发方法，使用cmake将libsophon中的库链接到自己的程序中。

x86 交叉编译程序
^^^^^^^^^^^^^^^^

如果您希望使用SOPHON SDK搭建交叉编译环境，您需要用到的是gcc-aarch64-linux-gnu工具链以及sophon-img relesae包里的libsophon_soc\_\ |ver|\ _aarch64.tar.gz。首先使用如下命令安装gcc-aarch64-linux-gnu工具链。

.. parsed-literal::

    sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

其次解压sophon-img relesae包里的libsophon_soc\_\ |ver|\ _aarch64.tar.gz。

.. parsed-literal::

    1. mkdir -p soc-sdk
    2. tar -zxf libsophon_soc\_\ |ver|\ _aarch64.tar.gz
    3. cp -rf libsophon_soc\_\ |ver|\ _aarch64/opt/sophon/libsophon-\ |ver|/lib  soc-sdk
    4. cp -rf libsophon_soc\_\ |ver|\ _aarch64/opt/sophon/libsophon-\ |ver|/include  soc-sdk

libsophon包含了bmrt,bmlib和bmcv的库。 opencv 和ffmpeg 相关的库在sophon-mw-soc\_\ |ver|\ _aarch64.tar.gz中，
其目录结构与libsophon类似 ，只需要拷贝lib和include下所有内容到soc-sdk即可。
如果需要使用第三方库，可以使用qemu在x86上构建虚拟环境安装，再将头文件和库文件拷贝到soc-sdk目录中，命令如下：

.. parsed-literal::

    构建qemu虚拟环境

    sudo apt-get install qemu-user-static
    mkdir rootfs
    cd rootfs

    构建Ubuntu 20.04的rootfs

    sudo qemu-debootstrap --arch=arm64 focal .
    sudo chroot . qemu-aarch64-static /bin/bash

    进入qemu后，安装软件，以gflag为例

    apt install software-properties-common
    add-apt-repository universe
    apt-get update
    apt-get install libgflags-dev

    通常情况下安装的so会在/usr/lib/aarch64-linux-gnu/libgflag* 下，只需要拷贝到soc-sdk/lib里即可。


下面以如下代码为例，介绍在您的代码中如何使用SOPHON SDK制作的soc-sdk交叉编译，以调用libbmlib中的函数：

.. parsed-literal::

    #include <bmlib_runtime.h>
    #include <stdio.h>

    int main(int argc, char const *argv[])
    {
        bm_status_t ret;
        unsigned int card_num = 0;

        ret = bm_get_card_num(&card_num);
        printf("card number: %d/%d\n", ret, card_num);

        return 0;
    }

首先按照如下步骤创建新的工作目录

.. parsed-literal::

    mkdir -p workspace && pushd workspace
    touch CMakeLists.txt
    touch get_dev_count.cpp

将上面的c++代码导入到get_dev_count.cpp中，在CMakeLists.txt中添加如下段落：

.. parsed-literal::

    cmake_minimum_required(VERSION 2.8)

    set(TARGET_NAME "test_bmlib")

    project(${TARGET_NAME} C CXX)

    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

    # 该demo链接了bmlib库，所以打开了BM_LIBS
    set(BM_LIBS bmlib bmrt)
    # 需要链接jpu相关的库可以打开JPU_LIBS
    # set(JPU_LIBS bmjpuapi bmjpulite)
    # 需要链接opencv相关的库可以打开OPENCV_LIBS
    # set(OPENCV_LIBS opencv_imgproc opencv_core opencv_highgui opencv_imgcodecs
            opencv_videoio)
    # 引入外部库，可以参看下面导入gflags的方法，打开EXTRA_LIBS
    # set(EXTRA_LIBS gflags)

    include_directories("${SDK}/include/")

    link_directories("${SDK}/lib/")

    set(src get_dev_count.cpp)
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    target_link_libraries(${target} ${BM_LIBS} pthread dl)
    # 未使用OPENCV和FFMPEG等库，所以不需要加下面的链接路径
    # target_link_libraries(${target} ${BM_LIBS} ${OPENCV_LIBS} ${FFMPEG_LIBS}
                                ${JPU_LIBS} ${EXTRA_LIBS} pthread dl)

接着使用cmake来构建程序。

.. parsed-literal::

    mkdir -p build && pushd build
    cmake -DSDK=/path_to_sdk/soc-sdk ..
    make

就可以在x86机器上编译出soc模式上运行的aarch64架构的程序。

上面例子只链接了bmlib的库，其它库如opencv，ffmpeg，其它lib同理。
