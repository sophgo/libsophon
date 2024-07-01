cmake_minimum_required(VERSION 3.6)
cmake_policy(SET CMP0048 NEW)
set(TARGET_NAME "bmcpu")

project(${TARGET_NAME} C CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmlib/src)

file(GLOB TEST_SRC_PATH ./*.cpp)
foreach(src ${TEST_SRC_PATH})
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    target_link_libraries(${target} bmlib pthread dl)
    install(TARGETS ${target}
        RUNTIME DESTINATION bin
        COMPONENT libsophon)
endforeach(src)
