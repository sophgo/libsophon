### 简介
请勿轻易更换libcmodel动态库
编译动态库需要在ftp://172.28.141.89/docker/bmnnsdk2_ubuntu16.04_py35.docker环境下

## BM1684 2023-08-01
commit id: 0b2507035a79fae10ed01abb9ae8e4fe84ed21fa
`cd TPU1684 && source scripts/envsetup.sh && rebuild_backend_lib_cmodel`
`cp ./build_runtime/c_model/libcmodel.so /workspace/libsophon/tpu-runtime/test/lib/libcmodel_bm1684.so`

## BM1684X
commit id: 1c5867c30b21d6ad471c36d8418150a1ef518c2d
`cd TPU1686 && source scripts/envsetup.sh bm1684x && rebuild_firmware_cmodel`
`cp ./build/firmware_core/libcmodel_firmware.so /workspace/libsophon/tpu-runtime/test/lib/libcmodel_bm1684x.so`

## BM1688
commit id: 1c5867c30b21d6ad471c36d8418150a1ef518c2d
`cd TPU1686 && source scripts/envsetup.sh bm1686 && rebuild_firmware_cmodel`
`cp ./build/firmware_core/libcmodel_firmware.so /workspace/libsophon/tpu-runtime/test/lib/libcmodel_bm1688.so`
