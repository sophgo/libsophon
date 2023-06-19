#!/bin/bash


BUILD_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
source $BUILD_DIR/envsetup_common.sh

#clean_vid_driver 
#build_vid_driver $1               || { echo "build vid_driver failed"; return $?; }
  
clean_vid_lib                     
build_vid_lib                     || { echo "build vid_lib failed";    return $?; }


for PLAT in palladium asic fpga; do
  clean_boda_nonos_test             
  build_boda_nonos_test             || { echo "build boda_nonos failed"; return $?; }

  clean_wave_nonos_test             
  build_wave_nonos_test             || { echo "build wave_nonos failed"; return $?; }
done

clean_boda_linux_test             
build_boda_linux_test             || { echo "build boda_linux failed"; return $?; }

clean_wave_linux_test
build_wave_linux_test             || { echo "build wave_linux failed"; return $?; } 
