#include "backend/backend.hpp"
#include "bmruntime.h"

namespace bmruntime {
uint64_t Backend::update_gdma_cmd_addr(const net_stage_t *stage,
                                       uint64_t origin_addr,
                                       bool is_src) const {
  if (origin_addr < ctx_start_addr()) {
    return origin_addr + gmem_offset();
  }

  bool io_alone = stage->io_size > 0;
  auto coeff_limit = io_alone ? stage->io_start : stage->ctx_start;
  if (origin_addr < coeff_limit) {
    if (false == is_src) {
      BMRT_LOG(FATAL,
               "gdma dst shouldn't be coeff, origin[0x%llx], ctx[0x%llx]",
               origin_addr, coeff_limit);
    }
    return origin_addr + stage->coeff_offset;
  }
  if (io_alone && origin_addr < stage->ctx_start) {
    return origin_addr + stage->io_offset;
  }
  int mem_index =
      get_mem_index(stage->ctx_borders, stage->ctx_start, origin_addr);
  BMRT_ASSERT_INFO(
      mem_index < stage->ctx_offset.size(),
      " addr 0x%llx is overflow, valid range is [0x%llx, 0x%llx)\n",
      origin_addr, stage->ctx_start,
      stage->ctx_start + stage->ctx_borders.back());
  return origin_addr + stage->ctx_offset[mem_index];
}

uint32_t Backend::gdma_cmd_size(const uint32_t *cmd, uint64_t offset,
                                bool last_cmd) const {
  uint32_t cmd_size = 96;
  uint32_t cmd_type = cmd[1] & 0xf;
  if (cmd_type == 0x6) {
    // DMA_sys
    cmd_size = 16;
  }
  if (last_cmd) {
    cmd_size = ALIGN(offset + cmd_size, 128) - offset;
  }
  return cmd_size;
}
} // namespace bmruntime