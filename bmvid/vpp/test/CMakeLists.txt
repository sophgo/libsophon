cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(vpp_test)

if(EXISTS ${PROJECT_ROOT}/../../bm_prebuilt_toolchains_win)
    set(PREBUILT_TOOLCHAINS ${PROJECT_ROOT}/../../bm_prebuilt_toolchains_win)
elseif(EXISTS ${PROJECT_ROOT}/../bm_prebuilt_toolchains_win)
    set(PREBUILT_TOOLCHAINS ${PROJECT_ROOT}/../bm_prebuilt_toolchains_win)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../driver/include/bm1684)
include_directories(${PROJECT_ROOT}/3rdparty/libbmcv/include)
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

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_definitions(-DPCIE_MODE)
function(test_window)
  foreach(src IN LISTS ARGN)
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    if(${target} STREQUAL vppmulti)
        add_definitions(-DHAVE_STRUCT_TIMESPEC)
        target_link_libraries(${target} libvpp-static libbmlib-static ${PTHREADS_LIB})
    else()
        target_link_libraries(${target} libvpp-static libbmlib-static)
    endif()
    install(TARGETS ${target} DESTINATION bin/test_window)
  endforeach()
endfunction()

test_window(
#   ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/i420tonv12/i420tonv12.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppborder/vppborder.c
#   ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppcmodel/vppcmodel.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppconvert/vppconvert.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppcropmulti/vppcropmulti.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppcropsingle/vppcropsingle.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppfbd_crop/vppfbd_crop.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppfbd_csc_resize/vppfbd_csc_resize.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppiondemo/vppiondemo.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppmatrix/vppmatrix.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppmisc/vppmisc.c
#   ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppmulti/vppmulti.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppresize/vppresize.c
  ${CMAKE_CURRENT_SOURCE_DIR}/bm1684/pcie/vppsplit/vppsplit.c
)

