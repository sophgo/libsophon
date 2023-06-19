#!/bin/bash

date >> result.txt
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
 
    md5sum -c vpp_convert_cmodel.md5
    
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
vpp_convert 1 1

#####################################################
### vpp_iondemo
#####################################################
echo "vpp_iondemo"
function vpp_iondemo()
{
    ./vpp_iondemo 1920 1080 img1.nv12  2 1540 864 out.bmp 3

    md5sum -c vpp_iondemo-cmodel.md5
    
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
### vpp_resize
#####################################################
echo "vpp_resize"
function vpp_resize()
{
    ./vpp_resize 1920 1080 img2.yuv 1 $1 $2 954 488 i420.yuv
    ./vpp_resize 1920 1080 bgr-org-opencv.bin  3 $1 $2 877 1011 bgr2.bmp
    ./vpp_resize 1920 1080 bgrp.bin 6 $1 $2 1386 674 bgrp2.bin
    ./vpp_resize 1920 1080 yuv444p.bin 7 $1 $2 2731 1673 yuv.444p
    
    md5sum -c vpp_resize_cmodel.md5
    
    result="$?"
    
    if [ "$result" -ne 0 ]; then
        echo "vpp_resize  -- error" >> result.txt
    else
        echo "vpp_resize  -- okay" >> result.txt
    fi
    
    rm i420.yuv  bgr2.bmp bgrp2.bin yuv.444p
}

vpp_resize 1 1
