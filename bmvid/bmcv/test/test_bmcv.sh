#!/bin/bash

simpletest=(
test_convert_to
test_cv_absdiff
test_cv_add_weighted
test_cv_axpy
test_cv_base64
test_cv_batch_topk
test_cv_bgrsplit
test_cv_calc_hist
test_cv_canny
test_cv_cmulp
test_cv_copy_to
test_cv_crop

test_cv_distance

test_cv_draw_rectangle

test_cv_fft_1d
test_cv_fft_2d
test_cv_fill_rectangle
test_cv_fusion
test_cv_gaussian_blur
test_cv_gemm
test_cv_image_align
test_cv_image_transpose
test_cv_img_scale
test_cv_jpeg
test_cv_json
test_cv_laplacian

test_cv_matmul
test_cv_min_max

test_cv_nms
test_cv_put_text
test_cv_pyramid
test_cv_sobel
test_cv_sort
test_cv_split
test_cv_storage_convert
test_cv_threshold
test_cv_transpose
test_cv_vpp

test_cv_vpp_loop
test_cv_vpp_random

test_cv_warp
test_cv_warp_bilinear
test_cv_warp_perspective
test_cv_width_align
test_cv_yuv2hsv
test_cv_yuv2rgb


test_resize
)

function set_env()
{
if [ -d $BMVID_OUTPUT_DIR/bmcv/test ];then

    echo "$BMVID_OUTPUT_DIR "
    pushd $BMVID_OUTPUT_DIR/bmcv/test
    OUTPUT_DIR=$BMVID_OUTPUT_DIR
fi

if [ -d $MM_OUTPUT_DIR/bmcv/test ];then

    echo "$MM_OUTPUT_DIR"
    pushd $MM_OUTPUT_DIR/bmcv/test
    OUTPUT_DIR=$MM_OUTPUT_DIR
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OUTPUT_DIR/bmcv/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OUTPUT_DIR/decode/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OUTPUT_DIR/../../bmvid/3rdparty/libbmcv/lib/$PRODUCTFORM/
}

function simple_test()
{

echo "$1"  >> test_bmcv.log
./$1 >> test_bmcv.log
result="$?"

if [ "$result" -ne 0 ]; then
    echo "$1	-- error" >> test_bmcv.log
    echo "$1	-- error"
else
    echo "$1	-- okay"  >> test_bmcv.log
    echo "$1	-- okay"
fi
}

set_env
date > test_bmcv.log
for i in "${!simpletest[@]}";
do
    #printf "%s\t%s\n" "$i" "${simpletest[$i]}"
    simple_test ${simpletest[$i]}
done


not_test=(
test_cv_dct
test_cv_draw_lines
test_cv_morph
)

test_with_parameters=(
test_cv_feature_match
test_cv_lkpyramid
test_cv_vpp_border
test_cv_vpp_stitch
test_perf_bmcv
test_perf_vpp
)













