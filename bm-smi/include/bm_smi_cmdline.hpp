#ifndef BM_SMI_CMDLINE_HPP
#define BM_SMI_CMDLINE_HPP
#include "gflags/gflags.h"
#include <ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <malloc.h>


class bm_smi_cmdline {
 public:
  bm_smi_cmdline(int argc, char *argv[]);
  ~bm_smi_cmdline();

  int validate_flags();

  int m_lms;
  std::string m_op;
  std::string m_value;
  std::string m_file;
  bool m_loop;
};

#endif
