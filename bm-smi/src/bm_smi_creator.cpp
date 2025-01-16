#include "../include/bm_smi_cmdline.hpp"
#include "../include/bm_smi_creator.hpp"

bm_smi_creator::bm_smi_creator() {}
bm_smi_creator::~bm_smi_creator() {}

class bm_smi_test* bm_smi_creator::create(bm_smi_cmdline &cmdline) {
  if (cmdline.m_op == "display") {
    return (class bm_smi_test*)(new bm_smi_display(cmdline));
  } else if (cmdline.m_op == "ecc") {
    return (class bm_smi_test*)(new bm_smi_ecc(cmdline));
  } else if (cmdline.m_op == "led") {
    return (class bm_smi_test*)(new bm_smi_led(cmdline));
  } else if (cmdline.m_op == "recovery") {
    return (class bm_smi_test*)(new bm_smi_recovery(cmdline));
  } else if (cmdline.m_op == "display_memory_detail") {
    return (class bm_smi_test*)(new bm_smi_display_memory_detail(cmdline));
  } else if (cmdline.m_op == "display_util_detail") {
    return (class bm_smi_test*)(new bm_smi_display_util_detail(cmdline));
  }
  return nullptr;
}
