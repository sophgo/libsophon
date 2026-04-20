export CHIP_ARCH=${1:-bm1684x}
export PS1="\[\e[1;35m\](${CHIP_ARCH}):\[\e[1;33m\]\w\[\e[1;34m\]\$ \[\e[0m\]"

PROJECT_ROOT=$(cd $(dirname ${BASH_SOURCE})/../; pwd)

function rebuild() {
  pushd ${PROJECT_ROOT} > /dev/null
  rm -rf build || return $?
  cmake -S . -B build -DTOOLCHAIN=$1 || return $?
  cmake --build build -j8 || return $?
  popd > /dev/null
}

function run_regression() {
  local chip_variants=(
    "bm1684x"
    "bm1688"
  )
  local cv184x_toolchains=(
    "musl_arm"
    "musl_arm64"
    "glibc_arm"
    "glibc_arm64"
  )

  # Build standalone chips
  for chip in "${chip_variants[@]}"; do
    echo "Building $chip..."
    CHIP_ARCH="$chip" rebuild || return $?
  done

  # Build cv184x with multiple toolchains
  echo "Building cv184x variants..."
  for toolchain in "${cv184x_toolchains[@]}"; do
    echo "Building cv184x with $toolchain..."
    CHIP_ARCH="cv184x" rebuild "$toolchain" || return $?
  done
}