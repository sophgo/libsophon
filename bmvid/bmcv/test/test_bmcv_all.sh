#!/bin/bash

echo "TEST_BMCV_TEST"

cases=(
 test_cv_warp
 test_resize
 test_cv_yuv2rgb
 test_cv_nms
 test_convert_to
 test_cv_sort
 test_cv_feature_match
 test_cv_crop
 test_cv_transpose
 test_cv_bgrsplit
 test_cv_gemm
 test_cv_img_scale
  test_cv_bitwise
)

function run_bmcv_test {
  test_app=$1
  arg=$2

  if [ ! -z $arg ]; then
    ./test_api_bmcv/$test_app $arg; OUT=$?
  else
    ./test_api_bmcv/$test_app; OUT=$?
  fi
  if [ $OUT -eq 0 ]; then
    echo "Run $test_app OK"
  else
    echo "Run $test_app Fail"
    return $OUT
  fi
}

case_count=0
fail_list=()
for test_case in ${cases[@]}
do
  case_count=$(($case_count+1))
  run_bmcv_test ${test_case}; ret=$?
  if [ $ret -ne 0 ]; then
    fail_list+=("${test_case}")
  fi
done

echo "Have run $case_count cases in total"

if [ ${#fail_list[@]} -eq 0 ]; then
  echo "TEST_BMCV_TEST PASSED"
else
  for test_case in ${fail_list[@]}
  do
    echo "Error occurs in $test_case"
  done
  echo "TEST_BMCV_TEST FAILED"
fi
