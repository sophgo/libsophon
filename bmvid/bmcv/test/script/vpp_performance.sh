#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)

if [ ! -d output ]; then
  mkdir -p output
fi

function convertto()
{
echo "format, output data type, time,fps, pixel per sec " > $2
#gray
./test_bm1684x_vpp_ax+b 1920 1080 14 input/gray.bin  1920 1080  14  output/1920*1080-gray.dst  3 1000 $2 $1
./test_bm1684x_vpp_ax+b 1920 1080 14 input/gray.bin  1920 1080  14  output/1920*1080-gray.dst  1 1000 $2 $1

#yuv420p
./test_bm1684x_vpp_ax+b 1920 1080 0 input/i420.yuv 1920 1080  0 output/1920*1080-i420.dst  3 1000 $2 $1
./test_bm1684x_vpp_ax+b 1920 1080 0 input/i420.yuv 1920 1080  0 output/1920*1080-i420.dst  1 1000 $2 $1


#yuv444p s8 u8 fp16 bpf16 fp32
./test_bm1684x_vpp_ax+b 1920 1080 2 input/yuv444p.bin  1920 1080  2  output/1920*1080-yuv444p.dst  3 1000 $2 $1
./test_bm1684x_vpp_ax+b 1920 1080 2 input/yuv444p.bin  1920 1080  2  output/1920*1080-yuv444p.dst  1 1000 $2 $1
./test_bm1684x_vpp_ax+b 1920 1080 2 input/yuv444p.bin  1920 1080  2  output/1920*1080-yuv444p.dst  5 1000 $2 $1 
./test_bm1684x_vpp_ax+b 1920 1080 2 input/yuv444p.bin  1920 1080  2  output/1920*1080-yuv444p.dst  6 1000 $2 $1
./test_bm1684x_vpp_ax+b 1920 1080 2 input/yuv444p.bin  1920 1080  2  output/1920*1080-yuv444p.dst  0 1000 $2 $1

#rgb24
./test_bm1684x_vpp_ax+b 1920 1080 10 input/rgb24.bin  1920 1080  10  output/1920*1080-rgb24.dst 3 1000 $2 $1
./test_bm1684x_vpp_ax+b 1920 1080 10 input/rgb24.bin  1920 1080  10  output/1920*1080-rgb24.dst 1 1000 $2 $1
}



function fillrectangle()
{
echo "time,fps, pixel per sec " > $2
./test_bm1684x_vpp_fill_rectangle 1920 1080 10 input/rgb24.bin 0 0 1920 1080 output/1920*1080-rgb24.dst 1000 $2 $1
}



function csc()
{
echo "w * h -> w * h, input_format -> output_format, algorithm, time, fps, pixel per sec " > $2
#420->rgb24,rgbp
./test_bm1684x_vpp_convert 1920 1080 0 input/i420.yuv 0 0 1920 1080 1920 1080 10 output/1920*1080-rgb24.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 0 input/i420.yuv 0 0 1920 1080 1920 1080 12 output/1920*1080-rgbp.dst 1 1000 $2 $1


#422->rgb24,rgbp
./test_bm1684x_vpp_convert 1920 1080 1 input/yuv422p.bin 0 0 1920 1080 1920 1080 10 output/1920*1080-rgb24.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 1 input/yuv422p.bin 0 0 1920 1080 1920 1080 12 output/1920*1080-rgbp.dst 1 1000 $2 $1

#444->rgb24,rgbp
./test_bm1684x_vpp_convert 1920 1080 2 input/yuv444p.bin 0 0 1920 1080 1920 1080 10 output/1920*1080-rgb24.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 2 input/yuv444p.bin 0 0 1920 1080 1920 1080 12 output/1920*1080-rgbp.dst 1 1000 $2 $1


#444->hsv180, hsv256
./test_bm1684x_vpp_convert 1920 1080 2 input/yuv444p.bin 0 0 1920 1080 1920 1080 26 output/1920*1080-hsv180.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 2 input/yuv444p.bin 0 0 1920 1080 1920 1080 27 output/1920*1080-hsv256.dst 1 1000 $2 $1

#rgb24->420,444,hsv180, hsv256
./test_bm1684x_vpp_convert 1920 1080 10 input/rgb24.bin 0 0 1920 1080 1920 1080 0 output/1920*1080-yuv420.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 10 input/rgb24.bin 0 0 1920 1080 1920 1080 2 output/1920*1080-yuv444.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 10 input/rgb24.bin 0 0 1920 1080 1920 1080 26 output/1920*1080-hsv180.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 10 input/rgb24.bin 0 0 1920 1080 1920 1080 27 output/1920*1080-hsv256.dst 1 1000 $2 $1


#rgbp->420,444,hsv180, hsv256
./test_bm1684x_vpp_convert 1920 1080 12 input/rgbp.bin 0 0 1920 1080 1920 1080 0 output/1920*1080-yuv420.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 12 input/rgbp.bin 0 0 1920 1080 1920 1080 2 output/1920*1080-yuv444.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 12 input/rgbp.bin 0 0 1920 1080 1920 1080 26 output/1920*1080-hsv180.dst 1 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 12 input/rgbp.bin 0 0 1920 1080 1920 1080 27 output/1920*1080-hsv256.dst 1 1000 $2 $1
}



function resize()
{
echo "w * h -> w * h, input_format -> output_format, algorithm,time, fps, pixel per sec " > $2
#gray->gray 1920*1080->800*600
./test_bm1684x_vpp_convert 1920 1080 14 input/gray.bin 0 0 1920 1080 800 600 14 output/1920*1080-gray.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 14 input/gray.bin 0 0 1920 1080 800 600 14 output/1920*1080-gray.dst 1 1000 $2 $1
#gray->gray 800*600->1920*1080
./test_bm1684x_vpp_convert 800 600 14 input/gray.bin 0 0 800 600 1920 1080 14 output/800*600-gray.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 800 600 14 input/gray.bin 0 0 800 600 1920 1080 14 output/800*600-gray.dst 1 1000 $2 $1

#420->420 1920*1080->800*600
./test_bm1684x_vpp_convert 1920 1080 0 input/i420.yuv 0 0 1920 1080 800 600 0 output/1920*1080-420p.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 0 input/i420.yuv 0 0 1920 1080 800 600 0 output/1920*1080-420p.dst 1 1000 $2 $1
#420->420 800*600->1920*1080
./test_bm1684x_vpp_convert 800 600 0 input/i420.yuv 0 0 800 600 1920 1080 0 output/800*600-420p.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 800 600 0 input/i420.yuv 0 0 800 600 1920 1080 0 output/800*600-420p.dst 1 1000 $2 $1

#444->444 1920*1080->800*600
./test_bm1684x_vpp_convert 1920 1080 2 input/yuv444p.bin 0 0 1920 1080 800 600 2 output/1920*1080-444p.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 2 input/yuv444p.bin 0 0 1920 1080 800 600 2 output/1920*1080-444p.dst 1 1000 $2 $1
#444->444 800*600->1920*1080
./test_bm1684x_vpp_convert 800 600 2 input/yuv444p.bin 0 0 800 600 1920 1080 2 output/800*600-444p.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 800 600 2 input/yuv444p.bin 0 0 800 600 1920 1080 2 output/800*600-444p.dst 1 1000 $2 $1

#rgb24->rgb24 1920*1080->800*600
./test_bm1684x_vpp_convert 1920 1080 10 input/rgb24.bin 0 0 1920 1080 800 600 10 output/1920*1080-rgb24.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 1920 1080 10 input/rgb24.bin 0 0 1920 1080 800 600 10 output/1920*1080-rgb24.dst 1 1000 $2 $1
#rgb24->rgb24 800*600->1920*1080
./test_bm1684x_vpp_convert 800 600 10 input/rgb24.bin 0 0 800 600 1920 1080 10 output/800*600-rgb24.dst 0 1000 $2 $1
./test_bm1684x_vpp_convert 800 600 10 input/rgb24.bin 0 0 800 600 1920 1080 10 output/800*600-rgb24.dst 1 1000 $2 $1
}



function padding()
{
echo "format, time, fps, pixel per sec " > $2
./test_bm1684x_vpp_padding 800 600 14 input/gray.bin 80 60 800 600 960 720 14 output/800*600-gray.dst 1000  $2 $1
./test_bm1684x_vpp_padding 800 600 0 input/i420.yuv 80 60 800 600 960 720 0 output/800*600-yuv420p.dst 1000  $2 $1
./test_bm1684x_vpp_padding 800 600 2 input/yuv444p.bin 80 60 800 600 960 720 2 output/800*600-yuv444p.dst 1000  $2 $1
./test_bm1684x_vpp_padding 800 600 10 input/rgb24.bin 80 60 800 600 960 720 10 output/800*600-rgb24.dst 1000  $2 $1
}



function draw_rectangle()
{
echo "format, time, fps, pixel per sec " > $2
./test_bm1684x_vpp_border   1920 1080 14 input/gray.bin 101 99 800 600 47 output/1920*1080-800*600-gray.dst 1000 $2 $1
./test_bm1684x_vpp_border   1920 1080 0 input/i420.yuv 101 99 800 600 47 output/1920*1080-800*600-yuv420p.dst 1000 $2 $1
./test_bm1684x_vpp_border   1920 1080 2 input/yuv444p.bin 101 99 800 600 47 output/1920*1080-800*600-yuv444p.dst 1000 $2 $1
./test_bm1684x_vpp_border   1920 1080 10 input/rgb24.bin 101 99 800 600 47 output/1920*1080-800*600-rgb24.dst 1000 $2 $1
}

if test $1
then
devid=$1
else
devid=0
fi

echo "test convertto"
convertto $devid output/1920x1080-linear_transform.csv
echo -e "\n"

echo "test fillrectangle"
fillrectangle $devid output/1920x1080-rgb24packed-fill-rectangle.csv
echo -e "\n"

echo "test csc"
csc $devid output/1920x1080-csc.csv
echo -e "\n"

echo "test scale"
resize $devid output/scale.csv
echo -e "\n"

echo "test padding"
padding $devid output/800x600-padding.csv
echo -e "\n"

echo "test draw_rectangle"
draw_rectangle $devid output/1920x1080-draw_rectangle.csv

