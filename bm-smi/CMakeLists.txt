cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0046 NEW)
cmake_policy(SET CMP0048 NEW)

set(TARGET_NAME "bm-smi")

project(${TARGET_NAME} C CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/src)

if("${PLATFORM}" STREQUAL "soc")
  link_directories(${LIB_DIR}/lib/)
  include_directories(${LIB_DIR}/include)
  add_definitions(-DSOC_MODE=1)
elseif("${PLATFORM}" STREQUAL "pcie_arm64")
  link_directories(${LIB_DIR}/lib/)
  include_directories(${LIB_DIR}/include)
elseif("${PLATFORM}" STREQUAL "pcie_loongarch64")
  link_directories(${LIB_DIR}/lib/)
  include_directories(${LIB_DIR}/include)
  include_directories(${LIB_DIR}/include/ncurses)
else()
  find_package(Curses REQUIRED)
  include_directories(${CURSES_INCLUDE_DIR})
  find_package(gflags COMPONENTS nothreads_static)
  include_directories(${GFLAGS_INCLUDE_DIR})
endif()

SET(SRCS
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

add_executable(${TARGET_NAME} ${SRCS})

if("${PLATFORM}" STREQUAL "soc")
  add_definitions(-DSOC_MODE=1)
  target_link_libraries(${TARGET_NAME} curses gflags_nothreads.a pthread bmlib dl)
elseif("${PLATFORM}" STREQUAL "pcie_arm64")
  target_link_libraries(${TARGET_NAME} curses gflags_nothreads.a pthread bmlib dl)
elseif("${PLATFORM}" STREQUAL "pcie_loongarch64")
  target_link_libraries(${TARGET_NAME} curses gflags_nothreads.a pthread bmlib dl)
else()
  target_link_libraries(${TARGET_NAME} ${CURSES_LIBRARY} gflags_nothreads.a pthread bmlib)
endif()

install(TARGETS ${TARGET_NAME}
  RUNTIME DESTINATION bin
  COMPONENT libsophon)
