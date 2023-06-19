#!/bin/sh

if [ $# -eq 0 ];then
echo "Usage:./yuv_md5.sh yuv_file_name [width] [height]"
return
fi

if [ -z $2 ];then
width=1920
else
width=$2
fi

if [ -z $3 ];then
height=1080
else
height=$3
fi


inputfile=$1
md5file=$inputfile.md5

ysize=$(( $width * $height ))
usize=$(( $ysize / 4 ))
framesize=$(( $ysize * 3 / 2 ))

filesize=`ls -l $inputfile | awk '{print $5}'`

framenum=$(( $filesize / $framesize))

if [ -f $md5file ];then
rm -rf $md5file
touch $md5file
fi

i=0
while [ $i -lt $framenum ];do
y=$(( $i * 6))
u=$(( $i * 6 + 4 ))
v=$(( $i * 6 + 5 ))
dd if=$inputfile bs=$usize count=4 skip=$y status=none | md5sum | cut -d ' ' -f1 >> $md5file
dd if=$inputfile bs=$usize count=1 skip=$u status=none | md5sum | cut -d ' ' -f1 >> $md5file
dd if=$inputfile bs=$usize count=1 skip=$v status=none | md5sum | cut -d ' ' -f1 >> $md5file

i=$(( $i + 1 ))
done



