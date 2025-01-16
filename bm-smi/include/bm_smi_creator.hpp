#ifndef BM_SMI_CREATOR_HPP
#define BM_SMI_CREATOR_HPP
#include "bm_smi_cmdline.hpp"
#include "bm_smi_test.hpp"
#include "bm_smi_display.hpp"
#include "bm_smi_ecc.hpp"
#include "bm_smi_led.hpp"
#include "bm_smi_recovery.hpp"
#include "bm_smi_display_memory_detail.hpp"
#include "bm_smi_display_util_detail.hpp"

class bm_smi_creator {
 public:
  bm_smi_creator();
  ~bm_smi_creator();

  class bm_smi_test* create(bm_smi_cmdline &cmdline);
};
#endif
