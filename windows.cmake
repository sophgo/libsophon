set(RELEASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release)
set(CMAKE_INSTALL_PREFIX ${RELEASE_DIR}/libsophon CACHE PATH "install path" FORCE)
set(CACHED_INSTALL_PATH TRUE)
set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../bm_prebuilt_toolchains_win CACHE PATH "path to prebuilt toolchain")

add_subdirectory(bmlib)
add_subdirectory(bm-smi)
add_subdirectory(tpu-bmodel)
add_subdirectory(tpu-cpuop)
add_subdirectory(tpu-runtime)
add_subdirectory(bmvid)

add_custom_target(PACK
    COMMAND cd ${RELEASE_DIR}
    COMMAND ren libsophon libsophon-${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E tar "cfv"
    "${PROJECT_NAME}_win_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR}.zip"
    --format=zip
    "libsophon-${PROJECT_VERSION}/")