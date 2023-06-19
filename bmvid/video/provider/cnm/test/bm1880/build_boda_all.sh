#!/bin/bash
#! /usr/bin/env bash
set -euo pipefail
this_dir=$(dirname $(readlink -f ${BASH_SOURCE[0]}))

cd $this_dir

source $this_dir/test/boda/build_common.sh

set -e

mkdir -p run
echo -e "\033[47;34m start to create test case...\033[0m"

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 0
build_test_case 0
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0 1
build_test_case 1

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 3 0 0 1 0 0 1 0 2
build_test_case 2
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 3 0 0 1 0 0 1 0 3
build_test_case 3

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0 4
build_test_case 4
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0 5
build_test_case 5

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 3 0 0 1 0xf 0 1 0 6
build_test_case 6
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 3 0 0 1 0xf 0 1 0 7
build_test_case 7

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0 0 1 0 8
build_test_case 8
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0 0 1 0 9
build_test_case 9

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0 0 1 0 10
build_test_case 10
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0 0 1 0 11
build_test_case 11

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0xf 0 1 0 12
build_test_case 12
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0xf 0 1 0 13
build_test_case 13

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 14
build_test_case 14
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 15
build_test_case 15

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 3 0 0 1 0x0 0 1 0 16
build_test_case 16
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 3 0 0 1 0x0 0 1 0 17
build_test_case 17

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 3 0 0 1 0xf 0 1 0 18
build_test_case 18
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 3 0 0 1 0xf 0 1 0 19
build_test_case 19

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 0 0 0 1 0x0 0 1 0 20
build_test_case 20
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 0 0 0 1 0x0 0 1 0 21
build_test_case 21

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 0 0 0 1 0xf 0 1 0 22
build_test_case 22
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 0 0 0 1 0xf 0 1 0 23
build_test_case 23

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 90 0 1 0 0 1 0 24
build_test_case 24
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 90 0 1 0 0 1 0 25
build_test_case 25

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 1 1 0 0 1 0 26
build_test_case 26
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 1 1 0 0 1 0 27
build_test_case 27

echo -e "\033[47;34m [VIDEO] AVC done! \033[0m"

set_codec_param 1 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0x7 0 1 0 28
build_test_case 28
set_codec_param 1 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0x7 0 1 0 29
build_test_case 29

echo -e "\033[47;34m [VIDEO] VC1 done! \033[0m"

set_codec_param 2 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 30
build_test_case 30
set_codec_param 2 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 31
build_test_case 31

echo -e "\033[47;34m [VIDEO] MPEG2 done! \033[0m"

set_codec_param 2 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 32
build_test_case 32
set_codec_param 2 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 33
build_test_case 33

echo -e "\033[47;34m [VIDEO] MPEG1 done! \033[0m"

set_codec_param 11 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 34
build_test_case 34
set_codec_param 11 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 35
build_test_case 35

echo -e "\033[47;34m [VIDEO] VP8 done! \033[0m"

set_codec_param 7 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 36
build_test_case 36
set_codec_param 7 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 37
build_test_case 37

echo -e "\033[47;34m [VIDEO] AVS done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 38
build_test_case 38
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 39
build_test_case 39

echo -e "\033[47;34m [VIDEO] MPEG4 done! \033[0m"

set_codec_param 0 0 TILED_FRAME_V_MAP 1 1 1 0 0 0 1 0xf 0 1 0 40
build_test_case 40
set_codec_param 0 0 TILED_FIELD_V_MAP 1 1 1 0 0 0 1 0xf 0 1 0 41
build_test_case 41

echo -e "\033[47;34m [VIDEO] MVC done! \033[0m"

set_codec_param 4 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 42
build_test_case 42
set_codec_param 4 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 43
build_test_case 43

echo -e "\033[47;34m [VIDEO] H263 done! \033[0m"

set_codec_param 5 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 44
build_test_case 44
set_codec_param 5 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 45
build_test_case 45

echo -e "\033[47;34m [VIDEO] DIV3 done! \033[0m"

set_codec_param 6 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 46
build_test_case 46
set_codec_param 6 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 47
build_test_case 47

echo -e "\033[47;34m [VIDEO] RV done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 256 1 0 48
build_test_case 48
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 256 1 0 49
build_test_case 49

echo -e "\033[47;34m [VIDEO] Sorenson done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 1 1 0 50
build_test_case 50
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 1 1 0 51
build_test_case 51

echo -e "\033[47;34m [VIDEO] Divx done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 2 1 0 52
build_test_case 52
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 2 1 0 53
build_test_case 53

echo -e "\033[47;34m [VIDEO] Xvid done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 5 1 0 54
build_test_case 54
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 5 1 0 55
build_test_case 55

echo -e "\033[47;34m [VIDEO] Divx4 done! \033[0m"

set_codec_param 9 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 56
build_test_case 56
set_codec_param 9 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0 57
build_test_case 57

echo -e "\033[47;34m [VIDEO] Theora done! \033[0m"

echo -e "\033[47;31m [VIDEO] FINISH! \033[0m"



