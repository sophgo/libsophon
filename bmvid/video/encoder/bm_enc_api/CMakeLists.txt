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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${PROJECT_ROOT}/allocator/ion/inc)
include_directories(${PROJECT_ROOT}/video/encoder/bm_enc_api/inc)
include_directories(${PROJECT_ROOT}/video/provider/cnm/encoder/inc)
include_directories(${PROJECT_ROOT}/video/provider/cnm/decoder/vdi)
include_directories(${PROJECT_ROOT}/3rdparty/libbmcv/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(
    ${PROJECT_ROOT}/../libsophon/release/libsophon/lib
    ${PROJECT_ROOT}/../libsophon/bmlib/release/lib
    ${PROJECT_ROOT}/../../libsophon/release/libsophon/lib
    ${PROJECT_ROOT}/../../libsophon/bmlib/release/lib
    ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows)

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
    ${PROJECT_ROOT}/video/encoder/bm_enc_api/src/encoder.c
    ${PROJECT_ROOT}/video/encoder/bm_enc_api/src/log.c
    ${PROJECT_ROOT}/video/encoder/bm_enc_api/src/misc.c
    ${PROJECT_ROOT}/video/encoder/bm_enc_api/src/queue.c
)

add_library(libbmvpuapi SHARED ${SRCS})
add_dependencies(libbmvpuapi libbmvpulite libbmion)
target_link_libraries(libbmvpuapi PUBLIC ${SETUPAPILIB_LIBRARY} libbmvpulite libbmion libbmlib)

add_library(libbmvpuapi-static STATIC ${SRCS})
add_dependencies(libbmvpuapi-static libbmvpulite-static libbmion-static)
target_link_libraries(libbmvpuapi-static PUBLIC ${SETUPAPILIB_LIBRARY} libbmvpulite-static libbmion-static libbmlib-static)

set(SRCSEXEC
    ${PROJECT_ROOT}/example/bmvpuenc.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/windows/libusb-1.0.18/examples/getopt/getopt1.c
    ${PROJECT_ROOT}/video/provider/cnm/decoder/vdi/windows/libusb-1.0.18/examples/getopt/getopt.c
)

add_executable(bmvpuenc ${SRCSEXEC})
add_dependencies(bmvpuenc libbmvpuapi libbmvpulite libbmion)
target_link_libraries(bmvpuenc ${SETUPAPILIB_LIBRARY} libbmion libbmvpulite libbmvpuapi)

install(TARGETS libbmvpuapi libbmvpuapi-static DESTINATION lib)

if(WIN32)
file(
    COPY ./inc/bmqueue.h ./inc/bmvpuapi.h ./inc/bmvpuapi_common.h ./inc/bmvpuapi_enc.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()