cmake_minimum_required(VERSION 2.8)
project(bmrt)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

add_definitions(-DDEBUG --std=c++11 -fPIC -Wall -Werror)
set(CMAKE_BUILD_TYPE "Debug")

if(DEFINED ENV{REL_TOP})
  include_directories($ENV{REL_TOP}/include/bmruntime)
  include_directories($ENV{REL_TOP}/include/bmlib)
  link_directories($ENV{REL_TOP}/lib/bmnn/pcie)
else()
  message("source scripts first!")
endif()

add_executable(demo demo.cpp)
target_link_libraries(demo bmrt bmlib)
