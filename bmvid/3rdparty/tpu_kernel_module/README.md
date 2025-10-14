
# TPU1686 commit_id
## BM1684X
### f8aba343f2d9425bb3e40a4bc8559af16960f1d2

cd /workspace/nntoolchain/TPU1686
source scripts/envsetup.sh
rebuild_firmware
cp build/firmware_core/libfirmware_core.so /workspace/bmvid/3rdparty/tpu_kernel_module/libbm1684x_kernel_module.so
