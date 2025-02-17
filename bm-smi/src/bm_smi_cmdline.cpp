#include "../include/bm_smi_cmdline.hpp"



DEFINE_string(opmode, "", "choose opmode to use bm-smi.");
DEFINE_string(opval, "", "get value for ecc or led.");
DEFINE_string(file, "", "target file to save smi log.");
DEFINE_int32(lms, 500, "sample interval in loop mode.");
DEFINE_bool(loop, true, "true is for loop mode, false is for only once mode.");
DECLARE_bool(help);
DECLARE_bool(helpshort);


bm_smi_cmdline::bm_smi_cmdline(int argc, char *argv[]) {

  gflags::SetUsageMessage("command line brew\n"
      "usage: bm-smi [--opmode=display] [--file=/xx/yy.txt]"
      " [--lms=500] [-loop]\n"
      "opmode:\n"
      "  SOC mode just only use display.\n"
      "file:\n"
      "  the target file to save smi log, default is empty.\n"
      "lms:\n"
      "  how many ms of the sample interval, default is 500.\n"
      "loop:\n"
      "  if -loop (default): smi sample device every lms ms.\n"
      "  if -noloop: smi sample device only once.\n");



  gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  if (FLAGS_help) {
    FLAGS_help = false;
    FLAGS_helpshort = true;
  }
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      printf("ERROR: unknown command line flag: %s\n", argv[i]);
    }
    printf("plase check input flags!\n");
  }
  gflags::HandleCommandLineHelpFlags();


  m_loop = FLAGS_loop;
  m_file = FLAGS_file;
  m_lms = FLAGS_lms;
  m_op = FLAGS_opmode;
  m_value = FLAGS_opval;

}

int bm_smi_cmdline::validate_flags() {
  if (m_op == "") {
    m_op = "display";
    return 0;
  } else {
    if ((m_op == "display") || (m_op == "display_util") || (m_op == "display_memory_detail") || (m_op == "display_util_detail"))
      return 0;
    else {
      printf("not support flag:%s, Please check!\n", m_op.c_str());
      return -1;
    }
  }
}

bm_smi_cmdline::~bm_smi_cmdline(){}
