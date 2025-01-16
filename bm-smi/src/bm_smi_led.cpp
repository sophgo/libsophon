#include "../include/bm_smi_led.hpp"

#ifdef __linux__
/* set led status */
static int bm_smi_set_led(int bmctl_fd, int dev_id, std::string flags_led) {
  int led_op;
  if (!flags_led.compare("on")) {
    led_op = 0;
  } else if (!flags_led.compare("off")) {
    led_op = 1;
  } else if (!flags_led.compare("blink")) {
    led_op = 2;
  } else {
    printf("error led status: %s\n", flags_led.c_str());
    return -EINVAL;
  }

  unsigned long led_arg = led_op << 8 | dev_id;

  if (ioctl(bmctl_fd, BMCTL_SET_LED, led_arg) < 0) {
    perror("bm-smi set LED failed!\n");
    return -1;
  }
  return 0;
}
#else
/* get number of devices in system*/
static int bm_smi_get_dev_cnt(HANDLE bmctl_device) {
    int dev_cnt;
    BOOL status = TRUE;
    DWORD bytesReceived = 0;

    status = DeviceIoControl(bmctl_device,
                             BMCTL_GET_DEV_CNT,
                             NULL,
                             0,
                             &dev_cnt,
                             sizeof(int),
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl BMCTL_GET_DEV_CNT failed 0x%x\n",
               GetLastError());
        CloseHandle(bmctl_device);
        return -EINVAL;
    }

    return dev_cnt;
}

/* set led status */
static int bm_smi_set_led(HANDLE bmctl_device, int dev_id, std::string flags_led) {
  int   led_op;
  BOOL  status        = TRUE;
  DWORD bytesReceived = 0;

  if (!flags_led.compare("on")) {
      led_op = 0;
  } else if (!flags_led.compare("off")) {
      led_op = 1;
  } else if (!flags_led.compare("blink")) {
      led_op = 2;
  } else {
      printf("error led status: %s\n", flags_led.c_str());
      return -EINVAL;
  }

  unsigned long led_arg = led_op << 8 | dev_id;
  status = DeviceIoControl(bmctl_device,
                           BMCTL_SET_LED,
                           &led_arg,
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
  return 0;
}

#endif

bm_smi_led::bm_smi_led(bm_smi_cmdline &cmdline) : bm_smi_test(cmdline) {
    int driver_version = 0;
    start_dev          = 0;
#ifdef __linux__
  fd = bm_smi_open_bmctl(&driver_version);
  dev_cnt = bm_smi_get_dev_cnt(fd);
#else
  bmctl_device = bm_smi_open_bmctl(&driver_version);
  dev_cnt      = bm_smi_get_dev_cnt(bmctl_device);
#endif
}

bm_smi_led::~bm_smi_led() {}

int bm_smi_led::validate_input_para() {
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

int bm_smi_led::run_opmode() {
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
    printf("set led failed!\n");
    printf("LED is not support on SOC mode.\n");
    return -1;
  } else {
    for (int i = start_dev; i < start_dev + dev_cnt; i++) {
#ifdef __linux__
      if (bm_smi_set_led(fd, i, g_cmdline.m_value)) {
        printf("set led failed!\n");
        close(fd);
#else
      if (bm_smi_set_led(bmctl_device, i, g_cmdline.m_value)) {
        printf("set led failed!\n");
        CloseHandle(bmctl_device);
#endif
        return -1;
      }
    }
  }

#ifdef __linux__
  close(fd);
#else
  CloseHandle(bmctl_device);
#endif
  return 0;
}

int bm_smi_led::check_result() { return 0; }

void bm_smi_led::case_help() {}