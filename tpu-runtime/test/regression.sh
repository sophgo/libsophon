#/bin/bash

SCRIPT_DIR=$(realpath `dirname ${BASH_SOURCE}`/../scripts)
source $SCRIPT_DIR/envsetup.sh

function test_bmrt_api() {
  pushd $SCRIPT_DIR/../test/
  source env.sh
  python3 python/main.py --archs "bm1684,bm1684x,bm1688"
  ret=$?
  popd
  return $ret
}

function main(){
  rebuild_tpu_runtime; ret=$?
  if [ $ret -ne 0 ]; then echo "rebuild_tpu_runtime failed"; return $ret; fi
  test_bmrt_api; ret=$?
  if [ $ret -ne 0 ]; then echo "test_bmrt_api regression failed"; return $ret; fi
}

main