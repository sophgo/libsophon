#!/bin/sh

date >> result.txt
###################################################
# vpp_boarder
###################################################

echo "vpp_border"
function vpp_border()
{
    ./vpp_border 1920 1080 bgr-org-opencv-bmp3.bin 32 32 32 32 $1 $2 out.bmp

    md5sum -c vpp_border.md5

    result="$?"

    if [ "$result" -ne 0 ]; then
        echo "vpp_border $1 $2 -- error" >> result.txt
    else
        echo "vpp_border $1 $2-- okay" >> result.txt
    fi

    rm out.bmp
}

vpp_border 0 0
vpp_border 0 1
vpp_border 1 0
vpp_border 1 1
function vpp_border_with_devfd()
{
    ./vpp_border_with_devfd 1920 1080 bgr-org-opencv-bmp3.bin 32 32 32 32 $1 $2 out.bmp

    md5sum -c vpp_border.md5

    result="$?"

    if [ "$result" -ne 0 ]; then
        echo "vpp_border_with_devfd $1 $2 -- error" >> result.txt
    else
        echo "vpp_border_with_devfd $1 $2-- okay" >> result.txt
    fi

    rm out.bmp
}

vpp_border_with_devfd 0 0
vpp_border_with_devfd 0 1
vpp_border_with_devfd 1 0
vpp_border_with_devfd 1 1
#####################################################
### vpp_convert
#####################################################
echo "vpp_convert"
function vpp_convert()
{
    ./vpp_convert 1920 1080 bgr-org-opencv.bin $1 $2 3 1 i420.yuv
    ./vpp_convert 1920 1080 rgbp.bin $1 $2  5 1 i420.bin
    ./vpp_convert 1920 1080 img1.nv12 $1 $2 2 6 bgrp3.bin
    ./vpp_convert 1920 1080 img2.yuv $1 $2 1 3 out.bmp
    ./vpp_convert 1920 1080 bgr-org-opencv.bin $1 $2 3 7 yuv444p
    ./vpp_convert 1920 1080 yuv444p.bin  $1 $2  7 8 yuv422p.bin
    ./vpp_convert 1920 1080 yuv444packed   $1 $2   9  3  bgr5.bmp 

    md5sum -c vpp_convert.md5
    
    result="$?"
    
    if [ "$result" -ne 0 ]; then
    	echo "vpp_convert  -- error" >> result.txt
    else
    	echo "vpp_convert  -- okay" >> result.txt
    fi
    rm out.bmp
    rm i420.yuv
    rm bgrp3.bin
    rm i420.bin
    rm yuv444p
    rm yuv422p.bin
    rm bgr5.bmp
}

# vpp_convert, yuv420p to bgr
vpp_convert 0 0
vpp_convert 0 1
vpp_convert 1 0
vpp_convert 1 1
#####################################################
### vpp_cropmulti
#####################################################
#
echo "vpp_cropmulti"
function vpp_cropmulti()
{
    ./vpp_cropmulti 1920 1080 bgr-org-opencv.bin bgr-org-opencv-10.bin bgr-org-opencv-222.bin $1 $2 out0.bmp out1.bmp out2.bmp

    md5sum -c vpp_cropmulti.md5

    result="$?"

    if [ "$result" -ne 0 ]; then
        echo "vpp_cropmulti $1 $2 -- error" >> result.txt
    else
        echo "vpp_cropmulti $1 $2 -- okay" >> result.txt
    fi

    rm out0.bmp out1.bmp out2.bmp
}

vpp_cropmulti 0 0
vpp_cropmulti 0 1
vpp_cropmulti 1 0
vpp_cropmulti 1 1
####################################################
## vpp_cropsingle
####################################################
echo "vpp_cropsingle"
function vpp_cropsingle()
{
    ./vpp_cropsingle 1920 1080 bgr-org-opencv-bmp3.bin 3 $1 $2 crop

    md5sum -c vpp_cropsingle.md5

    result="$?"

    if [ "$result" -ne 0 ]; then
        echo "vpp_cropsingle $1 $2 -- error" >> result.txt
    else
        echo "vpp_cropsingle $1 $2 -- okay" >> result.txt
    fi

    for i in $(seq 0 255)  
    do   
       outputimage=crop${i}.bmp
       rm ${outputimage} 
    done
}

vpp_cropsingle 0 0 
vpp_cropsingle 0 1 
vpp_cropsingle 1 0 
vpp_cropsingle 1 1 
#####################################################
### vppfbd_crop
#####################################################
#
echo "vppfbd_crop"
function vppfbd_crop()
{
    ./vppfbd_crop 1920 1080 fbc_offset_table_y.bin fbc_comp_data_y.bin fbc_offset_table_c.bin fbc_comp_data_c.bin 128 364 1080 586 1920 3 2012 768 out.bmp

    md5sum -c vppfbd_crop.md5

    result="$?"

    if [ "$result" -ne 0 ]; then
        echo "vppfbd_crop  -- error" >> result.txt
    else
        echo "vppfbd_crop  -- okay" >> result.txt
    fi

    rm out.bmp 2out.bmp
}

vppfbd_crop
#####################################################
### vppfbd_csc_resize
#####################################################
#
echo "vppfbd_csc_resize"
function vppfbd_csc_resize()
{
    ./vppfbd_csc_resize 1920 1080 fbc_offset_table_y.bin fbc_comp_data_y.bin fbc_offset_table_c.bin fbc_comp_data_c.bin 1920 3 984 638 out.bmp

    md5sum -c vppfbd_csc_resize.md5

    result="$?"

    if [ "$result" -ne 0 ]; then
        echo "vppfbd_csc_resize  -- error" >> result.txt
    else
        echo "vppfbd_csc_resize  -- okay" >> result.txt
    fi

    rm out.bmp
}

vppfbd_csc_resize
#####################################################
### vpp_iondemo
#####################################################
echo "vpp_iondemo"
function vpp_iondemo()
{
    ./vpp_iondemo 1920 1080 img1.nv12  2 1540 864 out.bmp 3

    md5sum -c vpp_iondemo.md5
    
    result="$?"
    
    if [ "$result" -ne 0 ]; then
    	echo "vpp_iondemo 1920 1080 img1.nv12  2 1540 864 out.bmp 3 -- error" >> result.txt
    else
    	echo "vpp_iondemo 1920 1080 img1.nv12  2 1540 864 out.bmp 3 -- okay" >> result.txt
    fi
    
    rm out.bmp
}
vpp_iondemo
#####################################################
### vppmisc
#####################################################
echo "vppmisc"
function vppmisc()
{
    ./vpp_misc 1920 1080 img1.nv12  2 crop 3
 
    md5sum -c vpp_misc.md5
    
    result="$?"
    
    if [ "$result" -ne 0 ]; then
    	echo "vpp_misc  -- error" >> result.txt
    else
    	echo "vpp_misc  -- okay" >> result.txt
    fi
    for i in $(seq 0 15)  
    do   
        outputimage=crop${i}.bmp
        rm ${outputimage}
    done  

}

vppmisc
#####################################################
### vpp_resize
#####################################################
echo "vpp_resize"
function vpp_resize()
{
    ./vpp_resize 1920 1080 img2.yuv 1 $1 $2 954 488 i420.yuv
    ./vpp_resize 1920 1080 bgr-org-opencv.bin  3 $1 $2 877 1011 bgr2.bmp
    ./vpp_resize 1920 1080 bgrp.bin 6 $1 $2 1386 674 bgrp2.bin
    ./vpp_resize 1920 1080 yuv444p.bin 7 $1 $2 2731 1673 yuv.444p

    md5sum -c vpp_resize.md5
    
    result="$?"
    
    if [ "$result" -ne 0 ]; then
        echo "vpp_resize  -- error" >> result.txt
    else
        echo "vpp_resize  -- okay" >> result.txt
    fi
    
    rm i420.yuv  bgr2.bmp bgrp2.bin yuv.444p
}

vpp_resize 0 0
vpp_resize 0 1
vpp_resize 1 0
vpp_resize 1 1

######################################################
#### vpp_split
######################################################

echo "vpp_split"
./vpp_split 1920 1080 bgr-org-opencv.bin 0 0 r.bmp g.bmp b.bmp

md5sum -c vpp_split.md5

result="$?"

if [ "$result" -ne 0 ]; then
	echo "vpp_split -- error" >> result.txt
else
	echo "vpp_split -- okay" >> result.txt
fi

rm b.bmp g.bmp r.bmp


