cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(vpp_driver)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/bm1684)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/libbmcv/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmvppapi/inc)

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

link_directories(
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/libbmcv/lib/pcie_windows)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

if(UNIX)
    set(SRCS
        src/bm1684/1684vppcmodel.c
        src/bm1684/vpplib.c
        src/vppion.c)
elseif(WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

    set(SRCS
        src/bm1684/vpplib.c
        src/bm1684/1684vppcmodel.c)
endif()

add_definitions(-DPCIE_MODE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_library(libvpp SHARED ${SRCS})
target_link_libraries(libvpp libbmlib)

add_library(libvpp-static STATIC ${SRCS})
target_link_libraries(libvpp-static libbmlib-static)

install(TARGETS libvpp libvpp-static DESTINATION lib)

if(WIN32)
file(
    COPY ./include/bm1684/bmvpp.h ./include/bm1684/vpplib.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()