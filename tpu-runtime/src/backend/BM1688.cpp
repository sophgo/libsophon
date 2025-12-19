#include "backend.hpp"
#include "bmruntime.h"

namespace bmruntime {

bool Backend_BM1688::convert_gdma(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);

  uint64_t offset = 0;
  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; cmd_idx++) {
    uint32_t cmd_size =
        gdma_cmd_size(src_ptr, offset, cmd_idx == params.num_cmd - 1);
    memcpy(dst_ptr, src_ptr, cmd_size);
    int cmd_type = (dst_ptr[1] & 0x0f);
    if (cmd_type != 0x6 && cmd_idx != params.num_cmd - 1 &&
        params.stage->io_size > 0) {
      u64 src_addr = ((u64)(dst_ptr[17] & 0xff) << 32) | ((u64)dst_ptr[16]);
      u64 dst_addr = ((u64)(dst_ptr[19] & 0xff) << 32) | ((u64)dst_ptr[18]);
      u32 const_fill = dst_ptr[1] & 0x80;
      bool src_in_global =
          ((src_addr >> 39) & 0x1) && !(cmd_type == 0 && const_fill);
      bool dst_in_global = (dst_addr >> 39) & 0x1;
      u64 fix_addr;
      if (src_in_global && ((src_addr >> 36) & 0x7) == 0) {
        fix_addr = update_gdma_cmd_addr(params.stage,
                                        src_addr & ((1ull << 35) - 1), true);
        fix_addr |= (1ull << 39);
        if (fix_addr != src_addr) {
          dst_ptr[16] = fix_addr & 0xffffffff;
          dst_ptr[17] =
              ((u32)((fix_addr >> 32) & 0xff)) | (dst_ptr[17] & 0xffffff00);
        }
      }
      if (dst_in_global && ((dst_addr >> 36) & 0x7) == 0) {
        fix_addr = update_gdma_cmd_addr(params.stage,
                                        dst_addr & ((1ull << 35) - 1), false);
        fix_addr |= (1ull << 39);
        if (fix_addr != dst_addr) {
          dst_ptr[18] = fix_addr & 0xffffffff;
          dst_ptr[19] =
              ((u32)((fix_addr >> 32) & 0xff)) | (dst_ptr[19] & 0xffffff00);
        }
      }
      // cmd type: 0:DMA_tensor, 1:DMA_matrix, 2:DMA_masked_select,
      // 3:DMA_general 4:DMA_cw_trans, 5:DMA_nonzero, 6:DMA_sys, 7:DMA_gather,
      // 8:DMA_scatter 9:DMA_reverse 10:DMA_compress 11: DMA_decompress fix
      // index_tensor or mask_tensor addr
      if (cmd_type == 2 || cmd_type == 7 || cmd_type == 8 || cmd_type == 0xa ||
          cmd_type == 0xb) {
        u64 index_addr = ((u64)(dst_ptr[21] & 0xff) << 32) | ((u64)dst_ptr[20]);
        if (((index_addr >> 39) & 0x1) && ((index_addr >> 36) & 0x7) == 0) {
          fix_addr = update_gdma_cmd_addr(
              params.stage, index_addr & ((1ull << 35) - 1), true);
          fix_addr |= (1ull << 39);
          if (fix_addr != index_addr) {
            dst_ptr[20] = fix_addr & 0xffffffff;
            dst_ptr[21] =
                ((u32)((fix_addr >> 32) & 0xff)) | (dst_ptr[21] & 0xffffff00);
          }
        }
      }
    }

    offset += cmd_size;
    dst_ptr += (cmd_size >> 2);
    src_ptr += (cmd_size >> 2);
  }
  return true;
}
} // namespace bmruntime