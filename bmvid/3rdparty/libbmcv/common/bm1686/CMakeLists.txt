cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0046 NEW)
project(cmodel C)

set(SRCS
  src/common.c
  src/global_conv_data_split.c
  src/global_lstm_data_split.c
  src/global_pooling_data_split.c
  src/global_upsample_data_split.c
  src/util.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../config)
add_library(common SHARED ${SRCS})
add_library(common-static STATIC ${SRCS})
install(TARGETS common common-static DESTINATION lib)
