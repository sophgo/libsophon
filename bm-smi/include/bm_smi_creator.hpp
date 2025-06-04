#ifndef BM_SMI_CREATOR_HPP
#define BM_SMI_CREATOR_HPP
#include "bm_smi_cmdline.hpp"
#include "bm_smi_test.hpp"
#include "bm_smi_display.hpp"

class bm_smi_creator {
 public:
  bm_smi_creator();
  ~bm_smi_creator();

  class bm_smi_test* create(bm_smi_cmdline &cmdline);
};
#endif
