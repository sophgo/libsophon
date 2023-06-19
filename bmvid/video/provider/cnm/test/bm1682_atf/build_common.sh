#!/bin/bash

#$1 codec_type: 
#		STD_AVC,    0
#		STD_VC1,	1
#		STD_MPEG2,	2
#		STD_MPEG4,	3
#		STD_H263,	4
#		STD_DIV3,	5
#		STD_RV,		6
#		STD_AVS,	7
#		STD_THO = 9,
#		STD_VP3,	10
#		STD_VP8,	11
#		STD_HEVC,	12
#		STD_VP9,	13
#		STD_AVS2,	14
#$2	wtl_en:  0 disable  1 enable
#$3	map_type: 
#		0 LINEAR_FRAME_MAP 
#		1 TILED_FRAME_V_MAP
#		3 TILED_FIELD_V_MAP
#		9 LINEAR_FRAME_MAP
#		10 COMPRESSED_FRAME_MAP
#$4 mvc_en: 0 disable 1 AVC MVC
#$5 t2l_en: 0 disable 1 enable
#$6 t2l_mode: 1 frame 2 field
#$7 cache_bypass: 0 cache enable 3 cache bypass all
#$8	rotate_en: 0 disable 1 enable
#$9 mirror_en: 0 disable 1 enable
#$10 bwb_en: 0 bw_opt disable 1 bw_opt enable
#$11 secAXI: 0 disable 0xf enable all
#$12 mpeg4_class: 0 mpeg4 1 divx 2 xvid 5 divx4 256 sorenson
#$13 wtl_mode: 0 wtl disable case 1 wtl frame mode 2 wtl field mode
#$14 wtl_format: 0 format_420 1 format_422 2 format_224 3 format 444 4 format 400 5 format_420_p10_16bit_MSB 6 format_420_p10_16bit_LSB 7 format_420_p10_32bit_MSB 8 format_420_p10_32bit_LSB
#                .... 9 format_422_p10_16bit_msb ... 13 format_yuyv .. 18 format_yvyu 23 format_uyvy 28 format_vyuy
function set_codec_param()
{
	boda_codec_type=$1
	boda_wtl_en=$2
	boda_map_typ=$3
	mvc_en=$4
	t2l_en=$5
	t2l_mode=$6
	cache_bypass=$7
	rotate_en=$8
	mirror_en=$9
	shift 9
	bwb_en=$1
	secaxi_en=$2
	mpeg4_class=$3
	wtl_mode=$4
    wtl_format=$5
	
	cp -f bl31/bl31_main.c.orig bl31/bl31_main.c
	
	sed -i "s/#CODA_CODEC_TYPE#/$boda_codec_type/" bl31/bl31_main.c
	sed -i "s/#CODA_WTL_ENABLE#/$boda_wtl_en/" bl31/bl31_main.c
	sed -i "s/#CODA_MAP_TYPE#/$boda_map_typ/" bl31/bl31_main.c
	sed -i "s/#MVC_ENABLE#/$mvc_en/" bl31/bl31_main.c
	sed -i "s/#CODA_TILE2LINEAR_ENABLE#/$t2l_en/" bl31/bl31_main.c
	sed -i "s/#CODA_TILE2LINEAR_MODE#/$t2l_mode/" bl31/bl31_main.c
	sed -i "s/#CODA_CACHE_BYPASS#/$cache_bypass/" bl31/bl31_main.c
	sed -i "s/#CODA_ROTATE_ENABLE#/$rotate_en/" bl31/bl31_main.c
	sed -i "s/#CODA_MIRROR_ENABLE#/$mirror_en/" bl31/bl31_main.c
	sed -i "s/#WAVE_BWB_ENABLE#/$bwb_en/" bl31/bl31_main.c
	sed -i "s/#SECAXI_EANBLE#/$secaxi_en/" bl31/bl31_main.c	
	sed -i "s/#MPEG4_CLASS#/$mpeg4_class/" bl31/bl31_main.c	
	sed -i "s/#WTL_MODE#/$wtl_mode/" bl31/bl31_main.c	
    sed -i "s/#WTL_FORMAT#/$wtl_format/" bl31/bl31_main.c
}

function build_test_case()
{
	if [ $1 -lt 58 ]; then	# boda test case 
		echo -e "\033[47;31m [VIDEO] boda building... \033[0m"
		./fpga_boda.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/debug/bl31/bl31.elf ./run/fpga/debug/test_case$1.elf 
		echo -e "\033[47;31m [VIDEO] test case $1 - fpga-debug build! \033[0m"
		./fpga_boda_release.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/release/bl31/bl31.elf ./run/fpga/release/test_case$1.elf
		echo -e "\033[47;31m [VIDEO] test case $1 - fpga-release build! \033[0m"
		./palladium_boda.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/debug/bl31/bl31.elf ./run/palladium/debug/test_case$1.elf 
		echo -e "\033[47;31m [VIDEO] test case $1 - palladium-debug build! \033[0m"
		./palladium_boda_release.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/release/bl31/bl31.elf ./run/palladium/release/test_case$1.elf
		echo -e "\033[47;31m [VIDEO] test case $1 - palladium-release build! \033[0m"
	else	
		echo -e "\033[47;31m [VIDEO] wave building... \033[0m"
		./fpga_wave.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/debug/bl31/bl31.elf ./run/fpga/debug/test_case$1.elf 
		echo -e "\033[47;31m [VIDEO] test case $1 - fpga-debug build! \033[0m"
		./fpga_wave_release.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/release/bl31/bl31.elf ./run/fpga/release/test_case$1.elf
		echo -e "\033[47;31m [VIDEO] test case $1 - fpga-release build! \033[0m"
		./palladium_wave.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/debug/bl31/bl31.elf ./run/palladium/debug/test_case$1.elf 
		echo -e "\033[47;31m [VIDEO] test case $1 - palladium-debug build! \033[0m"	
		./palladium_wave_release.sh > /dev/null 2>&1 && cp ./build/bm1682_fvp/release/bl31/bl31.elf ./run/palladium/release/test_case$1.elf
		echo -e "\033[47;31m [VIDEO] test case $1 - palladium-release build! \033[0m"	
	fi
	
	echo -e "\033[47;34m [VIDEO] test case $1 build! \033[0m"
}


