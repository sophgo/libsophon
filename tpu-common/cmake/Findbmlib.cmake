if (TARGET bmlib)
    add_library(bmlib::bmlib ALIAS bmlib)
    return()
endif()

include(FindPackageHandleStandardArgs)

find_path(
    bmlib_INCLUDE_DIR
    NAMES bmlib_runtime.h
    HINTS ${CMAKE_SOURCE_DIR}/../sophon-sdk/install/include)
find_library(
    bmlib_LIBRARY
    NAMES bmlib
    HINTS ${CMAKE_SOURCE_DIR}/../sophon-sdk/install/lib)

find_package_handle_standard_args(
    bmlib
    REQUIRED_VARS bmlib_INCLUDE_DIR bmlib_LIBRARY)

if (bmlib_FOUND)
    add_library(bmlib::bmlib IMPORTED SHARED)
    set_target_properties(
        bmlib::bmlib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${bmlib_INCLUDE_DIR}
        IMPORTED_LOCATION ${bmlib_LIBRARY})
endif()
