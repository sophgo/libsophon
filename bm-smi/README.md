# BM-SMI (Burning Matrix System Management Interface) 用户手册

## 项目概述

**BM-SMI**（Burning Matrix System Management Interface）是专为 Bitmain SoC 设备设计的系统管理工具，用于监控和管理 AI 加速卡的运行状态。该工具提供了类似 NVIDIA-SMI 的功能，可以实时监控 TPU 硬件的各种属性和性能指标。

## 功能特性

- **实时硬件监控**：显示 TPU 芯片温度、板卡温度、功耗等关键指标
- **时钟频率管理**：监控当前、最小和最大时钟频率
- **内存使用情况**：显示设备内存使用和总量
- **进程内存监控**：查看各进程占用的 TPU 内存
- **交互式界面**：基于 ncurses 的终端用户界面
- **数据导出**：支持将监控数据保存到文件

## 安装要求

- 支持 Bitmain SoC 平台
- 需要 libsophon 库
- ncurses 库支持
- gflags 库支持

## 编译和安装

```bash
# 编译项目
make

# 或使用 cmake
mkdir build && cd build
cmake ..
make
```

## 使用方法

### 启动
```bash 
# 启动：  
export SHOW_TPU_USAGE=1   

# 配置计算周期
默认5000 us  
export SET_TPU_WINDOWS=10000 //us
```

### 基本命令

```bash
# 默认显示模式
bm-smi

# 指定操作模式（目前主要用于 SoC 模式）
bm-smi --opmode=display

# 保存输出到文件
bm-smi --file=/path/to/output.txt

# 设置采样间隔（毫秒）
bm-smi --lms=1000

# 单次采样模式（非循环）
bm-smi --noloop
```

### 手动运行
```bash
# 将下列文件拷贝到sd卡中；
a. bm_smi; //存放在libsophon/install/libsophon-${version}/bin中
b. 3rd lib; //比如musl_arm环境3rd库存放在libsophon/3rdparty/arm/soc/lib
c. terminfo; //路径 libsophon/3rdparty

# 挂载SD卡
emmc环境: 
mount -t vfat /dev/mmcblk0p1 /mnt/sd/

# 配置环境变量
export TERMINFO=/mnt/sd/terminfo
export TERMINFO_DIRS=/mnt/sd/terminfo
export TERM=xterm-color
export LD_LIBRARY_PATH=/mnt/sd/lib/:$LD_LIBRARY_PATH

# 运行程序
./bm_smi
```


### 文件介绍
/tmp/bmcpu_app_usage  //tpu 1s平均使用率   
/tmp/tpu_usage.log  //tpu在计算周期中平均使用率   

### 交互式控制

在交互式界面中，您可以使用以下按键：

- **↑/↓ 方向键**：上下滚动
- **Page Up/Page Down**：翻页
- **Tab**：刷新窗口
- **任意其他键**：退出

## 输出信息说明

### 硬件信息
- **Card**：卡索引
- **Name**：板卡名称
- **Mode**：工作模式（PCIE/SOC）
- **SN**：序列号
- **TPU**：TPU ID

### 温度和功耗
- **boardT**：板卡温度
- **chipT**：芯片温度
- **boardP**：板卡功耗
- **TPU_P**：TPU 功耗
- **TPU_V**：TPU 电压
- **12V_ATX**：12V ATX 电流

### 时钟和性能
- **Minclk/Maxclk**：最小/最大时钟频率
- **Currclk**：当前时钟频率
- **TPU_C**：TPU 电流
- **Tpu-Util**：TPU 利用率

### 内存信息
- **Memory-Usage**：内存使用情况（已用/总量 MB）
- **Bus-ID**：设备总线 ID
- **Status**：设备状态（Active/Fault）

### 进程内存信息
- **TPU-ID**：TPU 设备 ID
- **PID**：进程 ID
- **Process name**：进程名称
- **Usage**：内存使用量

## 输出示例

```
Fri Jan  2 00:17:57 1970
+--------------------------------------------------------------------------------------------------+
| SDK Version:    0.4.9             Driver Version:  0.4.9                                         |
+---------------------------------------+----------------------------------------------------------+
|card  Name      Mode        SN         |TPU  boardT  chipT    TPU_P   TPU_V    CorrectN   Tpu-Util|
|12V_ATX  MaxP boardP Minclk Maxclk  Fan|Bus-ID      Status   Currclk   TPU_C   Memory-Usage       |
|=======================================+==========================================================|
|  0  MARS3-SOC   SOC      N/A          | 0     N/A      N/A     N/A       N/A      N/A        11% |
|   N/A   N/A   N/A  375M    1200M   N/A| N/A        Active    500M       N/A     4MB/   75MB      |
+=======================================+==========================================================+

+--------------------------------------------------------------------------------------------------+
| Processes:                                                                            TPU Memory |
|  TPU-ID       PID   Process name                                                      Usage      |

```

## 状态代码说明

- **F**：故障值（Fault Value）
- **N/A**：不支持或不可用
- 数值：正常读数

## 故障排除

1. **无法打开设备**：确认 `/dev/bmdev-ctl` 设备节点存在
2. **权限错误**：确保有足够的权限访问设备文件
3. **无数据显示**：检查驱动是否正确安装

## 开发架构

BM-SMI 采用模块化设计：

- **bm_smi_cmdline**：命令行参数解析
- **bm_smi_display**：显示功能实现
- **bm_smi_test**：测试基类
- **bm_smi_creator**：对象创建器

## 版本信息

- **当前版本**：0.4.9
- **构建日期**：20260106-174052

## 许可证

请参考项目根目录下的许可证文件。