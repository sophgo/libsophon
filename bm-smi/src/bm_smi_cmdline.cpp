#include "../include/bm_smi_cmdline.hpp"


#ifndef SOC_MODE
DEFINE_string(ecc, "", "ECC on DDR is on or off.");
DEFINE_int32(dev, 0xff, "which dev is selected to query, 0xff is for all.");
DEFINE_int32(start_dev, 0xff, "the first dev selected to query, defalut val is invalid");
DEFINE_int32(last_dev, 0xff, "last dev selected to query, defalut val is invalid.");
DEFINE_bool(recovery, false, "if true, to recovery on device .");
DEFINE_bool(text_format, false, "if true, only display attr's value.");
DEFINE_string(led, "", "pcie card LED status: on/off/blink");
#endif
DEFINE_string(opmode, "", "choose opmode to use bm-smi.");
DEFINE_string(opval, "", "get value for ecc or led.");
DEFINE_string(file, "", "target file to save smi log.");
DEFINE_int32(lms, 500, "sample interval in loop mode.");
DEFINE_bool(loop, true, "true is for loop mode, false is for only once mode.");
DECLARE_bool(help);
DECLARE_bool(helpshort);


bm_smi_cmdline::bm_smi_cmdline(int argc, char *argv[]) {
#ifndef SOC_MODE
  /* get and validate flags*/
  gflags::SetUsageMessage("command line brew\n"
      "usage: bm-smi [--ecc=on/off] [--file=/xx/yy.txt] [--dev=0/1...]"
      "[--start_dev=x] [--last_dev=y] [--text_format]"
      " [--lms=500] [--recovery] [-loop] [--led=on/off/blink]\n"
      "ecc:\n"
      "  set ecc status, default is off\n"
      "file:\n"
      "  the target file to save smi log, default is empty.\n"
      "dev:\n"
      "  which device to be selected to query, default is all.\n"
      "start_dev:\n"
      "  the first device to be selected to query, must chip0 of one card, default is invalid.\n"
      "last_dev:\n"
      "  the last device to be selected to query, default is invalid.\n"
      "lms:\n"
      "  how many ms of the sample interval, default is 500.\n"
      "loop:\n"
      "  if -loop (default): smi sample device every lms ms.\n"
      "  if -noloop: smi sample device only once.\n"
      "recovery:\n"
      "  recovery dev from fault to active status.\n"
      "text_format:\n"
      "  if true only display attr value from start_dev to last_dev.\n"
      "led:\n"
      "  pcie card LED status: on/off/blink.\n"
      "\n"
      "New usage: bm-smi [--opmode=display/ecc/led/recovery...]"
      "[--opval=on/off/...] [--file=/xx/yy.txt]"
      "[--dev=0/1...] [--start_dev=x] [--last_dev=y] [--text_format]"
      "[--lms=500] [-loop]\n"
      "opmode(default null):\n"
      "  choose different mode,example:display, ecc, led, recovery\n"
      "    display:                 means open bm-smi window and check info, use like ./bm-smi\n"
      "    ecc:                     means enable or disable ecc, collocation opval=on/off\n"
      "    led:                     means modify led status,  collocation opval=on/blink/off\n"
      "    recovery:                means recovery dev from fault to active status.\n"
      "    display_memory_detail:   show memory details. example, bm-smi --opmode=display_memory_detail\n"
      "    display_util_detail:     show VPU and JPU usage status, including decode, encode and JPU usage details.\n"
      "                             example,  bm-smi --opmode=display_util_detail\n"
      "opval(default null):\n"
      "  set mode value, use with opmode!\n"
      "    off:      for led/ecc\n"
      "    on:       for led/ecc\n"
      "    blink:    for led\n"
      "other flags have same usage, Both usage can be used!\n");

#else
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
#endif


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

#ifndef SOC_MODE
  m_dev = FLAGS_dev;
  m_ecc = FLAGS_ecc;
  m_start_dev = FLAGS_start_dev;
  m_last_dev = FLAGS_last_dev;
  m_recovery = FLAGS_recovery;
  m_text_format = FLAGS_text_format;
  m_led = FLAGS_led;
#endif
  m_loop = FLAGS_loop;
  m_file = FLAGS_file;
  m_lms = FLAGS_lms;
  m_op = FLAGS_opmode;
  m_value = FLAGS_opval;

}

int bm_smi_cmdline::validate_flags() {
  if (m_op == "") {
#ifndef SOC_MODE
    if (m_recovery) {
      m_op = "recovery";
      return 0;
    } else if (m_ecc != "") {
      m_op = "ecc";
      m_value = m_ecc;
      return 0;
    } else if (m_led != "") {
      m_op = "led";
      m_value = m_led;
      return 0;
    } else {
      m_op = "display";
      return 0;
    }
#else
    m_op = "display";
    return 0;
#endif
  } else {
#ifndef SOC_MODE
    if ((m_op == "display") || (m_op == "ecc") || (m_op == "led") || (m_op == "recovery") || (m_op == "display_memory_detail") 
    || (m_op == "display_util_detail")){
      return 0;
    } else {
      printf("not support flag:%s, Please check!\n", m_op.c_str());
      return -1;
    }
#else
    if ((m_op == "display") || (m_op == "display_util") || (m_op == "display_memory_detail") || (m_op == "display_util_detail"))
      return 0;
    else {
      printf("not support flag:%s, Please check!\n", m_op.c_str());
      return -1;
    }
#endif
  }
}

bm_smi_cmdline::~bm_smi_cmdline(){}
