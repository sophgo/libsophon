cmake_minimum_required(VERSION 3.6)
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
    target_compile_options(bmodel PRIVATE -fPIC)
    target_include_directories(bmodel PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(bmodel PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_features(bmodel PUBLIC cxx_std_11)

    #include(git-utils)
    #get_version_from_tag(version soversion revision)

    set(revision "1.0")
    add_executable(tpu_model tools/tpu_model.cpp tools/model_tool.cpp)
    target_link_libraries(tpu_model bmodel ${CMAKE_DL_LIBS})
    add_library(model_combine SHARED tools/tpu_model.cpp tools/model_combine.cpp)
    target_link_libraries(model_combine bmodel ${CMAKE_DL_LIBS})
    add_executable(demo tools/demo.cpp)
    target_link_libraries(demo model_combine ${CMAKE_DL_LIBS})
    target_compile_definitions(tpu_model PRIVATE VER="${revision}")
    target_compile_features(tpu_model PUBLIC cxx_std_11)

    add_custom_target(model_tool_link ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink tpu_model model_tool
        DEPENDS tpu_model)
    install(PROGRAMS
        "${CMAKE_CURRENT_BINARY_DIR}/model_tool"
        DESTINATION bin
        COMPONENT libsophon
    )
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
