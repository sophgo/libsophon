#ifndef MACRO_H
#define MACRO_H

#ifdef USING_CMODEL
  #define GET_GLOBAL_ADDR(ADDR) \
    ((u8 *)get_global_memaddr(get_cur_nodechip_idx()) + ADDR-GLOBAL_MEM_START_ADDR)
  #define GET_LOCAL_ADDR(LOCALMEM_IDX, LOCALMEM_OFFSET) \
    ((u8 *)get_local_memaddr_by_node(get_cur_nodechip_idx(), LOCALMEM_IDX) + LOCALMEM_OFFSET)
  #define GET_DTCM_ADDR(HEAP_ADDR) \
    ((u8 *)get_dtcm_memaddr_by_node(get_cur_nodechip_idx()) + HEAP_ADDR- DTCM_MEM_START_ADDR)
  #define GET_L2_SRAM_ADDR(ADDR) \
    ((u8 *)get_l2_sram(get_cur_nodechip_idx()) + ADDR - L2_SRAM_START_ADDR)
#else
  #define GET_GLOBAL_ADDR(ADDR) \
    ((u8 *)GLOBAL_MEM_START_ADDR_ARM + (ADDR) - GLOBAL_MEM_START_ADDR)
  #define GET_LOCAL_ADDR(LOCALMEM_IDX, LOCALMEM_OFFSET) \
    ((u8 *)LOCAL_MEM_START_ADDR + LOCALMEM_IDX* LOCAL_MEM_SIZE + LOCALMEM_OFFSET)
  #define GET_DTCM_ADDR(HEAP_ADDR) \
    ((u8 *)((int)HEAP_ADDR))
  #define GET_L2_SRAM_ADDR(ADDR) \
    ((u8 *)((int)ADDR))
#endif

#endif
