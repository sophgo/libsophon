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
LIBSOPHON_DIR=$(realpath $RUNTIME_DIR/../libsophon)
if [ ! -d $LIBSOPHON_DIR ]; then
  LIBSOPHON_DIR=$(realpath $RUNTIME_DIR/..)
fi

build_dep_lib () {
    local TARGET_DIR=`cd $LIBSOPHON_DIR/$1 && pwd`;
    pushd $TARGET_DIR;
    rm -rf build;
    mkdir build && cd build;
    cmake ../ -DPLATFORM=cmodel -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=11 \
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
build_dep_lib bmlib bmlib
if [ $ret -ne 0 ]; then exit $ret; fi
mv CMakeLists.txt.tmp $LIBSOPHON_DIR/bmlib/CMakeLists.txt

build_dep_lib tpu-bmodel
if [ $ret -ne 0 ]; then exit $ret; fi
build_dep_lib tpu-cpuop
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

backend=""

if [ -d $LIBSOPHON_DIR/../TPU1684 ]; then
  pushd $LIBSOPHON_DIR/../TPU1684
  source scripts/envsetup.sh
  build_backend_lib_cmodel
  RET=$?; if [ $RET -ne 0 ]; then exit $RET; fi;
  cp -v ./build_runtime/c_model/libcmodel.so $THIRDPARTY_DIR/lib/libcmodel_bm1684.so
  echo "TPU1684: `git rev-parse HEAD`" >> $THIRDPARTY_DIR/version.txt
  backend="$backend bm1684"
  popd
fi

if [ -d $LIBSOPHON_DIR/../TPU1686 ]; then
  pushd $LIBSOPHON_DIR/../TPU1686
  CHIPS=(bm1684x bm1686 sg2260)
  for chip in ${CHIPS[@]}; do
  source scripts/envsetup.sh ${chip}
  rm -rf build
  rebuild_firmware_cmodel
  RET=$?; if [ $RET -ne 0 ]; then exit $RET; fi;
  cp -v ./build/firmware_core/libcmodel_firmware.so $THIRDPARTY_DIR/lib/libcmodel_${chip}.so
  backend="$backend $chip"
  done
  echo "TPU1686: `git rev-parse HEAD`" >> $THIRDPARTY_DIR/version.txt
  popd
fi

find $THIRDPARTY_DIR

echo "Build thirdparty lib with backend [${backend} ] done"
