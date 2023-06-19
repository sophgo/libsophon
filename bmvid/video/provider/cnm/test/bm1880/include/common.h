#ifndef _COMMON_H_
#define _COMMON_H_

#ifdef USE_NPU_LIB
#include "bm_types.h"
#include "memmap.h"
#else
#include "bmtest_type.h"
#endif
#include "debug.h"

typedef u32 ENGINE_ID;
#define ENGINE_BD 0
#define ENGINE_ARM 1
#define ENGINE_GDMA 2
#define ENGINE_CDMA 3
#define ENGINE_HDMA 4
#define ENGINE_END 5

typedef struct _cmd_id_node_s {
  unsigned int bd_cmd_id;
  unsigned int gdma_cmd_id;
  unsigned int cdma_cmd_id;
} CMD_ID_NODE;

typedef void * P_TASK_DESCRIPTOR;

#define RAW_READ32(x)           (*(volatile u32 *)(x))
#define RAW_WRITE32(x, val)     (*(volatile u32 *)(x) = (u32)(val))

#define RAW_REG_READ32(base, off)          RAW_READ32(((u8 *)(base)) + (off))
#define RAW_REG_WRITE32(base, off, val)    RAW_WRITE32(((u8 *)(base)) + (off), (val))
#define RAW_MEM_READ32(base, off)          RAW_READ32(((u8 *)(base)) + (off))
#define RAW_MEM_WRITE32(base, off, val)    RAW_WRITE32(((u8 *)(base)) + (off), (val)

#define FW_ADDR_T               unsigned long

static inline int calc_offset(int *shape, int *offset)
{
  return ((offset[0] * shape[1] + offset[1]) * shape[2] + offset[2])
      * shape[3] + offset[3];
}

#define NPU_NUM                                 32
#define EU_NUM                                  16
#define MAX_NPU_NUM                             32
#define MAX_EU_NUM                              16
#define REG_BM1682_TOP_SOFT_RST                 0x014

//defined in bmkernel/../arch/../memmap.h
//#define LOCAL_MEM_ADDRWIDTH                     16
//#define LOCAL_MEM_SIZE                          (1 << 16)

#endif /* _COMMON_H_ */
