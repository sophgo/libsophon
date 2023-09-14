if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../../bm_prebuilt_toolchains_win CACHE STRING "path to prebuilt toolchain")

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

    set(GFLAGS gflags_debug)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/window)
include_directories(${PREBUILT_TOOLCHAINS}/gflags-windows/include)
include_directories(${PREBUILT_TOOLCHAINS}/pthreads-win32/prebuilt-dll-2-9-1-release/include)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../release/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../release/libsophon/lib)

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

find_library(PTHREADS_LIB
    NAMES pthreadVC2
    PATHS ${PREBUILT_TOOLCHAINS}/pthreads-win32/prebuilt-dll-2-9-1-release/lib/x64
    REQUIRED)

find_file(PTHREADS_DLL
    NAMES pthreadVC2.dll
    PATHS ${PREBUILT_TOOLCHAINS}/pthreads-win32/prebuilt-dll-2-9-1-release/dll/x64
)
file(COPY ${PTHREADS_DLL} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_definitions(-DHAVE_STRUCT_TIMESPEC)

set(SRCS

    # ./bm_firmware_update.cpp
    ./pcie_load.cpp
    ./sc5_burning_tool.cpp
    ./test_cdma_board.cpp
    ./test_cdma_perf.cpp
    ./test_gdma_perf.cpp
    ./test_reg.cpp
    ./test_update_fw.cpp
    ./update_boot_info.cpp
    # ./update_isl68127.cpp
    ./version.cpp
)

foreach(src ${SRCS})
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    target_link_libraries(${target} ${GFLAGS_LIB} ${PTHREADS_LIB} libbmlib-static)
endforeach(src)

# add_subdirectory(a53lite) # not implemented on windows
add_subdirectory(bmcpu)
