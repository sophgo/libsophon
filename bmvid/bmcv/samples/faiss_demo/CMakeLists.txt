# Copyright (c) Facebook, Inc. and its affiliates.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(sgfaiss
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

set(FAISS_TPU_SRC
  TpuIndex.cpp
  FaissException.cpp;

)

set(FAISS_TPU_HEADERS
  TpuIndex.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
message(STATUS "lib path ${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(FAISS_TPU_HEADERS ${FAISS_TPU_HEADERS})

add_library(sgfaiss SHARED ${FAISS_TPU_SRC})
target_sources(sgfaiss PRIVATE ${FAISS_TPU_SRC})
target_link_libraries(sgfaiss bmcv bmlib)

message(STATUS "start demo compile...")
add_executable(TpuIndexFlatdemo  tutorial/TpuIndexFlatdemo.cpp)
target_link_libraries(TpuIndexFlatdemo  sgfaiss)
set_target_properties(TpuIndexFlatdemo PROPERTIES LINK_FLAGS "-Wl,-rpath,${CMAKE_CURRENT_SOURCE_DIR}/install")

install(TARGETS sgfaiss TpuIndexFlatdemo DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)
install(FILES ./lib/libbmcv.so ./lib/libbmlib.so DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)
install(FILES ./README DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)

