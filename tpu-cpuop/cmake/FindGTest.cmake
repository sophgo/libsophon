include(FindPackageHandleStandardArgs)

set(gtest_src_dir /usr/src/gtest)
if (EXISTS ${gtest_src_dir})
    add_subdirectory(${gtest_src_dir}
        ${CMAKE_CURRENT_BINARY_DIR}/gtest
        EXCLUDE_FROM_ALL)
    add_library(GTest::gtest ALIAS gtest)
    add_library(GTest::main ALIAS gtest_main)
    return()
endif()

find_path(
    GTest_INCLUDE_DIR
    NAMES gtest/gtest.h)

find_library(
    GTest_LIBRARY
    NAMES gtest)

find_library(
    GTest_main_LIBRARY
    NAMES gtest_main)

find_package_handle_standard_args(
    GTest
    REQUIRED_VARS GTest_INCLUDE_DIR GTest_LIBRARY GTest_main_LIBRARY)

if (GTest_FOUND)
    add_library(GTest::gtest IMPORTED SHARED)
    set_target_properties(
        GTest::gtest PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${GTest_INCLUDE_DIR}
        IMPORTED_LOCATION "${GTest_LIBRARY}")
    add_library(GTest::main IMPORTED SHARED)
    set_target_properties(
        GTest::main PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${GTest_INCLUDE_DIR}
        IMPORTED_LOCATION "${GTest_main_LIBRARY}")
endif()
