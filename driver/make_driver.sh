#!/bin/bash

# =============================================
# Sophon 驱动编译脚本 - 环境变量配置版
# =============================================

set -e  # 遇到错误立即退出

# =============================================
# 颜色定义用于输出
# =============================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================
# 日志函数
# =============================================
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }

# =============================================
# 核心环境变量配置
# =============================================

# 编译器配置[5,10](@ref)
#export CC=aarch64-linux-gnu-gcc
#export CXX=aarch64-linux-gnu-g++
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
# 内核相关配置
export KERNELRELEASE="5.10.4-d1a9e9ece-sophon-custom"
export SOC_MODE="0"
export PLATFORM="pcie_arm64"
export SYNC_API_INT_MODE="1"
export TARGET_PROJECT="sg_pcie_device"
export FW_SIMPLE="0"
export PCIE_MODE_ENABLE_CPU="1"

# 路径配置[1,9](@ref)
export LINUX_SRC="/media/cvitek/ke.yi/mycode/sophon/dailybuild/gerrit_a2_release_code/linux_5.10/build/edge_wevb_emmc"
export DRIVER_PATH=$PWD

# 编译配置
export JOB_NUM=6
#export CFLAGS="-Wall -g -O2"
#export LDFLAGS="-Wl,-rpath,/usr/local/lib"

# =============================================
# 工具链路径检查与配置[9](@ref)
# =============================================
configure_toolchain() {
    log_info "配置工具链路径..."
    
    # 检查GCC是否存在[1](@ref)
    if ! command -v gcc &> /dev/null; then
        log_error "GCC编译器未找到，请先安装GCC"
        log_info "可以尝试: sudo apt install gcc g++"
        return 1
    else
        GCC_PATH=$(which gcc)
        log_info "GCC路径: $GCC_PATH"
        export CC="$GCC_PATH"
    fi
    
    # 添加自定义工具链路径（如果有）
    local custom_paths=(
        "/opt/toolchain/bin"
        "/usr/local/arm-gcc/bin"
        "$HOME/arm-gcc/bin"
    )
    
    for path in "${custom_paths[@]}"; do
        if [ -d "$path" ] && [[ ":$PATH:" != *":$path:"* ]]; then
            export PATH="$PATH:$path"
            log_info "已添加工具链路径: $path"
        fi
    done
}

# =============================================
# 路径验证函数[9](@ref)
# =============================================
validate_paths() {
    log_info "验证环境路径..."
    
    local missing_paths=0
    
    # 检查Linux源码路径[1](@ref)
    if [ ! -d "$LINUX_SRC" ]; then
        log_error "Linux源码路径不存在: $LINUX_SRC"
        missing_paths=$((missing_paths + 1))
    else
        log_info "Linux源码路径验证通过"
    fi
    
    # 检查驱动源码路径
    if [ ! -d "$DRIVER_PATH" ]; then
        log_error "驱动源码路径不存在: $DRIVER_PATH"
        missing_paths=$((missing_paths + 1))
    else
        log_info "驱动源码路径验证通过"
    fi
    
    # 检查关键目录是否存在
    local critical_dirs=(
        "$LINUX_SRC/include"
        "$LINUX_SRC/arch/arm64"
    )
    
    for dir in "${critical_dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            log_warning "可能缺少关键目录: $dir"
        fi
    done
    
    return $missing_paths
}

# =============================================
# 显示当前环境配置
# =============================================
show_environment() {
    log_info "=== 当前环境配置 ==="
    echo "编译器信息:"
    gcc --version 2>/dev/null || echo "GCC未找到"
    echo ""
    
    echo "环境变量:"
    echo "KERNELRELEASE: $KERNELRELEASE"
    echo "PLATFORM: $PLATFORM"
    echo "SOC_MODE: $SOC_MODE"
    echo "LINUX_SRC: $LINUX_SRC"
    echo "DRIVER_PATH: $DRIVER_PATH"
    echo "PATH: $PATH"
    echo "=============================="
}

# =============================================
# 编译执行函数
# =============================================
compile_driver() {
    log_info "开始编译驱动..."
    
    # 进入驱动目录
    cd "$DRIVER_PATH" || {
        log_error "无法进入驱动目录: $DRIVER_PATH"
        return 1
    }
    
    log_info "当前目录: $(pwd)"
    log_info "执行编译命令..."
    
    # 执行make命令[3,4](@ref)
    make -j${JOB_NUM} \
        KERNELRELEASE="${KERNELRELEASE}" \
        SOC_MODE="${SOC_MODE}" \
        PLATFORM="${PLATFORM}" \
        SYNC_API_INT_MODE="${SYNC_API_INT_MODE}" \
        TARGET_PROJECT="${TARGET_PROJECT}" \
        FW_SIMPLE="${FW_SIMPLE}" \
        PCIE_MODE_ENABLE_CPU="${PCIE_MODE_ENABLE_CPU}" \
        LINUX_SRC="${LINUX_SRC}" \
        -C "${LINUX_SRC}" \
        M="$(pwd)"   
    local result=$?
    if [ $result -eq 0 ]; then
        log_success "驱动编译成功!"
        return 0
    else
        log_error "驱动编译失败，退出码: $result"
        return $result
    fi
}

# =============================================
# 环境备份函数（可选）
# =============================================
backup_environment() {
    log_info "备份当前环境变量..."
    
    # 创建备份文件
    local backup_file="/tmp/sophon_env_backup_$(date +%Y%m%d_%H%M%S).txt"
    
    {
        echo "=== Sophon编译环境备份 ==="
        echo "备份时间: $(date)"
        echo "KERNELRELEASE=$KERNELRELEASE"
        echo "PLATFORM=$PLATFORM"
        echo "SOC_MODE=$SOC_MODE"
        echo "LINUX_SRC=$LINUX_SRC"
        echo "DRIVER_PATH=$DRIVER_PATH"
        echo "PATH=$PATH"
    } > "$backup_file"
    
    log_info "环境已备份至: $backup_file"
}

# =============================================
# 主函数
# =============================================
main() {
    local action="${1:-build}"
    
    log_info "Sophon驱动编译脚本启动"
    
    # 显示环境信息
    show_environment
    
    # 配置工具链
    if ! configure_toolchain; then
        log_error "工具链配置失败"
        exit 1
    fi
    
    # 验证路径
    if ! validate_paths; then
        log_error "路径验证失败，请检查环境变量设置"
        exit 1
    fi
    
    # 备份环境（可选）
    backup_environment
    
    case "$action" in
        "build")
            compile_driver
            ;;
        "clean")
            cd "$DRIVER_PATH" && make clean
            ;;
        "env")
            # 仅显示环境信息，不编译
            show_environment
            ;;
        *)
            log_error "未知操作: $action"
            echo "用法: $0 [build|clean|env]"
            exit 1
            ;;
    esac
}

# =============================================
# 脚本执行入口
# =============================================
if [ $# -gt 0 ]; then
    main "$1"
else
    main "build"
fi
