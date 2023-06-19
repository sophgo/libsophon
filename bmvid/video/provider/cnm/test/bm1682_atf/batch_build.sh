#!/bin/bash

source ./build_common.sh

set -e

mkdir -p run/fpga/debug
mkdir -p run/fpga/release
mkdir -p run/palladium/debug
mkdir -p run/palladium/release
echo -e "\033[47;34m start to create test case...\033[0m"

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0
build_test_case 0
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0
build_test_case 1

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 3 0 0 1 0 0 1 0
build_test_case 2
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 3 0 0 1 0 0 1 0
build_test_case 3

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0
build_test_case 4
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0
build_test_case 5

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 3 0 0 1 0xf 0 1 0
build_test_case 6
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 3 0 0 1 0xf 0 1 0
build_test_case 7

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0 0 1 0
build_test_case 8
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0 0 1 0
build_test_case 9

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0 0 1 0
build_test_case 10
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0 0 1 0
build_test_case 11

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0xf 0 1 0
build_test_case 12
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 0 0 0 1 0xf 0 1 0
build_test_case 13

set_codec_param 0 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 14
set_codec_param 0 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 15

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 3 0 0 1 0x0 0 1 0
build_test_case 16
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 3 0 0 1 0x0 0 1 0
build_test_case 17

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 3 0 0 1 0xf 0 1 0
build_test_case 18
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 3 0 0 1 0xf 0 1 0
build_test_case 19

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 0 0 0 1 0x0 0 1 0
build_test_case 20
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 0 0 0 1 0x0 0 1 0
build_test_case 21

set_codec_param 0 1 TILED_FRAME_V_MAP 0 0 1 0 0 0 1 0xf 0 1 0
build_test_case 22
set_codec_param 0 1 TILED_FIELD_V_MAP 0 0 1 0 0 0 1 0xf 0 1 0
build_test_case 23

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 90 0 1 0 0 1 0
build_test_case 24
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 90 0 1 0 0 1 0
build_test_case 25

set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 1 1 0 0 1 0
build_test_case 26
set_codec_param 0 0 LINEAR_FRAME_MAP 0 0 1 3 0 1 1 0 0 1 0
build_test_case 27

echo -e "\033[47;34m [VIDEO] AVC done! \033[0m"

set_codec_param 1 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0x7 0 1 0
build_test_case 28
set_codec_param 1 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0x7 0 1 0
build_test_case 29

echo -e "\033[47;34m [VIDEO] VC1 done! \033[0m"

set_codec_param 2 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 30
set_codec_param 2 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 31

echo -e "\033[47;34m [VIDEO] MPEG2 done! \033[0m"

set_codec_param 2 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 32
set_codec_param 2 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 33

echo -e "\033[47;34m [VIDEO] MPEG1 done! \033[0m"

set_codec_param 11 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 34
set_codec_param 11 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 35

echo -e "\033[47;34m [VIDEO] VP8 done! \033[0m"

set_codec_param 7 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 36
set_codec_param 7 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 37

echo -e "\033[47;34m [VIDEO] AVS done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 38
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 39

echo -e "\033[47;34m [VIDEO] MPEG4 done! \033[0m"

set_codec_param 0 0 TILED_FRAME_V_MAP 1 1 1 0 0 0 1 0xf 0 1 0
build_test_case 40
set_codec_param 0 0 TILED_FIELD_V_MAP 1 1 1 0 0 0 1 0xf 0 1 0
build_test_case 41

echo -e "\033[47;34m [VIDEO] MVC done! \033[0m"

set_codec_param 4 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 42
set_codec_param 4 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 43

echo -e "\033[47;34m [VIDEO] H263 done! \033[0m"

set_codec_param 5 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 44
set_codec_param 5 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 45

echo -e "\033[47;34m [VIDEO] DIV3 done! \033[0m"

set_codec_param 6 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 46
set_codec_param 6 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 47

echo -e "\033[47;34m [VIDEO] RV done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 256 1 0
build_test_case 48
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 256 1 0
build_test_case 49

echo -e "\033[47;34m [VIDEO] Sorenson done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 1 1 0
build_test_case 50
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 1 1 0
build_test_case 51

echo -e "\033[47;34m [VIDEO] Divx done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 2 1 0
build_test_case 52
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 2 1 0
build_test_case 53

echo -e "\033[47;34m [VIDEO] Xvid done! \033[0m"

set_codec_param 3 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 5 1 0
build_test_case 54
set_codec_param 3 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 5 1 0
build_test_case 55

echo -e "\033[47;34m [VIDEO] Divx4 done! \033[0m"

set_codec_param 9 0 TILED_FRAME_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 56
set_codec_param 9 0 TILED_FIELD_V_MAP 0 1 1 0 0 0 1 0xf 0 1 0
build_test_case 57

echo -e "\033[47;34m [VIDEO] Theora done! \033[0m"

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0
build_test_case 58
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0
build_test_case 59

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 1 0
build_test_case 60 
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 2 0
build_test_case 61

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0
build_test_case 62
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0
build_test_case 63

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0
build_test_case 64
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 2 0
build_test_case 65

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0
build_test_case 66
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0
build_test_case 67
 
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 1 0
build_test_case 68
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0xf 0 2 0
build_test_case 69

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0
build_test_case 70
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0
build_test_case 71

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 1 0
build_test_case 72
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0xf 0 2 0
build_test_case 73

echo -e "\033[47;34m [VIDEO] HEVC done! \033[0m"


set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0
build_test_case 74
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0
build_test_case 75

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 1 0
build_test_case 76
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 2 0
build_test_case 77

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0
build_test_case 78
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0
build_test_case 79

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 0
build_test_case 80
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 2 0
build_test_case 81

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 1 0
build_test_case 82
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0 0 2 0
build_test_case 83

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 1 0
build_test_case 84
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 0 0x3 0 2 0
build_test_case 85

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 1 0
build_test_case 86
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0 0 2 0
build_test_case 87

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 0
build_test_case 88
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 2 0
build_test_case 89


set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 5
build_test_case 90
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 6
build_test_case 91

set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 7
build_test_case 92
set_codec_param 12 1 COMPRESSED_FRAME_MAP 0 0 1 3 0 0 1 0x3 0 1 8
build_test_case 93



echo -e "\033[47;34m [VIDEO] HEVC 4k done! \033[0m"

echo -e "\033[47;31m [VIDEO] FINISH! \033[0m"



