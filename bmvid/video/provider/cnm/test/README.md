# fetch project bmtest

get submodule init
get submodule update

# link the folder of bmtest to bm1684

cd bm1684
ln -s ../bmtest/bm1684/boot/ .
ln -s ../bmtest/bm1684/common/ .
ln -s ../bmtest/bm1684/config.mk .
ln -s ../bmtest/bm1684/drivers/ .
ln -s ../bmtest/bm1684/include/ .
ln -s ../bmtest/bm1684/platform/ .
ln -s ../bmtest/bm1684/scripts/ .

cd test
ln -s ../../bmtest/bm1684/test/uart .

# Build ddr testcase

## Make fixes in bmtest as below

vi common/system.c +128
添加 while(1) ; 到ddr_init()后
vi common/system.c +123
把enable_caches注释掉

## 生成ddr_init程序

make clean; make -j4 CHIP=BM1684 RUN_ENV=SRAM PLATFORM=ASIC TEST_CASE=uart DEBUG=0
保存BM1684_uart.elf到BM1684_ddr.elf
CVD运行程序
Program-->Load-->BM1684_ddr.elf
Run-->Go
Run-->Stop

# build wave testcase

## Add cdma in bmtest as below

diff --git a/bm1684/drivers/config.mk b/bm1684/drivers/config.mk
index 52a1344..a47cba8 100644
--- a/bm1684/drivers/config.mk
+++ b/bm1684/drivers/config.mk
@@ -1,5 +1,9 @@
+SYSTEM_ID?=0
+DEFS += -DVIDEO_SYSTEM${SYSTEM_ID}
+
 include $(ROOT)/drivers/sdhci/config.mk
 include $(ROOT)/drivers/uart/config.mk
+include $(ROOT)/drivers/sysdma/config.mk

 INC += -I$(ROOT)/drivers/include

## building

make clean; make -j4 CHIP=BM1684 RUN_ENV=DDR PLATFORM=ASIC TEST_CASE=wave DEBUG=0
Program-->Load→BM1684_wave.elf
Run-->Go
