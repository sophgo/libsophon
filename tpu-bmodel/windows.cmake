if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../bm_prebuilt_toolchains_win CACHE PATH "path to prebuilt toolchain")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(OUT_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)

if(TARGET_TYPE STREQUAL "release")
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD")
        set(CMAKE_C_FLAGS_RELEASE "/MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(CMAKE_C_FLAGS_RELEASE "/MT")
    endif()
else()
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_DEBUG "/MDd")
        set(CMAKE_C_FLAGS_DEBUG "/MDd")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_C_FLAGS_DEBUG "/MTd")
    endif()
endif()

set(srcs
    src/bmodel.cpp
    tools/md5/md5.cpp
    tools/flatbuffers/idl_parser.cpp
    tools/flatbuffers/util.cpp
    tools/flatbuffers/idl_gen_text.cpp
    tools/flatbuffers/code_generators.cpp)

add_library(libbmodel-static STATIC ${srcs})
target_include_directories(libbmodel-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(libbmodel-static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(libbmodel-static PUBLIC cxx_std_11)

include(git-utils)
get_version_from_tag(version soversion revision)

add_executable(tpu_model tools/tpu_model.cpp)
target_link_libraries(tpu_model libbmodel-static)
target_compile_definitions(tpu_model PRIVATE VER="${revision}")
target_compile_features(tpu_model PUBLIC cxx_std_11)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${OUT_INCLUDE_DIR}
    PATTERN *.hpp
    PATTERN *.h)