# CMakeLists for libyuv
# Originally created for "roxlu build system" to compile libyuv on windows
# Run with -DTEST=ON to build unit tests

PROJECT ( YUV C CXX )	# "C" is required even for C++ projects
CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

option(ENABLE_PIC "Generate position independent code (necessary for shared libraries)" TRUE)
option(ENABLE_NEON                "Enable NEON instructions"                            ON)
option(TEST_OPT "Built unit tests" ON )
if(WINDOWS)

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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

option(TEST_OPT "Built unit tests" OFF )
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ${ENABLE_PIC})
SET ( ly_base_dir	${PROJECT_SOURCE_DIR} )
SET ( ly_src_dir	${ly_base_dir}/source )
SET ( ly_inc_dir	${ly_base_dir}/include )
SET ( ly_tst_dir	${ly_base_dir}/unit_test )
SET ( ly_lib_name	yuv )
SET ( ly_lib_static	${ly_lib_name} )
SET ( ly_lib_shared	${ly_lib_name}_shared )

FILE ( GLOB_RECURSE	ly_source_files ${ly_src_dir}/*.cc )
LIST ( SORT			ly_source_files )

FILE ( GLOB_RECURSE	ly_unittest_sources ${ly_tst_dir}/*.cc )
LIST ( SORT			ly_unittest_sources )

INCLUDE_DIRECTORIES( BEFORE ${ly_inc_dir} )

execute_process(
    COMMAND bash -c "git describe --tags --match v* | sed -e 's/.*\\([0-9][0-9]*\\.[0-9][0-9]*\\.[0-9][0-9]*\\).*/\\1/'"
    OUTPUT_VARIABLE SO_VERSION)
if(NOT "${SO_VERSION}" STREQUAL "")
    STRING(REGEX REPLACE "[\n\t\r]" "" SO_VERSION ${SO_VERSION})
    STRING(REGEX MATCH "[0-9]*" SO_NAME ${SO_VERSION})
endif()

# this creates the static library (.a)
ADD_LIBRARY				( ${ly_lib_static} STATIC ${ly_source_files} )

# this creates the shared library (.so)
ADD_LIBRARY				( ${ly_lib_shared} SHARED ${ly_source_files} )
SET_TARGET_PROPERTIES	( ${ly_lib_shared} PROPERTIES OUTPUT_NAME "${ly_lib_name}" )
SET_TARGET_PROPERTIES	( ${ly_lib_shared} PROPERTIES PREFIX "lib" )
if (NOT "${SO_VERSION}" STREQUAL "")
    SET_TARGET_PROPERTIES   ( ${ly_lib_shared} PROPERTIES SOVERSION ${SO_NAME} VERSION ${SO_VERSION})
endif()
# custom definition of bm libyuv.so
target_compile_definitions ( ${ly_lib_shared} PUBLIC -DLIBYUV_BUILDING_SHARED_LIBRARY )

# this creates the conversion tool
ADD_EXECUTABLE			( yuvconvert ${ly_base_dir}/util/yuvconvert.cc )
TARGET_LINK_LIBRARIES	( yuvconvert ${ly_lib_static} )

#INCLUDE ( FindJPEG )
#if (JPEG_FOUND)
#  include_directories( ${JPEG_INCLUDE_DIR} )
#  target_link_libraries( yuvconvert ${JPEG_LIBRARY} )
#  add_definitions( -DHAVE_JPEG )
#endif()
if(TEST_OPT)
  find_library(GTEST_LIBRARY gtest)
  if(GTEST_LIBRARY STREQUAL "GTEST_LIBRARY-NOTFOUND")
    set(GTEST_SRC_DIR ${ly_base_dir}/../3rdparty-tools/googletest/googletest CACHE STRING "Location of gtest sources")
    if(EXISTS ${GTEST_SRC_DIR}/src/gtest-all.cc)
      message(STATUS "building gtest from sources in ${GTEST_SRC_DIR}")
      set(gtest_sources ${GTEST_SRC_DIR}/src/gtest-all.cc)
      add_library(gtest STATIC ${gtest_sources})
      include_directories(${GTEST_SRC_DIR})
      include_directories(${GTEST_SRC_DIR}/include)
      set(GTEST_LIBRARY gtest)
    else()
      message(FATAL_ERROR "TEST is set but unable to find gtest library")
    endif()
  endif()

  add_executable(libyuv_unittest ${ly_unittest_sources})
  target_link_libraries(libyuv_unittest ${ly_lib_name} ${GTEST_LIBRARY})
  add_definitions(-DHAVE_PTHREAD=1)

  #find_library(PTHREAD_LIBRARY pthread)
  #target_link_libraries(libyuv_unittest pthread)
  #if(NOT PTHREAD_LIBRARY STREQUAL "PTHREAD_LIBRARY-NOTFOUND")
    target_link_libraries(libyuv_unittest pthread)
  #endif()
  #if (JPEG_FOUND)
  #  target_link_libraries(libyuv_unittest ${JPEG_LIBRARY})
  #endif()

  if(NACL AND NACL_LIBC STREQUAL "newlib")
    target_link_libraries(libyuv_unittest glibc-compat)
  endif()

  find_library(GFLAGS_LIBRARY gflags)
  if(NOT GFLAGS_LIBRARY STREQUAL "GFLAGS_LIBRARY-NOTFOUND")
    target_link_libraries(libyuv_unittest gflags)
    add_definitions(-DLIBYUV_USE_GFLAGS)
  endif()
endif()


# install the conversion tool, .so, .a, and all the header files
#INSTALL ( PROGRAMS ${CMAKE_BINARY_DIR}/yuvconvert			DESTINATION bin )
INSTALL ( TARGETS ${ly_lib_static}						DESTINATION lib )
INSTALL ( TARGETS ${ly_lib_shared} LIBRARY				DESTINATION lib RUNTIME DESTINATION bin )
INSTALL ( DIRECTORY ${PROJECT_SOURCE_DIR}/include/		DESTINATION include )

# create the .deb and .rpm packages using cpack
INCLUDE ( CM_linux_packages.cmake )

if(WIN32)
file(
    COPY ./include/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()