cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(libbmjpulite)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../jdi)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../jpuapi)

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
add_definitions(-DLIBBMJPULITE)
add_definitions(-DBM_PCIE_MODE)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(SRCS
	../jdi/linux/jdi.c
	src/jpu_lib.c
	src/jpu_io.c
	src/jpu_lib_dec.c
	src/jpu_lib_enc.c
	src/jpu_logging.c
	../jdi/linux/jdi.c
	../jdi/mm.c
	../jpuapi/jpuapi.c
	../jpuapi/jpuapifunc.c
	../bmjpulite/src/jpulog.c)

add_library(libbmjpulite SHARED ${SRCS})
target_link_libraries(libbmjpulite)

add_library(libbmjpulite-static STATIC ${SRCS})
target_link_libraries(libbmjpulite-static)

install(TARGETS libbmjpulite libbmjpulite-static DESTINATION lib)

if(WIN32)
file(
	COPY ./inc/jpu_io.h ./inc/jpu_lib.h ./inc/jpu_logging.h
	DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()