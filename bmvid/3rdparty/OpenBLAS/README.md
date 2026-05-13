# Description
OpenBLAS is introduced for qr_decomposition.

## OpenBLAS Project
```bash
https://github.com/OpenMathLib/OpenBLAS
[pcie_x86][soc_ARM]
commit cd3945b99881423035cd9cdd00928e5d1671f30a
[loongarch_anolis]
commit db0abfa9070f4f2b1f990c1738a6bcba04172001
```

## OpenBLAS Usage
This project utilizes OpenBLAS, an open source implementation of the Basic Linear Algebra Subprograms (BLAS) library. OpenBLAS is used in compliance with open source laws and licenses.

## OpenBLAS License
OpenBLAS is distributed under the BSD 3-Clause License. You can find the full text of the license [here](link-to-license).


# New Variable for CMake
Extra Export when Compiling bmcv_test
```bash
export USING_OPENBLAS=1
```

# BMCV Compiling CMD(PCIE)
## 1)Compile OpenBLAS(X86)
### 1.1)Update libopenblas.so(X86)
#### [Warning] if performance is required, keep RELEASE
#### 1.1.a)Update libopenblas.so(X86) for gerrit regression ubuntu-16.04(Low Performance)

```bash
sudo docker rm -f openblas_ubuntu_1604
sudo docker run --restart always --privileged -v /dev:/dev -td -v $PWD:/workspace --name openblas_ubuntu_1604 ubuntu:16.04 bash
sudo docker exec -it openblas_ubuntu_1604  bash
```
install g++-5 gcc-5 by reference to `https://askubuntu.com/questions/1235819/ubuntu-20-04-gcc-version-lower-than-gcc-7`
```bash
apt-get update
apt-get install build-essential
apt install g++-5 gcc-5 gfortran-5
```
```bash
cd OpenBLAS
make clean
make CC=gcc-5 HOSTCC=gcc-5 NO_FORTRAN=1
```

#### 1.1.b)Update libopenblas.so(X86) for ubuntu-20.04 (Use for Performance)
```bash
git clone https://github.com/OpenMathLib/OpenBLAS.git
cd OpenBLAS
make clean
make
#make DEBUG=1
```

### 1.2)Update libopenblas.so(X86)
```bash
cp OpenBLAS/libopenblas.so /mnt/software/bmvid/3rdparty/OpenBLAS/x86/libopenblas.so
cd /mnt/software/bmvid/3rdparty/OpenBLAS/x86/
ln -s libopenblas.so  libopenblas.so.0
```

## 2)Compile BMCV(X86)
### [Warning]
Directly compile in host-ubuntu:20.04, not in a docker container, so that GLIBC on CC/FC/Target will be 2.3.1

### 2.1)Export cross-compilers
```bash
cd /mnt/software/bmvid
export CHIP=bm1684

export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/x86-64-core-i7--glibc--stable/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/mips-loongson-gcc7.3-linux-gnu/2019.06-29/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin
export PATH=$PATH:/usr/sw/swgcc830_cross_tools-bb623bc9a/usr/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-elf/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin
```
### 2.2)Set variables and clean cmake
```bash
export USING_OPENBLAS=1
DEBUG=1
CHIP=bm1684

PRODUCTFORM=pcie

source build/envsetup.sh
unset USING_CMODEL
source build/build.sh
clean_bmcv_lib
```
###  2.3)Export Necessary Shared Objects
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib/cmodel/libbmcv.so
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-elf/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid//3rdparty/libbmcv/lib/pcie
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib/tpu_module/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/3rdparty/tpu_kernel_module/
```
###  2.4)Build BMCV and Test on X86
```bash
build_ion_lib
build_vpp_lib
build_jpu_lib
build_bmcv_lib
build_bmcv_test
```
###  2.5)Test Cluster(X86)
```bash
./test_cv_cluster 6k.txt output.txt 2400 256 0.01 2 8 2
./test_cv_qr
```


# BMCV Compiling CMD(SOC)
## 1)Cross-Compile OpenBLAS
### 1.1)make libopenblas.so for Soc-ARM on bmvid-X86
#### [Warning] Do not compile `libopenblas.so` on Soc-ARM!
#### [Requirement] Directly use `bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu`
#### [Warning] Do not pre-install  `aarch64-linux-gnu-gcc` or `gfortran-aarch64-linux-gnu` on X86, ensure `/usr/aarch64-linux-gnu-gcc` is empty
```bash
git clone https://github.com/OpenMathLib/OpenBLAS.git
cd OpenBLAS
export CC=/path/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
export CXX=/pathbm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++
export AR=/path/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-ar
export RANLIB=/path/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-ranlib
export HOSTCC=gcc
export CFLAGS="-march=armv8-a"
export CXXFLAGS="-march=armv8-a"
make TARGET=ARMV8
```

### 1.2)Update libopenblas.so from Soc-ARM to bmvid-X86
```bash
cp OpenBLAS/libopenblas.so /workspace/bmvid/3rdparty/OpenBLAS/arm/libopenblas.so
cd /mnt/software/bmvid/3rdparty/OpenBLAS/arm/
ln -s libopenblas.so  libopenblas.so.0
```

## 2)Cross-Compile BMCV(for Soc on X86)
### [Warning]
Directly compile in host-ubuntu:20.04, not in a docker container, so that GLIBC on CC/FC/Target will be 2.3.1

### 2.1)Export cross-compilers(X86)
```bash
cd /mnt/software/bmvid
export CHIP=bm1684

export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/x86-64-core-i7--glibc--stable/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/mips-loongson-gcc7.3-linux-gnu/2019.06-29/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin
export PATH=$PATH:/usr/sw/swgcc830_cross_tools-bb623bc9a/usr/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-elf/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin
```
### 2.2)Set variables and clean cmake(X86)
```bash
export USING_OPENBLAS=1
DEBUG=1
CHIP=bm1684

PRODUCTFORM=soc

source build/envsetup.sh
unset USING_CMODEL
source build/build.sh
clean_bmcv_lib
```
###  2.3)Export Necessary Shared Objects(X86)
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib/cmodel/libbmcv.so
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-elf/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid//3rdparty/libbmcv/lib/pcie
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib/tpu_module/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/3rdparty/tpu_kernel_module/
```
###  2.4)Build BMCV and Test on X86(X86)
```bash
build_ion_lib
build_vpp_lib
build_jpu_lib
build_bmcv_lib
build_bmcv_test
```
###  2.5)Prepare Files (ARM-Soc)
```bash
scp ./install/soc_bm1684_asic/bmcv/lib/libbmcv.so                             linaro@172.25.4.220:/home/linaro/QR/.
scp ./install/soc_bm1684_asic/bmcv/test/test_cv_cluster                       linaro@172.25.4.220:/home/linaro/QR/.
scp ./install/soc_bm1684_asic/bmcv/lib/tpu_module/libbm1684x_kernel_module.so linaro@172.25.4.220:/home/linaro/QR/.
scp ./6k.txt                                                                  linaro@172.25.4.220:/home/linaro/QR/.
```
###  2.7)Prepare Files (ARM-Soc)
```bash
cp libbmcv.so libbmcv.so.0
patchelf --set-rpath ./libbmcv.so.0 test_cv_cluster
ldd ./test_cv_cluster
sudo ln -s /home/linaro/libopenblas.so /usr/lib/libopenblas.so.0
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/linaro/QR/libbm1684x_kernel_module.so

```
###  2.6)Test Cluster(ARM-Soc)
```bash
./test_cv_cluster 6k.txt output.txt 2400 256 0.01 2 8 2 #4.52s
```

# BMCV Compiling CMD(LoongArch)
## 1)Cross-Compile OpenBLAS
### 1.1)make libopenblas.so for LoongArch on bmvid-X86
#### [Arch] loongarch_anolis in OpenBlas belongs to LA64_GENERIC
#### [Requirement] Directly use `bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu`
#### [Warning] Do not pre-install  `aarch64-linux-gnu-gcc` or `gfortran-aarch64-linux-gnu` on X86, ensure `/usr/aarch64-linux-gnu-gcc` is empty
```bash
git clone https://github.com/OpenMathLib/OpenBLAS.git
cd OpenBLAS

make TARGET=LA64_GENERIC NO_TEST=1 \
     HOSTCC=gcc \
     CC=/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin/loongarch64-linux-gnu-gcc \
     CXX=/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin/loongarch64-linux-gnu-g++ \
     AR=/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin/loongarch64-linux-gnu-ar \
     RANLIB=/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin/loongarch64-linux-gnu-ranlib \
     CROSS=loongarch64-linux-gnu- \
     CFLAGS="-march=loongarch64" \
     CXXFLAGS="-march=loongarch64" \
     LDFLAGS="-lpthread -lm"
```

### 1.2)Update libopenblas.so to bmvid-X86
```bash
cp OpenBLAS/libopenblas_la64_genericp-r0.3.29.dev.so /mnt/software/bmvid/3rdparty/OpenBLAS/loongarch/libopenblas.so
cd /mnt/software/bmvid/3rdparty/OpenBLAS/loongarch/
ln -s libopenblas.so  libopenblas.so.0
```

## 2)Cross-Compile BMCV(for LoongArch on X86)
### [Warning]
Directly compile in host-ubuntu:20.04, not in a docker container, so that GLIBC on CC/FC/Target will be 2.3.1

### 2.1)Export cross-compilers(X86)
```bash
cd /mnt/software/bmvid
export CHIP=bm1684

export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/x86-64-core-i7--glibc--stable/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/mips-loongson-gcc7.3-linux-gnu/2019.06-29/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/loongarch64-linux-gnu-2021-04-22-vector/bin
export PATH=$PATH:/usr/sw/swgcc830_cross_tools-bb623bc9a/usr/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-elf/bin
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin
```
### 2.2)Set variables and clean cmake(X86)
```bash
export USING_OPENBLAS=1
DEBUG=1
CHIP=bm1684

PRODUCTFORM=pcie_loongarch64

source build/envsetup.sh
unset USING_CMODEL
source build/build.sh
clean_bmcv_lib
```
###  2.3)Export Necessary Shared Objects(X86)
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib/cmodel/libbmcv.so
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib
export PATH=$PATH:/mnt/software/bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-elf/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid//3rdparty/libbmcv/lib/pcie
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/install/pcie_bm1684_asic/bmcv/lib/tpu_module/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/software/bmvid/3rdparty/tpu_kernel_module/
```
###  2.4)Build BMCV and Test on X86(X86)
```bash
build_ion_lib
build_vpp_lib
build_jpu_lib
build_bmcv_lib
build_bmcv_test
```


# Cross-Compile Checklist
# 1)libgfortran and gcc-linaro
### [Warning] Do not pre-install `aarch64-linux-gnu-gcc` or `gfortran-aarch64-linux-gnu` on X86, ensure empty of `/usr/aarch64-linux-gnu-gcc`
### [Requirement] Directly use `bm_prebuilt_toolchains/gcc-linaro-6.3.1-2017.05`
```bash
test_cv_cluster=>libgfortran.so.5 => /lib/x86_64-linux-gnu/libgfortran.so.5
x86/libopenblas.so=>libgfortran.so.5 => /lib/x86_64-linux-gnu/libgfortran.so.5
```
For ARM, their compatible relationship is:
```bash
test_cv_cluster=>libgfortran.so.5 => /lib/aarch64-linux-gnu/libgfortran.so.5
arm/libopenblas.so=>libgfortran.so.5 => /lib/aarch64-linux-gnu/libgfortran.so.5
```
# 2)GLIBC
bmcv requires a 2.31v-GLIBC, which compatible with ubuntu:20.04, `NOT` with tpuc_dev:latest docker image with ubuntu:22.04 (actually 2.35v-GLIBC there).


# Release
## 1) USING_OPENBLAS
### 1.1) using build_bmcv_lib
```bash
/bmvid/build/build.sh => bmcv/Makefile => bmcv/test/Makefile
```
### 1.2) Daily Build under libsophon
```bash
/bmvid/CMakeLists.txt => bmvid_x86_pcie_device.cmake => FindBMVidTarget.cmake => bmcv/Makefile => bmcv/test/Makefile
```

## 2) libopenblas.so
### 2.0) install deb
now  *.deb will install libopen.so(.0) from libsophon/build/bmvid/release/lib

### 2.1) Dynamic-Link Priority
when USING_OPENBLAS is on:
#### 2.1.a) build_bmcv_lib
- search `/opt/sophon/libsophon-current/lib` first
- search `/usr/bin/libopenblas.so`
- search `bmvid/install/lib`
- search `libsophon/bmvid/3rdparty/OpenBLAS` last
#### 2.1.b) libsophon-make(x86)
- search `/opt/sophon/libsophon-current/lib` first
- search `/usr/bin/libopenblas.so`
- search `libsophon/build/bmvid/release/lib`
- search `libsophon/bmvid/3rdparty/OpenBLAS` last

### 2.2) Compatiblity

|          | regression                              |  ubuntu-16.04  |                      |                            |                          |          |                    |X | offline|ubuntu-20.04               |                        |                           |                                |
|----------|-----------------------------------------|------|---------------------------|------------------------|----------------------------|---------------------------|----------|--|--------|---------------------------|------------------------|---------------------------|--------------------------------|
| Platform | Compiling Command                       |test  | test/Makefile:DYN_LDFLAGS | libbmcv                | bmcv/Makefile:DYN_LDFLAGS  | libopenblas               | Status   |X | test   | test/Makefile:DYN_LDFLAGS | libbmcv                | bmcv/Makefile:DYN_LDFLAGS | libopenblas (all ok)           |
| pcie     |build_bmcv_lib/libsophon-make-install    |test_X| <------------------------ | install/libbmcv.so     | <------------x-----------  | /usr/bin/libopenblas.so   |not exist |X | test_X | <------------------------ | install/libbmcv.so     | <------------------------ | /usr/bin/libopenblas.so        |
|          |                                         |      |                           |                        | <------------------------  | 3rdparty/libopenblas.so   |ok        |X |        |                           |                        | <------------------------ | 3rdparty/libopenblas.so        |
|          |                                         |      |                           |                        | <------------x-----------  | /opt/lib/libopenblas.so   |not exist |X |        |                           |                        | <------------------------ | /opt/lib/libopenblas.so        |
|          |dpkg                                     |      | <------------------------ | /opt/lib/libbmcv.so    | <------------x-----------  | /usr/bin/libopenblas.so   |not linked|X | test_X | <------------------------ | /opt/lib/libbmcv.so    | <------------------------ | /usr/bin/libopenblas.so        |
|          |                                         |      |                           |                        | <------------------------  | 3rdparty/libopenblas.so   |ok        |X |        |                           |                        | <------------------------ | 3rdparty/libopenblas.so        |
|          |                                         |      |                           |                        | <------------x-----------  | /opt/lib/libopenblas.so   |not exist |X |        |                           |                        | <------------------------ | /opt/lib/libopenblas.so        |
|          |libsophon-make                           |      | <------------------------ | release/lib/libbmcv.so | <------------x-----------  | /usr/bin/libopenblas.so   |not linked|X | test_X | <------------------------ | release/lib/libbmcv.so | <------------------------ | /usr/bin/libopenblas.so        |
|          |                                         |      |                           |                        | <------------------------  | release/lib/libopenblas.so|ok        |X |        |                           |                        | <------------------------ | release/lib/libopenblas.so     |
|          |                                         |      |                           |                        | <------------------------  | 3rdparty/libopenblas.so   |ok        |X |        |                           |                        | <------------------------ | 3rdparty/libopenblas.so        |
|          |                                         |      |                           |                        | <------------x-----------  | /opt/lib/libopenblas.so   |not exist |X |        |                           |                        | <------------------------ | /opt/lib/libopenblas.so        |




|Platform | regression     |   ubuntu-16.04  |                  |                            |                                | X | offline     |ubuntu-20.04                         |                  |                            |                                |
|------|------|-------------------------|------------------|----------------------------|--------------------------------|---|------|-------------------------|------------------|----------------------------|--------------------------------|
| test | test | test/Makefile:DYN_LDFLAGS | libbmcv          | bmcv/Makefile:DYN_LDFLAGS | libopenblas                     | X | test | test/Makefile:DYN_LDFLAGS | libbmcv          | bmcv/Makefile:DYN_LDFLAGS | libopenblas                     |
| soc  | test_X | <------------------------ | install/libbmcv.so | <------------------------ | 3rdparty/libopenblas.so        | X | test_X | <------------------------ | install/libbmcv.so | <------------------------ | 3rdparty/libopenblas.so        |
|      |      |                         |                  | <------------x----------- | /gcc-linaro-6.3.1/libopenblas.so (not exist) | X |      |                         |                  | <------------------------ | /gcc-linaro-6.3.1/libopenblas.so            |
|      |      | <------------x----------- | /gcc-linaro-6.3.1/libbmcv.so (not exist) | <----------x------------- | 3rdparty/libopenblas.so (nolink) | X | test_X | <----------x------------ | /gcc-linaro-6.3.1/libbmcv.so (not exist) | <-----------x------------ | 3rdparty/libopenblas.so (nolink) |
|      |      |                         |                  | <------------x----------- | /gcc-linaro-6.3.1/libopenblas.so (not exist) | X |      |                         |                  | <-----------x------------ | /gcc-linaro-6.3.1/libopenblas.so            |