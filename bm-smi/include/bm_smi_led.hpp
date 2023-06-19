#ifndef BM_SMI_LED_HPP
#define BM_SMI_LED_HPP
#include "bm_smi_test.hpp"
#include "bm_smi_cmdline.hpp"
#include "bm-smi.hpp"
#include <stdlib.h>


class bm_smi_led : public bm_smi_test {
 public:
  explicit bm_smi_led(bm_smi_cmdline &cmdline);
  ~bm_smi_led();

  int validate_input_para();
  int run_opmode();
  int check_result();
  void case_help();

 private:
  int dev_cnt;
  int start_dev;
};
#endif

