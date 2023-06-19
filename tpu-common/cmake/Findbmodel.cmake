if (TARGET bmodel)
    add_library(bmodel::bmodel ALIAS bmodel)
    return()
endif()

include(FindPackageHandleStandardArgs)

find_path(
    bmodel_INCLUDE_DIR
    NAMES bmodel.hpp
    HINTS ${CMAKE_SOURCE_DIR}/../tpu-bmodel/build/install/include)
find_library(
    bmodel_LIBRARY
    NAMES bmodel
    HINTS ${CMAKE_SOURCE_DIR}/../tpu-bmodel/build/install/lib)

find_package_handle_standard_args(
    bmodel
    REQUIRED_VARS bmodel_INCLUDE_DIR bmodel_LIBRARY)

if (bmodel_FOUND)
    add_library(bmodel::bmodel IMPORTED SHARED)
    set_target_properties(
        bmodel::bmodel PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${bmodel_INCLUDE_DIR}
        IMPORTED_LOCATION ${bmodel_LIBRARY})
endif()

