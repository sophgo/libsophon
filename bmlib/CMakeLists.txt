cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0046 NEW)
cmake_policy(SET CMP0048 NEW)
set(CMAKE_PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

set(TARGET_NAME "bmlib")

project(${TARGET_NAME} C CXX)



include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/linux)

set(SRCS
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
  src/bmlib_type.cpp
  src/bmcpu_app.cpp
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/debian-lib")
set(DEB_SCRIPT_TARGET "postinst")
add_custom_command(OUTPUT ${DEB_SCRIPT_TARGET}
    # post-install
    COMMAND echo "rm -f /opt/sophon/libsophon-current" > postinst
    COMMAND echo "ln -s ${CPACK_PACKAGING_INSTALL_PREFIX} /opt/sophon/libsophon-current" >> postinst
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
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/debian-lib
)
add_custom_target(libsophon_deb_script ALL DEPENDS ${DEB_SCRIPT_TARGET})

add_library(${TARGET_NAME} SHARED ${SRCS})
target_link_libraries(${TARGET_NAME} pthread -lrt -ldl)
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
    target_link_libraries(${TARGET_NAME} dl pthread -lrt)
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

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../build/tpu-bmodel/libmodel_combine.so
        DESTINATION lib
        COMPONENT libsophon)

if(BUILD_STATIC_LIB)
    set(STATIC_LIB_NAME ${TARGET_NAME}-static)

    add_library(${STATIC_LIB_NAME} STATIC ${SRCS})

    target_compile_options(${STATIC_LIB_NAME} PRIVATE 
        -fPIC
        -fvisibility=default
    )

    target_include_directories(${STATIC_LIB_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

    set_target_properties(${STATIC_LIB_NAME} PROPERTIES 
        OUTPUT_NAME ${TARGET_NAME}
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    target_link_libraries(${STATIC_LIB_NAME} pthread rt dl)

    add_custom_command(TARGET ${STATIC_LIB_NAME} POST_BUILD
        COMMAND ${CMAKE_RANLIB} $<TARGET_FILE:${STATIC_LIB_NAME}>
        COMMENT "Running ranlib on ${STATIC_LIB_NAME}"
    )

    install(TARGETS ${STATIC_LIB_NAME}
        ARCHIVE DESTINATION lib
        COMPONENT libsophon)
endif()

if(TARGET ${TARGET_NAME})
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${TARGET_NAME}>
            COMMENT "Stripping ${TARGET_NAME} after build"
        )
    else()
        message(STATUS "Debug mode: skipping strip for ${TARGET_NAME}")
    endif()
endif()

# if(TARGET ${STATIC_LIB_NAME})
#     add_custom_command(TARGET ${STATIC_LIB_NAME} POST_BUILD
#         COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${STATIC_LIB_NAME}>
#         COMMENT "Stripping ${STATIC_LIB_NAME} after build"
#     )
# endif()


if(NOT "${PLATFORM}" STREQUAL "cmodel")
    add_subdirectory(tools)
endif()
