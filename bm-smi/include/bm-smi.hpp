#ifndef BM_SMI_HPP
#define BM_SMI_HPP
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef __linux__
#include <ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define BMDEV_CTL "/dev/bmdev-ctl"
#else
#include <panel.h>
#include <curses.h>
#include <windows.h>
#include <psapi.h>
#endif
#include <errno.h>
#include <string.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <bmlib_runtime.h>
#include <bmlib_internal.h>
#include <bmlib_memory.h>

#define STRIP_FLAG_HELP 1
#define ATTR_FAULT_VALUE         (int)0xFFFFFC00
#define ATTR_NOTSUPPORTED_VALUE  (int)0xFFFFFC01

#define BM_SMI_FORMAT_HEIGHT 7  // lines of attributes display format header
#define BM_SMI_DEVATTR_HEIGHT 3  // lines of one device's attributes
#define BM_SMI_PROCHEADER_HEIGHT 5  // lines of proc_mem display header
#define BM_SMI_FORMAT_HEIGHT_UTIL 9 //lines of attributes display format header for util info
#define BM_SMI_FORMAT_HEIGHT_MEM 8 //lines of attributes display format header for memory info
#define BM_SMI_DEVATTR_HEIGHT_UTIL 5
#define BM_SMI_DEVATTR_HEIGHT_MEM 4

#define BUFFER_LEN 120  // columns of the text

#define MAX_NUM_VPU_CORE                5               /* four wave cores */
#define MAX_NUM_VPU_CORE_BM1686         3               /* four wave cores */
#define MAX_NUM_JPU_CORE                4
#define VPP_CORE_MAX                    2

/* bm_smi_attr defines all the attributes fetched from kernel;
 * it will be displayed by bm-smi and saved in files if needed.
 * this struct is also defined in driver/bm_uapi.h.*/
typedef struct bm_smi_attr {
  int dev_id;
  int chip_id;
  int chip_mode;  /*0---pcie; 1---soc*/
  int domain_bdf;
  int status;
  int card_index;
  int chip_index_of_card;

  int mem_used;
  int mem_total;
  int tpu_util;

  int board_temp;
  int chip_temp;
  int board_power;
  u32 tpu_power;
  int fan_speed;

  int vdd_tpu_volt;
  int vdd_tpu_curr;
  int atx12v_curr;

  int tpu_min_clock;
  int tpu_max_clock;
  int tpu_current_clock;
  int board_max_power;

  int ecc_enable;
  int ecc_correct_num;

  char sn[18];
  char board_type[6];

  /* vpu mem and instant info*/
  int vpu_instant_usage[MAX_NUM_VPU_CORE];
  signed long long vpu_mem_total_size[2];
  signed long long vpu_mem_free_size[2];
  signed long long vpu_mem_used_size[2];

  int jpu_core_usage[MAX_NUM_JPU_CORE];

  /*if or not to display board endline and board attr*/
  int board_endline;
  int board_attr;

  int vpp_instant_usage[VPP_CORE_MAX];
	int vpp_chronic_usage[VPP_CORE_MAX];

  bm_dev_stat_t stat;
} BM_SMI_ATTR, *BM_SMI_PATTR;

/*this struct defines the process gmem info for a device.
 * this info is fetched from kernel.
 * this struct is also defined in driver/bm_uapi.h.*/
struct bm_smi_proc_gmem {
  int dev_id;
#ifdef __linux__
  int pid[128];
  unsigned long gmem_used[128];
#else
  u64 pid[128];
  u64 gmem_used[128];
#endif
  int proc_cnt;
};
#endif
