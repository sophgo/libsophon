cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0046 NEW)
cmake_policy(SET CMP0048 NEW)

set(TARGET_NAME "bm-smi")
# set(CMAKE_VERBOSE_MAKEFILE ON)

project(${TARGET_NAME} C CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/src)

# 输出一下这个路径
message(STATUS "LIB_DIR: ${LIB_DIR}")
link_directories(${LIB_DIR}/lib/)
include_directories(${LIB_DIR}/include)
add_definitions(-DSOC_MODE=1)

SET(SRCS
    src/bm-smi.cpp
    src/bm_smi_cmdline.cpp
    src/bm_smi_test.cpp
    src/bm_smi_creator.cpp
    src/bm_smi_display.cpp
)

add_executable(${TARGET_NAME} ${SRCS})


add_definitions(-DSOC_MODE=1)
target_link_libraries(${TARGET_NAME} ncurses gflags_nothreads pthread bmlib dl)

install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION bin
    COMPONENT libsophon)