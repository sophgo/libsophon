cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
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
if(LINUX)
set(SRCS
  src/common.c
  src/global_conv_data_split.c
  src/global_lstm_data_split.c
  src/global_pooling_data_split.c
  src/global_upsample_data_split.c
  src/util.c
 )
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../config)

elseif(WINDOWS)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(SRCS
  src/common.c
  # src/global_conv_data_split.c
  # src/global_lstm_data_split.c
  # src/global_pooling_data_split.c
  # src/global_upsample_data_split.c
  src/util.c
 )
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include_win)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../config)
endif()

add_library(libcommon SHARED ${SRCS})
add_library(libcommon-static STATIC ${SRCS})
install(TARGETS libcommon libcommon-static DESTINATION lib)


