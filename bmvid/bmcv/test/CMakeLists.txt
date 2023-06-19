cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0046 NEW)
Project(bmcv_test)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "PROJECT_ROOT  ${PROJECT_ROOT}")
message(STATUS "$ENV{TARGET_PROJECT}_CMAKE ")
message(STATUS "CMAKE_INSTALL_PREFIX  ${CMAKE_INSTALL_PREFIX}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

include_directories(${PROJECT_ROOT}/bmcv/include)
include_directories(${PROJECT_ROOT}/bmcv/src)
include_directories(${PROJECT_ROOT}/vpp/driver/include/)
include_directories(${PROJECT_ROOT}/vpp/driver/include/bm1684/)
include_directories(${PROJECT_ROOT}/jpeg/driver/bmjpuapi/inc/)
include_directories(${PROJECT_ROOT}/3rdparty/libbmcv/include/)

if(EXISTS ${PROJECT_ROOT}/../../bm_prebuilt_toolchains_win)
    set(PREBUILT_PATH ${PROJECT_ROOT}/../../bm_prebuilt_toolchains_win)
elseif(EXISTS ${PROJECT_ROOT}/../bm_prebuilt_toolchains_win)
    set(PREBUILT_PATH ${PROJECT_ROOT}/..//bm_prebuilt_toolchains_win)
endif()

include_directories(${PREBUILT_PATH}/openblas_windows/include)

link_directories(${PREBUILT_PATH}/openblas_windows)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(
	${PROJECT_ROOT}/../libsophon/release/libsophon/lib
	${PROJECT_ROOT}/../libsophon/bmlib/release/lib
	${PROJECT_ROOT}/../../libsophon/release/libsophon/lib
	${PROJECT_ROOT}/../../libsophon/bmlib/release/lib
	${PROJECT_ROOT}/3rdparty/libbmcv/lib/pcie_windows)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

if(TARGET_TYPE STREQUAL "release")
    include_directories(${PREBUILT_PATH}/opencv-windows/x64-Release/include)
    link_directories(${PREBUILT_PATH}/opencv-windows/x64-Release/x64/vc16/lib)
	file(COPY ${PREBUILT_PATH}/opencv-windows/x64-Release/x64/vc16/bin/opencv_world410.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
else()
    include_directories(${PREBUILT_PATH}/opencv-windows/x64-Debug/include)
    link_directories(${PREBUILT_PATH}/opencv-windows/x64-Debug/x64/vc16/lib)
	file(COPY ${PREBUILT_PATH}/opencv-windows/x64-Debug/x64/vc16/bin/opencv_world410d.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()

include_directories(${PREBUILT_PATH}/pthreads-win32/prebuilt-dll-2-9-1-release/include)

find_library(PTHREADS_LIB
    NAMES pthreadVC2
    PATHS ${PREBUILT_PATH}/pthreads-win32/prebuilt-dll-2-9-1-release/lib/x64
    REQUIRED)
add_definitions(-DHAVE_STRUCT_TIMESPEC)

find_file(PTHREADS_DLL
    NAMES pthreadVC2.dll
    PATHS ${PREBUILT_PATH}/pthreads-win32/prebuilt-dll-2-9-1-release/dll/x64
)
file(COPY ${PTHREADS_DLL} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

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
if(LINUX)
function(test_bmcv)
  foreach(src IN LISTS ARGN)
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
    target_link_libraries(${target} libbmlib libbmcv pthread bmopenblas)
    install(TARGETS ${target} DESTINATION bmcv/test/)
  endforeach()
endfunction()

test_bmcv(
    test_convert_to.cpp
    test_cv_absdiff.cpp
    test_cv_add_weighted.cpp
    test_cv_bitwise.cpp
    test_cv_base64.cpp
    test_cv_batch_topk.cpp
    test_cv_bgrsplit.cpp
    test_cv_calc_hist.cpp
    test_cv_canny.cpp
    test_cv_cmulp.cpp
    test_cv_copy_to.cpp
    test_cv_crop.cpp
    test_cv_dct.cpp
    test_cv_distance.cpp
    test_cv_draw_lines.cpp
    test_cv_draw_rectangle.cpp
    test_cv_feature_match.cpp
    test_cv_fft_1d.cpp
    test_cv_fft_2d.cpp
    test_cv_fill_rectangle.cpp
    test_cv_gaussian_blur.cpp
    test_cv_gemm.cpp
    test_cv_image_align.cpp
    test_cv_image_transpose.cpp
    test_cv_img_scale.cpp
    test_cv_jpeg.cpp
    test_cv_json.cpp
    test_cv_lkpyramid.cpp
    test_cv_matmul.cpp
    test_cv_min_max.cpp
    test_cv_morph.cpp
    test_cv_nms.cpp
    test_cv_put_text.cpp
    test_cv_pyramid.cpp
    test_cv_sobel.cpp
    test_cv_sort.cpp
    test_cv_split.cpp
    test_cv_storage_convert.cpp
    test_cv_threshold.cpp
    test_cv_transpose.cpp
    test_cv_vpp.cpp
    test_cv_vpp_border.cpp
    test_cv_vpp_loop.cpp
    test_cv_vpp_random.cpp
    test_cv_vpp_stitch.cpp
    test_cv_warp.cpp
    test_cv_warp_bilinear.cpp
    test_cv_warp_perspective.cpp
    test_cv_warp_perspective_bilinear.cpp
    test_cv_width_align.cpp
    test_cv_yuv2hsv.cpp
    test_cv_yuv2rgb.cpp
    test_perf_bmcv.cpp
    test_perf_vpp.cpp
    test_resize.cpp
    test_cv_laplacian.cpp
    test_cv_axpy.cpp
    test_bm1686_vpp_convert.cpp
)
elseif(WINDOWS)

function(test_bmcv)
    foreach(src IN LISTS ARGN)
        get_filename_component(target ${src} NAME_WE)
        add_executable(${target} ${src} test_misc.cpp md5.cpp)
        #add_dependencies(${target} libbmcv-static libbmlib-static)
        if(TARGET_TYPE STREQUAL "release")
            target_link_libraries(${target} libbmcv-static libvpp-static libbmjpuapi-static libbmjpulite-static libbmion-static libbmlib-static opencv_world410 openblas-VS2019 ${PTHREADS_LIB})
        else()
            target_link_libraries(${target} libbmcv-static libvpp-static libbmjpuapi-static libbmjpulite-static libbmion-static libbmlib-static opencv_world410d openblas-VS2019 ${PTHREADS_LIB})
        endif()
        install(TARGETS ${target} DESTINATION bmcv/test)
    endforeach()
endfunction()

test_bmcv(
    test_convert_to.cpp
    test_cv_absdiff.cpp
    test_cv_add_weighted.cpp
    test_cv_bitwise.cpp
    test_cv_base64.cpp
    test_cv_batch_topk.cpp
    test_cv_bgrsplit.cpp
    test_cv_calc_hist.cpp
    test_cv_canny.cpp
    test_cv_cmulp.cpp
    test_cv_copy_to.cpp
    test_cv_crop.cpp
    test_cv_dct.cpp
    test_cv_distance.cpp
    test_cv_draw_lines.cpp
    test_cv_draw_rectangle.cpp
    test_cv_feature_match.cpp
    test_cv_fft_1d.cpp
    test_cv_fft_2d.cpp
    test_cv_fill_rectangle.cpp
    test_cv_gaussian_blur.cpp
    test_cv_gemm.cpp
    test_cv_image_align.cpp
    test_cv_image_transpose.cpp
    test_cv_img_scale.cpp
    test_cv_jpeg.cpp
    test_cv_json.cpp
    test_cv_lkpyramid.cpp
    test_cv_matmul.cpp
    test_cv_min_max.cpp
    test_cv_morph.cpp
    test_cv_nms.cpp
    test_cv_put_text.cpp
    test_cv_pyramid.cpp
    test_cv_sobel.cpp
    test_cv_sort.cpp
    test_cv_split.cpp
    test_cv_storage_convert.cpp
    test_cv_threshold.cpp
    test_cv_transpose.cpp
    test_cv_vpp.cpp
    test_cv_vpp_border.cpp
    test_cv_vpp_loop.cpp
    test_cv_vpp_random.cpp
    test_cv_vpp_stitch.cpp
    test_cv_warp.cpp
    test_cv_warp_bilinear.cpp
    test_cv_warp_perspective.cpp
    test_cv_warp_perspective_bilinear.cpp
    test_cv_width_align.cpp
    test_cv_yuv2hsv.cpp
    test_cv_yuv2rgb.cpp
    test_perf_bmcv.cpp
    test_perf_vpp.cpp
    test_resize.cpp
    test_cv_laplacian.cpp
    test_cv_axpy.cpp
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin/bm1684x)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin/bm1684x)
test_bmcv(
    bm1684x/test_bm1684x_fbd.cpp
	# bm1684x/test_bm1684x_slt.cpp
	bm1684x/test_bm1684x_vpp.cpp
	bm1684x/test_bm1684x_vpp_ax+b.cpp
	bm1684x/test_bm1684x_vpp_basic.cpp
	bm1684x/test_bm1684x_vpp_border.cpp
	# bm1684x/test_bm1684x_vpp_random.cpp
	bm1684x/test_bm1684x_vpp_resize.cpp
	bm1684x/test_bm1684x_vpp_stitch.cpp
	bm1684x/test_bm1684x_vpp_convert.cpp
	bm1684x/test_bm1684x_vpp_padding.cpp
	bm1684x/test_bm1684x_vpp_comparison.cpp
	bm1684x/test_bm1684x_vpp_border_comapre.cpp
	bm1684x/test_bm1684x_vpp_fill_rectangle.cpp
	bm1684x/test_bm1684x_vpp_postpadding-compare.cpp
	bm1684x/test_bm1684x_vpp_postpadding-haikang.cpp
	bm1684x/test_bm1684x_vpp_convert_to_comparison.cpp
	bm1684x/test_bm1684x_vpp_mul_crop_resize_haikang.cpp
)

file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/image
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

endif()
