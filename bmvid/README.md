BMVID Readme
=======================

This is bitmain video repository for all chips. Following development will be put in this repository.
- video driver
- video API library
- video baremetal test case

## **THIS PROJECT IS OBSOLETE**

If this project is not under maintaning any more, Please add this section and give the new project url.

## **I REPEAT, THIS Project IS OBSOLETE**

Requirements
------------
### Platforms
- Baremetal
- Linux linaro version 4.9.38

### Packages
- build-essential gdb vim cmake libtool autoconf autotools-dev automake python-dev python3-dev python-pip python-cffi python-numpy python-scipy gcc-arm-none-eabi libgoogle-glog-dev libboost-all-dev

Build Command
-------------
### Using shell to build

```
# source build/envsetup.sh
# source build/build.sh // show all available command
# build_bmvid
```

### Using Cmake to build

```
# mkdir buildit && cd buildit
# cmake -DCMAKE_TOOLCHAIN_FILE=../platforms/linux/x86_64-gnu.toolchain.cmake ..
# make help  // show all available build target
# make install
```

### Build in windows

```
# cd build
# .\build_windows.bat make_bmvid release MT 1
```

TODO
----

- one
- two
- three

License & Authors
-----------------
- Author:: Xun Li (<xun.li@bitmain.com>)

```text
Copyright:: 2018, Bitmain, Inc. All Rights Reserved.
