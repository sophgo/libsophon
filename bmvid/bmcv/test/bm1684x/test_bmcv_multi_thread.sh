#!/bin/bash

# 在后台持续监控短期使用率，并输出到临时文件
cat_proc() {
    while true; do
        cat /proc/vppinfo >> temp_vpp_usage.txt
        sleep 1
    done
}

# 启动监控函数
cat_proc &

# 记录监控函数的进程ID
VPPUSAGE=$!
echo "VPPUSAGE $VPPUSAGE"

./test_bm1684x_vpp_multi_thread  -t 96 -d 0 --sw 1920 --sh 1080 --sformat 2 --dw 1920 --dh 1080  --dformat 2 -l 1000  -c 0 --sname yuv444p.bin -m 0 &
VPPPID=$!
echo "VPPPID $VPPPID"

pidstat -u -p $VPPPID 1 > temp_cpu_stats.txt &
PIDSTATPID=$!
echo "PIDSTATPID $PIDSTATPID"

wait $VPPPID
kill $PIDSTATPID
kill $VPPUSAGE

# 从临时文件中提取CPU利用率列，并计算平均值和峰值
AVERAGE=$(awk '{sum += $4} END {print sum/NR}' temp_cpu_stats.txt)
PEAK=$(awk '{if ($4 > max) max = $4} END {print max}' temp_cpu_stats.txt)

# 打印均值和峰值
echo "Average CPU Usage: $AVERAGE%"
echo "Peak CPU Usage: $PEAK%"

# 清理临时文件
#rm temp_cpu_stats.txt


# 从临时文件中提取vpp短期使用率列，并计算平均值和峰值
cat temp_vpp_usage.txt | awk -F'[:,|%]' '
    /"id":0/ {sum0 += $5; count0++; if ($5 > max0) max0 = $5}
    /"id":1/ {sum1 += $5; count1++; if ($5 > max1) max1 = $5}
    END {print "vpp core 0: Average Usage =", sum0/count0"%", "Peak Usage =", max0"%";
         print "vpp core 1: Average Usage =", sum1/count1"%", "Peak Usage =", max1"%"}'

# 清理临时文件
#rm temp_usage_stats.txt


