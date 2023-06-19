#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>
#endif

int main(int argc, char *argv[])
{
#ifndef USING_CMODEL
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;
  unsigned int *itcm_buffer = NULL, *ddr_buffer = NULL;
  FILE *fp_itcm = NULL;
  FILE *fp_ddr = NULL;
  char *itcm_fn = NULL, *ddr_fn = NULL;
  int itcm_size = 0, ddr_size = 0;
  int devid = 0;

  printf("%s\n", argv[0]);

  if (argc < 4) {
    printf("Please input the itcm and ddr binary firmware files and devid\n");
    printf("If it is bm1684x, please input \"null\" as the first param\n");
    return -1;
  }

  itcm_fn = argv[1];
  ddr_fn = argv[2];
  devid = atoi(argv[3]);

  printf("input itcm file %s ddr file %s\n", itcm_fn, ddr_fn);

  ret = bm_dev_request(&handle, devid);
  if (ret != BM_SUCCESS || handle == NULL) {
    printf("bm_dev_request device sophon%d failed, ret = %d\n",devid, ret);
    return -1;
  }

  if (handle->misc_info.chipid != 0x1686) {
    fp_itcm = fopen(itcm_fn, "rb");
    if (!fp_itcm) {
      printf("open file %s failed\n", itcm_fn);
      return -1;
    }
  }

  fp_ddr = fopen(ddr_fn, "rb");
  if (!fp_ddr) {
    printf("open file %s failed\n", ddr_fn);
    return -1;
  }

  if (handle->misc_info.chipid != 0x1686) {
    fseek(fp_itcm, 0, SEEK_END);
    itcm_size = ftell(fp_itcm);
    fseek(fp_itcm, 0, SEEK_SET);
  }

  fseek(fp_ddr, 0, SEEK_END);
  ddr_size = ftell(fp_ddr);
  fseek(fp_ddr, 0, SEEK_SET);

  if (itcm_size) {
    itcm_buffer = (unsigned int *)malloc(itcm_size);
    if (!itcm_buffer) {
      printf("malloc buffer for firmware failed\n");
      return -1;
    }
    if (fread(itcm_buffer, 1, itcm_size, fp_itcm) != (size_t)itcm_size) {
      printf("read itcm file length %d failed\n", itcm_size);
      return -1;
    }
    fclose(fp_itcm);
  }

  if (ddr_size) {
    ddr_buffer = (unsigned int *)malloc(ddr_size);
    if (!ddr_buffer) {
      printf("malloc buffer for firmware failed\n");
      return -1;
    }
    if (fread(ddr_buffer, 1, ddr_size, fp_ddr) != (size_t)ddr_size) {
      printf("read itcm file length %d failed\n", ddr_size);
      return -1;
    }
    fclose(fp_ddr);
  }

  printf("itcm size %d ddr size %d\n", itcm_size, ddr_size);

  bm_fw_desc fw_desc;
  fw_desc.itcm_fw = itcm_buffer;
  fw_desc.itcmfw_size = itcm_size;
  fw_desc.ddr_fw = ddr_buffer;
  fw_desc.ddrfw_size = ddr_size;

  ret = bm_update_firmware_a9(handle, &fw_desc);
  if (ret != BM_SUCCESS) {
    printf("upgrade firmware failed ret = %d\n", ret);
  }
  printf("update firmware successfully.\n");
  if (itcm_buffer) {
    free(itcm_buffer);
  }
  if (ddr_buffer) {
    free(ddr_buffer);
  }

  bm_dev_free(handle);
#endif
  return 0;
}
