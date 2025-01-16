cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)
set(TARGET_NAME "bmcpu")

project(${TARGET_NAME} C CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../bmlib/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../bmlib/src)


# set(BMLIB_STATIC_LIB "/workspace/libsophon/build/bmlib/libbmlib.a")


link_directories("/workspace/libsophon/build/bmlib/")

file(GLOB TEST_SRC_PATH ./a53lite_repeat_loadlib.cpp)
foreach(src ${TEST_SRC_PATH})
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    # target_link_libraries(${target} pthread ${BMLIB_STATIC_LIB} dl -lrt)

    target_link_libraries(${target} bmlib pthread dl -lrt)
    install(TARGETS ${target}
        RUNTIME DESTINATION bin
        COMPONENT libsophon)
endforeach(src)

