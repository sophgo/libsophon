cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0046 NEW)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Install prefix")
set(common_dir ${CMAKE_CURRENT_SOURCE_DIR}/../tpu-common)
if (NOT EXISTS ${common_dir})
    message(FATAL_ERROR "${common_dir} does not exist")
endif()
list(APPEND CMAKE_MODULE_PATH ${common_dir}/cmake)

project(bmrt LANGUAGES CXX)

find_package(bmlib REQUIRED)
find_package(bmodel REQUIRED)
find_package(Threads REQUIRED)

include(gen_kernel_header.cmake)
add_custom_target(kernel_header DEPENDS ${KERNEL_HEADER_FILE})

file(GLOB_RECURSE srcs src/*.cpp src/*.c)

add_library(bmrt SHARED ${srcs})
add_library(bmrt_static STATIC ${srcs})
target_compile_options(bmrt_static PRIVATE -fPIC)
add_dependencies(bmrt kernel_header)
add_dependencies(bmrt_static kernel_header)

target_link_libraries(bmrt PUBLIC
    bmodel::bmodel bmlib::bmlib
    ${CMAKE_DL_LIBS}
    Threads::Threads)
target_link_libraries(bmrt_static PUBLIC
    bmodel::bmodel bmlib::bmlib
    ${CMAKE_DL_LIBS}
    Threads::Threads)
target_include_directories(bmrt PUBLIC
    ${common_dir}/base/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bmtap2
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR})
target_include_directories(bmrt_static PUBLIC
    ${common_dir}/base/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bmtap2
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR})

#include(git-utils)
#get_version_from_tag(version soversion revision)
#set_target_properties(bmrt PROPERTIES SOVERSION ${soversion})
#set_target_properties(bmrt_static PROPERTIES SOVERSION ${soversion})

# fix the soversion to be compatible with old application
set_target_properties(bmrt PROPERTIES SOVERSION "1.0")
set_target_properties(bmrt_static PROPERTIES SOVERSION "1.0")
set(app_srcs
    app/bmrt_test.cpp
    app/bmrt_test_case.cpp)
add_executable(bmrt_test ${app_srcs})
target_link_libraries(bmrt_test bmrt  bmrt_static)
target_compile_definitions(bmrt_test PRIVATE
    VER="${revision}")

set(runner_srcs
    app/model_runner/cnpy.cpp
    app/model_runner/model_runner.cpp)
add_executable(model_runner ${runner_srcs})

if("${ARCH}" STREQUAL "arm64")
    find_library(ZLIB_LIBRARY NAMES z PATHS ${LIB_DIR}/lib/)
    target_include_directories(model_runner PRIVATE ${LIB_DIR}/include/)
    target_link_libraries(model_runner bmrt bmrt_static  ${LIB_DIR}/lib/libz.so)
elseif("${ARCH}" STREQUAL "loongarch64")
    find_library(ZLIB_LIBRARY NAMES z PATHS ${LIB_DIR}/lib/)
    target_include_directories(model_runner PRIVATE ${LIB_DIR}/include/)
    target_link_libraries(model_runner bmrt bmrt_static ${LIB_DIR}/lib/libz.so)
else()
    target_link_libraries(model_runner bmrt bmrt_static z)
endif()

target_compile_definitions(model_runner PRIVATE
    VER="${revision}")
set_target_properties(bmrt_static PROPERTIES OUTPUT_NAME bmrt)
install(TARGETS bmrt bmrt_test bmrt_static model_runner
    LIBRARY DESTINATION lib
    COMPONENT libsophon
    RUNTIME DESTINATION bin
    COMPONENT libsophon)
install(
    DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${common_dir}/base/
    DESTINATION "include"
    COMPONENT libsophon-dev
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h")

set(BUILD_DOCS ON CACHE BOOL "Build documents")
if (BUILD_DOCS)
    add_subdirectory(docs)
endif()
