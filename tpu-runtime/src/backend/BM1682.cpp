#include "backend.hpp"
#include "bmruntime.h"

#include <utility>

namespace bmruntime {

bool Backend_BM1682::convert_bdc(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);
  for (auto cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, bdc_cmd_info.size);

    if (cmd_idx == params.num_cmd - 1)
      dst_ptr[0] |= (1 << 7); // cmd[0] |= (1 << BD_EOD_BIT);

    dst_ptr[19] += (uint32_t)(params.cmd_addr >> bdc_cmd_info.bits);
    // shift the commands to the tail parts of buffer
    // Using ugly code to fix the strange order of bdc
    for (uint32_t pos = 32 - 1; pos >= (32 - bdc_cmd_info.cnt); pos--)
      dst_ptr[pos] = dst_ptr[pos - 32 + bdc_cmd_info.cnt];

    for (uint32_t pos = 0; pos < 32 - bdc_cmd_info.cnt; pos++)
      dst_ptr[pos] = 0;

    for (uint32_t pos = 8; pos < 32; pos += 2)
      std::swap(dst_ptr[pos], dst_ptr[pos + 1]);

    // update pointer
    dst_ptr += (bdc_cmd_info.size >> 2);
    src_ptr += (bdc_cmd_info.size >> 2);
  }
  return true;
}

bool Backend_BM1682::convert_gdma(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);
  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, gdma_cmd_info.size);

    if (cmd_idx == params.num_cmd - 1)
      dst_ptr[0] |= (1 << 2); // cmd[0] |= (1 << GDMA_ACCPI0_EOD_BIT);

    // if (append_mem_offset != 0 || bmrt_arch_info::is_soc_mode()) {
    uint32_t gdma_direction = (dst_ptr[0] >> 6) & 0x3;
    uint64_t origin_addr, fix_addr;
    if (GDMA_DIR_S2L == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      // if origin_addr is below ctx_start, needn't append
      origin_addr = dst_ptr[12] + (((uint64_t)(dst_ptr[13] >> 24)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, true);
      if (fix_addr != origin_addr) {
        dst_ptr[12] = fix_addr & 0xffffffff;
        dst_ptr[13] = ((uint32_t)(((fix_addr >> 32) & 0xff) << 24)) |
                      (dst_ptr[13] & 0x00ffffff);
      }
    }

    if (GDMA_DIR_L2S == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      origin_addr =
          dst_ptr[11] + (((uint64_t)((dst_ptr[13] >> 16) & 0xff)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, false);
      if (fix_addr != origin_addr) {
        dst_ptr[11] = fix_addr & 0xffffffff;
        dst_ptr[13] = ((uint64_t)(((fix_addr >> 32) & 0xff) << 16)) |
                      (dst_ptr[13] & 0xff00ffff);
      }
    }
    dst_ptr[gdma_cmd_info.cnt - 1] += (params.cmd_addr >> gdma_cmd_info.bits);

    // update pointer
    dst_ptr += (gdma_cmd_info.cnt >> 2);
    src_ptr += (gdma_cmd_info.cnt >> 2);
  }
  return true;
}
} // namespace bmruntime