cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(libbmvppapi)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver/include/bm1684
    )
include_directories(${PROJECT_ROOT}/3rdparty/libbmcv/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(
    ${PROJECT_ROOT}/../libsophon/release/libsophon/lib
    ${PROJECT_ROOT}/../libsophon/bmlib/release/lib
    ${PROJECT_ROOT}/../../libsophon/release/libsophon/lib
    ${PROJECT_ROOT}/../../libsophon/bmlib/release/lib
    ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows)

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

option(BM_PCIE_MODE "BM_WIN32_1684" on)
if(BM_PCIE_MODE)
    add_definitions(-DBM_PCIE_MODE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib/)
set(SRCS src/bmvpp_1684.c src/bmvpp_common.c src/bmvpp_logging.c)

add_library(libbmvppapi SHARED ${SRCS})
target_link_libraries(libbmvppapi libvpp libbmlib)

add_library(libbmvppapi-static STATIC ${SRCS})
target_link_libraries(libbmvppapi-static libbmlib-static)

install(TARGETS libbmvppapi libbmvppapi-static DESTINATION lib)

#add_definitions(-DBM_PCIE_WIN32_MODE)
function(test_window)
    foreach(src IN LISTS ARGN)
        get_filename_component(target ${src} NAME_WE)
        add_executable(${target} ${src})
        target_link_libraries(${target} libbmvppapi-static libbmlib-static libvpp)
        install(TARGETS ${target} DESTINATION bin/test_window)
    endforeach()
endfunction()

test_window(
  ${CMAKE_CURRENT_SOURCE_DIR}/test/bmvpptest.c
)

if(WIN32)
file(
    COPY ./inc/bmvppapi.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()