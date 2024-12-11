cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0046 NEW)
cmake_policy(SET CMP0048 NEW)
set(CMAKE_PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

set(TARGET_NAME "bmlib")

project(${TARGET_NAME} C CXX)

if(WIN32)

include(windows.cmake)

else()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/linux)

set(SRCS
  src/bmkernel_runtime.cpp
  src/bmlib_mmpool.cpp
  src/bmcpu_runtime.cpp
  src/bmlib_runtime.cpp
  src/bmlib_util.cpp
  src/bmlib_log.cpp
  src/bmlib_device.cpp
  src/bmlib_memory.cpp
  src/a53lite_api.cpp
  src/bmlib_profile.cpp
  src/linux/bmlib_ioctl.cpp
  src/bmlib_md5.cpp
  src/rbtree.c
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/debian-lib")
set(DEB_SCRIPT_TARGET "postinst")
add_custom_command(OUTPUT ${DEB_SCRIPT_TARGET}
    # post-install
    COMMAND echo "rm -f /opt/sophon/libsophon-current" > postinst
    COMMAND echo "ln -s ${CPACK_PACKAGING_INSTALL_PREFIX} /opt/sophon/libsophon-current" >> postinst
    #COMMAND echo "ln -s ${CPACK_PACKAGING_INSTALL_PREFIX} /opt/sophon/libsophon-0.4.10" >> postinst
    COMMAND echo >> postinst
    COMMAND echo "cp /opt/sophon/libsophon-current/data/libsophon.conf /etc/ld.so.conf.d/" >> postinst
    COMMAND echo "ldconfig" >> postinst
    COMMAND echo "cp /opt/sophon/libsophon-current/data/libsophon-bin-path.sh /etc/profile.d/" >> postinst
    # pre-remove
    COMMAND echo "rm -f /etc/ld.so.conf.d/libsophon.conf" > prerm
    COMMAND echo "ldconfig" >> prerm
    COMMAND echo "rm -f /etc/profile.d/libsophon-bin-path.sh" >> prerm
    # post-remove
    COMMAND echo "rm -f /opt/sophon/libsophon-current" > postrm
    COMMAND echo "rm -rf /opt/sophon/libsophon-0.4.10" >> postrm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/debian-lib
)
add_custom_target(libsophon_deb_script ALL DEPENDS ${DEB_SCRIPT_TARGET})

add_library(${TARGET_NAME} SHARED ${SRCS})
add_definitions(-DUSING_INT_CDMA=1)
if("${PLATFORM}" STREQUAL "soc")
    add_definitions(-DSOC_MODE=1)
endif()
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
string(REGEX MATCH "[0-9]+" sover "${CMAKE_PROJECT_VERSION}")

set_target_properties(${TARGET_NAME} PROPERTIES SOVERSION ${sover})

if("${PLATFORM}" STREQUAL "cmodel")
    message(STATUS "USING_CMODEL")
    target_compile_definitions(${TARGET_NAME} PUBLIC -DUSING_CMODEL=1)
    target_link_libraries(${TARGET_NAME} dl pthread)
endif()

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "riscv64")
    add_definitions(-DSMMU_MODE=1)
endif()

install(TARGETS ${TARGET_NAME}
        LIBRARY DESTINATION lib
        COMPONENT libsophon)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/debian-lib/libsophon.conf
        DESTINATION ./data
        COMPONENT libsophon)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/debian-lib/libsophon-bin-path.sh
        DESTINATION ./data
        COMPONENT libsophon)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/debian-dev/libsophon-config.cmake
        DESTINATION ./data
        COMPONENT libsophon-dev)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION "include"
        COMPONENT libsophon-dev
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "*.h")

if(BUILD_STATIC_LIB)
    set(STATIC_LIB_NAME ${TARGET_NAME}-static)
    add_library(${STATIC_LIB_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}>)
    target_compile_options(${STATIC_LIB_NAME} PRIVATE -fPIC)
    target_include_directories(${STATIC_LIB_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(${STATIC_LIB_NAME} PROPERTIES SOVERSION ${sover})
    set_target_properties(${STATIC_LIB_NAME} PROPERTIES OUTPUT_NAME ${TARGET_NAME})
    install(TARGETS ${STATIC_LIB_NAME}
        LIBRARY DESTINATION lib
        COMPONENT libsophon)
endif()

if(NOT "${PLATFORM}" STREQUAL "cmodel")
    add_subdirectory(tools)
endif()

endif()
