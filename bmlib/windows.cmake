set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../bm_prebuilt_toolchains_win CACHE PATH "path to prebuilt toolchain")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(OUT_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(OUT_DATA_DIR ${CMAKE_INSTALL_PREFIX}/data)

if(TARGET_TYPE STREQUAL "release")
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD")
        set(CMAKE_C_FLAGS_RELEASE "/MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(CMAKE_C_FLAGS_RELEASE "/MT")
    endif()
else()
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_DEBUG "/MDd")
        set(CMAKE_C_FLAGS_DEBUG "/MDd")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_C_FLAGS_DEBUG "/MTd")
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/window)

set(SRCS
    src/bmkernel_runtime.cpp
    src/bmlib_mmpool.cpp
    src/bmcpu_runtime.cpp
    src/bmlib_runtime.cpp
    src/bmlib_util.cpp
    src/bmlib_log.cpp
    src/bmlib_device.cpp
    src/bmlib_memory.cpp
    src/a53lite_api.cpp
    src/bmlib_profile.cpp
    src/window/bmlib_ioctl.cpp
    src/bmlib_md5.cpp
)

add_library(libbmlib SHARED ${SRCS})
add_library(libbmlib-static STATIC ${SRCS})

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${OUT_INCLUDE_DIR}
    PATTERN *.hpp
    PATTERN *.h)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/windows-dev/libsophon-config.cmake
    DESTINATION ${OUT_DATA_DIR})

add_subdirectory(tools)