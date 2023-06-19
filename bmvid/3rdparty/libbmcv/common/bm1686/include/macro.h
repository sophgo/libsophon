#ifndef MACRO_H
#define MACRO_H

#ifdef USING_CMODEL
  #define GET_GLOBAL_ADDR(ADDR) \
    ((u8 *)get_gmem_addr(get_cur_nodechip_idx()) + ADDR-GLOBAL_MEM_START_ADDR)
  #define GET_LOCAL_ADDR(LOCALMEM_IDX, LOCALMEM_OFFSET) \
    ((u8 *)get_npu_lmem_addr(get_cur_nodechip_idx(), LOCALMEM_IDX) + LOCALMEM_OFFSET)
  #define GET_DTCM_ADDR(HEAP_ADDR) \
    ((u8 *)get_dtcm_addr(get_cur_nodechip_idx()) + HEAP_ADDR- DTCM_MEM_START_ADDR)
  #define GET_L2_SRAM_ADDR(ADDR) \
    ((u8 *)get_l2_sram(get_cur_nodechip_idx()) + ADDR - L2_SRAM_START_ADDR)
#else
  #define GET_GLOBAL_ADDR(ADDR) \
    ((u8 *)GLOBAL_MEM_START_ADDR_ARM + (ADDR) - GLOBAL_MEM_START_ADDR)
  #define GET_LOCAL_ADDR(LOCALMEM_IDX, LOCALMEM_OFFSET) \
    ((u8 *)LOCAL_MEM_START_ADDR + LOCALMEM_IDX* LOCAL_MEM_SIZE + LOCALMEM_OFFSET)
  #define GET_DTCM_ADDR(HEAP_ADDR) \
    ((u8 *)(HEAP_ADDR))
  #define GET_L2_SRAM_ADDR(ADDR) \
    ((u8 *)(ADDR))
#endif

#endif
