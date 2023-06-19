#include "bmrt_arch_info.h"

namespace bmruntime {

bmrt_arch_info* bmrt_arch_info::sta_bmtpu_ptr;

bmrt_arch_info::bmrt_arch_info(const string& arch_name)
{
    sta_bmtpu_ptr = this;
#ifdef SOC_MODE
    m_soc_mode = true;
#else
    m_soc_mode = false;
#endif
    m_arch_name = arch_name;
    if (arch_name == "BM1682") {
      target_bmtpu_arch = BM1682;
    } else if (arch_name == "BM1684") {
      target_bmtpu_arch = BM1684;
    } else if (arch_name == "BM1682_PCIE" || arch_name == "BM1682_SOC") {
      target_bmtpu_arch = BM1682; // for compatible
      m_arch_name = "BM1682";
    } else if (arch_name == "BM1684_PCIE" || arch_name == "BM1684_SOC") {
      target_bmtpu_arch = BM1684; // for compatible
      m_arch_name = "BM1684";
    } else if (arch_name == "BM1880") {
      target_bmtpu_arch = BM1880;
    } else if (arch_name == "BM1684X") {
      target_bmtpu_arch = BM1684X;
    } else if (arch_name == "BM1686") {
      target_bmtpu_arch = BM1686;
    } else {
      BMRT_LOG(FATAL, "Error: unknown processor name [%s]",  arch_name.c_str());
    }
}

bmrt_arch_info::~bmrt_arch_info() {
}

int bmrt_arch_info::get_npu_num()
{
  int npu_num = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
    case BM1684:
    case BM1684X:
      npu_num = 64;
      break;
    case BM1880:
    case BM1686:
      npu_num = 32;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return npu_num;
}

int bmrt_arch_info::get_eu_num(bm_data_type_t dtype)
{
  int eu_num = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
    case BM1684:
      eu_num = 32;
      break;
    case BM1880:
    case BM1684X:
      eu_num = 16;
      break;
    case BM1686:
      eu_num = 4;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return eu_num *= (4 / bmrt_data_type_size(dtype));
}

int bmrt_arch_info::get_lmem_size()
{
  int lmem_size = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
    case BM1684X:
      lmem_size = (1<<18);  //256KB
      break;
    case BM1684:
      lmem_size = (1<<19);  //512KB
      break;
    case BM1686:
      lmem_size = (1<<17);  //128KB
    case BM1880:
      lmem_size = (1<<16);  //64KB
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return lmem_size;
}

/* FIX VALUE : 0x100000000 as pcie mode */
u64 bmrt_arch_info::get_gmem_start()
{
  u64 gmem_start = 0;
  BMRT_ASSERT_INFO(sta_bmtpu_ptr != NULL,"sta_bmtpu_ptr shouldn't be NULL\n");
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      gmem_start = 0x100000000;
      break;
    case BM1684:
    case BM1880:
    case BM1684X:
    case BM1686:
      gmem_start = 0x100000000;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return gmem_start;
}

u64 bmrt_arch_info::get_gmem_start_soc()
{
  u64 gmem_start = 0;
  BMRT_ASSERT_INFO(sta_bmtpu_ptr != NULL,"sta_bmtpu_ptr shouldn't be NULL\n");
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      gmem_start = 0x200000000;
      break;
    case BM1684:
    case BM1880:
      gmem_start = 0x100000000;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return gmem_start;
}

u64 bmrt_arch_info::get_gmem_offset_soc()
{
  u64 gmem_offset = 0;
  BMRT_ASSERT_INFO(sta_bmtpu_ptr != NULL,"sta_bmtpu_ptr shouldn't be NULL\n");
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      gmem_offset = 0x100000000;
      break;
    case BM1684:
    case BM1880:
    case BM1684X:
    case BM1686:
      gmem_offset = 0x0;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return gmem_offset;
}

int bmrt_arch_info::get_lmem_banks()
{
  int lmem_banks = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
    case BM1684:
    case BM1880:
      lmem_banks = 8;
      break;
    case BM1684X:
    case BM1686:
      lmem_banks = 16;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return lmem_banks;
}

int bmrt_arch_info::get_lmem_bank_size()
{
  int lmem_bank_size = get_lmem_size() / get_lmem_banks();
  return lmem_bank_size;
}

u64 bmrt_arch_info::get_gmem_cmd_start_offset()
{
  u64 gmem_start = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
    case BM1684:
    case BM1880:
    case BM1684X:
    case BM1686:
      gmem_start = 0x0;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return gmem_start;
}

/* reserve device memory for arm/iommu, and start to arrange tensor address space after that
 * Corespoding to GLOBAL_MEM_START_ADDR + EFECTIVE_GMEM_START in driver memory layout
 */
u64 bmrt_arch_info::get_ctx_start_addr()
{
  u64 ctx_start_addr = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      ctx_start_addr = (get_gmem_start() + 0x5004000 + 0x100000 + 0x80000); /// add iommu 0x40000entry*4, warp_affine_tbl size is 0x80000
      break;
    case BM1684:
    case BM1880:
      ctx_start_addr = (get_gmem_start() + 0x5000000 + 0x100000);
      break;
    case BM1684X:
    case BM1686:
      // BM1684X does not has arm reserved and const memory
      ctx_start_addr = get_gmem_start();
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return ctx_start_addr;
}

u64 bmrt_arch_info::get_soc_base_distance()
{
  u64 soc_base = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      soc_base = 0x100000000;
      break;
    case BM1880:
    case BM1684:
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }

  return soc_base;
}

u64 bmrt_arch_info::get_bdc_engine_cmd_aligned_bit()
{
  u64 cmd_bit = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      cmd_bit = 8;
      break;
    case BM1684:
    case BM1880:
      cmd_bit = 7;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return cmd_bit;
}

u64 bmrt_arch_info::get_gdma_engine_cmd_aligned_bit()
{
  u64 cmd_bit = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
    case BM1880:
      cmd_bit = 6;
      break;
    case BM1684:
      cmd_bit = 7;
      break;
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return cmd_bit;
}

u32 bmrt_arch_info::get_bdc_cmd_num()
{
  u32 num = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      num = 24;
      break;
    case BM1880:
    case BM1684:
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return num;
}

u32 bmrt_arch_info::get_gdma_cmd_num()
{
  u32 num = 0;
  switch(sta_bmtpu_ptr->target_bmtpu_arch) {
    case BM1682:
      num = 15;
      break;
    case BM1880:
    case BM1684:
    default:
      BMRT_LOG(FATAL, "Unknown bmtpu arch");
  }
  return num;
}

}

