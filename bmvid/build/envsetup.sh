#!/bin/bash

# README
# CHIP: bm1682, bm1880, bm1684
# SUBTYPE:
#     for bm1682:
#             asic: for EVB, single chip board and high density server and other 8g memory layout
#             *_4g: for 4g memory layout
#     for bm1880:
#             asic: for real chip
# PRODUCTFORM:
#     for bm1682:
#             soc: default, for soc products, includeing box and hds
#             pcie: pcie board
#     for bm1684:
#             soc: default, for soc products, including box and hds
#             pcie: pcie board in x86_64 server
#             pcie_arm64: pcie board in arm64 server
#             pcie_mips64: pcie board in mips server
#             pcie_sw64: pcie board in sunway server
#             pcie_loongarch64: pcie board in loong server
# BM_MEDIA_ION:
#     jpu/vpu used ion allocator
# ENABLE_BMCPU:
#     for bm1682: alway disable
#     for bm1684:
#             soc: always disable
#             pcie/pcie_arm64/pcie_mips64: default is enable to use arm cpu on pcie board
##

#You may set KERNEL_PATH here to avoid input later
#KERNEL_PATH=/home/your/dir/linux-linaro-stable/linux/bm1682_asic

export DEBUG=${DEBUG:-0}
export CHIP=${CHIP:-bm1682}
export SUBTYPE=${SUBTYPE:-asic}
export PRODUCTFORM=${PRODUCTFORM:-soc}
export BM_MEDIA_ION=${BM_MEDIA_ION:-0}

if [ ${PRODUCTFORM} = "soc" -o ${PRODUCTFORM} = "pcie_arm64" ]; then
    export ARCH=arm64
    export SUBARCH=arm64
else
    export ARCH=x86_64
    export SUBARCH=x86_64
fi

if [ $CHIP = bm1684 -a $PRODUCTFORM = soc ]; then
    export BM_MEDIA_ION=1
else
    export BM_MEDIA_ION=0
fi

function gettop
{
    local TOPFILE=build/envsetup.sh
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

#SYSTEM PATH
unset BMVID_TOP_DIR
unset BMVID_OUTPUT_DIR
unset PKG_CONFIG_PATH

export BMVID_TOP_DIR=$(gettop)
export BMVID_OUTPUT_DIR=$BMVID_TOP_DIR/install/${PRODUCTFORM}_${CHIP}_${SUBTYPE}

if [ "$PRODUCTFORM" = "soc" -o "$PRODUCTFORM" = "pcie_arm64" ]
then
    TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-gcc`) || {
    ret = $?
    echo "Add aarch64 linux toolchain to PATH!"
    return $ret
    }
    export CROSS_COMPILE=$TOOLCHAIN_PATH/aarch64-linux-gnu-
elif [ "$PRODUCTFORM" = "pcie_mips64" ]
then
    TOOLCHAIN_PATH=$(dirname `which mips-linux-gnu-gcc`) || {
    ret = $?
    echo "Add loong linux toolchain to PATH!"
    return $ret
    }
    export CROSS_COMPILE=${TOOLCHAIN_PATH}/mips-linux-gnu-
elif [ "$PRODUCTFORM" = "pcie_sw64" ]
then
    TOOLCHAIN_PATH=$(dirname `which sw_64-sunway-linux-gnu-gcc`) || {
    ret = $?
    echo "Add sunway linux toolchain to PATH!"
    return $ret
    }
    export CROSS_COMPILE=${TOOLCHAIN_PATH}/sw_64-sunway-linux-gnu-
elif [ "$PRODUCTFORM" = "pcie_loongarch64" ]
then
    TOOLCHAIN_PATH=$(dirname `which loongarch64-linux-gnu-gcc`) || {
    ret = $?
    echo "Add loongarch64 linux toolchain to PATH!"
    return $ret
    }
    export CROSS_COMPILE=${TOOLCHAIN_PATH}/loongarch64-linux-gnu-
else
    TOOLCHAIN_PATH=$(dirname `which gcc`) || {
    ret = $?
    echo "Add x86_64 linux toolchain to PATH!"
    return $ret
    }
    export CROSS_COMPILE=${TOOLCHAIN_PATH}/
fi

${BMVID_TOP_DIR}/build/version.sh

#print
echo "TOP = $BMVID_TOP_DIR"
echo "CHIP = $CHIP, SUBTYPE = $SUBTYPE, PRODUCTFORM = $PRODUCTFORM, DEBUG = $DEBUG, BM_MEDIA_ION = ${BM_MEDIA_ION}"

