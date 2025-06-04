#ifndef BM_SMI_TEST_HPP
#define BM_SMI_TEST_HPP
#include <sys/stat.h>
#include <pthread.h>
#include "bmlib_log.h"
#include "bmlib_internal.h"
#include "bm_smi_cmdline.hpp"
#include "bm-smi.hpp"

class bm_smi_test {
 public:
  explicit bm_smi_test(bm_smi_cmdline &cmdline);
  virtual ~bm_smi_test();

  virtual int validate_input_para() = 0;
  virtual int run_opmode() = 0;
  virtual int check_result() = 0;
  virtual void case_help();

  bm_smi_cmdline &g_cmdline;
  int fd;
};

int bm_smi_open_bmctl(int *driver_version);
int bm_smi_get_dev_cnt(int bmctl_fd);
#endif
