#!/bin/bash

#You may set KERNEL_PATH here to avoid input later
#KENEL_PATH=/home/xun/projects/kernel/linux-linaro-stable/linux/bm1682_asic

DEBUG=0
CHIP=bm1682
#PLAT=palladium
PLAT=asic
#PLAT=fpga

function gettop
{
  local TOPFILE=build/envsetup_common.sh
  if [ -n "$TOP" -a -f "$TOP/$TOPFILE" ] ; then
    # The following circumlocution ensures we remove symlinks from TOP.
    (cd $TOP; PWD= /bin/pwd)
  else
    if [ -f $TOPFILE ] ; then
      # The following circumlocution (repeated below as well) ensures
      # that we record the true directory name and not one that is
      # faked up with symlink names.
      PWD= /bin/pwd
    else
      local HERE=$PWD
      T=
      while [ \( ! \( -f $TOPFILE \) \) -a \( $PWD != "/" \) ]; do
        \cd ..
        T=`PWD= /bin/pwd -P`
      done
      \cd $HERE
      if [ -f "$T/$TOPFILE" ]; then
        echo $T
      fi
    fi
  fi
}

function set_kernel_path()
{
  if [ ! -n "$KERNEL_PATH" -o ! -d "$KERNEL_PATH" ] ; then
     if [ -n "$1" -a -d "$1" ] ; then
       K=$1
     else
       read -p "input KERNEL_PATH:" K
     fi
  else
     K=$KERNEL_PATH
  fi
  if [ -f "$K/vmlinux" ] ; then
     echo $K
  fi
}

function build_vid_driver()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi
  if [ ! -d $OUTPUT_DIR/decode ] ; then
    mkdir -p $OUTPUT_DIR/decode
  fi
  export KERNEL_PATH=$(set_kernel_path $1)
  export ARCH=arm64
  export SUBARCH=arm64

  pushd $BMVID_PATH/driver/vdi/linux/driver
  make KERNELDIR=$KERNEL_PATH
  cp -f vpu.ko load.sh coda960.out picasso.bin $OUTPUT_DIR/decode
  popd
}

function clean_vid_driver()
{
  pushd $BMVID_PATH/driver/vdi/linux/driver
  make clean
  popd

  if [ -d  $OUTPUT_DIR/decode ] ; then
    rm -rf $OUTPUT_DIR/decode
  fi
}

function build_vid_lib()
{
  pushd $BMVID_PATH/driver
  make PRODUCT=boda OS=linux BUILD_ORG_TEST=yes CHIP=$CHIP SUBTYPE=$PLAT DEBUG=$DEBUG
  popd
}

function clean_vid_lib()
{
  pushd $BMVID_PATH/driver
  make clean
  popd
}


function build_boda_nonos_test()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi

  pushd $BMVID_PATH/test/bm1682_atf
  source ./build_common.sh
  set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 #case14
  
  if [ "$PLAT" = "palladium" ]; then
        echo "build for palladium"
        ./palladium_boda.sh  || return $?
  elif [ "$PLAT" = "asic" ]; then
        echo "build for asic"
        ./asic_boda.sh || return $?
  elif [ "$PLAT" = "fpga" ]; then
        echo "build for fpga"
        ./fpga_boda.sh || return $?
  fi

  cp -f build/bm1682_fvp/debug/bl31/bl31.elf $OUTPUT_DIR/${PLAT}_boda_debug_case14.elf
  
  popd 
}


function clean_boda_nonos_test()
{
  if [ -d  $OUTPUT_DIR ] ; then
    rm -f $OUTPUT_DIR/${PLAT}_boda_debug_case*.elf
  fi 
}

function build_wave_nonos_test()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi

  pushd $BMVID_PATH/test/bm1682_atf
  source ./build_common.sh
  set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0  #case64


  if [ "$PLAT" = "palladium" ]; then
        ./palladium_wave.sh  || return $?
  elif [ "$PLAT" = "asic" ]; then
        ./asic_wave.sh || return $?
  elif [ "$PLAT" = "fpga" ]; then
        ./fpga_wave.sh || return $?
  fi

  cp -f build/bm1682_fvp/debug/bl31/bl31.elf $OUTPUT_DIR/${PLAT}_wave_debug_case64.elf
  
  popd
}


function clean_wave_nonos_test()
{
  if [ -d  $OUTPUT_DIR ] ; then
    rm -f $OUTPUT_DIR/${PLAT}_wave_debug_case*.elf
  fi
}

function build_wave_linux_test()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi

  pushd $BMVID_PATH/driver
  make PRODUCT=wave OS=linux BUILD_ORG_TEST=yes SUBTYPE=$PLAT CHIP=$CHIP test
  cp -f release/w5_dec_test $OUTPUT_DIR/
  popd
}

function clean_wave_linux_test()
{
  pushd $BMVID_PATH/driver
  make clean
  popd

  if [ -d  $OUTPUT_DIR ] ; then
    rm -f $OUTPUT_DIR/w5_dec_test
  fi
}

function build_boda_linux_test()
{
  if [ ! -d $OUTPUT_DIR ] ; then
    mkdir -p $OUTPUT_DIR
  fi

  pushd $BMVID_PATH/driver
  make PRODUCT=boda OS=linux BUILD_ORG_TEST=yes SUBTYPE=$PLAT CHIP=$CHIP test
  cp -f release/boda960_dec_test $OUTPUT_DIR/
  popd
}

function clean_boda_linux_test()
{
  pushd $BMVID_PATH/driver
  make clean
  popd

  if [ -d  $OUTPUT_DIR ] ; then
    rm -f $OUTPUT_DIR/boda960_dec_test
  fi
}


function clean_all()
{
  clean_vid_driver 
  clean_vid_lib
  clean_boda_nonos_test
  clean_wave_nonos_test
  clean_boda_linux_test
  clean_wave_linux_test
}

#SYSTEM PATH
TOP_DIR=$(gettop)
OUTPUT_DIR=$TOP_DIR/install/soc_${CHIP}_${PLAT}

#SOURCE PATH
BMVID_PATH=$TOP_DIR

TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-gcc`) || {
	echo "Add aarch64 linux toolchain to PATH!" $? 
	return $?
	}
export CROSS_COMPILE=$TOOLCHAIN_PATH/aarch64-linux-gnu- 

#print
echo "TOP = $TOP_DIR"
echo "CHIP = $CHIP, DEBUG = $DEBUG PLAT = $PLAT"

