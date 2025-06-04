#include "../include/bm_smi_cmdline.hpp"
#include "../include/bm_smi_creator.hpp"

bm_smi_creator::bm_smi_creator() {}
bm_smi_creator::~bm_smi_creator() {}

class bm_smi_test* bm_smi_creator::create(bm_smi_cmdline &cmdline) {
  if (cmdline.m_op == "display") {
    return (class bm_smi_test*)(new bm_smi_display(cmdline));
  }
  return nullptr;
}
