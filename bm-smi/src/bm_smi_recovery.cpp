#include "../include/bm_smi_recovery.hpp"

bm_smi_recovery::bm_smi_recovery(bm_smi_cmdline &cmdline) : bm_smi_test(cmdline) {
  start_dev = 0;
#ifdef __linux__
  int driver_version = 0;
  fd = bm_smi_open_bmctl(&driver_version);
  dev_cnt = bm_smi_get_dev_cnt(fd);
#endif
}

bm_smi_recovery::~bm_smi_recovery() {}

int bm_smi_recovery::validate_input_para() {

  return 0;
}

int bm_smi_recovery::run_opmode() {
#ifdef __linux__

  close(fd);
  return 0;


#else
  return 0;
#endif
}

int bm_smi_recovery::check_result() { return 0; }

void bm_smi_recovery::case_help() {}
