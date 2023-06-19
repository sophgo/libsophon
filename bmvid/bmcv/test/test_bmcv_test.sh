#!/bin/bash

cv_instance=(
  "test_cv_warp"
  "test_resize"
  "test_cv_yuv2rgb"
  "test_cv_nms"
  "test_convert_to"
  "test_cv_sort"
  "test_cv_feature_match"
  "test_cv_crop"
  "test_cv_transpose"
  "test_cv_bgrsplit"
  "test_cv_gemm"
  "test_cv_img_scale"
  "test_cv_copy_to"
  "test_cv_bitwise"
)

function run_bmcv_test {
  test_app=$1
  arg=$2

  if [ ! -z $arg ]; then
    $test_app $arg; OUT=$?
  else
    $test_app; OUT=$?
  fi

  if [ $OUT -eq 0 ]; then
    echo "Run $test_app OK"
    return 0;
  else
    echo "Run $test_app Fail"
    return $OUT
  fi
}

function bmcv_test_regression {
  echo "TEST_BMCV_TEST"
  for i in ${!cv_instance[*]}; do
      echo "------------${cv_instance[$i]} test starts-----------"
      run_bmcv_test ${cv_instance[i]}
      RET=$?
      if [ $RET -ne 0 ] ; then
        echo "*** Error TEST ${cv_instance[$i]} ***" ;
        return -1;
      fi
      echo "***${cv_instance[$i]} PASSED***" ;
  done
  echo "TEST_BMCV_TEST PASSED"
  return 0;
}
