**bmx264** is an wrapper around x264 stable that runs on A53 CPU for PCIE mode.

The x264 stable version is cde9a93 on 3 Jul 2020.

-----------------
How to compiling x264 for A53 CPU:

```
./configure \
        --host=aarch64-linux-gnu \
        --cross-prefix=$(dirname `which aarch64-linux-gnu-gcc`)/aarch64-linux-gnu- \
        --disable-cli --disable-avs --disable-ffms --disable-swscale --disable-lavf \
        --enable-shared \
        --bit-depth=8 \
        --disable-interlaced

make clean
make  -j`nproc`

```

-----------------

