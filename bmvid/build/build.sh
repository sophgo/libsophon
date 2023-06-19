#!/bin/bash
#set -x

function build_ion_lib()
{
    if [ $CHIP != bm1684 ]; then
        echo "For $CHIP, ignore build_ion_lib()"
        return 0
    fi

    MAKE_OPT="CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}"

    pushd ${BMVID_TOP_DIR}/allocator/ion
    make clean ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make install ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make clean ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

function clean_ion_lib()
{
    if [ $CHIP != bm1684 ]; then
        echo "For $CHIP, ignore clean_ion_lib()"
        return 0
    fi

    MAKE_OPT="CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}"

    pushd ${BMVID_TOP_DIR}/allocator/ion
    make uninstall ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make clean ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

function build_vpu_driver()
{
    if [ ! -n "$KERNEL_PATH" -o ! -d "$KERNEL_PATH" ] ; then
        echo "Invalid KERNEL_PATH. Ignore vpu driver building"
        return 0;
    fi
    echo "KERNEL_PATH: ${KERNEL_PATH}"

    if [ ! -d ${BMVID_OUTPUT_DIR} ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/drv ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/drv
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/lib/vpu_firmware ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/lib/vpu_firmware
    fi

    pushd ${BMVID_TOP_DIR}/video/driver/linux
    if [ ${PRODUCTFORM} = "soc" ]; then
        make KERNELDIR=$KERNEL_PATH CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG ION=${BM_MEDIA_ION}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        cp -f vpu.ko load.sh unload.sh ${BMVID_OUTPUT_DIR}/drv/
    fi
    if [ "$CHIP" = "bm1682" ]; then
        cp -f coda960.out picasso.bin ${BMVID_OUTPUT_DIR}/lib/vpu_firmware/
    elif [ "$CHIP" = "bm1880" ]; then
        cp -f coda960.out ${BMVID_OUTPUT_DIR}/lib/vpu_firmware/
    elif [ "$CHIP" = "bm1684" ]; then
        cp -f chagall_dec.bin chagall.bin ${BMVID_OUTPUT_DIR}/lib/vpu_firmware/
    else
        echo "can't copy firmware. please check it."
        popd
        return -1;
    fi

    popd

    return 0
}

function clean_vpu_driver()
{
    pushd ${BMVID_TOP_DIR}/video/driver/linux
    make CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG clean
    popd

    if [ -d  ${BMVID_OUTPUT_DIR}/drv ] ; then
        for f in vpu.ko load.sh unload.sh
        do
            rm -f ${BMVID_OUTPUT_DIR}/drv/${f}
        done
    fi
    rm -rf ${BMVID_OUTPUT_DIR}/lib/vpu_firmware
    return 0
}

function build_vpu_lib()
{
    if [ ! -d ${BMVID_OUTPUT_DIR} ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/lib ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/lib
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/bin ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/bin
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/include ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/include
    fi

    pushd ${BMVID_TOP_DIR}/video/provider/cnm

    if [ $CHIP = bm1684 ]; then
        PRODUCT=wave
    else
        PRODUCT=boda
    fi

    make PRODUCT=$PRODUCT OS=linux BUILD_ORG_TEST=no CHIP=$CHIP SUBTYPE=$SUBTYPE PRODUCTFORM=$PRODUCTFORM DEBUG=$DEBUG INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} ION=${BM_MEDIA_ION} ION_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make install-lib PRODUCT=$PRODUCT INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=${BMVID_OUTPUT_DIR}/
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make install-bin PRODUCT=$PRODUCT INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=${BMVID_OUTPUT_DIR}/bin/
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    if [ ${CHIP} = bm1684 ]; then
        MAKE_OPT="CHIP=${CHIP} PRODUCTFORM=${PRODUCTFORM} SUBTYPE=${SUBTYPE} ION=${BM_MEDIA_ION} DEBUG=${DEBUG} INSTALL_DIR=${BMVID_OUTPUT_DIR}"
        echo ${MAKE_OPT}

        if [ ${PRODUCTFORM} = soc ]; then
            pushd ${BMVID_TOP_DIR}/video/provider/cnm
            make -f Makefile.wave521c ${MAKE_OPT}
            if [ $? -ne 0 ]; then
                popd
                return -1
            fi

            make -f Makefile.wave521c install ${MAKE_OPT}
            if [ $? -ne 0 ]; then
                popd
                return -1
            fi
            popd
        fi

        pushd ${BMVID_TOP_DIR}/video/provider/cnm/encoder
        make ${MAKE_OPT} PRO=0
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        make install ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        popd

        pushd ${BMVID_TOP_DIR}/video/encoder/bm_enc_api
        make ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        make install ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        popd
    fi

    return 0
}

function clean_vpu_lib()
{
    pushd ${BMVID_TOP_DIR}/video/provider/cnm
    if [ $CHIP = bm1684 ]; then
        PRODUCT=wave
    else
        PRODUCT=boda
    fi

    make PRODUCT=$PRODUCT clean OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE}
    popd

    if [ -d  ${BMVID_OUTPUT_DIR}/lib ] ; then
        rm -f ${BMVID_OUTPUT_DIR}/lib/lib*video*.so
    fi

    if [ -d  ${BMVID_OUTPUT_DIR}/bin ] ; then
        for f in bm_test vpuinfo vpureset
        do
            rm -f ${BMVID_OUTPUT_DIR}/bin/${f}
        done
    fi

    if [ $CHIP = bm1684 ]; then
        MAKE_OPT="CHIP=${CHIP} PRODUCTFORM=${PRODUCTFORM} SUBTYPE=${SUBTYPE} ION=${BM_MEDIA_ION} DEBUG=${DEBUG} INSTALL_DIR=${BMVID_OUTPUT_DIR}"

        if [ ${PRODUCTFORM} = soc ]; then
            pushd ${BMVID_TOP_DIR}/video/provider/cnm
            make -f Makefile.wave521c clean ${MAKE_OPT}
            if [ $? -ne 0 ]; then
                popd
                return -1
            fi

            make -f Makefile.wave521c uninstall ${MAKE_OPT}
            if [ $? -ne 0 ]; then
                popd
                return -1
            fi
            popd
        fi

        pushd ${BMVID_TOP_DIR}/video/provider/cnm/encoder
        make clean ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        make uninstall ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        popd

        pushd ${BMVID_TOP_DIR}/video/encoder/bm_enc_api
        make clean ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        make uninstall ${MAKE_OPT}
        if [ $? -ne 0 ]; then
            popd
            return -1
        fi
        popd
    fi

    return 0
}

# build jpu driver and generate jpu.ko file
function build_jpu_driver()
{
    if [ ! -n "$KERNEL_PATH" -o ! -d "$KERNEL_PATH" ] ; then
        echo "Invalid KERNEL_PATH. Ignore vpu driver building"
        return 0;
    fi
    echo "KERNEL_PATH: ${KERNEL_PATH}"

    if [ ! -d ${BMVID_OUTPUT_DIR} ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/drv ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/drv
    fi

    pushd ${BMVID_TOP_DIR}/jpeg/driver/
    ./update_version.sh
    popd

    pushd ${BMVID_TOP_DIR}/jpeg/driver/jdi/linux/driver
    make KERNELDIR=$KERNEL_PATH CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG ION=$BM_MEDIA_ION
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    cp -f jpu.ko load_jpu.sh unload_jpu.sh ${BMVID_OUTPUT_DIR}/drv/
    popd

    pushd ${BMVID_TOP_DIR}/jpeg/driver/
    git checkout -- include/version.h
    popd

    return 0
}

function clean_jpu_driver()
{
    pushd ${BMVID_TOP_DIR}/jpeg/driver/jdi/linux/driver
    make CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG clean
    popd

    if [ -d  ${BMVID_OUTPUT_DIR}/drv ] ; then
        for f in jpu.ko load_jpu.sh unload_jpu.sh
        do
            rm -f ${BMVID_OUTPUT_DIR}/drv/${f}
        done
    fi
    return 0
}

function build_jpu_lib()
{
    if [ $CHIP != bm1684 ]; then
        echo "For ${CHIP}, ignore build_jpu_lib()"
        return 0
    fi
    if [ ! -d ${BMVID_OUTPUT_DIR}/bin ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/bin
    fi

    if [ ! -d ${BMVID_OUTPUT_DIR}/lib ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/lib
    fi

    if [ ! -d ${BMVID_OUTPUT_DIR}/include ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/include
    fi

    pushd ${BMVID_TOP_DIR}/jpeg/driver/
    ./update_version.sh
    popd

    # build bmjpulite
    pushd ${BMVID_TOP_DIR}/jpeg/driver/bmjpulite
    make clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make install CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    # build bmjpuapi
    pushd ${BMVID_TOP_DIR}/jpeg/driver/bmjpuapi
    make clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG ION=${BM_MEDIA_ION} INSTALL_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make install CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    pushd ${BMVID_TOP_DIR}/jpeg/driver/
    git checkout -- include/version.h
    popd

    return 0
}

function clean_jpu_lib()
{
    # clean bmjpulite
    pushd ${BMVID_TOP_DIR}/jpeg/driver/bmjpulite
    make clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    # uninstall bmjpulite
    make uninstall CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    # clean bmjpuapi
    pushd ${BMVID_TOP_DIR}/jpeg/driver/bmjpuapi
    make clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    # uninstall bmjpuapi
    make uninstall CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

# make and install bmvppapi
function build_bmvppapi()
{
    if [ $CHIP != bm1684 ]; then
        echo "For ${CHIP}, ignore build_bmvppapi()"
        return 0
    fi

    MAKE_OPT="CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}"

    pushd ${BMVID_TOP_DIR}/vpp/bmvppapi
    make clean ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make install ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

# clean and uninstall bmvppapi
function clean_bmvppapi()
{
    if [ $CHIP != bm1684 ]; then
        echo "For ${CHIP}, ignore clean_bmvppapi()"
        return 0
    fi

    MAKE_OPT="CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=${BMVID_OUTPUT_DIR}"

    pushd ${BMVID_TOP_DIR}/vpp/bmvppapi
    make clean ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make uninstall ${MAKE_OPT}
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

function build_vpp_lib()
{
    clean_vpp_lib || return $?

    if [ ! -d ${BMVID_OUTPUT_DIR}/vpp/bin ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/vpp/bin
    fi

    if [ ! -d ${BMVID_OUTPUT_DIR}/vpp/lib ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/vpp/lib
    fi

    if [ ! -d ${BMVID_OUTPUT_DIR}/vpp/include ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/vpp/include
    fi

    pushd ${BMVID_TOP_DIR}/vpp/driver/

    make -f makefile_lib.mak clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    if [ ${PRODUCTFORM} = "soc" ]; then
        make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE}
    elif [ ${PRODUCTFORM} = "pcie_mips64" ]; then
        make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} BUILD_CONFIGURATION=mipsLinux
    elif [ ${PRODUCTFORM} = "pcie_sw64" ]; then
        make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} BUILD_CONFIGURATION=sunwayLinux
    elif [ ${PRODUCTFORM} = "pcie_loongarch64" ]; then
        make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} BUILD_CONFIGURATION=loongLinux
    elif [ ${PRODUCTFORM} = "pcie_arm64" ]; then
        make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} BUILD_CONFIGURATION=arm64Linux
    else
        make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} BUILD_CONFIGURATION=x86Linux
    fi

    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make -f makefile_lib.mak install OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=${BMVID_OUTPUT_DIR}/vpp
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

function clean_vpp_lib()
{
    pushd ${BMVID_TOP_DIR}/vpp/driver/
    make -f makefile_lib.mak clean CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG
    popd

    if [ -d  ${BMVID_OUTPUT_DIR}/vpp/include ] ; then
        rm -f ${BMVID_OUTPUT_DIR}/vpp/include/vppion.h
        rm -f ${BMVID_OUTPUT_DIR}/vpp/include/vpplib.h
    fi

    if [ -d  ${BMVID_OUTPUT_DIR}/vpp/lib ] ; then
        rm -f ${BMVID_OUTPUT_DIR}/vpp/lib/libvpp.a
    fi

    if [ -d  ${BMVID_OUTPUT_DIR}/vpp/bin ] ; then
        for f in bm_vpp_test bm_vpp_resize
        do
            rm -f ${BMVID_OUTPUT_DIR}/vpp/bin/${f}
        done
    fi

    return 0
}

function build_vpp_app()
{
    clean_vpp_app || return $?

    if [ ! -d ${BMVID_OUTPUT_DIR}/vpp/bin ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/vpp/bin
    fi

    pushd ${BMVID_TOP_DIR}/vpp/test

    make -f Makefile CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} VPP_LIB_DIR=${BMVID_TOP_DIR}/vpp/driver  productform=soc
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make -f Makefile install CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=${BMVID_OUTPUT_DIR}/vpp/bin VPP_LIB_DIR=${BMVID_TOP_DIR}/vpp/driver  productform=soc
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    popd

    return 0
}

function clean_vpp_app()
{
    pushd ${BMVID_TOP_DIR}/vpp/test

    make -f Makefile clean CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=${BMVID_OUTPUT_DIR}/vpp/bin VPP_LIB_DIR=${BMVID_TOP_DIR}/vpp/driver productform=${PRODUCTFORM}

    popd
}

function build_vpp_lib_pcie()
{
    clean_vpp_lib || return $?

    if [ ! -d $BMVID_OUTPUT_DIR/vpp/bin ]; then
        mkdir -p $BMVID_OUTPUT_DIR/vpp/bin
    fi

    if [ ! -d $BMVID_OUTPUT_DIR/vpp/lib ]; then
        mkdir -p $BMVID_OUTPUT_DIR/vpp/lib
    fi

    if [ ! -d $BMVID_OUTPUT_DIR/vpp/include ]; then
        mkdir -p $BMVID_OUTPUT_DIR/vpp/include
    fi

    pushd $BMVID_TOP_DIR/vpp/driver/

    make -f makefile_lib.mak clean
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make -f makefile_lib.mak CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} BUILD_CONFIGURATION=x86Linux
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    make -f makefile_lib.mak install OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=$BMVID_OUTPUT_DIR/vpp/
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd
    return 0
}

function build_vpp_app_pcie()
{
    clean_vpp_app || return $?

    if [ ! -d $BMVID_OUTPUT_DIR/vpp/bin ]; then
        mkdir -p $BMVID_OUTPUT_DIR/vpp/bin
    fi

    pushd $BMVID_TOP_DIR/vpp/test

    make -f Makefile CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} VPP_LIB_DIR=$BMVID_TOP_DIR/vpp/driver productform=pcie
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    make -f Makefile install CHIP=$CHIP SUBTYPE=$SUBTYPE DEBUG=$DEBUG INSTALL_DIR=release/${PRODUCTFORM}_${CHIP}_${SUBTYPE} OBJDIR=obj/${PRODUCTFORM}_${CHIP}_${SUBTYPE} DESTDIR=$BMVID_OUTPUT_DIR/vpp/bin VPP_LIB_DIR=$BMVID_TOP_DIR/vpp/driver productform=pcie
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi

    popd

    return 0
}

function build_bmvid()
{
    build_ion_lib    || return $?

    if [ ${PRODUCTFORM} = "soc" ]; then
        build_vpu_driver || return $?
    fi
    build_vpu_lib    || return $?

    if [ ${PRODUCTFORM} = "soc" ]; then
        build_jpu_driver || return $?
    fi
    build_jpu_lib    || return $?

    build_vpp_lib    || return $?

    if [ ${PRODUCTFORM} = "soc" ]; then
        build_vpp_app    || return $?
    #else
        # TODO depend on external bmcv
        #build_vpp_lib_pcie    || return $?
        #build_vpp_app_pcie    || return $?
    fi

    build_bmvppapi   || return $?

    build_libyuv     || return $?

    build_bmcv_lib   || return $?

    build_bmcv_test   || return $?
}

function clean_bmvid()
{
    clean_ion_lib    || return $?

    if [ ${PRODUCTFORM} = "soc" ]; then
        clean_vpu_driver || return $?
    fi
    clean_vpu_lib    || return $?

    if [ ${PRODUCTFORM} = "soc" ]; then
        clean_jpu_driver || return $?
    fi
    clean_jpu_lib    || return $?

    clean_bmvppapi   || return $?

    if [ ${PRODUCTFORM} = "soc" ]; then
        clean_vpp_lib    || return $?
        clean_vpp_app    || return $?
    fi

    clean_libyuv     || return $?

    clean_bmcv_lib   || return $?

    clean_bmcv_test   || return $?
}

function cmake_libyuv()
{
    if [ ${PRODUCTFORM} = "pcie" ]; then
        CMAKE_TOOLCHAIN=../x86_toolchain/x86_64.toolchain.cmake
        ENABLE_NEON=OFF
    else
        CMAKE_TOOLCHAIN=../toolchain/aarch64-gnu.toolchain.cmake
        ENABLE_NEON=ON
    fi

    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
          -DENABLE_NEON=${ENABLE_NEON} \
          -DTEST_OPT=$1 \
          -DCMAKE_INSTALL_PREFIX=${BMVID_OUTPUT_DIR} \
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN} .. \
          -DCMAKE_MAKE_PROGRAM=make
  if [ $? -ne 0 ]; then
      return -1
  fi
  return 0
}

function build_libyuv_internal()
{
  if [ ! -d ${BMVID_TOP_DIR}/3rdparty/libyuv ] ;then
    echo "no libyuv source"
    return -1
  fi
  if [ ! -d ${BMVID_OUTPUT_DIR} ] ; then
    mkdir -p ${BMVID_OUTPUT_DIR}
  fi

  pushd ${BMVID_TOP_DIR}/3rdparty/libyuv
  if [ ! -f build/cmake_install.cmake ]; then
    if [ $DEBUG -eq 1 ]; then
        BUILD_TYPE=DEBUG
    else
        BUILD_TYPE=RELEASE
    fi

    rm -rf build/*;
    mkdir -p build;

    pushd build
    cmake_libyuv $1
    ret=$?
    popd
    if [ $ret -ne 0 ]; then
        popd
        return $ret;
    fi
  fi

  pushd build
  make VERBOSE=1 install -j`nproc`
  ret=$?
  popd
  if [ $ret -ne 0 ]; then
      popd
      return $ret;
  fi

  if [ ! -d ${BMVID_OUTPUT_DIR}'/test' ] ; then
    mkdir -p ${BMVID_OUTPUT_DIR}'/test'
  fi
  cp -f build/yuvconvert ${BMVID_OUTPUT_DIR}'/test'

  popd

  return 0
}

function build_libyuv()
{
    build_libyuv_internal OFF
    return $?
}

function build_libyuvtest()
{
   if [ ! -d ${BMVID_TOP_DIR}'/3rdparty/3rdparty-tools/googletest/googletest' ] ;then
       pushd $BMVID_TOP_DIR
       git submodule init
       git submodule update
       popd
   fi
   clean_libyuv
   build_libyuv_internal ON
   return $?
}

function clean_libyuv()
{
  pushd ${BMVID_TOP_DIR}/3rdparty/libyuv
  rm -rf build/*
  popd

  if [ -d ${BMVID_OUTPUT_DIR}  ]; then
    rm -rf ${BMVID_OUTPUT_DIR}/lib/libyuv.*
    rm -rf ${BMVID_OUTPUT_DIR}/include/libyuv.*
  fi
  return 0
}

function build_bmcv_lib()
{
    #if [ $CHIP != bm1684 -a $CHIP != bm1686 ]; then
    #    echo "For ${CHIP}, ignore build_bmcv_lib()"
    #    return 0
    #fi

#    better to execute those dependency manually
#    build_ion_lib || return $?
#    build_jpu_lib || return $?
#    build_vpp_lib || return $?

    if [ -n "$1" ]; then
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="USING_CMODEL=$1 OUT_DIR=${BMCV_OUTPUT_DIR}"

    else
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="OUT_DIR=${BMCV_OUTPUT_DIR}"
    fi

    MAKE_OPT="$MAKE_OPT CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG BMCV_ROOT=${BMVID_TOP_DIR}"

    BMCPU_TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-g++`) || {
            ret=$?
            echo "Add aarch64 linux toolchain to PATH!"
            return $ret
        }
    BMCPU_CROSS_COMPILE=${BMCPU_TOOLCHAIN_PATH}/aarch64-linux-gnu-g++


    if [ ! -d $BMCV_OUTPUT_DIR/bmcv/lib ]; then
        mkdir -p $BMCV_OUTPUT_DIR/bmcv/lib
    fi

    if [ ! -d $BMCV_OUTPUT_DIR/bmcv/include ]; then
        mkdir -p $BMCV_OUTPUT_DIR/bmcv/include
    fi

    pushd ${BMVID_TOP_DIR}/bmcv
    make bmcv ${MAKE_OPT} BMCPU_CROSS_COMPILE=$BMCPU_CROSS_COMPILE
    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

function clean_bmcv_lib()
{
    if [ $CHIP != bm1684 -a $CHIP != bm1686 ]; then
        echo "For ${CHIP}, ignore build_bmcv_lib()"
        return 0
    fi

#    clean_ion_lib || return ?
#    clean_jpu_lib || return ?
#    clean_vpp_lib || return ?

    if [ -n "$1" ]; then
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="USING_CMODEL=$1 OUT_DIR=${BMCV_OUTPUT_DIR}"
    else
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="OUT_DIR=${BMCV_OUTPUT_DIR}"
    fi
    MAKE_OPT="$MAKE_OPT CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG BMCV_ROOT=${BMVID_TOP_DIR}"

    BMCPU_TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-g++`) || {
            ret=$?
            echo "Add aarch64 linux toolchain to PATH!"
            return $ret
        }
    BMCPU_CROSS_COMPILE=${BMCPU_TOOLCHAIN_PATH}/aarch64-linux-gnu-g++

    pushd ${BMVID_TOP_DIR}/bmcv
    make clean ${MAKE_OPT} BMCPU_CROSS_COMPILE=$BMCPU_CROSS_COMPILE

    if [ $? -ne 0 ]; then
        @popd
        return -1
    fi
    popd

    return 0
}

function build_bmcv_cmodel()
{
    if [ ! -d ${BMVID_OUTPUT_DIR}/bmcv/ ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/bmcv/
    fi

    if [ ! -d ${BMVID_OUTPUT_DIR}/bmcv/lib/ ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/bmcv/lib/
    fi

    if [ ! -d ${BMVID_OUTPUT_DIR}/bmcv/lib/cmodel/ ] ; then
        mkdir -p ${BMVID_OUTPUT_DIR}/bmcv/lib/cmodel/
    fi

    build_bmcv_lib 1 || return $?
}

function clean_bmcv_cmodel()
{
    clean_bmcv_lib 1 || return $?
}

function build_bmcv_test()
{
    if [ $CHIP != bm1684 -a $CHIP != bm1686 ]; then
        echo "For ${CHIP}, ignore build_bmcv_lib()"
        return 0
    fi

#    better to execute those dependency manually
#    build_ion_lib || return $?
#    build_jpu_lib || return $?
#    build_vpp_lib || return $?

    if [ -n "$1" ]; then
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="USING_CMODEL=$1 OUT_DIR=${BMCV_OUTPUT_DIR}"

    else
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="OUT_DIR=${BMCV_OUTPUT_DIR}"
    fi

    BMCPU_TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-g++`) || {
            ret=$?
            echo "Add aarch64 linux toolchain to PATH!"
            return $ret
        }
    BMCPU_CROSS_COMPILE=${BMCPU_TOOLCHAIN_PATH}/aarch64-linux-gnu-g++

    MAKE_OPT="$MAKE_OPT CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG BMCV_ROOT=${BMVID_TOP_DIR}"

    pushd ${BMVID_TOP_DIR}/bmcv
    make test_bmcv ${MAKE_OPT} BMCPU_CROSS_COMPILE=$BMCPU_CROSS_COMPILE -j`nproc`

    if [ $? -ne 0 ]; then
        popd
        return -1
    fi
    popd

    return 0
}

function clean_bmcv_test()
{
    if [ $CHIP != bm1684 -a $CHIP != bm1686 ]; then
        echo "For ${CHIP}, ignore build_bmcv_lib()"
        return 0
    fi

#    clean_ion_lib || return ?
#    clean_jpu_lib || return ?
#    clean_vpp_lib || return ?

    if [ -n "$1" ]; then
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="USING_CMODEL=$1 OUT_DIR=${BMCV_OUTPUT_DIR}"
    else
        BMCV_OUTPUT_DIR=${BMVID_OUTPUT_DIR}
        MAKE_OPT="OUT_DIR=${BMCV_OUTPUT_DIR}"
    fi
    MAKE_OPT="$MAKE_OPT CHIP=$CHIP PRODUCTFORM=$PRODUCTFORM SUBTYPE=$SUBTYPE DEBUG=$DEBUG BMCV_ROOT=${BMVID_TOP_DIR}"

    BMCPU_TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-g++`) || {
            ret=$?
            echo "Add aarch64 linux toolchain to PATH!"
            return $ret
        }
    BMCPU_CROSS_COMPILE=${BMCPU_TOOLCHAIN_PATH}/aarch64-linux-gnu-g++

    pushd ${BMVID_TOP_DIR}/bmcv
    make clean_test ${MAKE_OPT} BMCPU_CROSS_COMPILE=$BMCPU_CROSS_COMPILE

    if [ $? -ne 0 ]; then
        @popd
        return -1
    fi
    popd

    return 0
}

if [ -z ${BMVID_OUTPUT_DIR} ]; then
    echo "warning: source envsetup_sh at first!!!!!"
    exit 0
fi

if [ -z $PRODUCTFORM ]; then
    PRODUCTFORM=soc
fi

if [ "$1" = "build_ion_lib" ]; then
   build_ion_lib
   exit $?
fi

if [ "$1" = "clean_ion_lib" ]; then
   clean_ion_lib
   exit $?
fi

if [ "$1" = "build_jpu_driver" ]; then
   build_jpu_driver
   exit $?
fi

if [ "$1" = "clean_jpu_driver" ]; then
   clean_jpu_driver
   exit $?
fi

if [ "$1" = "build_jpu_lib" ]; then
   build_jpu_lib
   exit $?
fi

if [ "$1" = "clean_jpu_lib" ]; then
   clean_jpu_lib
   exit $?
fi

if [ "$1" = "build_vpu_driver" ]; then
   build_vpu_driver
   exit $?
fi

if [ "$1" = "clean_vpu_driver" ]; then
   clean_vpu_driver
   exit $?
fi

if [ "$1" = "build_vpu_lib" ]; then
   build_vpu_lib
   exit $?
fi

if [ "$1" = "clean_vpu_lib" ]; then
   clean_vpu_lib
   exit $?
fi

if [ "$1" = "build_vpp_lib" ]; then
   build_vpp_lib
   exit $?
fi

if [ "$1" = "clean_vpp_lib" ]; then
   clean_vpp_lib
   exit $?
fi

if [ "$1" = "build_bmvppapi" ]; then
   build_bmvppapi
   exit $?
fi

if [ "$1" = "clean_bmvppapi" ]; then
   clean_bmvppapi
   exit $?
fi

if [ "$1" = "build_libyuv" ]; then
   build_libyuv
   exit $?
fi

if [ "$1" = "build_libyuvtest" ]; then
   build_libyuvtest
   exit $?
fi

if [ "$1" = "clean_libyuv" ]; then
   clean_libyuv
   exit $?
fi

if [ "$1" = "build_vpp_app" ]; then
   build_vpp_app
   exit $?
fi

if [ "$1" = "clean_vpp_app" ]; then
   clean_vpp_app
   exit $?
fi

if [ "$1" = "build_vpp_lib_pcie" ]; then
   build_vpp_lib_pcie
   exit $?
fi

if [ "$1" = "build_bmvid" ]; then
   build_bmvid
   exit $?
fi

if [ "$1" = "build_vpp_app_pcie" ]; then
   build_vpp_app_pcie
   exit $?
fi

if [ "$1" = "build_bmcv_lib" ]; then
    build_bmcv_lib
    exit $?
fi

if [ "$1" = "clean_bmcv_lib" ]; then
    clean_bmcv_lib
    exit $?
fi

if [ "$1" = "build_bmcv_cmodel" ]; then
    build_bmcv_cmodel
    exit $?
fi

if [ "$1" = "clean_bmcv_cmodel" ]; then
    clean_bmcv_cmodel
    exit $?
fi

if [ "$1" = "build_bmcv_test" ]; then
    build_bmcv_test
    exit $?
fi

if [ "$1" = "clean_bmcv_test" ]; then
    clean_bmcv_test
    exit $?
fi

echo "Usage:"
echo "  $0 build_jpu_driver     -- build jpu driver"
echo "  $0 clean_jpu_driver     -- clean jpu driver"
echo "  $0 build_jpu_lib        -- build jpu lib"
echo "  $0 clean_jpu_lib        -- clean jpu lib"
echo "  $0 build_vpu_driver     -- build vpu driver"
echo "  $0 clean_vpu_driver     -- clean vpu driver"
echo "  $0 build_vpu_lib        -- build vpu lib"
echo "  $0 clean_vpu_lib        -- clean vpu lib"
echo "  $0 build_bmvid          -- build bmvid"
echo "  $0 clean_bmvid          -- clean bmvid"

echo "  $0 build_libyuv         -- build libyuv"
echo "  $0 build_libyuvtest     -- build libyuv with test"
echo "  $0 clean_libyuv         -- clean libyuv"

echo "  $0 build_vpp_lib        -- build vpp lib"
echo "  $0 clean_vpp_lib        -- clean vpp lib"
echo "  $0 build_vpp_app        -- build vpp app"
echo "  $0 clean_vpp_app        -- clean vpp app"
echo "  $0 build_vpp_lib_pcie   -- build vpp lib for pcie mode"
echo "  $0 build_vpp_app_pcie   -- build vpp app for pcie mode"
if [ $CHIP = bm1684 ]; then
    echo "  $0 build_ion_lib        -- build ion lib"
    echo "  $0 clean_ion_lib        -- clean ion lib"
    echo "  $0 build_bmvppapi       -- build libbmvppapi"
    echo "  $0 clean_bmvppapi       -- clean libbmvppapi"
fi
echo "  $0 build_bmcv_lib       -- build bmcv lib"
echo "  $0 build_bmcv_cmodel    -- build bmcv lib for cmodel"
echo "  $0 clean_bmcv_lib       -- clean bmcv lib"
echo "  $0 clean_bmcv_cmode     -- clean bmcv lib for cmodel"
echo "  $0 build_bmcv_test      -- build bmcv test case"
echo "  $0 clean_bmcv_test      -- clean bmcv test case"
