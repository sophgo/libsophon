cmake_minimum_required(VERSION 3.13)
#project(bmvid_win C CXX)
cmake_policy(SET CMP0046 NEW)


message(STATUS "-------------")
message(STATUS ${PROJECT_ROOT})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${PROJECT_ROOT}/video/provider/cnm/encoder/inc)
include_directories(${PROJECT_ROOT}/video/provider/cnm/decoder/vpuapi/wave)
include_directories(${PROJECT_ROOT}/video/driver)

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
    ${PROJECT_ROOT}/video/provider/cnm/encoder/src/vdi_win.c
    ${PROJECT_ROOT}/video/provider/cnm/encoder/src/enc.c
    ${PROJECT_ROOT}/video/provider/cnm/encoder/src/logging.c
    ${PROJECT_ROOT}/video/provider/cnm/encoder/src/common.c
)

add_library(libbmvpulite SHARED ${SRCS})
add_dependencies(libbmvpulite libbmion)
target_link_libraries(libbmvpulite ${SETUPAPILIB_LIBRARY} ${CMAKE_INSTALL_PREFIX}/lib/libbmion.lib)

add_library(libbmvpulite-static STATIC ${SRCS})
add_dependencies(libbmvpulite-static  libbmion-static)
target_link_libraries(libbmvpulite-static ${SETUPAPILIB_LIBRARY} ${CMAKE_INSTALL_PREFIX}/lib/libbmion.lib)

install(TARGETS libbmvpulite libbmvpulite-static DESTINATION lib)

if(WIN32)
file(
	COPY ./inc/bmvpu.h ./inc/bmvpu_types.h ./inc/bmvpu_logging.h
	DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()