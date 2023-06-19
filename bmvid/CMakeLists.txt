cmake_minimum_required(VERSION 3.13)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type: Release Debug RelWithDebInfo MinSizeRel." FORCE)
endif()

project(bmvid_win C CXX)

if(CMAKE_HOST_WIN32)
    set(WINDOWS 1)
elseif(CMAKE_HOST_APPLE)
    set(MACOS 1)
elseif(CMAKE_HOST_UNIX)
    set(LINUX 1)
endif()
cmake_policy(SET CMP0046 NEW)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(WINDOWS)
        if (RUNTIME_LIB STREQUAL "MD")
            set(CMAKE_CXX_FLAGS_RELEASE "/MD" CACHE STRING "CXX compile option.")
            set(CMAKE_C_FLAGS_RELEASE "/MD" CACHE STRING "C compile option.")
        else()
            set(CMAKE_CXX_FLAGS_RELEASE "/MT"  CACHE STRING "CXX compile option.")
            set(CMAKE_C_FLAGS_RELEASE "/MT" CACHE STRING "C compile option.")
        endif()
    elseif(LINUX)
        set(DEBUG 0)
    endif()
else()
    if(WINDOWS)
        if (RUNTIME_LIB STREQUAL "MD")
            set(CMAKE_CXX_FLAGS_DEBUG "/MDd" CACHE STRING "CXX compile option.")
            set(CMAKE_C_FLAGS_DEBUG "/MDd" CACHE STRING "C compile option.")
        else()
            set(CMAKE_CXX_FLAGS_DEBUG "/MTd" CACHE STRING "CXX compile option.")
            set(CMAKE_C_FLAGS_DEBUG "/MTd" CACHE STRING "C compile option.")
        endif()
    elseif(LINUX)
        set(DEBUG 1)
    endif()
endif()


########################
#  Target
set(CHIP_LIST bm1684 bm1686)
set(CHIP_NAME bm1684 CACHE STRING "chip: bm1686, bm1684")
set_property(CACHE CHIP_NAME PROPERTY STRINGS ${CHIP_LIST})

set(HOST_ARCH_LIST aarch64 x86)
set(HOST_ARCH x86 CACHE STRING "host arch: aarch64 x86" FORCE)
set_property(CACHE HOST_ARCH PROPERTY STRINGS ${HOST_ARCH_LIST})

set(PLATFORM_LIST pcie soc pice_arm64 pcie_loongarch64)
set(PLATFORM pcie CACHE STRING "platform: pcie soc pice_arm64 pcie_loongarch64")
set_property(CACHE PLATFORM PROPERTY STRINGS ${PLATFORM_LIST})

set(SUBTYPE_LIST asic palladium fpga)
set(SUBTYPE asic CACHE STRING "subtype: asic palladium fpga")
set_property(CACHE SUBTYPE PROPERTY STRINGS ${SUBTYPE_LIST})

set(BMVID_OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/release CACHE STRING "bmvid compile output location")

set(MIDDELWARE_SOC_PLATFORM ${PLATFORM})
if(CHIP_NAME STREQUAL "bm1684" AND PLATFORM STREQUAL "soc")
    set(BM_MEDIA_ION 1)
else()
    set(BM_MEDIA_ION 0)
endif()

message(STATUS "PLATFORM.................... ${PLATFORM}")

set(FIRMWARE_CORE_SRCS_C "" CACHE INTERNAL "FIRMWARE_CORE_SRCS_C" )

if(DEFINED ENV{TARGET_PROJECT})
    set(TARGET_PROJECT $ENV{TARGET_PROJECT} CACHE STRING "target project name.")
else()
    set(TARGET_PROJECT ${CHIP_NAME}_${PLATFORM} CACHE STRING "target project name.")
endif()

if(KERNEL_PATH)
    find_path(KERNEL_PATH vmlinux
        PATHS ${KERNEL_PATH}
        NO_DEFAULT_PATH)
else()
    if(PLATFORM STREQUAL "soc")
        set(KERNEL_PATH ${SOC_LINUX_DIR})
    endif()
endif()

if(KERNEL_PATH)
    message(STATUS "KERNEL_PATH................. ${KERNEL_PATH}")
endif()

########################
# Add common configuarations
if(${HOST_ARCH} STREQUAL "x86" AND ${PLATFORM} STREQUAL "pcie")
include(cmake/bmvid_${HOST_ARCH}_${PLATFORM}_device.cmake)
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/)
find_package(BMVidTarget)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_MESSAGE LAZY)
if(CMAKE_SYSTEM_NAME STREQUAL Generic)
	set(CMAKE_POSITION_INDEPENDENT_CODE OFF CACHE INTERNAL "POSITION_INDEPENDENT_CODE")
	set(BUILD_SHARED_LIBS OFF)
elseif()
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	set(BUILD_SHARED_LIBS ON)
endif()
message(STATUS "TARGET_PROJECT.............. ${TARGET_PROJECT}")

########################
#  Project
#enable_testing()
set(PROJECT_ROOT ${PROJECT_SOURCE_DIR})
if (CHIP_NAME STREQUAL bm1684)

	if(CMAKE_SYSTEM_NAME STREQUAL Generic)#arm9 cross compile
	elseif(LINUX)
        message(STATUS "Building system............. Linux")

        execute_process(
            COMMAND bash -c "./version.sh"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/)

        if(${PLATFORM} STREQUAL "soc")
            ADD_TARGET_VPU_DRIVER(vpu_driver ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} libsophon ${BMVID_OUT_PATH}/driver/)
            ADD_TARGET_JPU_DRIVER(jpu_driver ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} libsophon ${BMVID_OUT_PATH}/driver/)
        endif()
        ADD_TARGET_JPU_LIB(jpu_lib ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} libsophon ${BMVID_OUT_PATH} ${BMVID_OUT_PATH})
        ADD_TARGET_VPU_LIB(vpu_lib ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} libsophon ${BMVID_OUT_PATH} ${BMVID_OUT_PATH})
        ADD_TARGET_YUV_LIB(yuv_lib ${PLATFORM}  ${DEBUG} libsophon ${BMVID_OUT_PATH})
        ADD_TARGET_VPP_LIB(vpp_lib ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} libsophon ${BMVID_OUT_PATH} ${BMVID_OUT_PATH})
        ADD_TARGET_BMVID_DOC(bmvid_doc)
        if (TARGET doc)
            add_dependencies(doc bmvid_doc)
        endif()

        if(${CHIP_NAME} STREQUAL "bm1684")
            ADD_TARGET_ION_LIB(ion_lib ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} libsophon ${BMVID_OUT_PATH})
            ADD_TARGET_BMCV_LIB(bmcv_lib ${CHIP_NAME} ${PLATFORM} ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} 
                libsophon ${BMVID_OUT_PATH} ${BMVID_OUT_PATH} ${BMVID_OUT_PATH} ${BMVID_OUT_PATH})

            add_dependencies(vpp_lib ion_lib)
            add_dependencies(jpu_lib ion_lib)
            add_dependencies(vpu_lib ion_lib)
            add_dependencies(bmcv_lib jpu_lib vpp_lib)
        endif()

        if ("${BMCPU_USED}" STREQUAL "ON" AND "${CHIP_NAME}" STREQUAL "bm1684")

            set(BMVID_BMCPU_OUT_PATH  ${CMAKE_CURRENT_BINARY_DIR}/release/bmcpu/ CACHE STRING "bmcpu-opencv compile output location")

            ADD_TARGET_JPU_LIB(bmcpu_jpu_lib bm1684 soc ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} bmcpu ${BMVID_BMCPU_OUT_PATH} ${BMVID_BMCPU_OUT_PATH})
            ADD_TARGET_VPU_LIB(bmcpu_vpu_lib bm1684 soc ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} bmcpu ${BMVID_BMCPU_OUT_PATH} ${BMVID_BMCPU_OUT_PATH})
            ADD_TARGET_YUV_LIB(bmcpu_yuv_lib soc ${DEBUG} bmcpu ${BMVID_BMCPU_OUT_PATH})
            ADD_TARGET_VPP_LIB(bmcpu_vpp_lib bm1684 soc ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} bmcpu ${BMVID_BMCPU_OUT_PATH} ${BMVID_BMCPU_OUT_PATH})
            ADD_TARGET_ION_LIB(bmcpu_ion_lib bm1684 soc ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} bmcpu ${BMVID_BMCPU_OUT_PATH})
            ADD_TARGET_BMCV_LIB(bmcpu_bmcv_lib bm1684 soc ${SUBTYPE} ${DEBUG} ${BM_MEDIA_ION} bmcpu
                ${BMVID_BMCPU_OUT_PATH} ${BMVID_BMCPU_OUT_PATH} ${BMVID_BMCPU_OUT_PATH} ${BMVID_BMCPU_OUT_PATH})

            add_dependencies(bmcpu_ion_lib ion_lib)  # avoid compile ion_lib bmcpu_ion_lib in parallel
            add_dependencies(bmcpu_yuv_lib yuv_lib)  # avoid compile yuv_lib bmcpu_yuv_lib in parallel
            add_dependencies(bmcpu_vpp_lib vpp_lib)
            add_dependencies(bmcpu_jpu_lib jpu_lib)
            add_dependencies(bmcpu_vpu_lib vpu_lib)
            add_dependencies(bmcpu_bmcv_lib bmcv_lib)

            add_dependencies(bmcpu_vpp_lib bmcpu_ion_lib)
            add_dependencies(bmcpu_jpu_lib bmcpu_ion_lib)
            add_dependencies(bmcpu_vpu_lib bmcpu_ion_lib)
            add_dependencies(bmcpu_bmcv_lib bmcpu_jpu_lib bmcpu_vpp_lib)

        endif()

    elseif(WINDOWS)
        message(STATUS "entry WINDOW")

        if(NOT CACHED_INSTALL_PATH)
            set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
        endif()

        # ion_lib
        add_subdirectory(allocator/ion)

        # vpu_lib
        add_subdirectory(video/provider/cnm/encoder)
        add_subdirectory(video/encoder/bm_enc_api)
        add_subdirectory(video/provider/cnm)
        file(COPY
            video/driver/linux/chagall.bin
            video/driver/linux/chagall_dec.bin
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/vpu_firmware
        )

        # vpp_lib
        add_subdirectory(vpp/driver)
        add_subdirectory(vpp/bmvppapi)
        add_subdirectory(vpp/test)

        # jpu_lib
        add_subdirectory(jpeg/driver/bmjpulite)
        add_subdirectory(jpeg/driver/bmjpuapi)

        # bmcv_lib
        add_subdirectory(bmcv)
        add_subdirectory(bmcv/test)

        # yuv_lib
        set(CMAKE_TOOLCHAIN_FILE ../x86_toolchain/x86_64.toolchain.cmake)
        option(ENABLE_NEON "default disable neon on windows" OFF)
        option(TEST_OPT "default not build unit test" OFF)
        add_subdirectory(3rdparty/libyuv)
    endif()
endif()

########################
#  message
message(STATUS "INSTALL PATH................ ${CMAKE_INSTALL_PREFIX}")
message(STATUS "USING_CMODEL................ ${USING_CMODEL}")

