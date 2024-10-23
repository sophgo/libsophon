cmake_minimum_required(VERSION 3.6)
cmake_policy(SET CMP0048 NEW)

project("libsophon"
    VERSION 0.4.10)

set(CMAKE_CXX_STANDARD 11)

message(STATUS "PROJECT .................... " ${PROJECT_NAME})
message(STATUS "VERSION .................... " ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
message(STATUS "CMAKE_VERSION .............. " ${CMAKE_VERSION})
message(STATUS "CMAKE_BINARY_DIR ........... " ${CMAKE_BINARY_DIR})
message(STATUS "CMAKE_SOURCE_DIR ........... " ${CMAKE_SOURCE_DIR})
message(STATUS "CMAKE_GENERATOR ............ " ${CMAKE_GENERATOR})
message(STATUS "CMAKE_C_COMPILER_ID ........ " ${CMAKE_C_COMPILER_ID})
message(STATUS "CMAKE_CXX_COMPILER_ID ...... " ${CMAKE_CXX_COMPILER_ID})
message(STATUS "CMAKE_BUILD_TYPE ........... " ${CMAKE_BUILD_TYPE})
message(STATUS "CMAKE_TOOLCHAIN_FILE ....... " ${CMAKE_TOOLCHAIN_FILE})
message(STATUS "TARGET ..................... " ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "SOC_KERNEL_PATH............. " ${SOC_LINUX_DIR})

string(TIMESTAMP COMPILE_TIME %Y%m%d-%H%M%S)
set(BUILD_DATE ${COMPILE_TIME})

configure_file("${CMAKE_SOURCE_DIR}/bmlib/include/version.h.in"
    "${CMAKE_SOURCE_DIR}/bm-smi/include/version.h")
configure_file("${CMAKE_SOURCE_DIR}/bmlib/include/version.h.in"
    "${CMAKE_SOURCE_DIR}/driver/version.h")

if(WIN32)
    include(windows.cmake)

else()
    # cpack common config
    include(InstallRequiredSystemLibraries)
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "riscv64")
        set(CPACK_GENERATOR "RPM;TGZ" CACHE STRING "CPack Generator")
        set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    else()
        set(CPACK_GENERATOR "DEB;TGZ" CACHE STRING "CPack Generator")
        set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    endif()
    set(CPACK_GENERATOR "DEB;TGZ" CACHE STRING "CPack Generator")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    if("${PLATFORM}" STREQUAL "soc")
        set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}_soc_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR})
    else()
        set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR})
    endif()
    set(CPACK_PACKAGING_INSTALL_PREFIX  "/opt/sophon/libsophon-${CPACK_PACKAGE_VERSION}")
    # make an extra level, so driver can be put under install folder
    set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/libsophon-${CPACK_PACKAGE_VERSION}")

    # DEB config
    set(CPACK_DEB_COMPONENT_INSTALL YES)
    set(CPACK_COMPONENTS_GROUPING IGNORE)
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        set(DEB_ARCH "amd64")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
        set(DEB_ARCH "arm64")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "riscv64")
        set(DEB_ARCH "riscv64")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "loongarch64")
        set(DEB_ARCH "loongarch64")
    else()
        message(FATAL_ERROR "Unknown processor:" ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    if("${PLATFORM}" STREQUAL "soc")
        set(CPACK_DEBIAN_PACKAGE_NAME "sophon-soc")
    else()
        set(CPACK_DEBIAN_PACKAGE_NAME "sophon")
    endif()
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "sophon team")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${DEB_ARCH})
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})
    # DEB dependency
    if("${PLATFORM}" STREQUAL "soc")
        set(CPACK_DEBIAN_LIBSOPHON-DEV_PACKAGE_DEPENDS "libc6 (>= 2.23), sophon-soc-libsophon (= ${CPACK_DEBIAN_PACKAGE_VERSION})")
    else()
        set(CPACK_DEBIAN_LIBSOPHON-DEV_PACKAGE_DEPENDS "libc6 (>= 2.23), sophon-libsophon (= ${CPACK_DEBIAN_PACKAGE_VERSION})")
    endif()
    set(CPACK_DEBIAN_LIBSOPHON_PACKAGE_DEPENDS "libc6 (>= 2.23), libncurses5 (>= 6.0)")
    set(CPACK_DEBIAN_DRIVER_PACKAGE_DEPENDS "dkms")
    # driver package extra
    set(CPACK_DEBIAN_DRIVER_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_BINARY_DIR}/driver/debian/postinst;"
        "${CMAKE_CURRENT_BINARY_DIR}/driver/debian/prerm;")
    # libsophon package extra
    set(CPACK_DEBIAN_LIBSOPHON_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_BINARY_DIR}/bmlib/debian-lib/postinst;"
        "${CMAKE_CURRENT_BINARY_DIR}/bmlib/debian-lib/prerm"
        "${CMAKE_CURRENT_BINARY_DIR}/bmlib/debian-lib/postrm")
    # libsophon-dev package extra
    set(CPACK_DEBIAN_LIBSOPHON-DEV_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/bmlib/debian-dev/postinst;"
        "${CMAKE_SOURCE_DIR}/bmlib/debian-dev/prerm")

    #RPM config
    set(CPACK_RPM_PACKAGE_AUTOREQPROV OFF)
    set(CPACK_RPM_COMPONENT_INSTALL YES)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "arm64")
    endif()
    if("${PLATFORM}" STREQUAL "soc")
        set(CPACK_RPM_PACKAGE_NAME "sophon-soc")
    else()
        set(CPACK_RPM_PACKAGE_NAME "sophon")
    endif()
    # RPM dependency
    if("${PLATFORM}" STREQUAL "soc")
        set(CPACK_RPM_LIBSOPHON-DEV_PACKAGE_REQUIRES "sophon-soc-libsophon = ${CPACK_DEBIAN_PACKAGE_VERSION}")
    else()
        set(CPACK_RPM_LIBSOPHON-DEV_PACKAGE_REQUIRES "sophon-libsophon = ${CPACK_DEBIAN_PACKAGE_VERSION}")
    endif()
    set(CPACK_RPM_PACKAGE_REQUIRES "dkms, kernel-devel, systemd, pciutils, redhat-lsb-core")

    SET(CPACK_RPM_DRIVER_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/driver/debian/postinst")
    set(CPACK_RPM_DRIVER_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/driver/debian/prerm")
    SET(CPACK_RPM_LIBSOPHON_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/bmlib/debian-lib/postinst")
    SET(CPACK_RPM_LIBSOPHON_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/bmlib/debian-lib/prerm")
    SET(CPACK_RPM_LIBSOPHON_POST_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/bmlib/debian-lib/postrm")
    SET(CPACK_RPM_LIBSOPHON-DEV_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_SOURCE_DIR}/bmlib/debian-dev/postinst")
    SET(CPACK_RPM_LIBSOPHON-DEV_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_SOURCE_DIR}/bmlib/debian-dev/prerm")
    SET(CHIP_NAME bm1686 CACHE STRING "chip: bm1686")

    function (optional_add_subdirectory d)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${d}/CMakeLists.txt")
            add_subdirectory(${d})
        endif()
    endfunction()

    # subdirectories may depend on former settings
    add_subdirectory(bmlib)
    if("${PLATFORM}" STREQUAL "soc")
        optional_add_subdirectory(tpu-cpuop)
        if(EXISTS "${CMAKE_SOURCE_DIR}/tpu-bmodel/CMakeLists.txt")
            add_subdirectory(tpu-bmodel)
        else()
            message(FATAL_ERROR "The tpu-bmodel directory does not contain a valid CMakeLists.txt file.\nPlease run 'git submodule update --init' in libsophon")
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/tpu-runtime/CMakeLists.txt")
            add_subdirectory(tpu-runtime)
        else()
            message(FATAL_ERROR "The tpu-runtime directory does not contain a valid CMakeLists.txt file.\nPlease run 'git submodule update --init' in libsophon")
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/libsophav/CMakeLists.txt")
            add_subdirectory(libsophav)
        else()
            message(FATAL_ERROR "The libsophav directory does not contain a valid CMakeLists.txt file.\nPlease run 'git submodule update --init' in libsophon")
        endif()
    endif()
    if (NOT "${PLATFORM}" STREQUAL "cmodel")
        add_subdirectory(doc)
        add_subdirectory(driver)
        add_subdirectory(bm-smi)
        add_dependencies(bm-smi bmlib)
    endif()

    # remove unspecified component introduced by libyuv
    get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
    list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

    # run cpack after subdirectories, so it would pack by component
    include(CPack)
    
endif()
