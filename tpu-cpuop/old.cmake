cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0046 NEW)
project(cpu-ops LANGUAGES CXX VERSION 1.0.0)

if(CMAKE_VERSION VERSION_LESS 3.12)
    set(CMAKE_PROJECT_VERSION 1.0.0)
endif()

if(NOT CACHED_INSTALL_PATH)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/release)
endif()

set(PREBUILT_TOOLCHAINS ${CMAKE_CURRENT_SOURCE_DIR}/../../bm_prebuilt_toolchains_win CACHE PATH "path to prebuilt toolchain")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

set(OUT_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)

if(TARGET_TYPE STREQUAL "release")
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD")
        set(CMAKE_C_FLAGS_RELEASE "/MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(CMAKE_C_FLAGS_RELEASE "/MT")
    endif()
else()
    if(RUNTIME_LIB STREQUAL "MD")
        set(CMAKE_CXX_FLAGS_DEBUG "/MDd")
        set(CMAKE_C_FLAGS_DEBUG "/MDd")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_C_FLAGS_DEBUG "/MTd")
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_INSTALL_PREFIX}/lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
    src/bbox_util.cpp
    src/bbox_util_opt.cpp
    src/cpu.cpp
    src/cpu_adaptive_average_pooling.cpp
    src/cpu_adaptive_max_pooling.cpp
    src/cpu_affine_grid_generator.cpp
    src/cpu_affine_grid_sampler.cpp
    src/cpu_anakin_detect_out.cpp
    src/cpu_argsortlayer.cpp
    src/cpu_bbox_transform.cpp
    src/cpu_binary.cpp
    src/cpu_box_nms.cpp
    src/cpu_box_with_nms_limit.cpp
    src/cpu_collect_rpn_proposals.cpp
    src/cpu_crop_and_resize.cpp
    src/cpu_debug.cpp
    # src/cpu_deform_gather.cpp
    # src/cpu_deform_psroipooling.cpp
    src/cpu_distribute_fpn_proposals.cpp
    src/cpu_distribute_fpn_proposals_roi_align_concat.cpp
    src/cpu_embedding.cpp
    src/cpu_embedding_bag.cpp
    src/cpu_gather.cpp
    src/cpu_gather_pytorch.cpp
    src/cpu_gathernd.cpp
    src/cpu_gathernd_tf.cpp
    src/cpu_generate_proposals.cpp
    src/cpu_grid_sampler.cpp
    src/cpu_index_put.cpp
    src/cpu_layer.cpp
    src/cpu_masked_select.cpp
    src/cpu_nms.cpp
    # src/cpu_onnx_nms.cpp
    # src/cpu_paddle_deformable_conv.cpp
    # src/cpu_paddle_matrix_nms.cpp
    # src/cpu_paddle_multiclass_nms.cpp
    # src/cpu_paddle_yolo_box.cpp
    src/cpu_pytorch_index.cpp
    src/cpu_pytorch_roi_align.cpp
    src/cpu_random_uniform.cpp
    src/cpu_random_uniform_int.cpp
    src/cpu_resize_interpolation.cpp
    # src/cpu_reverse_sequence.cpp
    src/cpu_roi_poolinglayer.cpp
    src/cpu_roialignlayer.cpp
    src/cpu_rpnproposallayer.cpp
    src/cpu_scatter_nd.cpp
    src/cpu_sort_per_dim.cpp
    src/cpu_ssd_detect_out.cpp
    src/cpu_tensorflow_nms_v5.cpp
    src/cpu_topk.cpp
    src/cpu_topk_ascending.cpp
    src/cpu_topk_mx.cpp
    src/cpu_unary.cpp
    src/cpu_utils.cpp
    src/cpu_where.cpp
    src/cpu_where_squeeze_gather.cpp
    src/cpu_yololayer.cpp
    src/cpu_yolov3_detect_out.cpp
    src/NormalizedBBox.cpp
)
add_library(libcpuop SHARED ${SRCS})
target_compile_features(libcpuop PUBLIC cxx_std_11)
set_target_properties(libcpuop PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION})

add_library(libcpuop-static STATIC ${SRCS})
target_compile_features(libcpuop-static PUBLIC cxx_std_11)
set_target_properties(libcpuop-static PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION})

file(GLOB_RECURSE user_srcs user/*.cpp)

add_library(libusercpu SHARED ${user_srcs})
target_compile_features(libusercpu PUBLIC cxx_std_11)
set_target_properties(libusercpu PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION})

add_library(libusercpu-static STATIC ${user_srcs})
target_compile_features(libusercpu-static PUBLIC cxx_std_11)
set_target_properties(libusercpu-static PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION})

set(ENABLE_TESTING OFF CACHE BOOL "Enable testings")

if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bmcpu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bmcpu_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/user/user_bmcpu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/user/user_bmcpu_common.h
    DESTINATION ${OUT_INCLUDE_DIR}
    PATTERN *.hpp
    PATTERN *.h)