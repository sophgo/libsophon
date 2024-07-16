#include<unistd.h>
#include<termios.h>
#include "../include/bm-smi.hpp"
#include "../include/bm_smi_cmdline.hpp"
#include "../include/bm_smi_test.hpp"
#include "../include/bm_smi_creator.hpp"

int main(int argc, char *argv[]) {
  int ret;
  struct termios stTermCfg;
  bm_smi_cmdline cmdline(argc, argv);
  ret = cmdline.validate_flags();
  if (ret != 0)
    return -EINVAL;

  bm_smi_test *smi_test = nullptr;
  bm_smi_creator creator;
  tcgetattr(0, &stTermCfg);
  smi_test = creator.create(cmdline);
  ret = smi_test->validate_input_para();
  if (ret) {
    printf("validate input param failed!\n");
    tcsetattr(0, TCSANOW, &stTermCfg);
    return -EINVAL;
  }

  ret = smi_test->run_opmode();
  if (ret) {
    printf("run_opmode failed!\n");
    tcsetattr(0, TCSANOW, &stTermCfg);
    return -EINVAL;
  }

  tcsetattr(0, TCSANOW, &stTermCfg);
  return 0;
}
