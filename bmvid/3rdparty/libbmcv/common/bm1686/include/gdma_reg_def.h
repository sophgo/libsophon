#ifndef __GDMA_REG_DEF_H__
#define __GDMA_REG_DEF_H__

// gdma reg id defines
#define GDMA_ID_SCATTER_DMA_ENABLE          { 0, 1 }
#define GDMA_ID_SYNC_ID_RESET               { 2, 1 }
#define GDMA_ID_INTR_DISABLE                { 3, 1 }
#define GDMA_ID_AGGR_READ_LMEM_ENABLE       { 4, 1 }
#define GDMA_ID_PARALLEL_ENABLE             { 5, 1 }
#define GDMA_ID_MEM_SIZE_PER_NPU            { 6, 1 }
#define GDMA_ID_EU_NUM_PER_NPU              { 7, 1 }
#define GDMA_ID_NPU_NUM                     { 8, 2 }
#define GDMA_ID_AGGR_WRITE_ENABLE           { 10, 1 }
#define GDMA_ID_AGGR_READ_DDR_ENABLE        { 11, 1 }
#define GDMA_ID_INTR_DES_DISABLE            { 32, 1 }
#define GDMA_ID_INTR_INVALID_DES_DISABLE    { 33, 1 }
#define GDMA_ID_INTR_CMD_OVERFLOW_DISABLE   { 34, 1 }
#define GDMA_ID_INTR_SRC_RD_ERR_DISABLE     { 35, 1 }
#define GDMA_ID_INTR_DST_RD_ERR_DIABLE      { 36, 1 }
#define GDMA_ID_INTR_DES_RD_ERR_DISABLE     { 37, 1 }
#define GDMA_ID_INTR_DES_WR_ERR_DISABLE     { 38, 1 }
#define GDMA_ID_DES_INTR_STATUS             { 40, 1 }
#define GDMA_ID_DES_INVALID_ERR             { 41, 1 }
#define GDMA_ID_CMD_OVERFLOW_ERR            { 42, 1 }
#define GDMA_ID_SRC_RD_ERR                  { 43, 1 }
#define GDMA_ID_DST_RD_ERR                  { 44, 1 }
#define GDMA_ID_DES_RD_ERR                  { 45, 1 }
#define GDMA_ID_DST_WR_ERR                  { 46, 1 }
#define GDMA_ID_FIFO_STATUS                 { 48, 8 }
#define GDMA_ID_INTR_SYNC_ID                { 64, 16 }
#define GDMA_ID_CURRENT_SYNC_ID             { 80, 16 }
#define GDMA_ID_DES_ADDR                    { 96, 32 }
#define GDMA_ID_PIO_GDMA_ENABLE             { 128 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_DES_TYPE                    { 129 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_CHAIN_END                   { 130 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_INTR_EN                     { 131 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_BARRIER_ENABLE              { 132 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_STRIDE_ENABLE               { 133 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_DIRECTION                   { 134 - GDMA_DES_OFFSET, 2 }
#define GDMA_ID_ACC_WRITE_ENABLE            { 136 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_COMMON_MODE                 { 137 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_PREFETCH_DISABLE            { 138 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_HOLD_DES_VALUE              { 139 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_CMD_ID                      { 144 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_SPECIAL_FUNC                { 160 - GDMA_DES_OFFSET, 3 }
#define GDMA_ID_DST_DATA_FORMAT             { 163 - GDMA_DES_OFFSET, 3 }
#define GDMA_ID_CHW_COPY                    { 166 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_SYS_MEM_TYPE                { 167 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_SRC_DATA_FORMAT             { 168 - GDMA_DES_OFFSET, 3 }
#define GDMA_ID_LRN_SHIFT_NUM               { 171 - GDMA_DES_OFFSET, 4 }
#define GDMA_ID_LRN_SHIFT_DIR               { 175 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_ENG0_SYNC_ID                { 176 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_ENG1_SYNC_ID                { 192 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_ENG3_SYNC_ID                { 208 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_CONSTANT_VALUE              { 224 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SRC_NSTRIDE                 { 256 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SRC_CSTRIDE                 { 288 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SRC_HSTRIDE                 { 320 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SRC_WSTRIDE                 { 352 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_DST_NSTRIDE                 { 384 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_DST_CSTRIDE                 { 416 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_DST_HSTRIDE                 { 448 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_DST_WSTRIDE                 { 480 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SRC_NSIZE                   { 512 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_SRC_CSIZE                   { 528 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_SRC_HSIZE                   { 544 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_SRC_WSIZE                   { 560 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_DST_NSIZE                   { 576 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_DST_CSIZE                   { 592 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_DST_HSIZE                   { 608 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_DST_WSIZE                   { 624 - GDMA_DES_OFFSET, 16 }
#define GDMA_ID_SRC_START_ADDR_L32          { 640 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_DST_START_ADDR_L32          { 672 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SRC_START_ADDR_H8           { 704 - GDMA_DES_OFFSET, 8 }
#define GDMA_ID_DST_START_ADDR_H8           { 712 - GDMA_DES_OFFSET, 8 }
#define GDMA_ID_SRC_HSHIFT                  { 736 - GDMA_DES_OFFSET, 8 }
#define GDMA_ID_SRC_WSHIFT                  { 744 - GDMA_DES_OFFSET, 8 }
#define GDMA_ID_DST_HSHIFT                  { 752 - GDMA_DES_OFFSET, 8 }
#define GDMA_ID_DST_WSHIFT                  { 760 - GDMA_DES_OFFSET, 8 }
#define GDMA_ID_LOCALMEM_MASK_L32           { 768 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_LOCALMEM_MASK_H32           { 800 - GDMA_DES_OFFSET, 32 }
#define GDMA_ID_SINGLE_STEP                 { 832 - GDMA_DES_OFFSET, 1 }
#define GDMA_ID_DEBUG_MODE                  { 833 - GDMA_DES_OFFSET, 1 }

// gdma pack defines
#define GDMA_PACK_SCATTER_DMA_ENABLE(val)        { GDMA_ID_SCATTER_DMA_ENABLE, (val) }
#define GDMA_PACK_SYNC_ID_RESET(val)             { GDMA_ID_SYNC_ID_RESET, (val) }
#define GDMA_PACK_INTR_DISABLE(val)              { GDMA_ID_INTR_DISABLE, (val) }
#define GDMA_PACK_AGGR_READ_LMEM_ENABLE(val)     { GDMA_ID_AGGR_READ_LMEM_ENABLE, (val) }
#define GDMA_PACK_PARALLEL_ENABLE(val)           { GDMA_ID_PARALLEL_ENABLE, (val) }
#define GDMA_PACK_MEM_SIZE_PER_NPU(val)          { GDMA_ID_MEM_SIZE_PER_NPU, (val) }
#define GDMA_PACK_EU_NUM_PER_NPU(val)            { GDMA_ID_EU_NUM_PER_NPU, (val) }
#define GDMA_PACK_NPU_NUM(val)                   { GDMA_ID_NPU_NUM, (val) }
#define GDMA_PACK_AGGR_WRITE_ENABLE(val)         { GDMA_ID_AGGR_WRITE_ENABLE, (val) }
#define GDMA_PACK_AGGR_READ_DDR_ENABLE(val)      { GDMA_ID_AGGR_READ_DDR_ENABLE, (val) }
#define GDMA_PACK_INTR_DES_DISABLE(val)          { GDMA_ID_INTR_DES_DISABLE, (val) }
#define GDMA_PACK_INTR_INVALID_DES_DISABLE(val)  { GDMA_ID_INTR_INVALID_DES_DISABLE, (val) }
#define GDMA_PACK_INTR_CMD_OVERFLOW_DISABLE(val) { GDMA_ID_INTR_CMD_OVERFLOW_DISABLE, (val) }
#define GDMA_PACK_INTR_SRC_RD_ERR_DISABLE(val)   { GDMA_ID_INTR_SRC_RD_ERR_DISABLE, (val) }
#define GDMA_PACK_INTR_DST_RD_ERR_DIABLE(val)    { GDMA_ID_INTR_DST_RD_ERR_DIABLE, (val) }
#define GDMA_PACK_INTR_DES_RD_ERR_DISABLE(val)   { GDMA_ID_INTR_DES_RD_ERR_DISABLE, (val) }
#define GDMA_PACK_INTR_DES_WR_ERR_DISABLE(val)   { GDMA_ID_INTR_DES_WR_ERR_DISABLE, (val) }
#define GDMA_PACK_DES_INTR_STATUS(val)           { GDMA_ID_DES_INTR_STATUS, (val) }
#define GDMA_PACK_DES_INVALID_ERR(val)           { GDMA_ID_DES_INVALID_ERR, (val) }
#define GDMA_PACK_CMD_OVERFLOW_ERR(val)          { GDMA_ID_CMD_OVERFLOW_ERR, (val) }
#define GDMA_PACK_SRC_RD_ERR(val)                { GDMA_ID_SRC_RD_ERR, (val) }
#define GDMA_PACK_DST_RD_ERR(val)                { GDMA_ID_DST_RD_ERR, (val) }
#define GDMA_PACK_DES_RD_ERR(val)                { GDMA_ID_DES_RD_ERR, (val) }
#define GDMA_PACK_DST_WR_ERR(val)                { GDMA_ID_DST_WR_ERR, (val) }
#define GDMA_PACK_FIFO_STATUS(val)               { GDMA_ID_FIFO_STATUS, (val) }
#define GDMA_PACK_INTR_SYNC_ID(val)              { GDMA_ID_INTR_SYNC_ID, (val) }
#define GDMA_PACK_CURRENT_SYNC_ID(val)           { GDMA_ID_CURRENT_SYNC_ID, (val) }
#define GDMA_PACK_DES_ADDR(val)                  { GDMA_ID_DES_ADDR, (val) }
#define GDMA_PACK_PIO_GDMA_ENABLE(val)           { GDMA_ID_PIO_GDMA_ENABLE, (val) }
#define GDMA_PACK_DES_TYPE(val)                  { GDMA_ID_DES_TYPE, (val) }
#define GDMA_PACK_CHAIN_END(val)                 { GDMA_ID_CHAIN_END, (val) }
#define GDMA_PACK_INTR_EN(val)                   { GDMA_ID_INTR_EN, (val) }
#define GDMA_PACK_BARRIER_ENABLE(val)            { GDMA_ID_BARRIER_ENABLE, (val) }
#define GDMA_PACK_STRIDE_ENABLE(val)             { GDMA_ID_STRIDE_ENABLE, (val) }
#define GDMA_PACK_DIRECTION(val)                 { GDMA_ID_DIRECTION, (val) }
#define GDMA_PACK_ACC_WRITE_ENABLE(val)          { GDMA_ID_ACC_WRITE_ENABLE, (val) }
#define GDMA_PACK_COMMON_MODE(val)               { GDMA_ID_COMMON_MODE, (val) }
#define GDMA_PACK_PREFETCH_DISABLE(val)          { GDMA_ID_PREFETCH_DISABLE, (val) }
#define GDMA_PACK_HOLD_DES_VALUE(val)            { GDMA_ID_HOLD_DES_VALUE, (val) }
#define GDMA_PACK_CMD_ID(val)                    { GDMA_ID_CMD_ID, (val) }
#define GDMA_PACK_SPECIAL_FUNC(val)              { GDMA_ID_SPECIAL_FUNC, (val) }
#define GDMA_PACK_DST_DATA_FORMAT(val)           { GDMA_ID_DST_DATA_FORMAT, (val) }
#define GDMA_PACK_CHW_COPY(val)                  { GDMA_ID_CHW_COPY, (val) }
#define GDMA_PACK_SYS_MEM_TYPE(val)              { GDMA_ID_SYS_MEM_TYPE, (val) }
#define GDMA_PACK_SRC_DATA_FORMAT(val)           { GDMA_ID_SRC_DATA_FORMAT, (val) }
#define GDMA_PACK_LRN_SHIFT_NUM(val)             { GDMA_ID_LRN_SHIFT_NUM, (val) }
#define GDMA_PACK_LRN_SHIFT_DIR(val)             { GDMA_ID_LRN_SHIFT_DIR, (val) }
#define GDMA_PACK_ENG0_SYNC_ID(val)              { GDMA_ID_ENG0_SYNC_ID, (val) }
#define GDMA_PACK_ENG1_SYNC_ID(val)              { GDMA_ID_ENG1_SYNC_ID, (val) }
#define GDMA_PACK_ENG3_SYNC_ID(val)              { GDMA_ID_ENG3_SYNC_ID, (val) }
#define GDMA_PACK_CONSTANT_VALUE(val)            { GDMA_ID_CONSTANT_VALUE, (val) }
#define GDMA_PACK_SRC_NSTRIDE(val)               { GDMA_ID_SRC_NSTRIDE, (val) }
#define GDMA_PACK_SRC_CSTRIDE(val)               { GDMA_ID_SRC_CSTRIDE, (val) }
#define GDMA_PACK_SRC_HSTRIDE(val)               { GDMA_ID_SRC_HSTRIDE, (val) }
#define GDMA_PACK_SRC_WSTRIDE(val)               { GDMA_ID_SRC_WSTRIDE, (val) }
#define GDMA_PACK_DST_NSTRIDE(val)               { GDMA_ID_DST_NSTRIDE, (val) }
#define GDMA_PACK_DST_CSTRIDE(val)               { GDMA_ID_DST_CSTRIDE, (val) }
#define GDMA_PACK_DST_HSTRIDE(val)               { GDMA_ID_DST_HSTRIDE, (val) }
#define GDMA_PACK_DST_WSTRIDE(val)               { GDMA_ID_DST_WSTRIDE, (val) }
#define GDMA_PACK_SRC_NSIZE(val)                 { GDMA_ID_SRC_NSIZE, (val) }
#define GDMA_PACK_SRC_CSIZE(val)                 { GDMA_ID_SRC_CSIZE, (val) }
#define GDMA_PACK_SRC_HSIZE(val)                 { GDMA_ID_SRC_HSIZE, (val) }
#define GDMA_PACK_SRC_WSIZE(val)                 { GDMA_ID_SRC_WSIZE, (val) }
#define GDMA_PACK_DST_NSIZE(val)                 { GDMA_ID_DST_NSIZE, (val) }
#define GDMA_PACK_DST_CSIZE(val)                 { GDMA_ID_DST_CSIZE, (val) }
#define GDMA_PACK_DST_HSIZE(val)                 { GDMA_ID_DST_HSIZE, (val) }
#define GDMA_PACK_DST_WSIZE(val)                 { GDMA_ID_DST_WSIZE, (val) }
#define GDMA_PACK_SRC_START_ADDR_L32(val)        { GDMA_ID_SRC_START_ADDR_L32, (val) }
#define GDMA_PACK_DST_START_ADDR_L32(val)        { GDMA_ID_DST_START_ADDR_L32, (val) }
#define GDMA_PACK_SRC_START_ADDR_H8(val)         { GDMA_ID_SRC_START_ADDR_H8, (val) }
#define GDMA_PACK_DST_START_ADDR_H8(val)         { GDMA_ID_DST_START_ADDR_H8, (val) }
#define GDMA_PACK_SRC_HSHIFT(val)                { GDMA_ID_SRC_HSHIFT, (val) }
#define GDMA_PACK_SRC_WSHIFT(val)                { GDMA_ID_SRC_WSHIFT, (val) }
#define GDMA_PACK_DST_HSHIFT(val)                { GDMA_ID_DST_HSHIFT, (val) }
#define GDMA_PACK_DST_WSHIFT(val)                { GDMA_ID_DST_WSHIFT, (val) }
#define GDMA_PACK_LOCALMEM_MASK_L32(val)         { GDMA_ID_LOCALMEM_MASK_L32, (val) }
#define GDMA_PACK_LOCALMEM_MASK_H32(val)         { GDMA_ID_LOCALMEM_MASK_H32, (val) }
#define GDMA_PACK_SINGLE_STEP(val)               { GDMA_ID_SINGLE_STEP, (val) }
#define GDMA_PACK_DEBUG_MODE(val)                { GDMA_ID_DEBUG_MODE, (val) }

// gdma default values
#define GDMA_PACK_AGGR_READ_LMEM_ENABLE_DEFAULT     GDMA_PACK_AGGR_READ_LMEM_ENABLE(0x1)
#define GDMA_PACK_PARALLEL_ENABLE_DEFAULT           GDMA_PACK_PARALLEL_ENABLE(0x1)
#define GDMA_PACK_EU_NUM_PER_NPU_DEFAULT            GDMA_PACK_EU_NUM_PER_NPU(0x1)
#define GDMA_PACK_INTR_INVALID_DES_DISABLE_DEFAULT  GDMA_PACK_INTR_INVALID_DES_DISABLE(0x1)
#define GDMA_PACK_PREFETCH_DISABLE_DEFAULT          GDMA_PACK_PREFETCH_DISABLE(0x1)
#define GDMA_PACK_CHW_COPY_DEFAULT                  GDMA_PACK_CHW_COPY(0x1)
#define GDMA_PACK_LRN_SHIFT_NUM_DEFAULT             GDMA_PACK_LRN_SHIFT_NUM(0x1)
#define GDMA_PACK_SRC_WSTRIDE_DEFAULT               GDMA_PACK_SRC_WSTRIDE(0x1)
#define GDMA_PACK_LOCALMEM_MASK_L32_DEFAULT         GDMA_PACK_LOCALMEM_MASK_L32(0xFFFFFFFF)
#define GDMA_PACK_LOCALMEM_MASK_H32_DEFAULT         GDMA_PACK_LOCALMEM_MASK_H32(0xFFFFFFFF)

// default list
#define GDMA_PACK_DEFAULTS { \
  GDMA_PACK_PREFETCH_DISABLE_DEFAULT, \
  GDMA_PACK_CHW_COPY_DEFAULT, \
  GDMA_PACK_LRN_SHIFT_NUM_DEFAULT, \
  GDMA_PACK_SRC_WSTRIDE_DEFAULT, \
  GDMA_PACK_LOCALMEM_MASK_L32_DEFAULT, \
  GDMA_PACK_LOCALMEM_MASK_H32_DEFAULT \
}

#endif  // __GDMA_REG_DEF_H__
