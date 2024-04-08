#/bin/bash

SCRIPT_DIR=$(realpath `dirname ${BASH_SOURCE}`/../scripts)
source $SCRIPT_DIR/envsetup.sh

function main(){
  rebuild_tpu_runtime
}

main
