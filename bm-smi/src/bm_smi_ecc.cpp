#include "../include/bm_smi_ecc.hpp"

#ifdef __linux__
/* set ecc on/off */
static int bm_smi_set_ecc(int bmctl_fd, int dev_id, std::string flags_ecc) {
#ifndef SOC_MODE
  int ecc_op;
  unsigned long ecc_arg = 0UL;

  if (!flags_ecc.compare("on")) {
    ecc_op = 1;
  } else if (!flags_ecc.compare("off")) {
    ecc_op = 0;
  } else {
    printf("invalid ecc arg %s!\n", flags_ecc.c_str());
    return -EINVAL;
  }

  ecc_arg = ecc_op << 8 | dev_id;
  if (ioctl(bmctl_fd, BMCTL_SET_ECC, ecc_arg) < 0) {
    perror("bm-smi set ECC failed!\n");
    return -1;
  } else {
    printf("set sophon%d ddr ecc to %s, please reboot PC/Server\n",
      dev_id, flags_ecc.c_str());
  }
#endif
  return 0;
}
#else
/* set led status */
static int bm_smi_set_ecc(HANDLE      bmctl_device,
                          int         dev_id,
                          std::string flags_ecc) {
    int           ecc_op;
    unsigned long ecc_arg       = 0UL;
    BOOL  status        = TRUE;
    DWORD bytesReceived = 0;


    if (!flags_ecc.compare("on")) {
        ecc_op = 1;
    } else if (!flags_ecc.compare("off")) {
        ecc_op = 0;
    } else {
        printf("invalid ecc arg %s!\n", flags_ecc.c_str());
        return -EINVAL;
    }

    ecc_arg = ecc_op << 8 | dev_id;
    status = DeviceIoControl(bmctl_device,
                             BMCTL_SET_ECC,
                             &ecc_arg,
                             sizeof(u32),
                             NULL,
                             0,
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl BMCTL_SET_LED failed 0x%x\n", GetLastError());
        CloseHandle(bmctl_device);
        return -EPERM;
    }
    printf("set sophon%d ddr ecc to %s, please reboot PC/Server\n",
           dev_id,
           flags_ecc.c_str());
    return 0;
}
#endif

bm_smi_ecc::bm_smi_ecc(bm_smi_cmdline &cmdline) : bm_smi_test(cmdline) {
  int driver_version = 0;
  start_dev = 0;
#ifdef __linux__
  fd = bm_smi_open_bmctl(&driver_version);
  dev_cnt = bm_smi_get_dev_cnt(fd);
#else
  bmctl_device = bm_smi_open_bmctl(&driver_version);
  dev_cnt      = bm_smi_get_dev_cnt(bmctl_device);
#endif
}

bm_smi_ecc::~bm_smi_ecc() {}

int bm_smi_ecc::validate_input_para() {
#ifndef SOC_MODE
  if ((g_cmdline.m_dev != 0xff) && ((g_cmdline.m_dev < 0) || (g_cmdline.m_dev >= dev_cnt))) {
    printf("error dev = %d\n", g_cmdline.m_dev);
    return -EINVAL;
  }

  if (g_cmdline.m_dev == 0xff) {
    start_dev = 0;
  } else {
    start_dev = g_cmdline.m_dev;
    dev_cnt = 1;
  }
#endif
  return 0;
}

int bm_smi_ecc::run_opmode() {
  bm_handle_t handle;
  bm_status_t ret = BM_SUCCESS;
  struct bm_misc_info misc_info;

  ret = bm_dev_request(&handle, start_dev);
  if (ret != BM_SUCCESS)
    return -EINVAL;

  ret = bm_get_misc_info(handle, &misc_info);
  if (ret != BM_SUCCESS)
    return -EINVAL;

  free(handle);

  if (misc_info.pcie_soc_mode == 1) {
    printf("set ecc failed!\n");
    printf("ECC is not support on SOC mode.\n");
    return -1;
  } else {
    for (int i = start_dev; i < start_dev + dev_cnt; i++) {
#ifdef __linux__
      if (bm_smi_set_ecc(fd, i, g_cmdline.m_value)) {
        printf("set ecc failed!\n");
        close(fd);
#else
      if (bm_smi_set_ecc(bmctl_device, i, g_cmdline.m_value)) {
        printf("set ecc failed!\n");
        CloseHandle(bmctl_device);
#endif
        return -1;
      }
    }
#ifdef __linux__
    close(fd);
#else
    CloseHandle(bmctl_device);
#endif
    return 0;
  }
}

int bm_smi_ecc::check_result() { return 0; }

void bm_smi_ecc::case_help() {}
