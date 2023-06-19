#!/bin/bash
#! /usr/bin/env bash
set -euo pipefail
this_dir=$(dirname $(readlink -f ${BASH_SOURCE[0]}))

cd $this_dir

source $this_dir/test/wave/build_common_core1.sh

set -e

mkdir -p run
mkdir -p run/core1
echo -e "\033[47;34m start to create test case...\033[0m"



set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 0
build_test_case 0

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 1 0 1
build_test_case 1 

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 2
build_test_case 2

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0 3
build_test_case 3

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 4
build_test_case 4
 
set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 1 0 5
build_test_case 5

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 6
build_test_case 6

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0 7
build_test_case 7

echo -e "\033[47;34m [VIDEO] AVC done! \033[0m"


set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 8
build_test_case 8

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 1 0 9
build_test_case 9

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 10
build_test_case 10

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 0 11
build_test_case 11

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 12
build_test_case 12

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 1 0 13
build_test_case 13

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 14
build_test_case 14

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 0 15
build_test_case 15

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 5 16
build_test_case 16
set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 6 17
build_test_case 17

set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 7 18
build_test_case 18
set_codec_param 0 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 8 19
build_test_case 19


echo -e "\033[47;34m [VIDEO] AVC 4k done! \033[0m"



set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 58
build_test_case 58
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0 59
build_test_case 59

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 1 0 60
build_test_case 60 
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 2 0 61
build_test_case 61

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 62
build_test_case 62
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0 63
build_test_case 63

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0 64
build_test_case 64
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 2 0 65
build_test_case 65

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 66
build_test_case 66
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0 67
build_test_case 67
 
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 1 0 68
build_test_case 68
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 2 0 69
build_test_case 69

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 70
build_test_case 70
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0 71
build_test_case 71

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0 72
build_test_case 72
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 2 0 73
build_test_case 73

echo -e "\033[47;34m [VIDEO] HEVC done! \033[0m"


set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 74
build_test_case 74
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0 75
build_test_case 75

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 1 0 76
build_test_case 76
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 2 0 77
build_test_case 77

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 78
build_test_case 78
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0 79
build_test_case 79

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 0 80
build_test_case 80
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 2 0 81
build_test_case 81

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0 82
build_test_case 82
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0 83
build_test_case 83

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 1 0 84
build_test_case 84
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 2 0 85
build_test_case 85

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 86
build_test_case 86
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0 87
build_test_case 87

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 0 88
build_test_case 88
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 2 0 89
build_test_case 89


set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 5 90
build_test_case 90
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 6 91
build_test_case 91

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 7 92
build_test_case 92
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 8 93
build_test_case 93


echo -e "\033[47;34m [VIDEO] HEVC 4k done! \033[0m"

echo -e "\033[47;31m [VIDEO] FINISH! \033[0m"



