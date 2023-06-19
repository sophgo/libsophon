#!/bin/bash
#rm test/boda/libbmvideo.a

# BOARD=FPGA, ASIC
# SLT_TEST
#   - slt, slt test
#   - else, normal test
make clean; make TEST_CASE=boda BOARD=ASIC NPU_EN=0 RUN_ENV=DDR DEBUG=0 SLT_TEST=0
