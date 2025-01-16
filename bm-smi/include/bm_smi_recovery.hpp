#ifndef BM_SMI_RECOVERY_HPP
#define BM_SMI_RECOVERY_HPP
#include "bm_smi_test.hpp"
#include "bm_smi_cmdline.hpp"
#include "bm-smi.hpp"
#include <stdlib.h>



class bm_smi_recovery : public bm_smi_test {
 public:
  explicit bm_smi_recovery(bm_smi_cmdline &cmdline);
  ~bm_smi_recovery();

  int validate_input_para();
  int run_opmode();
  int check_result();
  void case_help();

 private:
  int dev_cnt;
  int start_dev;
  int fd;
};
#endif

