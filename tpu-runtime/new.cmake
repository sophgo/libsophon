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

# dump kernel_module.so to kernel_module.h
set(KERNEL_MODULE_PATH "${PROJECT_SOURCE_DIR}/lib/libbm1684x_kernel_module.so")
set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/kernel_module.h")
add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND echo "const unsigned char kernel_module_data[] = {" > ${OUTPUT_FILE}
    COMMAND hexdump -v -e '8/1 \"0x%02x,\" \"\\n\"' ${KERNEL_MODULE_PATH} >> ${OUTPUT_FILE}
    COMMAND echo "}\;" >> ${OUTPUT_FILE}
)
add_custom_target(kernel_header DEPENDS ${OUTPUT_FILE})

file(GLOB_RECURSE srcs src/*.cpp src/*.c)

add_library(bmrt SHARED ${srcs})
add_dependencies(bmrt kernel_header)
target_link_libraries(bmrt PUBLIC
    bmodel::bmodel bmlib::bmlib
    ${CMAKE_DL_LIBS}
    Threads::Threads)
target_include_directories(bmrt PUBLIC
    ${common_dir}/base/
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bmtap2
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR})

include(git-utils)
get_version_from_tag(version soversion revision)

set_target_properties(bmrt PROPERTIES SOVERSION ${soversion})

set(app_srcs
    app/bmrt_test.cpp
    app/bmrt_test_case.cpp)
add_executable(bmrt_test ${app_srcs})
target_link_libraries(bmrt_test bmrt)
target_compile_definitions(bmrt_test PRIVATE
    VER="${revision}")

install(TARGETS bmrt bmrt_test
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
