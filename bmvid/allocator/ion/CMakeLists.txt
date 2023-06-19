cmake_minimum_required(VERSION 3.13)
project(bmvid_win C CXX)
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

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_ROOT}/cmake)
message(STATUS "cmake_module_path: ${CMAKE_MODULE_PATH}")
find_package(SETUPAPILIB_LIBRARY)

if(SETUPAPILIB_FOUND)
    message(STATUS "found SetupAPI ${SETUPAPILIB_INCLUDE_DIR} ${SETUPAPILIB_LIBRARY}")
    include_directories(${SETUPAPILIB_INCLUDE_DIR})
else()
    message(STATUS "err: not found SETUPAPILIB_FOUND")
endif(SETUPAPILIB_FOUND)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)
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
set(SRCS
  ${PROJECT_ROOT}/allocator/ion/src/bmionapi.c
  ${PROJECT_ROOT}/allocator/ion/src/bm_ion.c
  ${PROJECT_ROOT}/allocator/ion/src/bm_pcie_buffer.c
  ${PROJECT_ROOT}/allocator/ion/src/bm_pcie_win32_buffer.cpp
)

add_library(libbmion SHARED ${SRCS})
target_link_libraries(libbmion ${SETUPAPILIB_LIBRARY})

add_library(libbmion-static STATIC ${SRCS})
target_link_libraries(libbmion-static ${SETUPAPILIB_LIBRARY})

install(TARGETS libbmion libbmion-static DESTINATION lib)

add_executable(test_ion example/test_ion.c)
target_link_libraries(test_ion libbmion)

if(WIN32)
file(
    COPY ./inc/bm_ion.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()