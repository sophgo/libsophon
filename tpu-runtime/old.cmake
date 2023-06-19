if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../bm_prebuilt_toolchains_win CACHE STRING "path to prebuilt toolchain")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(OUT_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)

if(TARGET_TYPE STREQUAL "release")
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD")
        set(CMAKE_C_FLAGS_RELEASE "/MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(CMAKE_C_FLAGS_RELEASE "/MT")
    endif()

    set(GFLAGS gflags)
else()
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_DEBUG "/MDd")
        set(CMAKE_C_FLAGS_DEBUG "/MDd")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_C_FLAGS_DEBUG "/MTd")
    endif()

    set(GFLAGS_NAME gflags_debug)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/bmtap2/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../tpu-bmodel/include/flatbuffers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../tpu-bmodel/include/md5)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../tpu-bmodel/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../tpu-common/base)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libsophon/bmlib/include)
include_directories(${PREBUILT_TOOLCHAINS}/gflags-windows/include)
add_definitions(-DVER="3.1.0")

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/release/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../tpu-bmodel/release/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../release/libsophon/lib)

find_library(GFLAGS_LIB
    NAMES ${GFLAGS}
    PATHS
    ${PREBUILT_TOOLCHAINS}/gflags-windows/lib/Release
    ${PREBUILT_TOOLCHAINS}/gflags-windows/lib/Debug
    REQUIRED)

find_file(GFLAGS_DLL
    NAMES ${GFLAGS}.dll
    PATHS
    ${PREBUILT_TOOLCHAINS}/gflags-windows/bin/Release
    ${PREBUILT_TOOLCHAINS}/gflags-windows/bin/Debug
)
file(COPY ${GFLAGS_DLL} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

file(GLOB_RECURSE SRCS src/*.cpp)

# dump kernel_module.so to kernel_module.h
set(KERNEL_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(KERNEL_MODULE "${CMAKE_BINARY_DIR}/kernel_module.h")
add_custom_target(GEN_KERNEL_MODULE
    COMMAND cd ${KERNEL_MODULE_DIR}
    COMMAND echo const \\> ${KERNEL_MODULE}
    COMMAND xxd -i libbm1684x_kernel_module.so >> ${KERNEL_MODULE}
    COMMAND echo const unsigned char *kernel_module_data = libbm1684x_kernel_module_so\; >> ${KERNEL_MODULE}
    COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(libbmrt SHARED ${SRCS})
add_dependencies(libbmrt GEN_KERNEL_MODULE)
target_include_directories(libbmrt PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(libbmrt libbmodel-static libbmlib ${CMAKE_DL_LIBS})

add_library(libbmrt-static STATIC ${SRCS})
add_dependencies(libbmrt-static GEN_KERNEL_MODULE)
target_include_directories(libbmrt-static PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(libbmrt-static libbmodel-static libbmlib-static ${CMAKE_DL_LIBS})

add_executable(bmrt_test app/bmrt_test.cpp app/bmrt_test_case.cpp)
target_link_libraries(bmrt_test libbmrt-static ${GFLAGS_LIB})

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../tpu-common/base/
    DESTINATION ${OUT_INCLUDE_DIR}
    PATTERN *.hpp
    PATTERN *.h)

file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/lib/libbm1684x_kernel_module.so
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/tpu_module
)