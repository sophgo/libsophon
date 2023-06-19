cmake_minimum_required(VERSION 3.13)
#project(bmvid_win C CXX)
cmake_policy(SET CMP0046 NEW)


message(STATUS "-------------")
message(STATUS ${PROJECT_ROOT})
if(TARGET_TYPE STREQUAL "release")
    if (RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD")
        set(CMAKE_C_FLAGS_RELEASE "/MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(CMAKE_C_FLAGS_RELEASE "/MT")
    endif()
else()
    if (RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_DEBUG "/MDd")
        set(CMAKE_C_FLAGS_DEBUG "/MDd")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_C_FLAGS_DEBUG "/MTd")
    endif()
endif()
include_directories(${PROJECT_ROOT}/video/provider/cnm)
#include_directories(${PROJECT_ROOT}/video/provider/cnm/encoder/inc)
include_directories(${PROJECT_ROOT}/video/provider/cnm/ffmpeg/include)#ok
include_directories(${PROJECT_ROOT}/video/decoder/bm_dec_api/inc)#ok
include_directories(${PROJECT_ROOT}/allocator/ion/inc)
include_directories(${PROJECT_ROOT}/video/provider/cnm/decoder/vdi)
include_directories(${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi)#ok
include_directories(${PROJECT_ROOT}/video/driver)
include_directories(${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper)#ok
include_directories(${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc)#ok
include_directories(${PROJECT_ROOT}/video/provider/cnm/sample_v2/component)#ok
include_directories(${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_decoder)#ok
include_directories(${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_encoder)#ok
include_directories(${PROJECT_ROOT}/3rdparty/libbmcv/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_ROOT}/cmake)
message(STATUS "cmake_module_path: ${CMAKE_MODULE_PATH}")
find_package(SETUPAPILIB_LIBRARY)

if(SETUPAPILIB_FOUND)
    message(STATUS "found SetupAPI ${SETUPAPILIB_INCLUDE_DIR} ${SETUPAPILIB_LIBRARY}")
    include_directories(${SETUPAPILIB_INCLUDE_DIR})
else()
    message(STATUS "err: not found SETUPAPILIB_FOUND")
endif(SETUPAPILIB_FOUND)

option(CHIP_BM1684 "BM_WIN32_1684" on)
option(BM_PCIE_MODE "BM_WIN32_1684" on)
option(TEST_VPU_ONECORE_FPGA "TEST_VPU_ONECORE_FPGA" on)

if(BM_WIN32_1684)
    add_definitions(-DBM_WIN32_1684)
endif()

if(CHIP_BM1684)
    add_definitions(-DCHIP_BM1684)
endif()

if(BM_PCIE_MODE)
    add_definitions(-DBM_PCIE_MODE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(SRCS
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/main_helper.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/vpuhelper.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/bitstream/bitstreamreader.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/bitstream/bitstreamfeeder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/bitstream/bsfeeder_fixedsize_impl.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/bitstream/bsfeeder_framesize_impl.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/bitstream/bsfeeder_size_plus_es_impl.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/comparator/bin_comparator_impl.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/comparator/comparator.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/comparator/md5_comparator_impl.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/comparator/yuv_comparator_impl.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/cfgParser.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_decoder/decoder_listener.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_encoder/encoder_listener.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/cnm_video_helper.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/container.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/datastructure.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/debug.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/yuv/yuvfeeder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/yuv/yuvLoaderfeeder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/yuv/yuvCfbcfeeder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/datastructure.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/bw_monitor.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/pf_monitor.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/helper/misc/md5.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component/cnm_app.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component/cnm_task.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component/component.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_encoder/component_enc_encoder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_encoder/component_enc_feeder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_encoder/component_enc_reader.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_decoder/component_dec_decoder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_decoder/component_dec_feeder.c
    ${PROJECT_ROOT}/video/provider/cnm/sample_v2/component_decoder/component_dec_renderer.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi/product.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi/vpuapifunc.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi/vpuapi.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi/coda9/coda9.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi/wave/wave5.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/windows/vdi.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/windows/vdi_osal.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/mm.c
    ${PROJECT_ROOT}/video/decoder/bm_dec_api/src/bm_video_interface.c
    ${PROJECT_ROOT}/video/decoder/bm_dec_api/src/bm_video_syscxt.c
)

add_library(libbmvideo SHARED ${SRCS})
target_link_libraries(libbmvideo ${SETUPAPILIB_LIBRARY} ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows/libbmlib-static.lib)
add_library(libbmvideo-static STATIC ${SRCS})
target_link_libraries(libbmvideo-static ${SETUPAPILIB_LIBRARY} ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows/libbmlib-static.lib)

set(OPT_SRCS
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/windows/libusb-1.0.18/examples/getopt/getopt1.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/windows/libusb-1.0.18/examples/getopt/getopt.c
)

add_executable(bm_test ${PROJECT_ROOT}/example/bm_test.c ${OPT_SRCS})
target_link_libraries(bm_test ${SETUPAPILIB_LIBRARY} libbmvideo-static)

add_executable(vpuinfo ${PROJECT_ROOT}/example/vpu_info.c ${OPT_SRCS})
target_link_libraries(vpuinfo libbmvideo-static libbmion-static)

add_executable(vpureset ${PROJECT_ROOT}/example/vpu_reset.c ${OPT_SRCS})
target_link_libraries(vpureset libbmvideo-static libbmion-static)

install(TARGETS libbmvideo libbmvideo-static DESTINATION lib)

if(WIN32)
file(
    COPY ${PROJECT_ROOT}/video/decoder/bm_dec_api/inc/bm_video_interface.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()