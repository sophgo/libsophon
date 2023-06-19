#!/bin/bash
# This script is called in Jenkins.
# source build/build_check.sh

#set -x

BUILD_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
echo ${BUILD_DIR}

BMVID_BRANCH=$(git branch)
#BMVID_BRANCH=$(git rev-parse --abbrev-ref HEAD)
#BMVID_BRANCH=$(git branch | grep \* | cut -d ' ' -f2)
echo "On branch ${BMVID_BRANCH}"

#case ${BMVID_BRANCH} in
#    "mw-1.0.8")
#        export CHIP=bm1682
#        export BM_MEDIA_ION=0
#        ;;
#    "mw-1.1.0")
#        export CHIP=bm1682
#        export BM_MEDIA_ION=0
#        ;;
#    "mw-1.1.1")
#        export CHIP=bm1682
#        export BM_MEDIA_ION=0
#        ;;
#    "mw-1.1.2")
#        export CHIP=bm1682
#        export BM_MEDIA_ION=0
#        ;;
#    "mw-1.1.3")
#        export CHIP=bm1682
#        export BM_MEDIA_ION=0
#        ;;
#    "mw-1.1.4")
#        export CHIP=bm1682
#        export BM_MEDIA_ION=0
#        ;;
#    "mw-2.0.0")
#        export CHIP=bm1684
#        export BM_MEDIA_ION=1
#        ;;
#    "mw-2.0.1")
#        export CHIP=bm1684
#        export BM_MEDIA_ION=1
#        ;;
#    "master")
#        export CHIP=bm1684
#        export BM_MEDIA_ION=1
#        ;;
#    *)
#        echo "Invalid branch name: ${BMVID_BRANCH}"
#        return -1
#        ;;
#esac

# TODO get current branch name

export DEBUG=0
export SUBTYPE=asic

export PRODUCTFORM=soc

for chip in 'bm1682' 'bm1684'; do

    echo -e "\n\n\n"
    echo "################################################################"
    echo "#"
    echo "#   Building bmvid for ${chip} soc mode"
    echo "#"
    echo "################################################################"
    echo -e "\n\n"

    export CHIP=${chip}
    if [ ${chip} = bm1682 ]; then
        export BM_MEDIA_ION=0
    else
        export BM_MEDIA_ION=1
    fi

    source ${BUILD_DIR}/envsetup.sh
    if [ $? != 0 ]; then
        echo "source ${BUILD_DIR}/envsetup.sh failed"
        return -1;
    fi

    source ${BUILD_DIR}/build.sh
    if [ $? != 0 ]; then
        echo "source ${BUILD_DIR}/build.sh failed"
        return -1;
    fi

    clean_bmvid
    if [ $? != 0 ]; then
        echo "clean bmvid failed!"
        return -1;
    fi

    build_bmvid
    if [ $? != 0 ]; then
        echo "build bmvid failed!"
        return -1;
    fi
done


export CHIP=bm1684
export BM_MEDIA_ION=0

for pcie_arch in 'pcie' 'pcie_arm64'; do
    export PRODUCTFORM=${pcie_arch}

    echo -e "\n\n\n"
    echo "################################################################"
    echo "#"
    echo "#   Building bmvid for bm1684 ${pcie_arch} mode"
    echo "#"
    echo "################################################################"
    echo -e "\n\n"

    source ${BUILD_DIR}/envsetup.sh
    if [ $? != 0 ]; then
        echo "source ${BUILD_DIR}/envsetup.sh failed"
        return -1;
    fi

    source ${BUILD_DIR}/build.sh
    if [ $? != 0 ]; then
        echo "source ${BUILD_DIR}/build.sh failed"
        return -1;
    fi

    clean_bmvid
    if [ $? != 0 ]; then
        echo "clean bmvid failed!"
        return -1;
    fi

    build_bmvid
    if [ $? != 0 ]; then
        echo "build bmvid failed!"
        return -1;
    fi
done

