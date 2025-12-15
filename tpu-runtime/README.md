# TPU-RUNTIME

## 工程说明

tpu-runtime依赖于`libsophon`，最终编译会产生`libbmrt.so`和`bmrt_test`两个文件，并随着libsophon生成的deb包发布出去。
其中
1. `libbmrt.so`是加载`bmodel`并执行的核心库，用于模型推理程序开发。
2. `bmrt_test`用于测试`bmodel`的应用程序

基本的软件关系如下
```
            ┌─────────────┐ ┌──────────────┐
            │   app       │ │  bmrt_test   │
            └─────────┬───┘ └───┬──────────┘
                      │         │
                   ┌──▼─────────▼──┐
kernel_module ---->│  libbmrt.so   │
                   └───────┬───────┘
                           │
                   ┌───────▼───────┐
                   │  libbmlib.so  │
                   └───────┬───────┘
                           │
                   ┌───────▼───────┐
                   │    driver     │
                   └───────┬───────┘
                           │
───────────────────────────┼───────────────────────────────
                           │
                   ┌────────▼──────┐
                   │    TPU        │
                   │    Devices    │
                   └───────────────┘
```

注意：
1. tpu-runtime提交后，要与libsophon绑定才能release出去。
2. tpu-runtime库不区分cmodel版和设备版，只有一套代码。但libbmlib是区分cmodel和设备的
3. 如果是支持动态加载kernel_module的设备，需要提供缺省的kernel_module, 会以静态数组的方法编译到程序中
4. 如果tpu-runtime代码更新后，需要与libsophon进行submodule绑定，才会在打包libsophon里生效

## 环境准备
```shell
# 针对所用后端下载对应工程：TPU1684或TPU1686
git clone https://username@gerrit-ai.sophgo.vip:8443/a/TPU1686
git clone https://username@gerrit-ai.sophgo.vip:8443/a/libsophon
cd libsophon
git submodule update --init tpu-common # 不用--recursive
git submodule update --init tpu-bmodel # 不用--recursive
git submodule update --init tpu-cpuop # 不用--recursive
git submodule update --init tpu-runtime # 不用--recursive
 ./install_build_dependencies.sh
```
注意：
目前BM1684X和A2的libsophon在两个不同的分支，BM1684x是master分支，发版用的是release分支; A2是a2_dev分支，发版用的是a2_release分支
tpu-runtime用master分支，和在libsophon任意分支下编译，但由于bmlib在不同分支下功能不同, 建议：
* 如果编译A2上的runtime, libsophon要切换到`remotes/origin/a2_dev`分支, 可支持cmodel和设备模式
* 如果编译BM1684X上的runtime, 使用libsophon的master分支

## 更新缺省kernel_module
缺省的kernel_module在tpu-runtime/lib目录下, 这里的kernel_module仅对实际设备有用，不影响cmodel模式
如果kernel_module修改了bug, 可以采用如下方式更新kernel_module, 尽量不要手动更新，维护version比较麻烦
```shell
source scripts/envsetup.sh
# 更新BM1684X动态加载的kernel_module, TPU1686_PATH可选，默认在tpu-runtime/../../TPU1686
update_firmware bm1684x [TPU1686_PATH]
# 更新A2动态加载的kernel_module, TPU1686_PATH可选，默认在tpu-runtime/../../TPU1686
update_firmware bm1686 [TPU1686_PATH]
```

## X86平台编译
目前推荐的编译方式有两种，
1. 在libsophon中编译
```shell
#当前在libsophon目录下
mkdir build && cd build
cmake ../ -DPLATFORM=cmodel -DCMAKE_BUILD_TYPE=Debug
make -j

# 可选，生成libsophon的deb包
cpack
```
2. 利用回归脚本
```shell
# 当前在tpu-runtime目录下
source scripts/envsetup.sh
# 默认是Release版本， 通过EXTRA_CONFIG可以配置编译Debug版本
# export EXTRA_CONFIG="-DCMAKE_BUILD_TYPE=Debug"
rebuild_tpu_runtime
# 或
build_tpu_runtime

# 此外build_thirdparty目录中有cmodel版的libbmlib.so等依赖库，也可以取用, 但注意不要放到设备上
```

## SOC平台编译(在A53上运行)：
```shell
source scripts/envsetup.sh
# 默认是Release版本， 通过EXTRA_CONFIG可以配置编译Debug版本
# export EXTRA_CONFIG="-DCMAKE_BUILD_TYPE=Debug"
rebuild_tpu_runtime_soc
# 然后将编译好的文件,复制到目标机器工作目录中, 这里以'soc_device:work_dir'为例
# 包括bmrt_test和libbmrt.so文件
scp build/*bmrt* soc_device:work_dir

# 此外build_thirdparty目录中还有libbmlib.so等依赖库，也可以取用
```

## 设备上测试
在目标机器上, 可以不用更换libsophon, 采用如下方式
```
# 利用`ssh target`命令登陆目标机器

# 指定寻找libbmrt.so的路径
export LD_LIBRARY_PATH=work_dir

# 可以测试了
cd work_dir
./bmrt_test --bmodel test.bmodel
```

## kernel_module的临时调试
引入了环境变量BMRUNTIME_USING_FIRMWARE，用于在特定路径下加载kernel_module

kernel_module通常是在TPU1686下生成出来，可用如下命令
```
cd TPU1686
# ${CHIP_ARCH} 可以为 bm1684x或bm1686
source scripts/envsetup.sh ${CHIP_ARCH}
rebuild_firmware
# 会生成kernel_module在build/firmware_core/libfirmware_core.so
# 可将该文件复制到目标机器上, 如/home/linaro目录

# 在目标机器上
export BMRUNTIME_USING_FIRMWARE="/home/linaro/libfirmware_core.so"
# 然后调用bmrt_test等命令即可使用外部的kernel_module运行了
```

## 回归测试
`scripts/envsetup.sh`提供了runtime的脚本函数
```shell
cmodel_run_bmodel xxx.bmodel
cmodel_run_bmodel bmodel_dir
cmodel_batch_run_bmodel dir_include_bmodels

# 进入gdb调试模式
export EXTRA_EXEC="gdb --args"
cmodel_run_bmodel xxx.bmodel
```

目前线上回归的入口脚本为`test/regression.sh`，后续如要加bmodel测试可在里面修改

## 其他
### BMRT_LOG控制：
目前BMRT_LOG分为-1: DEBUG, 0: INFO, 1: WARNING, 2: WRONG, 3: FATAL几个等级;
可以通过BMRT_LOG_VERION环境变量来控制，如`export BMRT_LOG_VERSION=-1`, 表示所有大于等于-1等级的日志都会打印;
默认是0, 只打印INFO,WARNING, WRONG, FATAL信息。
