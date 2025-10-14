#!/bin/bash

failed_count=0
count=1
failed_scripts=""
bmcv_case=${1:-'tpu'}
loop=${2:-1}

run_command() {
  local command="$1"
  local description="$1"

  echo "Running: $description"
  eval "$command"

  if [ $? -ne 0 ]; then
    echo "Command failed: $description"
    failed_count=$((failed_count + 1))
    failed_scripts="$failed_scripts$description\n"
  fi

  echo ""
}

run_tpu() {
  run_command "test_cv_absdiff"
  run_command "test_cv_add_weighted"
  run_command "test_cv_as_strided"
  run_command "test_cv_axpy"
  run_command "test_cv_base64"
  run_command "test_cv_batch_topk"
  run_command "test_cv_bayer2rgb"
  run_command "test_cv_bgrsplit"
  run_command "test_cv_bitwise"
  run_command "test_cv_calc_hist"
  run_command "test_cv_cmulp"
  run_command "test_cv_copy_to"
  run_command "test_cv_copy_to_param"
  run_command "test_cv_crop"
  run_command "test_cv_dct"
  run_command "test_cv_gemm"
  run_command "test_cv_image_align"
  run_command "test_cv_hm_distance"
  run_command "test_cv_jpeg"
  run_command "test_cv_laplacian"
  run_command "test_cv_min_max"
  run_command "test_cv_nms"
  run_command "test_cv_pyramid"
  run_command "test_cv_quantify"
  run_command "test_cv_sort"
  run_command "test_cv_threshold"
  run_command "test_cv_transpose"
  run_command "test_cv_warp_affine"
  run_command "test_cv_warp_perspective"
  run_command "test_cv_width_align"
  run_command "test_faiss_indexflatIP"
  run_command "test_faiss_indexPQ"
  run_command "test_matrix_log"
  run_command "test_perf_bmcv"
  # run_command "test_cv_rotate" --fail
  # run_command "test_faiss_indexflatL2" --fail
  # run_command "test_cv_yuv2hsv" --fail
  # run_command "test_cv_yuv2rgb" --fail
  # run_command "test_cv_warp" --fail
  # run_command "test_cv_split" --fail
  # run_command "test_cv_storage_convert" --fail
  # run_command "test_cv_sobel" --fail
  # run_command "test_cv_put_text" --fail
  # run_command "test_cv_morph" --fail
  # run_command "test_cv_multi_crop_resize" --fail
  # run_command "test_cv_lkpyramid" --fail
  # run_command "test_cv_matmul" --fail
  # run_command "test_cv_matmul_t_opt" --fail
  # run_command "test_cv_image_transpose" --fail
  # run_command "test_cv_img_scale" --fail
  # run_command "test_cv_json" --fail
  # run_command "test_cv_hist_balance" --modify the command
  # run_command "test_cv_distance" --fail
  # run_command "test_cv_draw_lines" --fail
  # run_command "test_cv_draw_rectangle" --fail
  # run_command "test_cv_feature_match" --fail
  # run_command "test_cv_fft_1d" --fail
  # run_command "test_cv_fft_2d" --fail
  # run_command "test_cv_fill_rectangle" --fail
  # run_command "test_cv_fusion" --fail
  # run_command "test_cv_gaussian_blur" --fail
  # run_command "test_cv_canny" --fail
  # run_command "test_yolov3_detect_out" --fail
}

if [ $bmcv_case = "tpu" ]; then
  eval "mkdir -p out"
  while [ $count -le $loop ]
  do
      run_tpu
      ((count++))
  done
fi

if [ $failed_count -gt 0 ]; then
  echo "Total failed commands: $failed_count"
  echo -e "Failed scripts:\n$failed_scripts"
else
  echo "All tests pass!"
fi