Development using libsophon 
-------------------------------

PCIE MODE
~~~~~~~~~~~~~~

After libsophon is installed, it is recommended to use cmake to link the library in libsophon to your own program.
You can add the following paragraphs in CMakeLists.txt of your own program:

.. code-block:: python

    find_package(libsophon REQUIRED)
    include_directories(${LIBSOPHON_INCLUDE_DIRS})

    add_executable(${YOUR_TARGET_NAME} ${YOUR_SOURCE_FILES})

    target_link_libraries(${YOUR_TARGET_NAME} ${the_libbmlib.so})
    #The ${the_libbmlib.so} above means that the libbmlib.so library should be linked, and other libraries are the same.


In your code you can call functions in libbmlib:

.. code-block:: python

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

Compiling program on the SOC board
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to develop in Linux environment running on a SOC-mode board, you need to install the sophon-soc-libsophon-dev_0.4.1_arm64.deb toolkit, using the following command:

.. code-block:: shell

    sudo dpkg -i sophon-soc-libsophon-dev_0.4.1_arm64.deb

After installation, you can refer to the PCIE MODE development method and use cmake to link the libraries in libsophon to your own program.

x86 cross-compiling programs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to build cross-compiling environment using the SOPHON SDK, you will need the gcc-aarch64-linux-gnu toolchain and libsophon_soc_0.4.1_aarch64.tar.gz in libsophon relesae package.
Install the gcc-aarch64-linux-gnu toolchain with the following command:

.. code-block:: shell

    sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

Next, extract libsophon_0.4.1_aarch64.tar.gz from the libsophon relesae package.

.. code-block:: shell

    1. mkdir -p soc-sdk
    2. tar -zxf libsophon_soc_0.4.1_aarch64.tar.gz
    3. cp -rf libsophon_soc_0.4.1_aarch64/opt/sophon/libsophon-0.4.1/lib  soc-sdk
    4. cp -rf libsophon_soc_0.4.1_aarch64/opt/sophon/libsophon-0.4.1/include  soc-sdk

Libsophon contains bmrt, bmlib and bmcv libraries. 

Opencv and ffmpeg libraries are in sophon-mw-soc_0.4.1_aarch64.tar.gz, which the directory structure is similar to libsophon, just copy all the contents in lib and include to soc-sdk. 

If you need to use third-party libraries, you can use qemu to build a virtual environment on x86 and then copy the header and library files into the soc-sdk directory with the following command:

.. code-block:: shell

    # Build qemu virtual environment:

    sudo apt-get install qemu-user-static
    mkdir rootfs
    cd rootfs

    # Build Ubuntu 20.04 roofs:

    sudo qemu-debootstrap --arch=arm64 focal .
    sudo chroot . qemu-aarch64-static /bin/bash

    # After entering qemu, install the software.
    # gflag are used as an example:

    apt-get update
    apt-get install libgflags-dev

    # The installed ".so" files are usually under /usr/lib/aarch64-linux-gnu/libgflag*, just copy them to soc-sdk/lib.


Following code is an example of how to use the soc-sdk cross-compilation produced by SOPHON SDK in your code, to call functions in libbmlib:

.. code-block:: python

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

First create a new working directory as follows:

.. code-block:: shell

    mkdir -p workspace && pushd workspace
    touch CMakeLists.txt
    touch get_dev_count.cpp

Import the above c++ code into get_dev_count.cpp and add the following paragraph to CMakeLists.txt:

.. code-block:: shell

    cmake_minimum_required(VERSION 2.8)

    set(TARGET_NAME "test_bmlib")

    project(${TARGET_NAME} C CXX)

    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

    # This demo linked to bmlib library, so opened BM_LIBS.
    set(BM_LIBS bmlib bmrt)
    # You can open JPU_LIBS if you need to link jpu related libraries.
    # set(JPU_LIBS bmjpuapi bmjpulite)
    # You can open OPENCV_LIBS if you need to link opencv related libraries.
    # set(OPENCV_LIBS opencv_imgproc opencv_core opencv_highgui opencv_imgcodecs opencv_videoio)
    # To link external libraries, see the following method for importing gflags, and open EXTRA_LIBS.
    # set(EXTRA_LIBS gflags)

    include_directories("${SDK}/include/")

    link_directories("${SDK}/lib/")

    set(src get_dev_count.cpp)
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    target_link_libraries(${target} ${BM_LIBS})
    # Libraries such as OPENCV and FFMPEG are not used, so there is no need to add the following link path.
    # target_link_libraries(${target} ${BM_LIBS} ${OPENCV_LIBS} ${FFMPEG_LIBS} ${JPU_LIBS} ${EXTRA_LIBS} pthread)

Then build program using cmake:

.. code-block:: shell

    mkdir -p build && pushd build
    cmake -DSDK=/path_to_sdk/soc-sdk ..
    make

In this way, you can compile an aarch64 architecture program running in soc mode on an x86 machine.

the above example only links to the bmlib library, other libraries such as opencv, ffmpeg, and other libs are the same.
