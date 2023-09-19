if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../bm_prebuilt_toolchains_win CACHE STRING "path to prebuilt toolchain")

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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/src/window)

include_directories(${PREBUILT_TOOLCHAINS}/gflags-windows/include/)
include_directories(${PREBUILT_TOOLCHAINS}/PDCurses-3.8/PDCurses-3.8)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/release/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../release/libsophon/lib)

find_library(PDCURSES_LIB
    NAMES pdcurses
    PATHS ${PREBUILT_TOOLCHAINS}/PDCurses-3.8/PDCurses-3.8/wincon
    REQUIRED)

find_file(PDCURSES_DLL
    NAMES pdcurses.dll
    PATHS ${PREBUILT_TOOLCHAINS}/PDCurses-3.8/PDCurses-3.8/wincon
)
file(COPY ${PDCURSES_DLL} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

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

set(SRCS
    src/bm-smi.cpp
    src/bm_smi_cmdline.cpp
    src/bm_smi_test.cpp
    src/bm_smi_creator.cpp
    src/bm_smi_display.cpp
    src/bm_smi_ecc.cpp
    src/bm_smi_led.cpp
    src/bm_smi_recovery.cpp
    src/bm_smi_display_memory_detail.cpp
    src/bm_smi_display_util_detail.cpp
)

add_executable(bm-smi ${SRCS})
target_link_libraries(bm-smi ${PDCURSES_LIB} ${GFLAGS_LIB} libbmlib-static)
