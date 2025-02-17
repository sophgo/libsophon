#ifndef BM_SMI_CMDLINE_HPP
#define BM_SMI_CMDLINE_HPP
#include "gflags/gflags.h"
#ifdef __linux__
#include <ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <malloc.h>
#else
#include <string.h>
#include <panel.h>
#include <curses.h>
#pragma comment(lib, "pdcurses.lib")
#endif

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
