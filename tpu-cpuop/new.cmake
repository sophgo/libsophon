cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0046 NEW)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Install prefix")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

project(cpu-ops LANGUAGES CXX VERSION 1.0.0)
if (CMAKE_VERSION VERSION_LESS 3.12)
  set(CMAKE_PROJECT_VERSION 1.0.0)
endif()

file(GLOB_RECURSE srcs src/*.cpp)
add_library(cpuop SHARED ${srcs})
target_include_directories(cpuop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(cpuop PUBLIC cxx_std_11)
set_target_properties(cpuop PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION})

file(GLOB_RECURSE user_srcs user/*.cpp)
add_library(usercpu SHARED ${user_srcs})
target_compile_features(usercpu PUBLIC cxx_std_11)
target_include_directories(usercpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(usercpu PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION})

set(ENABLE_TESTING OFF CACHE BOOL "Enable testings")
if (ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS cpuop usercpu
    LIBRARY DESTINATION lib
    COMPONENT libsophon)
install(FILES
    include/bmcpu.h
    include/bmcpu_common.h
    user/user_bmcpu.h
    user/user_bmcpu_common.h
    DESTINATION include
    COMPONENT libsophon)
