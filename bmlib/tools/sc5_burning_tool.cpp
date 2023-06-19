#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <regex>
#include <iostream>
// #include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "gflags/gflags.h"

#ifdef __linux__
#include <unistd.h>
#else
#undef false
#undef true
#endif

#ifndef USING_CMODEL
DEFINE_int32(dev, 0, "device id");
DEFINE_string(sn, "", "serial number");
DEFINE_string(mac0, "", "mac 0 address");
DEFINE_string(mac1, "", "mac 1 address");
DEFINE_int32(board_type, -1, "board type");
DEFINE_bool(show, false, "display SN and MAC0/MAC1 address");

using std::string;

static int bm_burn_sn(bm_handle_t handle, string &sn) {
  std::regex sn_regex("^\\w{17}$");
  string sn_get(17, 0);

  if (!sn.empty()) {
    std::cout << "+------------------------------------+\n";
    std::cout << "The SN to burn: " << sn << std::endl;
    // regex check
    if (!std::regex_match(sn, sn_regex)) {
      std::cerr << "The SN format is incorrect. A 17 bytes string is required.\n";
      return -1;
    } else {
      #ifdef __linux__
      if (0 == ioctl(handle->dev_fd, BMDEV_SN, sn.c_str())) {
      #else
      if (0 == platform_ioctl(handle, BMDEV_SN, (void *)(sn.c_str()))) {
      #endif
        std::cout << "SC5 burn SN completed.\n";
      } else {
        std::cerr << "SC5 burn SN failed!\n";
        return -1;
      }
    }

    #ifdef __linux__
    if (0 == ioctl(handle->dev_fd, BMDEV_SN, sn_get.c_str())) {
    #else
    if (0 == platform_ioctl(handle, BMDEV_SN, (void *)(sn_get.c_str()))) {
    #endif
      if (sn != sn_get) {
        std::cerr << "SC5 burn SN compare failed!\n";
        std::cout << "The SN to burn: " << sn << std::endl;
        std::cout << "The SN get after burn: " << sn_get << std::endl;
        return -1;
      }
    } else {
      std::cerr << "Read SN failed!\n";
      return -1;
    }
  }
  return 0;
}

static int bm_burn_board_type(bm_handle_t handle, char b_type) {
  if (b_type >= 0) {
    std::cout << "+------------------------------------+\n";
    printf("The board type to burn: %d\n", b_type);
    if (b_type <= 10) {
      #ifdef __linux__
      if (0 == ioctl(handle->dev_fd, BMDEV_BOARD_TYPE, &b_type)) {
      #else
      if (0 == platform_ioctl(handle, BMDEV_BOARD_TYPE, &b_type)) {
      #endif
        std::cout << "SC5 burn board type completed.\n";
      } else {
        std::cerr << "SC5 burn board type failed!\n";
        return -1;
      }
    } else {
      std::cerr << "The board type is invalid.\n";
      return -1;
    }
  }
  return 0;
}

static void display_mac(unsigned char *mac_bytes) {
  for (int i = 0; i < 5; i++) {
    printf("%02x:", mac_bytes[i]);
  }
  printf("%02x\n", mac_bytes[5]);
}

static int convert_mac_to_bytes(string &mac, unsigned char *mac_bytes) {
  string mac_byte = "0x00";
  char *str;
  std::regex mac_regex("^((\\d|[a-f]|[A-F]){2}:){5}(\\d|[a-f]|[A-F]){2}$");

  if (!std::regex_match(mac, mac_regex)) {
    std::cerr << "The mac format is not correct.\n";
    return -1;
  }
  for (int i = 0; i < 6; i++) {
    mac_byte[2] = mac[i * 3];
    mac_byte[3] = mac[i * 3 + 1];
    mac_bytes[i] = (unsigned char)strtod(mac_byte.c_str(), &str);
  }
  return 0;
}

static int bm_burn_mac(bm_handle_t handle, string &mac, int id) {
  int ret = 0;
  unsigned char mac_bytes[6] = {0};

  if (!mac.empty()) {
    std::cout << "+------------------------------------+\n";
    std::cout << "The MAC" << id << " address to burn: " << mac <<std::endl;
    if (convert_mac_to_bytes(mac, mac_bytes)) {
      std::cerr << "Cannot convert given mac address to valid mac bytes\n";
      return -1;
    } else {
      display_mac(mac_bytes);

      switch (id) {
      case 0:
        #ifdef __linux__
        ret = ioctl(handle->dev_fd, BMDEV_MAC0, mac_bytes);
        #else
        ret = platform_ioctl(handle, BMDEV_MAC0, mac_bytes);
        #endif
        break;
      case 1:
        #ifdef __linux__
        ret = ioctl(handle->dev_fd, BMDEV_MAC1, mac_bytes);
        #else
        ret = platform_ioctl(handle, BMDEV_MAC1, mac_bytes);
        #endif
        break;
      default:
        std::cerr << "Only MAC0/MAC1 are supported.\n";
      }
      if (!ret) {
        std::cout << "SC5 burn MAC" << id << " completed.\n";
      } else {
        std::cerr << "SC5 burn MAC" << id << " failed!\n";
        return -1;
      }
    }
  }
  return 0;
}

static int bm_display_sn(bm_handle_t handle) {
  string sn(17, 0);

  #ifdef __linux__
  if (0 == ioctl(handle->dev_fd, BMDEV_SN, sn.c_str())) {
  #else
  if (0 == platform_ioctl(handle, BMDEV_SN, (void *)(sn.c_str()))) {
  #endif
    std::cout << "SN : " << sn << std::endl;
  } else {
    std::cerr << "Read SN failed!\n";
    return -1;
  }
  return 0;
}

static int bm_display_mac(bm_handle_t handle, int id) {
  unsigned char mac_bytes[6] = {0};
  int ret = -1;

  switch (id) {
      case 0:
        #ifdef __linux__
        ret = ioctl(handle->dev_fd, BMDEV_MAC0, mac_bytes);
        #else
        ret = platform_ioctl(handle, BMDEV_MAC0, mac_bytes);
        #endif
        break;
      case 1:
        #ifdef __linux__
        ret = ioctl(handle->dev_fd, BMDEV_MAC1, mac_bytes);
        #else
        ret = platform_ioctl(handle, BMDEV_MAC1, mac_bytes);
        #endif
        break;
      default:
        std::cerr << "Only MAC0/MAC1 are supported.\n";
  }
  if (!ret) {
    std::cout << "MAC" << id << " : ";
    display_mac(mac_bytes);
  } else {
    std::cerr << "Read MAC " << id << " failed!\n";
    return -1;
  }
  return 0;
}

static int bm_display_board_type(bm_handle_t handle) {
  char b_type = -1;

  #ifdef __linux__
  if (0 == ioctl(handle->dev_fd, BMDEV_BOARD_TYPE, &b_type)) {
  #else
  if (0 == platform_ioctl(handle, BMDEV_BOARD_TYPE, &b_type)) {
  #endif
    printf("Board Type : %d\n", b_type);
  } else {
    std::cerr << "Read Board Type failed!\n";
    return -1;
  }
  return 0;
}

static int bm_display_burning_info(bm_handle_t handle) {
  std::cout << "\n**************************************\n";
  std::cout << "The Burned Info on this device\n";
  std::cout << "+------------------------------------+\n";
  bm_display_sn(handle);
  std::cout << "+------------------------------------+\n";
  bm_display_mac(handle, 0);
  std::cout << "+------------------------------------+\n";
  bm_display_mac(handle, 1);
  std::cout << "+------------------------------------+\n";
  bm_display_board_type(handle);
  std::cout << "+------------------------------------+\n\n";
  return 0;
}

int main(int argc, char* argv[]) {
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;

  gflags::SetUsageMessage(
    "command line format\n"
    "usage: burn MAC0/MAC1 address and serial number into SC5 card\n"
    "[--dev=0/1//]\n"
    "[--sn=XXXXXXXX]\n"
    "[--mac0=aa:bb:cc:dd:ee:ff]\n"
    "[--mac1=aa:bb:cc:dd:ee:ff]\n"
    "[--show]\n");

  printf("%s\n", argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  printf("device id: %d\n", FLAGS_dev);

  ret = bm_dev_request(&handle, FLAGS_dev);
  if (ret != BM_SUCCESS || handle == NULL) {
    printf("bm_dev_request device %d failed, ret = %d\n", FLAGS_dev, ret);
    return -1;
  }

  bm_burn_sn(handle, FLAGS_sn);
  bm_burn_mac(handle, FLAGS_mac0, 0);
  bm_burn_mac(handle, FLAGS_mac1, 1);
  bm_burn_board_type(handle, (char)FLAGS_board_type);

  if (FLAGS_show) {
    bm_display_burning_info(handle);
  }
  bm_dev_free(handle);
  return 0;
}

#else
int main(int argc, char *argv[]) {
  return 0;
}
#endif
