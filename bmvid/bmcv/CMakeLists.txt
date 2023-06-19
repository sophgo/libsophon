cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(bmcv)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "PROJECT_ROOT  ${PROJECT_ROOT}")
message(STATUS "$ENV{TARGET_PROJECT}_CMAKE ")
message(STATUS "CMAKE_INSTALL_PREFIX  ${CMAKE_INSTALL_PREFIX}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../vpp/driver/include/bm1684/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../jpeg/driver/bmjpuapi/inc/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/libjpeg-turbo/binary/include) #for turbo jpeg
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/libbmcv/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(
    ${PROJECT_ROOT}/../libsophon/release/libsophon/lib
    ${PROJECT_ROOT}/../libsophon/bmlib/release/lib
    ${PROJECT_ROOT}/../../libsophon/release/libsophon/lib
    ${PROJECT_ROOT}/../../libsophon/bmlib/release/lib
    ${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/libjpeg-turbo/binary/lib/windows) #for turbo jpeg

if(TARGET_TYPE STREQUAL "release")
    if (RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD")
        set(CMAKE_C_FLAGS_RELEASE "/MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(CMAKE_C_FLAGS_RELEASE "/MT")
    endif()
else()
    if (RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_DEBUG "/MDd")
        set(CMAKE_C_FLAGS_DEBUG "/MDd")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_C_FLAGS_DEBUG "/MTd")
    endif()
endif()


if(WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

    set(SRCS
        src/bmcv_api_absdiff.cpp
        src/bmcv_api_add_weighted.cpp
        src/bmcv_api_bitwise.cpp
        src/bmcv_api_base64.cpp
        src/bmcv_api_batch_topk.cpp
        src/bmcv_api_bgrsplit.cpp
        src/bmcv_api_calc_hist.cpp
        src/bmcv_api_canny.cpp
        src/bmcv_api_cmulp.cpp
        src/bmcv_api_convert_to.cpp
        src/bmcv_api_copy_to.cpp
        src/bmcv_api_crop.cpp
        src/bmcv_api_cv_sort.cpp
        src/bmcv_api_dct.cpp
        src/bmcv_api_distance.cpp
        src/bmcv_api_draw_lines.cpp
        src/bmcv_api_feature_match.cpp
        src/bmcv_api_fft.cpp
        src/bmcv_api_fill_rectangle.cpp
        src/bmcv_api_gaussian_blur.cpp
        src/bmcv_api_gemm.cpp
        src/bmcv_api_fusion.cpp
        src/bmcv_api_img_scale.cpp
        src/bmcv_api_jpeg_dec.cpp
        src/bmcv_api_jpeg_enc.cpp
        src/bmcv_api_lkpyramid.cpp
        src/bmcv_api_matmul.cpp
        src/bmcv_api_min_max.cpp
        src/bmcv_api_morph.cpp
        src/bmcv_api_nms.cpp
        src/bmcv_api_put_text.cpp
        src/bmcv_api_pyramid.cpp
        src/bmcv_api_resize.cpp
        src/bmcv_api_sobel.cpp
        src/bmcv_api_sort.cpp
        src/bmcv_api_split.cpp
        src/bmcv_api_storage_convert.cpp
        src/bmcv_api_threshold.cpp
        src/bmcv_api_transpose.cpp
        src/bmcv_api_warp_affine.cpp
        src/bmcv_api_warp_perspective.cpp
        src/bmcv_api_yuv2hsv.cpp
        src/bmcv_api_yuv2rgb.cpp
        src/bmcv_draw_rectangle.cpp
        src/bmcv_image_ext.cpp
        src/bmcv_image_serialize.cpp
        src/bmcv_internal.cpp
        src/bmcv_vpp_internal.cpp
        src/bmcv_api_laplacian.cpp
        src/bmcv_api_axpy.cpp
        src/bmcv_api_hm_distance.cpp
        src/bmcv_api_csc_convert_to.cpp
        src/bm1684/bmcv_1684_vpp_internal.cpp
        src/bm1684x/bmcv_1684x_vpp_cmodel.cpp
        src/bm1684x/bmcv_1684x_vpp_internal.cpp
        # src/md5.cpp
    )
endif()

add_library(libbmcv SHARED ${SRCS})

if(WIN32)
    target_link_libraries(libbmcv libvpp libbmjpuapi libbmjpulite libbmion libbmlib turbojpeg-static)#common#libbmlib
elseif(LINUX)
    target_link_libraries(libbmcv libbmlib)
endif()

add_library(libbmcv-static STATIC ${SRCS})

if(WIN32)
    target_link_libraries(libbmcv-static)#libbmlib-static
    get_filename_component(LIB_EXE_PATH ${CMAKE_LINKER} DIRECTORY)
    find_program(LIB_EXE NAMES lib.exe HINTS ${LIB_EXE_PATH})
    find_library(LIB_TURBO_JPEG_STATIC NAMES turbojpeg-static HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/libjpeg-turbo/binary/lib/windows)
    add_custom_command(TARGET libbmcv-static POST_BUILD
        COMMAND ${LIB_EXE} /out:tmp.lib ${CMAKE_INSTALL_PREFIX}/lib/libbmcv-static.lib ${LIB_TURBO_JPEG_STATIC}
        COMMAND ${CMAKE_COMMAND} -E rename tmp.lib ${CMAKE_INSTALL_PREFIX}/lib/libbmcv-static.lib)
elseif(LINUX)
    target_link_libraries(libbmcv-static libbmlib-static libcommon-static)
endif()

install(TARGETS libbmcv libbmcv-static DESTINATION lib)

if(WIN32)
file(
    COPY ./include/bmcv_api.h
         ./include/bmcv_api_ext.h
         ./include/bmcv_api_ext_c.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
endif()