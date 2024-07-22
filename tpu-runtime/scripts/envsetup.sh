#/bin/bash

SCRIPT_DIR=`dirname $(realpath ${BASH_SOURCE})`
RUNTIME_DIR=$(realpath $SCRIPT_DIR/..)
export THIRDPARTY_DIR=$RUNTIME_DIR/build_thirdparty

PREBUILT_DIR=`cd $RUNTIME_DIR/../../bm_prebuilt_toolchains && pwd`
CROSS_TOOLCHAIN=$PREBUILT_DIR/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/
C_COMPILER=$CROSS_TOOLCHAIN/bin/aarch64-linux-gnu-gcc
CXX_COMPILER=$CROSS_TOOLCHAIN/bin/aarch64-linux-gnu-g++

function build_tpu_runtime_soc() {
  pushd $RUNTIME_DIR
  rm -rf build
  mkdir build && cd build
  ONLY_TEST=1 cmake ../ -DCMAKE_C_COMPILER=$C_COMPILER  -DCMAKE_CXX_COMPILER=$CXX_COMPILER -DCMAKE_BUILD_TYPE=Release $EXTRA_CONFIG
  make -j$((`nproc`-1))
  RET=$?; if [ $RET -ne 0 ]; then popd; return $RET; fi
  popd
}


function rebuild_tpu_runtime_soc() {
  $SCRIPT_DIR/build_thirdparty_soc.sh
  RET=$?; if [ $RET -ne 0 ]; then popd; return $RET; fi
  build_tpu_runtime_soc
}

function build_tpu_runtime() {
  pushd $RUNTIME_DIR
  rm -rf build
  mkdir build && cd build
  ONLY_TEST=1 cmake ../ -DCMAKE_BUILD_TYPE=Release $EXTRA_CONFIG
  make -j$((`nproc`-1))
  RET=$?; if [ $RET -ne 0 ]; then popd; return $RET; fi
  popd
}

function rebuild_tpu_runtime(){
  $SCRIPT_DIR/build_thirdparty.sh
  RET=$?; if [ $RET -ne 0 ]; then return $RET; fi
  build_tpu_runtime
}

function show_bmodel_chip() {
  local bmodel_name="$1"
  local chip_str=`$THIRDPARTY_DIR/bin/tpu_model --info ${bmodel_name} | grep chip`
  local chip_name=${chip_str##* }
  echo "${chip_name,,}"
}

function cmodel_run_bmodel() {
  local bmodel_name="$1"
  if [ -z "$bmodel_name" ]; then
    echo "bmodel name should not be null!"
    return -1
  fi
  local bmodel_args="--bmodel"
  if [ -d "$bmodel_name" ]; then
    bmodel_args="--context_dir"
    bmodel_name="$bmodel_name/compilation.bmodel"
    if [ ! -e "$bmodel_name/output_ref_data.dat" ]; then
      bmodel_args="--compare 0 --context_dir"
    fi
  fi
  local chip=`show_bmodel_chip ${bmodel_name}`
  if [ $chip = 'bm1688' ]; then
    chip="bm1686"
  elif [ $chip = 'bm1690' ]; then
    chip="sg2260"
  fi
  TPUKERNEL_FIRMWARE_PATH=$THIRDPARTY_DIR/lib/libcmodel_${chip}.so $EXTRA_EXEC $RUNTIME_DIR/build/bmrt_test $bmodel_args $*
}

function cmodel_batch_run_bmodel() {
  local bmodel_dir="$1"
  local all_bmodels=`find  "$bmodel_dir" -name *.bmodel`
  for bmodel_name in ${all_bmodels[@]}; do
    if [ "`basename $bmodel_name`" == "compilation.bmodel" ]; then
        bmodel_name=`dirname $bmodel_name`
    fi
    cmodel_run_bmodel $bmodel_name
    RET=$?; if [ $RET -ne 0 ]; then return $RET; fi
  done
}

function update_firmware(){
    CHIP_ARCH=${1:-bm1684x}
    TPU1686_PATH=${2:-$RUNTIME_DIR/../../TPU1686}
    FIRMWARE_PATH=$RUNTIME_DIR/lib
    pushd $TPU1686_PATH
    source scripts/envsetup.sh $CHIP_ARCH
    rebuild_firmware
    LIB_SUFFIX=${CHIP_ARCH}
    if [ "${CHIP_ARCH}" == "bm1686" ]; then
        LIB_SUFFIX="tpulv60"
    fi
    LIBNAME=lib${LIB_SUFFIX}_kernel_module
    cp -v build/firmware_core/libfirmware_core.so ${FIRMWARE_PATH}/${LIBNAME}.so
    echo "TPU1686: `git rev-parse HEAD`" > ${FIRMWARE_PATH}/${LIBNAME}_version.txt
    echo "updated: `date`" >> ${FIRMWARE_PATH}/${LIBNAME}_version.txt
    popd
    cat ${FIRMWARE_PATH}/${LIBNAME}_version.txt
}

