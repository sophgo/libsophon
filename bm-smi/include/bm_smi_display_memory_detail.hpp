#ifndef BM_SMI_DISPLAY_MEMORY_DETAIL_HPP
#define BM_SMI_DISPLAY_MEMORY_DETAIL_HPP
#include "bm-smi.hpp"
#include "bm_smi_test.hpp"
#include "bm_smi_cmdline.hpp"
#include <stdlib.h>
#ifdef _WIN32
#pragma comment(lib, "libbmlib-static.lib")
#endif


class bm_smi_display_memory_detail : public bm_smi_test {
 public:
  explicit bm_smi_display_memory_detail(bm_smi_cmdline &cmdline);
  ~bm_smi_display_memory_detail();

  int validate_input_para();
  int run_opmode();
  int check_result();
  void case_help();

};
#endif
