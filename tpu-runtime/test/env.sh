#/bin/bash
export BMRT_TEST_TOP="$PWD"

function bmrt_set_libcmodel() {
  local runtime_arch=$1
  export TPUKERNEL_FIRMWARE_PATH=${BMRT_TEST_TOP}/lib/libcmodel_${runtime_arch}.so
}

#注意: 回归环境较老，更新的动态库需要在FTP/docker/bmnnsdk2_ubuntu16.04_py35.docker环境中编译
function bmrt_update_libcmodel() {
  local backend_path=$1
  local arch_name=$2
  local libpath=${BMRT_TEST_TOP}/lib
  pushd ${backend_path}
  local backened_name=${arch_name}
  if [ ${arch_name} == "bm1688" ]; then
    backened_name="bm1686"
  fi
  if [ ${arch_name} == "bm1690" ]; then
    backened_name="sg2260"
  fi
  source scripts/envsetup.sh ${backened_name}
  rebuild_firmware_cmodel
  cp ./build/firmware_core/libcmodel_firmware.so ${libpath}/libcmodel_${arch_name}.so
  popd
}

function bmrt_clean_test_env() {
  unset ${BMRT_TEST_TOP}
  unset ${TPUKERNEL_FIRMWARE_PATH}
}