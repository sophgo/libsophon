#include "../include/bm-smi.hpp"
#include "../include/bm_smi_cmdline.hpp"
#include "../include/bm_smi_test.hpp"
#include "../include/bm_smi_creator.hpp"

int main(int argc, char *argv[]) {
  int ret;
  bm_smi_cmdline cmdline(argc, argv);
  ret = cmdline.validate_flags();
  if (ret != 0)
    return -EINVAL;

  bm_smi_test *smi_test = nullptr;
  bm_smi_creator creator;
  smi_test = creator.create(cmdline);
  ret = smi_test->validate_input_para();
  if (ret) {
    printf("validate input param failed!\n");
    return -EINVAL;
  }

  ret = smi_test->run_opmode();
  if (ret) {
    printf("run_opmode failed!\n");
    return -EINVAL;
  }

  return 0;
}
