# ===================================================================================
#  The LIBSOPHON CMake configuration file
#
#  Usage from an external project:
#    In your CMakeLists.txt, add these lines:
#
#    set(LIBSOPHON_DIR  path_to_libsophon/data)
#    find_package(LIBSOPHON REQUIRED)
#    include_directories(${LIBSOPHON_INCLUDE_DIRS})
#
#    # link all libsophon libraries
#    target_link_libraries(MY_TARGET_NAME ${LIBSOPHON_LIBS})
#    # link libsophon xxx library, e.g., link libbmcv.lib:
#    target_link_libraries(MY_TARGET_NAME ${the_libbmcv.lib})
#
#    This file will define the following variables:
#      - LIBSOPHON_LIBS                     : The list of all libs for LIBSOPHON modules.
#      - LIBSOPHON_INCLUDE_DIRS             : The LIBSOPHON include directories.
#      - LIBSOPHON_LIB_DIRS                 : The LIBSOPHON library directories.
#      - the_xxx.lib                        : A LIBSOPHON library, e.g., ${the_libbmcv.lib} is the path of libbmcv.lib.
#
# ===================================================================================


set(LIBSOPHON_INCLUDE_DIRS "${LIBSOPHON_DIR}/../include")
set(LIBSOPHON_LIB_DIRS "${LIBSOPHON_DIR}/../lib")

file(GLOB lib_names RELATIVE ${LIBSOPHON_LIB_DIRS} ${LIBSOPHON_LIB_DIRS}/*.lib)

foreach(name ${lib_names})
    find_library(the_${name} "${name}" PATHS ${LIBSOPHON_LIB_DIRS})
    if(the_${name})
        list(APPEND LIBSOPHON_LIBS "${the_${name}}")
    endif()
endforeach()

