#ifndef _GEN_CMD_COMMON_
#define _GEN_CMD_COMMON_

#include "common.h"

/* FIXME: dependency loop. */
#include "engine_internal.h"

/*
 * legacy code goes here, and within here
 */

/*
 * CAUTION & FIXME: following MACROs are dangrous
 *   as it assumes BASE to be a pointer to u32
 *   add offset are in unit of dword rather than byte
 */
#define WRITE_DESC_REG(BASE, IDX, DATA)        \
  fw_write32((FW_ADDR_T)((u32 *)(BASE) + (IDX)), (DATA))
#define WRITE_BD_DES_REG(BASE, IDX, DATA)      WRITE_DESC_REG(BASE, IDX, DATA)
#define WRITE_CDMA_DES_REG(BASE, IDX, DATA)    WRITE_DESC_REG(BASE, IDX, DATA)
#define WRITE_GDMA_DES_REG(BASE, IDX, DATA)    WRITE_DESC_REG(BASE, IDX, DATA)

/*
 * cmd_id related
 */
#define GDMA_CMD_ID_PROC_CORE                       \
  unsigned int cmd_id0, cmd_id2, cmd_id3;           \
  if (pid_node != NULL) {                           \
    pid_node->gdma_cmd_id++;                        \
    cmd_id0 = pid_node->bd_cmd_id;                  \
    cmd_id2 = pid_node->gdma_cmd_id;                \
    cmd_id3 = pid_node->cdma_cmd_id;                \
  } else {                                          \
    cmd_id0 = 0;                                    \
    cmd_id2 = 0;                                    \
    cmd_id3 = 0;                                    \
  }

#define CDMA_CMD_ID_PROC_CORE                       \
  unsigned int cmd_id0, cmd_id2, cmd_id3;           \
  if (pid_node != NULL) {                           \
    pid_node->cdma_cmd_id++;                        \
    cmd_id0 = pid_node->bd_cmd_id;                  \
    cmd_id2 = pid_node->gdma_cmd_id;                \
    cmd_id3 = pid_node->cdma_cmd_id;                \
  } else {                                          \
    cmd_id0 = 0;                                    \
    cmd_id2 = 0;                                    \
    cmd_id3 = 0;                                    \
  }

#define BD_CMD_ID_PROC_CORE                         \
  pid_node->bd_cmd_id++;                            \
  unsigned int bd_cmd_id = pid_node->bd_cmd_id;     \
  unsigned int gdma_cmd_id = pid_node->gdma_cmd_id; \
  unsigned int cdma_cmd_id = pid_node->cdma_cmd_id;

#define GDMA_CMD_ID_PROC GDMA_CMD_ID_PROC_CORE
#define CDMA_CMD_ID_PROC CDMA_CMD_ID_PROC_CORE
#define BD_CMD_ID_PROC BD_CMD_ID_PROC_CORE

static inline void cmd_id_divide(CMD_ID_NODE * p_cmd_src,
    CMD_ID_NODE * p_cmd_dst0, CMD_ID_NODE * p_cmd_dst1)
{
  p_cmd_dst0->bd_cmd_id = p_cmd_src->bd_cmd_id;
  p_cmd_dst0->gdma_cmd_id = p_cmd_src->gdma_cmd_id;
  p_cmd_dst0->cdma_cmd_id = p_cmd_src->cdma_cmd_id;

  p_cmd_dst1->bd_cmd_id = p_cmd_src->bd_cmd_id;
  p_cmd_dst1->gdma_cmd_id = p_cmd_src->gdma_cmd_id;
  p_cmd_dst1->cdma_cmd_id = p_cmd_src->cdma_cmd_id;
}

static inline void cmd_id_merge(CMD_ID_NODE *p_cmd_dst,
    CMD_ID_NODE *p_cmd_src0, CMD_ID_NODE *p_cmd_src1)
{
  p_cmd_dst->bd_cmd_id = math_max(p_cmd_src0->bd_cmd_id, p_cmd_src1->bd_cmd_id);
  p_cmd_dst->gdma_cmd_id = math_max(p_cmd_src0->gdma_cmd_id, p_cmd_src1->gdma_cmd_id);
  p_cmd_dst->cdma_cmd_id = math_max(p_cmd_src0->cdma_cmd_id, p_cmd_src1->cdma_cmd_id);
}

#endif /* _GEN_CMD_COMMON_ */
