#! /usr/bin/env bash
set -euo pipefail
this_dir=$(dirname $(readlink -f ${BASH_SOURCE[0]}))

cd $this_dir

make clean
for i in test/*; do 
  make TEST_CASE=$(basename $i) BOARD=ASIC RUN_ENV=DDR NPU_EN=0
done
