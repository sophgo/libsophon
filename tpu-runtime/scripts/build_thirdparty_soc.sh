#!/bin/bash

#######################################
# required projects hierarchy
# .
# ├── libsophon(LIBSOPHON_DIR)
# │   ├── bmlib
# │   ├── tpu-bmodel
# │   ├── tpu-common
# │   ├── tpu-cpuop
# │   └── ...
# ├── tpu-runtime(RUNTIME_DIR)
# │   ├── scripts
# │   └── ...
# ├── TPU1684
# │   ├── ...
# │   └── ...
# └── TPU1686
#     ├── ...
#     └── ...

RUNTIME_DIR=$(realpath `dirname ${BASH_SOURCE}`/..)
THIRDPARTY_DIR=$RUNTIME_DIR/build_thirdparty
LIBSOPHON_DIR=$(realpath $RUNTIME_DIR/..)

PREBUILT_DIR=`cd $RUNTIME_DIR/../../bm_prebuilt_toolchains && pwd`
CROSS_TOOLCHAIN=$PREBUILT_DIR/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/
C_COMPILER=$CROSS_TOOLCHAIN/bin/aarch64-linux-gnu-gcc
CXX_COMPILER=$CROSS_TOOLCHAIN/bin/aarch64-linux-gnu-g++

build_soc_lib () {
    local TARGET_DIR=`cd $RUNTIME_DIR/../$1 && pwd`;
    pushd $TARGET_DIR;
    rm -rf build;
    mkdir build && cd build;
    cmake ../ -DPLATFORM=soc -DCMAKE_C_COMPILER=$C_COMPILER -DCMAKE_CXX_COMPILER=$CXX_COMPILER \
              -DPROJECT_VERSION_MAJOR=9 -DPROJECT_VERSION_MINOR=9 -DPROJECT_VERSION_PATCH=9 \
              -DCMAKE_PROJECT_VERSION=9.9.9
    ret=$?
    if [ $ret -eq 0 ]; then
        make -j $2;
        ret=$?
    fi
    popd
    if [ $ret -ne 0 ]; then
        echo "build $1 failed!"
    fi
    return $ret
}


mkdir -p $THIRDPARTY_DIR/bin
mkdir -p $THIRDPARTY_DIR/lib
mkdir -p $THIRDPARTY_DIR/include

# build dependent lib
if [ ! -d "$LIBSOPHON_DIR" ]; then
  echo "Please clone 'libsophon' project and its submodules outside tpu-runtime directory"
  exit -1
fi

cp $LIBSOPHON_DIR/bmlib/CMakeLists.txt CMakeLists.txt.tmp
sed -i '/SOVERSION/d' $LIBSOPHON_DIR/bmlib/CMakeLists.txt
build_soc_lib bmlib bmlib
if [ $ret -ne 0 ]; then exit $ret; fi
mv CMakeLists.txt.tmp $LIBSOPHON_DIR/bmlib/CMakeLists.txt

build_soc_lib tpu-bmodel
if [ $ret -ne 0 ]; then exit $ret; fi
build_soc_lib tpu-cpuop
if [ $ret -ne 0 ]; then exit $ret; fi

pushd $LIBSOPHON_DIR
cp -v bmlib/build/*.so* $THIRDPARTY_DIR/lib/
cp -v tpu-cpuop/build/*.so* $THIRDPARTY_DIR/lib/
cp -v tpu-bmodel/build/*.a $THIRDPARTY_DIR/lib/
cp -v tpu-bmodel/build/tpu_model $THIRDPARTY_DIR/bin
cp -v bmlib/include/bmlib_runtime.h $THIRDPARTY_DIR/include
cp -v bmlib/src/*.h $THIRDPARTY_DIR/include
cp -v tpu-common/base/* $THIRDPARTY_DIR/include
cp -r tpu-bmodel/include/* $THIRDPARTY_DIR/include

echo "libsophon: `git rev-parse HEAD`" > $THIRDPARTY_DIR/version.txt

cd tpu-bmodel
echo "tpu-bmodel: `git rev-parse HEAD`" >> $THIRDPARTY_DIR/version.txt
cd ../

cd tpu-cpuop
echo "tpu-cpuop: `git rev-parse HEAD`" >> $THIRDPARTY_DIR/version.txt
cd ../
popd

