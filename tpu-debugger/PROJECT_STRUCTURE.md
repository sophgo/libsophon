# TPU Debugger 项目结构

## 编译
- 当前支持芯片，也就是下述的 your.chip：bm1684x, bm1688, cv186x, cv184x(mars3).
- cv184x支持多种不同的编译器，通过 TOOLCHAIN 来设定：musl_arm, musl_arm64, glibc_arm, glibc_arm64.
  ```bash
  source scripts/envsetup.sh your.chip, 例如 source scripts/envsetup.sh bm1688
  cmake -S . -B build -DTOOLCHAIN=glibc_arm64 [-DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=XXX]
  cmake --build build -j
  ```
- 可以查看 scripts/envsetup.sh 中的 `rebuild` 函数来确认编译命令.

## Release
- 使用 scripts/release.sh 来完成功能的 release，该脚本接收2个参数，分别表示芯片名称和使用的编译器，芯片名称包括 bm1684x, bm1688, cv184x(mars3)，编译器主要针对 cv184x，可以有 musl_arm, musl_arm64, glibc_arm, glibc_arm64 这几种选择.例如：
  * release cv184x musl_arm64：source scripts/release.sh cv184x musl_arm64
  * release bm1688: source scripts/release.sh cv184x
- release 后的文件存储在 tpu-debugger/install/下，主要是一个 bin 文件：bin/tpu-debugger.

## 项目概述

用于 ASIC TPU 硬件调试的工具，支持多芯片、多核心、多引擎的寄存器读取和异常检测。
**寄存器定义直接内联在代码中**，编译时固定，无需运行时配置。

## 核心特性

1. **多芯片支持**: BM1688, CV186X, CV184X, BM1684X 等
2. **多核支持**: 每个芯片可有多个核心
3. **多引擎支持**: TIU_CMD, TIU_CTRL, DMA_CMD, DMA_CTRL
4. **异常检测**: 自动检测寄存器异常值
5. **编译时配置**: 寄存器定义直接写在代码中
6. **SoC模式**: 使用 /dev/mem 直接读取寄存器

## 文件结构

```
tpu-debugger/
├── app/
│   └── tpu-debugger.cpp          # 主程序入口
├── include/
│   ├── register.hpp              # 寄存器定义和字段提取
│   ├── engine.hpp                # 引擎类（TIU/DMA）
│   ├── chip.hpp                  # 芯片基类和注册表
│   ├── readers.hpp               # 寄存器读取器（SoC/PCIe）
│   ├── debugger.hpp              # 调试器主类
│   ├── mmap.hpp                  # 内存映射工具
│   ├── utils.hpp                 # 工具函数（binaryToHexString等）
│   ├── json_dump.hpp             # JSON导出功能
│   ├── json_dumper_base.hpp      # JSON导出器基类
│   ├── json_reader.hpp           # JSON文件读取器
│   ├── reg_value.h               # 寄存器值枚举定义
│   ├── magic_enum/               # magic_enum库
│   │   └── magic_enum.hpp
│   ├── nlohmann/                 # JSON库
│   │   ├── json.hpp
│   │   └── json_fwd.hpp
│   └── chips/
│       ├── bm1688/
│       │   ├── bm1688.hpp            # BM1688 寄存器定义（内联）
│       │   └── bm1688_json_dumper.hpp # BM1688 JSON导出器
│       ├── bm1684x/
│       │   ├── bm1684x.hpp           # BM1684X 寄存器定义（内联）
│       │   └── bm1684x_json_dumper.hpp # BM1684X JSON导出器
│       └── cv184x/
│           └── cv184x.hpp            # CV184X 寄存器定义（内联）
├── src/
│   ├── register.cpp              # 寄存器解析实现
│   ├── engine.cpp                # 引擎实现
│   ├── chip.cpp                  # 芯片管理实现
│   ├── readers.cpp               # 读取器实现
│   ├── debugger.cpp              # 调试器实现
│   ├── chip_registry.cpp         # 芯片注册
│   ├── utils.cpp                 # 工具函数实现
│   ├── json_dump.cpp             # JSON导出实现
│   ├── json_dumper_base.cpp      # JSON导出器基类实现
│   ├── json_reader.cpp           # JSON文件读取器实现
│   └── chips/
│       ├── bm1688/bm1688_json_dumper.cpp  # BM1688 JSON导出器实现
│       └── bm1684x/bm1684x_json_dumper.cpp # BM1684X JSON导出器实现
└── CMakeLists.txt                # 构建配置
```

## 架构设计

### 1. 寄存器定义 (RegDescriptor)

- 支持 1024bit 寄存器
- 字段配置：名称、起始bit、宽度、描述
- 支持设置期望值和有效值（用于异常检测）

```cpp
RegDescriptor tiu_ctrl("TIU_CTRL", base_addr);
tiu_ctrl.addField({"cfg-en", 0, 1, "Configuration enable"});
tiu_ctrl.addField({"cmd-id", 2, 24, "Command ID"});
tiu_ctrl.setFieldExpectedValue("cfg-en", 1);  // 期望值为1
tiu_ctrl.setFieldExpectedValue("cfg_cmd_illegal", 0);  // 期望无错误
```

### 2. 引擎 (Engine)

- 每个核心有 4 个引擎：TIU_CMD, TIU_CTRL, DMA_CMD, DMA_CTRL
- 每个引擎包含多个寄存器定义
- 独立读取和异常检测

### 3. 芯片配置 (IChipConfig)

- 纯接口类，定义芯片信息和方法
- 寄存器定义直接内联在 `getXXXRegisters()` 方法中
- 编译时确定，运行无开销

```cpp
class BM1688Config : public IChipConfig {
public:
    ChipInfo getChipInfo() const override { ... }

    std::vector<RegDescriptor> getTIUCtrlRegisters(uint32_t core_id) const override {
        std::vector<RegDescriptor> regs;
        RegDescriptor tiu_ctrl("TIU_CTRL", base_addr);
        tiu_ctrl.addField({...});
        tiu_ctrl.setFieldExpectedValue(...);
        regs.push_back(tiu_ctrl);
        return regs;
    }
    // ...
};
```

### 4. 芯片注册

使用宏自动注册芯片：

```cpp
REGISTER_CHIP(BM1688Chip)
```

## 使用方法

### 基本用法

```bash
# 列出支持的芯片
./tpu-debugger --list-chips

# 运行调试（默认SoC模式，只显示异常）
./tpu-debugger

# 显示所有寄存器
./tpu-debugger --all

# 只检查指定核心
./tpu-debugger --core 0 --core 1

# 只检查指定引擎
./tpu-debugger --engine tiu_ctrl

# 使用PCIe模式
./tpu-debugger --pcie --device 0
```

## 添加/修改寄存器定义

直接编辑芯片头文件，例如 `include/chips/bm1688.hpp`：

```cpp
std::vector<RegDescriptor> createTIUCtrlRegs(uint32_t core_id) const {
    std::vector<RegDescriptor> regs;

    uint64_t base = 0x26000100ull + core_id * 0x10000ull;
    RegDescriptor tiu_ctrl("TIU_CTRL", base);

    // 添加字段：名称, 起始bit, 宽度, 描述
    tiu_ctrl.addField({"cfg-en", 0, 1, "Configuration enable"});
    tiu_ctrl.addField({"cmd-id", 2, 24, "Command ID"});
    tiu_ctrl.addField({"my_new_field", 100, 8, "My new field"});  // 新增字段

    // 设置期望值（可选，用于异常检测）
    tiu_ctrl.setFieldExpectedValue("cfg-en", 1);
    tiu_ctrl.setFieldExpectedValue("cfg_cmd_illegal", 0);

    regs.push_back(tiu_ctrl);
    return regs;
}
```

然后重新编译即可。

## 扩展新芯片

1. 在 `include/chips/` 创建头文件
2. 继承 `IChipConfig` 接口，内联实现寄存器定义
3. 在 `src/chip_registry.cpp` 注册芯片

示例：

```cpp
// include/chips/new_chip.hpp
#pragma once
#include "chip.hpp"

namespace tpu_debugger {

class NewChipConfig : public IChipConfig {
public:
    ChipInfo getChipInfo() const override {
        return {0x1234, "NewChip", 1, 0x26000000ull, ...};
    }

    std::vector<RegDescriptor> getTIUCmdRegisters(uint32_t core_id) const override {
        // 内联寄存器定义
    }
    // ... 其他方法
};

class NewChip : public Chip {
public:
    explicit NewChip(std::shared_ptr<IRegisterReader> reader)
        : Chip(NewChipConfig().getChipInfo(), reader) {
        NewChipConfig config;
        initialize(config);
    }
};

} // namespace tpu_debugger
```

```cpp
// src/chip_registry.cpp
#include "chips/new_chip.hpp"

REGISTER_CHIP(NewChip)
```

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 注意事项

1. SoC模式需要 root 权限访问 /dev/mem
2. 寄存器地址需要根据实际硬件手册调整
3. 1024bit 寄存器读取需要 32 个 32bit 读取操作
4. 修改寄存器定义后需要重新编译

