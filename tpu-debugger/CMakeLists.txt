cmake_minimum_required(VERSION 3.10)
project(tpu-debugger)

# Set toolchain
set(TOOLCHAIN_ARCH ${TOOLCHAIN} CACHE STRING "Toolchain architecture" FORCE)
if(TOOLCHAIN_ARCH STREQUAL "")
    set(TOOLCHAIN_ARCH "musl_arm64" CACHE STRING "Toolchain architecture" FORCE)
endif()
message(STATUS "toolchain arch: ${TOOLCHAIN_ARCH}")

# Include toolchains, maybe included from the parent project
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(${CMAKE_CURRENT_SOURCE_DIR}/toolchain.cmake)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -O0 -DMAGIC_ENUM_SUPPORTED")

# Extract Git commit hash and build date
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

message(STATUS "Building version: 0.1.0-dev (commit: ${GIT_COMMIT_HASH}, date: ${BUILD_DATE})")

# Add definitions to compile flags directly to ensure they are available in source files
add_compile_definitions(
    VERSION="0.1.0-dev"
    COMMIT_HASH="${GIT_COMMIT_HASH}"
    BUILD_DATE="${BUILD_DATE}"
)

# Define source files
set(SOURCES
    src/utils.cpp
    src/register.cpp
    src/engine.cpp
    src/chip.cpp
    src/readers.cpp
    src/debugger.cpp
    src/chip_registry.cpp
    src/json_dump.cpp
    src/json_reader.cpp
    src/json_dumper_base.cpp
    src/chips/bm1684x/bm1684x_json_dumper.cpp
    src/chips/bm1688/bm1688_json_dumper.cpp
    src/chips/cv184x/cv184x_json_dumper.cpp
)

# Define header files/directories
set(HEADERS_DIR
    include
    include/chips
)

# Create library from sources
add_library(${PROJECT_NAME}-lib ${SOURCES})

# Add include directories for the library
target_include_directories(${PROJECT_NAME}-lib PRIVATE ${HEADERS_DIR})

# Create executable
add_executable(${PROJECT_NAME} app/tpu-debugger.cpp)

# Link the library to the executable
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)
target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:${PROJECT_NAME}-lib>" "-Wl,--no-whole-archive")

# Add include directories for the executable
target_include_directories(${PROJECT_NAME} PRIVATE ${HEADERS_DIR})

# Add system libraries that may be needed
target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS} -lpthread)

# Installation rules (optional)
# Set default install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(STATUS "Setting default install prefix to ${CMAKE_SOURCE_DIR}/install")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Default install prefix" FORCE)
endif()
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin)

# For development purposes, you might want to add these useful targets:
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# Enable testing if needed
#enable_testing()
