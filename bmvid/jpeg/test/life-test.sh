#!/bin/sh

PWD=$(pwd)
JPEG_PATH=${PWD}/jpeg

echo "JPU life test loop [ hit CTRL+C to stop]"

# change file permission
chmod +x bm_jpg_test
chmod +x jpurun
chmod +x jpurun422
chmod +x load_jpu.sh

# load jpu driver
./load_jpu.sh

# CASE 1
function test_case1
{
    rm -rf dec0.jpg
    ./bm_jpg_test -t 1 -i ${JPEG_PATH}/2_yuv420.jpg -o dec
	cmp ${JPEG_PATH}/2_yuv420.yuv dec0.jpg
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "case 1 test failed" | tee -a jpu_life_test.log
    else
        echo "case 1 test passed" | tee -a jpu_life_test.log
    fi
}

# CASE 2
function test_case2
{
    rm -rf enc0.jpg
    ./bm_jpg_test -t 2 -w 1280 -h 1024 -i ${JPEG_PATH}/2_yuv420.yuv -o enc
	cmp ${JPEG_PATH}/2_yuv420_enc.jpg enc0.jpg
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "case 2 test failed" | tee -a jpu_life_test.log
    else
        echo "case 2 test passed" | tee -a jpu_life_test.log
    fi
}

# Life Loop Test
echo `date` >> jpu_life_test.log
loop_num=1
# while :
# do
    loop_num=`expr $loop_num + 1`
    test_case1
    test_case2
    
# done
