#ifndef MEMMAP_H
#define MEMMAP_H

#ifdef _WIN32
#include "bm_config_win.h"
#else
#include "bm_config.h"
#endif
// =============================================
// The following is allocation for l2 sram
// For lookup table
#define SFU_TABLE_SIZE              256
#define INT2FLOAT_TABLE_SIZE        256
#define UINT2FLOAT_TABLE_SIZE       INT2FLOAT_TABLE_SIZE
#define SFU_TAILOR_TABLE_SIZE       32
#define SFU_TAILOR_L_TABLE_SIZE     64
#define UNIFIED_LOOKUP_TABLE_SIZE   256
#define SERIAL_NUMBER_SIZE          2048

#define WARP_MAX_W                  (256)
#define WARP_MAX_H                  (256)
#define WARP_TABLE_SIZE             (WARP_MAX_W * WARP_MAX_H * 2)
#define EX_INT_TABLE_OFFSET         0x300000
#define EX_TABLE_OFFSET             (EX_INT_TABLE_OFFSET + SFU_TABLE_SIZE * sizeof(float))
#define LNX_TABLE_OFFSET            (EX_TABLE_OFFSET + SFU_TABLE_SIZE * sizeof(float))
#define EX_TAILOR_OFFSET            (LNX_TABLE_OFFSET + SFU_TABLE_SIZE * sizeof(float))
#define LNX_TAILOR_OFFSET           (EX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define ARCSINX_TAILOR_OFFSET       (LNX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define UINT2FLOAT_TABLE_OFFSET     (ARCSINX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define INT2FLOAT_TABLE_OFFSET      (UINT2FLOAT_TABLE_OFFSET + UINT2FLOAT_TABLE_SIZE * sizeof(float))
#define SINX_TAILOR_OFFSET          (INT2FLOAT_TABLE_OFFSET + INT2FLOAT_TABLE_SIZE * sizeof(float))
#define SERIAL_NUMBER_OFFSET        (SINX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define WARP_TABLE_OFFSET           (SERIAL_NUMBER_OFFSET + SERIAL_NUMBER_SIZE * sizeof(float))
#define UNIFIED_LOOKUP_TABLE_OFFSET (WARP_TABLE_OFFSET + WARP_TABLE_SIZE)
#define L2_STATIC_END_OFFSET        (UNIFIED_LOOKUP_TABLE_OFFSET + UNIFIED_LOOKUP_TABLE_SIZE * sizeof(float))

// For dynamic IR info
#define DYNAMIC_IR_OFFSET           0x22000
#define DYNAMIC_IR_MAX_SIZE         0x80000  // 512KB
// Start offset for L2 sram alloc
#define L2_ALLOC_START_OFFSET       (DYNAMIC_IR_OFFSET + DYNAMIC_IR_MAX_SIZE)
#define L2_SRAM_USER_START_ADDR     (L2_SRAM_START_ADDR | L2_ALLOC_START_OFFSET)
#define L2_SRAM_USER_SIZE           (EX_INT_TABLE_OFFSET - L2_ALLOC_START_OFFSET)
// ============================================

#define ITCM_MEM_START_ADDR            0x00000000
#define ITCM_MEM_SIZE                  (1<<19)           // itcm size 512K
#define DTCM_MEM_START_ADDR            0x02000000
#define DTCM_MEM_SIZE                  (1<<19)           // dtcm size 512K

// DTCM layout:
// ======[0,     111KB+768byte)===== reserved for normal software.
// ======[111KB+768byte, 111KB+896byte)===== for mcu information.
// ======[111KB+896byte, 112KB)===== for device information.
// ======[112KB, 128KB)===== share memory(message queue for host and device)
// ======[128KB, 512KB)===== for bmcv.
#define DTCM_MEM_RESERVED_SIZE         (111*1024+640)     // 111KB + 640 byte reserved for software.
#define DTCM_MEM_VETH_INFO_SIZE        (128)              // 128 byte for virtual ethernet information.
#define DTCM_MEM_MCU_INFO_SIZE         (128)              // 128 byte for mcu information.
#define DTCM_MEM_DEV_INFO_SIZE         (128)              // 128 byte for device information.
#define DTCM_MEM_SHARE_MEM_SIZE        (16*1024)          // 16KB for share memory.
#define DTCM_MEM_BMCV_SIZE             (381*1024)         // 381KB for bmcv.
#define DTCM_MEM_COMM_INFO_SIZE        (128)              // 128 byte for dahua communication
#define DTCM_MEM_MSG_INFO_SIZE         (2*1024+896)       // 2KB+896 for dahua msg

#define DTCM_MEM_RESERVED_OFFSET       (0)
#define DTCM_MEM_VETH_INFO_OFFSET      (DTCM_MEM_RESERVED_OFFSET + DTCM_MEM_RESERVED_SIZE)
#define DTCM_MEM_MCU_INFO_OFFSET       (DTCM_MEM_VETH_INFO_OFFSET + DTCM_MEM_VETH_INFO_SIZE)
#define DTCM_MEM_DEV_INFO_OFFSET       (DTCM_MEM_MCU_INFO_OFFSET + DTCM_MEM_MCU_INFO_SIZE)
#define DTCM_MEM_SHARE_OFFSET          (DTCM_MEM_DEV_INFO_OFFSET + DTCM_MEM_DEV_INFO_SIZE)
#define DTCM_MEM_BMCV_OFFSET           (DTCM_MEM_SHARE_OFFSET + DTCM_MEM_SHARE_MEM_SIZE)
#define DTCM_MEM_COMM_OFFSET           (DTCM_MEM_BMCV_OFFSET + DTCM_MEM_BMCV_SIZE)
#define DTCM_MEM_MSG_OFFSET            (DTCM_MEM_COMM_OFFSET + DTCM_MEM_COMM_INFO_SIZE)
#define DTCM_MEM_STATIC_END_OFFSET     (DTCM_MEM_MSG_OFFSET + DTCM_MEM_MSG_INFO_SIZE)

#define DTCM_MEM_RESERVED_START_ADDR   (DTCM_MEM_START_ADDR + DTCM_MEM_RESERVED_OFFSET)
#define SHARE_MEM_START_ADDR           (DTCM_MEM_START_ADDR + DTCM_MEM_SHARE_OFFSET)

#define LOCAL_MEM_START_ADDR           0x08000000
#define LOCAL_MEM_ADDRWIDTH            CONFIG_LOCAL_MEM_ADDRWIDTH
#define L2_SRAM_START_ADDR             0x10000000

#define COUNT_RESERVED_DDR_SWAP        0x1000000
#define COUNT_RESERVED_DDR_INSTR       0x1000000
#define COUNT_RESERVED_DDR_IMAGE_SCALE 0x2000000

#define API_MESSAGE_EMPTY_SLOT_NUM 2

#define GLOBAL_MEM_START_ADDR_ARM      0x80000000
#define GLOBAL_MEM_CMD_START_OFFSET    0x0
#define GLOBAL_MEM_START_ADDR          0x100000000

#define SPI_BASE_ADDR                  0x06000000
#define TOP_REG_CTRL_BASE_ADDR         (0x50010000)
#define SHARE_REG_BASE_ADDR            (TOP_REG_CTRL_BASE_ADDR + 0x80)

#define I2C0_REG_CTRL_BASE_ADDR        0x5001a000
#define I2C1_REG_CTRL_BASE_ADDR        0x5001c000
#define I2C2_REG_CTRL_BASE_ADDR        0x5001e000

#define NV_TIMER_CTRL_BASE_ADDR        0x50010180
#define OS_TIMER_CTRL_BASE_ADDR        0x50022000
#define INT0_CTRL_BASE_ADDR            0x50023000
#define EFUSE_BASE                     0x50028000
#define GPIO_CTRL_BASE_ADDR            0x50027000
#define PCIE_BUS_SCAN_ADDR             0x60000000
#define PWM_CTRL_BASE_ADDR             0x50029000
#define DDR_CTRL_BASE_ADDR             0x68000000
#define GPIO0_CTRL_BASE_ADDR           0x50027000

#define UART_CTRL_BASE_ADDR            0x50118000
// There're 8 device lock register, the lower 4 for PCIe and upper 4 for SoC.
#define TOP_REG_DEVICE_LOCK0           0x50010040
#define TOP_REG_DEVICE_LOCK1           0x50010044
#define TOP_REG_DEVICE_LOCK2           0x50010048
#define TOP_REG_DEVICE_LOCK3           0x5001004c
#define TOP_GP_REG_ARM9_IRQ_STATUS     0x500100b8
#define TOP_GP_REG_ARM9_IRQ_SET        0x50010190
#define TOP_GP_REG_ARM9_IRQ_CLR        0x50010194
#define TOP_GP_REG_A53_IRQ_STATUS      0x500100bc
#define TOP_GP_REG_A53_IRQ_SET         0x50010198
#define TOP_GP_REG_A53_IRQ_CLR         0x5001019c
#define TOP_REG_DEVICE_LOCK_CDMA       TOP_REG_DEVICE_LOCK0
#define TOP_REG_DEVICE_LOCK_CLK        TOP_REG_DEVICE_LOCK1
#define TOP_REG_CLK_ENABLE0            0x50010800
#define TOP_REG_CLK_ENABLE1            0x50010804

#define GDMA_ENGINE_BASE_ADDR_AHB      0x58000000
#define BD_ENGINE_BASE_ADDR_AHB        0x58001000
#define MMU_ENGINE_BASE_ADDR           0x58002000
#define CDMA_ENGINE_BASE_ADDR          0x58003000
#define GDE_BASE_ADDR                  0x58005000

#define GDMA_ENGINE_BASE_ADDR_PIO      0x58000000
#define GDMA_ENGINE_BASE_ADDR_DTCM     0x02080000

#ifdef FAST_REG_ACCESS
  #define GDMA_ENGINE_BASE_ADDR          0x2080000
  #define BD_ENGINE_BASE_ADDR            0x2081000
#else
  #define GDMA_ENGINE_BASE_ADDR          0x58000000
  #define BD_ENGINE_BASE_ADDR            0x58001000
#endif

#define BD_ENGINE_COMMAND_NUM          (32)
#define BD_ENGINE_MAIN_CTRL            (BD_ENGINE_BASE_ADDR + 0x00000100)
#define BD_ENGINE_MAIN_CTRL_AHB        (BD_ENGINE_BASE_ADDR_AHB + 0x00000100)
#define BDC_CMD_BASE_ADDR              (BD_ENGINE_BASE_ADDR + 0x00000000)
#define BDC_CMD_BASE_ADDR_AHB          (BD_ENGINE_BASE_ADDR_AHB + 0x00000000) // for arm read
#define BDC_INT_CLR                    (1)

#define GDMA_REG_COUNT (23)
#define GDMA_DES_OFFSET (128)
#define GDMA_ENGINE_MAIN_CTRL            (GDMA_ENGINE_BASE_ADDR + 0x0)
#define GDMA_ENGINE_MAIN_CTRL_AHB        (GDMA_ENGINE_BASE_ADDR_AHB + 0x0)
#define GDMA_CMD_BASE_ADDR               (GDMA_ENGINE_BASE_ADDR + (GDMA_DES_OFFSET/32)*4)
#define GDMA_CMD_BASE_ADDR_AHB           (GDMA_ENGINE_BASE_ADDR_AHB + (GDMA_DES_OFFSET/32)*4) // for arm read
#define GDMA_ENGINE_COMMAND_NUM  GDMA_REG_COUNT

#define GDE_REG_COUNT (10)
#define GDE_CMD_BASE_ADDR               (GDE_BASE_ADDR)
#define GDE_ENGINE_COMMAND_NUM  GDE_REG_COUNT

#define CDMA_REG_COUNT (128)
#define CDMA_CMD_BASE_ADDR       (CDMA_ENGINE_BASE_ADDR + 0x800)
#define CDMA_ENGIN_COMMAND_NUM  CDMA_REG_COUNT

#endif  /* MEMMAP_H */
