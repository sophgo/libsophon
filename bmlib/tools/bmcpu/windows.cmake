if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../../../bm_prebuilt_toolchains_win CACHE STRING "path to prebuilt toolchain")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

include_directories(${PREBUILT_TOOLCHAINS}/pthreads-win32/prebuilt-dll-2-9-1-release/include)

find_library(PTHREADS_LIB
    NAMES pthreadVC2
    PATHS ${PREBUILT_TOOLCHAINS}/pthreads-win32/prebuilt-dll-2-9-1-release/lib/x64
    REQUIRED)

find_file(PTHREADS_DLL
    NAMES pthreadVC2.dll
    PATHS ${PREBUILT_TOOLCHAINS}/pthreads-win32/prebuilt-dll-2-9-1-release/dll/x64
)
file(COPY ${PTHREADS_DLL} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../release/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../release/libsophon/lib)

add_definitions(-DHAVE_STRUCT_TIMESPEC)

set(TEST_SRC_PATH
    src/test_get_log.cpp
    src/test_lib_cpu.cpp
    src/test_reset_cpu.cpp
    src/test_set_log.cpp
    src/test_start_cpu.cpp
    src/test_sync_time.cpp
)

foreach(src ${TEST_SRC_PATH})
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    target_link_libraries(${target} libbmlib-static ${PTHREADS_LIB})
endforeach(src)

file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/prebuild/user_algo.so
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/sophon-rpc
)