#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef USE_NPU_LIB
#include "memmap.h"
#endif

#define ENABLE_PRINT

//#define NO_PLL_INIT
//#define NO_DDR_INIT

#define CONFIG_HEAP_SIZE                (12288)

#define CONFIG_TIMER_FREQ               (50000000)

#define CONFIG_NPU_CTRL_REG0            (0x00)
#define CONFIG_NPU_CTRL_REG1            (0x04)
#define CONFIG_NPU_RED_REG0             (0x08)
#define CONFIG_NPU_RED_REG1             (0x0c)
#define CONFIG_NPU_RED_REG2             (0x10)
#define CONFIG_NPU_RED_REG3             (0x14)
#define CONFIG_NPU_MON_BUF              (0x80) //0x80~0x100
#define CONFIG_LOCAL_MEM_CTRL_SIZE      (0x400)

#define CONFIG_NPU_CTRL_REG_BASE        (0x43ff0000)
#define CONFIG_LOCALMEM_CLOUMN          (8)
#define CONFIG_LOCALMEM_ROW_WIDTH       (0x80)

#define CONFIG_DRAM_SIZE                0x80000000
#define CONFIG_DRAM_TOTAL_BANK          2
#define REG_BM1680_TOP_DDR_ADDR_MODE    0x1C0

#ifdef BOARD_ASIC
#define CONFIG_SYS_CLOCK          25000000
#define CONFIG_MAIN_PLL_CLK       (225 * 1000 * 1000)       //(300 * 1000 * 1000)
#define CONFIG_TIMER_CLK_FAST     (CONFIG_MAIN_PLL_CLK / 5)
#define CONFIG_TIMER_CLK_SLOW     (100 * 1000)
#elif defined(BOARD_FPGA) || defined(BOARD_FPGA_MUL)
#define CONFIG_SYS_CLOCK          10000000
#define CONFIG_TIMER_CLK_FAST     10000000
#define CONFIG_TIMER_CLK_SLOW     10000000
#elif defined(BOARD_PALLADIUM)
#define CONFIG_SYS_CLOCK          10000000  // ??
#define CONFIG_TIMER_CLK_FAST     10000000  // ??
#define CONFIG_TIMER_CLK_SLOW     10000000  // ??
#else
#error "Undefined BOARD"
#endif

#endif
