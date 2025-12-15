#include "backend.hpp"
#include "bmruntime.h"

namespace bmruntime {

bool Backend_BM1684::convert_bdc(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);

  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, bdc_cmd_info.size);

    if (cmd_idx == params.num_cmd - 1) {
      dst_ptr[0] |= (1 << 1);
    }

    /* special request: exchange cmd[i] and cmd[31-i] */
    for (uint32_t i = 0; i < bdc_cmd_info.cnt / 2; ++i) {
      uint32_t tmp = dst_ptr[i];
      dst_ptr[i] = dst_ptr[bdc_cmd_info.cnt - 1 - i];
      dst_ptr[bdc_cmd_info.cnt - 1 - i] = tmp;
    }

    dst_ptr += (bdc_cmd_info.size >> 2);
    src_ptr += (bdc_cmd_info.size >> 2);
  }
  return true;
}

bool Backend_BM1684::convert_gdma(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);
  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, gdma_cmd_info.size);
    if (cmd_idx == params.num_cmd - 1)
      dst_ptr[0] |= (1 << 2);

    uint64_t gdma_direction = (dst_ptr[0] >> 6) & 0x3;
    uint64_t origin_addr = 0, fix_addr = 0;
    if (GDMA_DIR_S2L == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      origin_addr =
          (uint64_t)dst_ptr[16] + (((uint64_t)(dst_ptr[18] & 0xff)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, true);
      if (fix_addr != origin_addr) {
        dst_ptr[16] = fix_addr & 0xffffffff;
        dst_ptr[18] =
            ((uint64_t)((fix_addr >> 32) & 0xff)) | (dst_ptr[18] & 0xffffff00);
      }
    }
    if (GDMA_DIR_L2S == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      origin_addr = (uint64_t)dst_ptr[17] +
                    (((uint64_t)((dst_ptr[18] >> 8) & 0xff)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, false);
      if (fix_addr != origin_addr) {
        dst_ptr[17] = fix_addr & 0xffffffff;
        dst_ptr[18] = ((uint64_t)(((fix_addr >> 32) << 8) & 0xff00)) |
                      (dst_ptr[18] & 0xffff00ff);
      }
    }

    dst_ptr += (gdma_cmd_info.size >> 2);
    src_ptr += (gdma_cmd_info.size >> 2);
  }
  return true;
}
} // namespace bmruntime