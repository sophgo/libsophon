cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0046 NEW)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Install prefix")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
project(tpu-bmodel LANGUAGES CXX)

if(WIN32)
    include(windows.cmake)

else()
    set(srcs
        src/bmodel.cpp
        tools/md5/md5.cpp
        tools/flatbuffers/idl_parser.cpp
        tools/flatbuffers/util.cpp
        tools/flatbuffers/idl_gen_text.cpp
        tools/flatbuffers/code_generators.cpp)

    add_library(bmodel STATIC ${srcs})
    target_include_directories(bmodel PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(bmodel PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_features(bmodel PUBLIC cxx_std_11)

    include(git-utils)
    get_version_from_tag(version soversion revision)

    add_executable(tpu_model tools/tpu_model.cpp)
    target_link_libraries(tpu_model bmodel)
    target_compile_definitions(tpu_model PRIVATE VER="${revision}")
    target_compile_features(tpu_model PUBLIC cxx_std_11)

    install(TARGETS tpu_model bmodel
        RUNTIME DESTINATION bin
        COMPONENT libsophon
        ARCHIVE DESTINATION lib
        COMPONENT libsophon)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION "include"
        COMPONENT libsophon-dev
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")
endif()