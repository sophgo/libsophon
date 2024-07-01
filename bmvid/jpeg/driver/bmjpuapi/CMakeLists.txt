cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(libbmjpuapi)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bmjpulite/inc)
include_directories(${PROJECT_ROOT}/3rdparty/libbmcv/include)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

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

add_definitions(-DCHIP_BM1684)
add_definitions(-DUSE_ION_MEMORY)
add_definitions(-DBM_PCIE_MODE)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(SRCS
	src/bmjpuapi.c
	src/bmjpuapi_jpeg.c
	src/bmjpuapi_jpulib.c
	src/bmjpuapi_parse_jpeg.c)

add_library(libbmjpuapi SHARED ${SRCS})
target_link_libraries(libbmjpuapi libbmjpulite ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows/libbmlib.lib)

add_library(libbmjpuapi-static STATIC ${SRCS})
target_link_libraries(libbmjpuapi-static libbmjpulite-static ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows/libbmlib-static.lib)
install(TARGETS libbmjpuapi libbmjpuapi-static DESTINATION lib)

set(target bmjpegdec)
add_executable(${target} example/jpeg_dec.c example/jpeg_common.c example/getopt.c)
target_link_libraries(${target} libbmjpuapi-static libbmjpulite-static)
install(TARGETS ${target} DESTINATION bin/)

set(target bmjpegenc)
add_executable(${target} example/jpeg_enc.c example/jpeg_common.c example/getopt.c)
target_link_libraries(${target} libbmjpuapi-static libbmjpulite-static)
install(TARGETS ${target} DESTINATION bin/)

set(target bmjpegmul)
add_executable(${target} example/jpeg_multi.c example/bmjpurun.c example/jpeg_common.c example/getopt.c)
target_link_libraries(${target} libbmjpuapi-static libbmjpulite-static)
install(TARGETS ${target} DESTINATION bin/)

if(WIN32)
file(
	COPY ./inc/bm_jpeg_interface.h
	DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()
