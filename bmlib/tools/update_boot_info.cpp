#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include "gflags/gflags.h"
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#ifdef __linux__
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <asm-generic/errno-base.h>
#else
#undef false
#undef true
#endif

using namespace std;

#define BMDEV_CTL "/dev/bmdev-ctl"
#define N 3
#define BOARD_TYPE_EVB 0x0
#define BOARD_TYPE_SA5 0x1
#define BOARD_TYPE_SC5 0x2
#define BOARD_TYPE_SE5 0x3
#define BOARD_TYPE_SM5_P 0x4
#define BOARD_TYPE_SM5_S 0x5
#define BOARD_TYPE_SA6 0x6
#define BOARD_TYPE_BM1684X_EVB 0x20

DEFINE_string(ddr_ecc_enable, "", "ECC on DDR0 is on or off");
DEFINE_int64(ddr_0a_size, -1, "ddr0a size");
DEFINE_int64(ddr_0b_size, -1, "ddr0b size");
DEFINE_int64(ddr_1_size, -1, "ddr1 sie");
DEFINE_int64(ddr_2_size, -1, "ddr2 size");
DEFINE_int32(ddr_vendor_id, -1, "ddr manufacturer");
DEFINE_int32(ddr_power_mode, -1, "0 for lpddr4, 1 for lpddr4x");
DEFINE_int32(ddr_gmem_mode, -1, "0 for gmem_normal, 1 for gmem_tpu_only");
DEFINE_int32(ddr_rank_mode, -1, "1 for 2222, 2 for 4444, 3 for 2244");
DEFINE_string(fan_exist, "", "fan  is exist");
DEFINE_int32(tpu_min_clk, 0, "min tpu clock Mhz");
DEFINE_int32(tpu_max_clk, 0, "max tpu clock Mhz");
DEFINE_int32(max_board_power, 0, "max board power Wa");
DEFINE_string(temp_sensor_exist, "", "have temperature_sensor  is yes or no");
DEFINE_string(chip_power_sensor_exist, "", "have temperature_sensor  is yes or no");
DEFINE_string(board_power_sensor_exist, "", "have temperature_sensor  is yes or no");
DEFINE_int32(dev, 0xff, "which dev is selected to query, 0xff is for all.");
DEFINE_bool(board_version, false, "show board version info.[7:0] hw_version, [15:8] board_type");
DEFINE_bool(display, false, "only display info");
DEFINE_bool(clean, false, "clean spi boot info");
DEFINE_bool(check, false, "check spi boot info");
DEFINE_string(a53_enable, "", "enable a53");
DEFINE_string(dyn_enable, "", "enable dyn");
DEFINE_int64(heap2_size, -1, "heap2 size");

#ifdef __linux__
static int bm_open_bmctl() {
  char dev_ctl_name[20];
  int fd;

  sprintf(dev_ctl_name, BMDEV_CTL);
  fd = open(dev_ctl_name, O_RDWR);
  if (fd == -1) {
    perror("no sophon device found on this PC or Server\n");
    exit(EXIT_FAILURE);
  }

  return fd;
}

static int bm_get_dev_cnt(int bmctl_fd) {
  int dev_cnt;

  if (ioctl(bmctl_fd, BMCTL_GET_DEV_CNT, &dev_cnt) < 0) {
    perror("can not get devices number on this PC or Server\n");
    exit(EXIT_FAILURE);
  }

  return dev_cnt;
}
#else
HANDLE bm_open_bmctl(int *driver_version) {
    HANDLE bmdev_ctl;
    DWORD  errNum        = 0;
    BOOL   status        = TRUE;
    DWORD  bytesReceived = 0;
    UCHAR  outBuffer     = 0;
    int    ret           = 0;

    bmdev_ctl = CreateFileW(L"\\\\.\\bmdev_ctl",
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (bmdev_ctl == INVALID_HANDLE_VALUE) {
        errNum = GetLastError();
        if (!(errNum == ERROR_FILE_NOT_FOUND ||
              errNum == ERROR_PATH_NOT_FOUND)) {
            printf("CreateFile failed!  ERROR_FILE_NOT_FOUND = %d\n", errNum);
        } else {
            printf("CreateFile failed!  errNum = %d\n", errNum);
        }
        CloseHandle(bmdev_ctl);
    } else {
        status = DeviceIoControl(bmdev_ctl,
                                 BMCTL_GET_DRIVER_VERSION,
                                 NULL,
                                 0,
                                 driver_version,
                                 sizeof(s32),
                                 &bytesReceived,
                                 NULL);
        if (status == FALSE) {
            printf("DeviceIoControl BMCTL_GET_SMI_ATTR failed 0x%x\n",
                   GetLastError());
            CloseHandle(bmdev_ctl);
            ret             = -1;
            *driver_version = 1 << 16;
        }
        return bmdev_ctl;
    }
    return NULL;
}

/* get number of devices in system*/
int bm_get_dev_cnt(HANDLE bmctl_device) {
    int   dev_cnt;
    BOOL  status        = TRUE;
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
#endif

#define BOOT_INFO_VERSION (0xabcd)
#define BOOT_INFO_VERSION_V1 (0xabcd0001)
static void dump_boot_info(struct bm_boot_info boot_info, int dev_id) {
  printf("<===dump boot info for dev %d start===>\n", dev_id);
  printf("deadbeef = 0x%x \n", boot_info.deadbeef);
  printf("ddr_ecc_enable = 0x%x \n", boot_info.ddr_ecc_enable);
  printf("ddr_0a_size = 0x%llx \n", boot_info.ddr_0a_size);
  printf("ddr_0b_size = 0x%llx \n", boot_info.ddr_0b_size);
  printf("ddr_1_size = 0x%llx \n", boot_info.ddr_1_size);
  printf("ddr_2_size = 0x%llx \n", boot_info.ddr_2_size);
  printf("ddr_vendor_id = 0x%x \n", boot_info.ddr_vendor_id);
  printf("ddr_power_mode = 0x%x \n", boot_info.ddr_mode & 0xffff);
  printf("ddr_gmem_mode = 0x%x \n", (boot_info.ddr_mode >> 16) & 0xffff);
  printf("ddr_rank_mode = 0x%x \n", boot_info.ddr_rank_mode);
  printf("fan_exist = 0x%x \n", boot_info.fan_exist);
  printf("tpu_min_clk = 0x%x \n", boot_info.tpu_min_clk);
  printf("tpu_max_clk = 0x%x \n", boot_info.tpu_max_clk);
  printf("max_board_power = 0x%x \n", boot_info.max_board_power);
  printf("temp_sensor_exist = 0x%x \n", boot_info.temp_sensor_exist);
  printf("chip_power_sensor_exist = 0x%x \n", boot_info.chip_power_sensor_exist);
  printf("board_power_sensor_exist = 0x%x \n", boot_info.board_power_sensor_exist);
  if (((boot_info.boot_info_version >> 16) & 0xffff) == BOOT_INFO_VERSION) {
    printf("boot_info_version = 0x%x\n", boot_info.boot_info_version & 0xffff);
  }
  if (boot_info.boot_info_version == BOOT_INFO_VERSION_V1) {
    printf("a53 enable = 0x%x\n", boot_info.append.append_v1.a53_enable);
    printf("dyn enable = 0x%x\n", boot_info.append.append_v1.dyn_enable);
    printf("heap2 size = 0x%llx\n", boot_info.append.append_v1.heap2_size);
  }
}

struct bm_boot_info deault_boot_info[N] = {
  {
    0xdeadbeef, 0x0,
    0x80000000, 0x80000000, 0x80000000, 0x80000000,
    0x0, 0x1, 0x1,
    0x226, 0x4b,
    0x1, 0x1, 0x1, 0x1,
    0x4b
  },
  {
    0xdeadbeef, 0x0,
    0x80000000, 0x80000000, 0x100000000, 0x100000000,
    0x0, 0x1, 0x3,
    0x226, 0x4b,
    0x1, 0x1, 0x1, 0x1,
    0x4b
  },
  {
    0xdeadbeef, 0x0,
    0x100000000, 0x100000000, 0x100000000, 0x100000000,
    0x0, 0x1, 0x2,
    0x3E8, 0x4b,
    0x1, 0x0, 0x1, 0x0,
    0x1E
  }
};

static int check_boot_info(struct bm_boot_info boot_info, int board_version)
{
  int index = 0;
  int board_type = 0;
  int hw_version = 0;

  board_type = (board_version >> 8) & 0xff;
  hw_version = (board_version & 0xff);

  if (boot_info.deadbeef != 0xdeadbeef)
    return -1;
  switch (board_type) {
  case BOARD_TYPE_EVB:
    if (hw_version == 0x0)
      index = 0;
    else
      index = 1;
  break;
  case BOARD_TYPE_SM5_P:
    if (hw_version >= 0x11)
      index = 1;
    else
      index = 0;
  break;
  case BOARD_TYPE_BM1684X_EVB:
      index = 2;
      break;
  case BOARD_TYPE_SC5:
  case BOARD_TYPE_SE5:
  case BOARD_TYPE_SA6:
  default:
    return -1;
  }

  if (boot_info.ddr_ecc_enable != deault_boot_info[index].ddr_ecc_enable)
    return -1;
  if (boot_info.ddr_0a_size != deault_boot_info[index].ddr_0a_size)
    return -1;
  if (boot_info.ddr_rank_mode != deault_boot_info[index].ddr_rank_mode)
    return -1;
  if (boot_info.tpu_max_clk != deault_boot_info[index].tpu_max_clk)
    return -1;
  if (boot_info.max_board_power != deault_boot_info[index].max_board_power)
    return -1;
  return 0;
}

static int update_boot_info(int dev_id) {
  struct bm_boot_info boot_info;
  struct bm_misc_info misc_info;
  bm_handle_t handle;

  bm_dev_request(&handle, dev_id);
  if (bm_get_boot_info(handle, &boot_info) != 0) {
    printf("dev = 0x%x get boot info fail \n", dev_id);
    return -1;
  }

  if (FLAGS_board_version) {
    if(BM_SUCCESS != bm_get_misc_info(handle, &misc_info)) {
      printf("update boot info dev = 0x%x get misc info fail \n", dev_id);
      return -1;
    }
    printf("update boot info dev = 0x%x board_version = 0x%x\n", dev_id,
      misc_info.board_version);
    return 0;
  }

  if (FLAGS_check) {
    if(BM_SUCCESS != bm_get_misc_info(handle, &misc_info)) {
      printf("update boot info dev = 0x%x get misc info fail \n", dev_id);
      return -1;
    }
    if (check_boot_info(boot_info, misc_info.board_version) !=0) {
      printf("check boot info failed. dev = 0x%x board_version = 0x%x\n", dev_id,
              misc_info.board_version);
      dump_boot_info(boot_info, dev_id);
      return -1;
    }
    printf("check boot info Success. dev = 0x%x board_version = 0x%x\n", dev_id,
              misc_info.board_version);
    return 0;
  }

  if (FLAGS_display) {
      dump_boot_info(boot_info, dev_id);
      return 0;
  }

  if (!FLAGS_ddr_ecc_enable.empty()) {
    if (!FLAGS_ddr_ecc_enable.compare("on"))
      boot_info.ddr_ecc_enable = 1;
    else if (!FLAGS_ddr_ecc_enable.compare("off"))
      boot_info.ddr_ecc_enable = 0;
    else
      printf("invalid ddr ecc arg %s!\n", FLAGS_ddr_ecc_enable.c_str());
  }

  if (FLAGS_ddr_0a_size >= 0) boot_info.ddr_0a_size = FLAGS_ddr_0a_size;
  if (FLAGS_ddr_0b_size >= 0) boot_info.ddr_0b_size = FLAGS_ddr_0b_size;
  if (FLAGS_ddr_1_size >= 0) boot_info.ddr_1_size = FLAGS_ddr_1_size;
  if (FLAGS_ddr_2_size >= 0) boot_info.ddr_2_size = FLAGS_ddr_2_size;

  if (FLAGS_ddr_vendor_id >= 0) boot_info.ddr_vendor_id = FLAGS_ddr_vendor_id;
  if (FLAGS_ddr_power_mode >= 0) {
	boot_info.ddr_mode &= ~(0xffff);
	boot_info.ddr_mode = boot_info.ddr_mode | (FLAGS_ddr_power_mode & 0xffff);
  }
  if (FLAGS_ddr_gmem_mode >= 0) {
	boot_info.ddr_mode &= ~(0xffff << 16);
	boot_info.ddr_mode = boot_info.ddr_mode | ((FLAGS_ddr_gmem_mode & 0xffff) << 16);
  }
  if (FLAGS_ddr_rank_mode >= 0) boot_info.ddr_rank_mode = FLAGS_ddr_rank_mode;

  if (!FLAGS_fan_exist.empty()) {
    if (!FLAGS_fan_exist.compare("yes"))
      boot_info.fan_exist = 1;
    else if (!FLAGS_fan_exist.compare("no"))
      boot_info.fan_exist = 0;
    else
      printf("invalid fan arg %s!\n", FLAGS_fan_exist.c_str());
  }

  if (FLAGS_tpu_max_clk > 0) boot_info.tpu_max_clk = FLAGS_tpu_max_clk;
  if (FLAGS_tpu_min_clk > 0) boot_info.tpu_min_clk = FLAGS_tpu_min_clk;
  if (FLAGS_max_board_power > 0) boot_info.max_board_power = FLAGS_max_board_power;

  if (!FLAGS_temp_sensor_exist.empty()) {
    if (!FLAGS_temp_sensor_exist.compare("yes"))
      boot_info.temp_sensor_exist = 1;
    else if (!FLAGS_temp_sensor_exist.compare("no"))
      boot_info.temp_sensor_exist = 0;
    else
      printf("invalid temp_sensor arg %s!\n", FLAGS_temp_sensor_exist.c_str());
  }

  if (!FLAGS_chip_power_sensor_exist.empty()) {
    if (!FLAGS_chip_power_sensor_exist.compare("yes"))
      boot_info.chip_power_sensor_exist = 1;
    else if (!FLAGS_chip_power_sensor_exist.compare("no"))
      boot_info.chip_power_sensor_exist = 0;
    else
      printf("invalid chip_power_sensor arg %s!\n", FLAGS_chip_power_sensor_exist.c_str());
  }

  if (!FLAGS_board_power_sensor_exist.empty()) {
    if (!FLAGS_board_power_sensor_exist.compare("yes"))
      boot_info.board_power_sensor_exist = 1;
    else if (!FLAGS_board_power_sensor_exist.compare("no"))
      boot_info.board_power_sensor_exist = 0;
    else
      printf("invalid board_power_sensor arg %s!\n", FLAGS_board_power_sensor_exist.c_str());
  }

  boot_info.deadbeef = 0xdeadbeef;
  boot_info.boot_info_version = BOOT_INFO_VERSION_V1;

  if (FLAGS_clean) {
    memset(&boot_info, 0xff, sizeof(struct bm_boot_info));
    printf("clean boot info dev = 0x%x \n", dev_id);
  }

  if (!FLAGS_a53_enable.empty()) {
    if (!FLAGS_a53_enable.compare("on"))
      boot_info.append.append_v1.a53_enable = 1;
    else if (!FLAGS_a53_enable.compare("off"))
      boot_info.append.append_v1.a53_enable = 0;
    else
      printf("invalid a53_enable arg %s!\n", FLAGS_a53_enable.c_str());
  }

  if (!FLAGS_dyn_enable.empty()) {
    if (!FLAGS_dyn_enable.compare("on"))
      boot_info.append.append_v1.dyn_enable = 1;
    else if (!FLAGS_dyn_enable.compare("off"))
      boot_info.append.append_v1.dyn_enable = 0;
    else
      printf("invalid dyn_enable arg %s!\n", FLAGS_dyn_enable.c_str());
  }

  if (FLAGS_heap2_size >= 0) boot_info.append.append_v1.heap2_size = FLAGS_heap2_size;

  if (bm_update_boot_info(handle, &boot_info) != 0) {
    printf("dev = 0x%x update boot info fail \n", dev_id);
    return -1;
  }
  bm_dev_free(handle);
  dump_boot_info(boot_info, dev_id);
  return 0;
}

int main(int argc, char *argv[]) {
  int dev_cnt = 0;
  #ifdef __linux__
  int fd;
  #else
  HANDLE fd;
  int *  version;
  #endif
  int i = 0;

  gflags::SetUsageMessage(
      "command line format\n"
      "usage: update boot info in spi flash \n"
      "[--ddr_ecc_enable=on/off] \n"
      "[--ddr_0a_size=0x100000000] \n"
      "[--ddr_0b_size=0x100000000]\n"
      "[--ddr_1_size=0x100000000] \n"
      "[--ddr_2_size=0x100000000]\n"
      "[--ddr_vendor_id=0x0/0x1 \n"
      "[--ddr_power_mode=0x0/0x1 \n"
      "[--ddr_gmem_mode=0x0/0x1 \n"
      "[--ddr_rank_mode=0x1/0x2/0x2 \n"
      "[--fan_exist=yes/no] \n"
      "[--tpu_min_clk=1] [--tpu_max_clk=550] \n"
      "[--max_board_power=75] \n"
      "[--temp_sensor_exist=yes/no]\n"
      "[--chip_power_sensor_exist=yes/no]\n"
      "[--board_power_sensor_exist=yes/no]\n"
      "[--dev=0/1...] \n"
      "[--display/--nodisplay] \n"
      "[--board_version/--noboard_version...] \n"
      "[--clean/--noclean...] \n"
      "[--check/--nocheck...] \n"
      "[--a53_enable=on/off] \n"
      "[--dyn_enable=on/off] \n"
      "[--heap2_size=0x40000000] \n"
      "ddr_ecc_enable(default empty):\n"
      "  if off: disable ddr ecc.\n"
      "  if on:  enable ddr ecc.\n"
      "ddr_0a_size(default 0x100000000):\n"
      "ddr_0b_size(default 0x100000000):\n"
      "ddr_1_size(default 0x100000000):\n"
      "ddr_2_size(default 0x100000000):\n"
      "ddr_vendor_id(default 0x0):\n"
      "  ddr manufacturer\n"
      "ddr_power_mode(default 0x0):\n"
      "  0x0 : lpddr4, 0x1: lpddr4x\n"
      "ddr_rank_num(default 0x1):\n"
      "  0x1 : 1 rank, 0x2: 2 rank\n"
      "fan_exist(default empty):\n"
      "  if no: do not have fan on device.\n"
      "  if yes: have fan on device.\n"
      "tpu_min_clk(tpu min clock, default 1 Mhz):\n"
      "tpu_max_clk(tpu max clock, default 550 Mhz):\n"
      "max_board_power(max board power, default 75 Wa):\n"
      "temp_sensor_exist(default empty):\n"
      "  if no: do not have temperature sendor on device.\n"
      "  if yes: have temperature sendor on device.\n"
      "chip_power_sensor_exist(default empty):\n"
      "  if no: do not have chip power sendor on device.\n"
      "  if yes: have chip power sendor on device.\n"
      "board_power_sensor_exist(default empty):\n"
      "  if no: do not have board power sendor on device.\n"
      "  if yes: have board power sendor on device.\n"
      "display(default false):\n"
      "  if --display:  display boot info.\n"
      "board_version(default false):\n"
      "  if --board_version:  display board_version.\n"
      "clean(default false):\n"
      "  if --clean:  clean boot info.\n"
      "check(default false):\n"
      "  if --check:  check boot info.\n"
      "a53_enable(default on):\n"
      "  if --a53_enable:  enable a53 feature.\n"
      "dyn_enable(default on):\n"
      "  if --dyn_enable:  enable dyn feature.\n"
      "heap2_size(default -1):\n"
      "  if --heap2_size:  set heap2 size.\n");

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  #ifdef __linux__
  fd = bm_open_bmctl();
  dev_cnt = bm_get_dev_cnt(fd);
  #else
  fd = bm_open_bmctl(version);
  dev_cnt = bm_get_dev_cnt(fd);
  #endif
  if ((FLAGS_dev != 0xff) && ((FLAGS_dev < 0) || (FLAGS_dev >= dev_cnt))) {
    printf("error dev = %d\n", FLAGS_dev);
    return -EINVAL;
  }
  if (FLAGS_dev == 0xff) {
    for (i = 0; i < dev_cnt; i++) {
      if (update_boot_info(i) != 0)
        return -1;
    }
  } else {
    if (update_boot_info(FLAGS_dev) != 0)
      return -1;
  }

  return 0;
}
