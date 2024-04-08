cmake_minimum_required(VERSION 2.8)

project(bmrt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

message(STATUS "third party: " $ENV{THIRDPARTY_DIR})
find_package(Threads REQUIRED)
link_directories($ENV{THIRDPARTY_DIR}/lib)

include(gen_kernel_header.cmake)
add_custom_target(kernel_header DEPENDS ${KERNEL_HEADER_FILE})

file(GLOB_RECURSE srcs src/*.cpp src/*.c)
add_library(bmrt SHARED ${srcs})
target_link_libraries(bmrt PUBLIC bmodel bmlib ${CMAKE_DL_LIBS} Threads::Threads)
target_include_directories(bmrt PUBLIC
    $ENV{THIRDPARTY_DIR}
    $ENV{THIRDPARTY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bmtap2
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
    )
add_dependencies(bmrt kernel_header)

set(app_srcs
    app/bmrt_test.cpp
    app/bmrt_test_case.cpp)
add_executable(bmrt_test ${app_srcs})
target_link_libraries(bmrt_test bmrt)
target_compile_definitions(bmrt_test PRIVATE VER="test")
