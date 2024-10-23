#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmlib_runtime.h"

static long long to_long_long(const char* s){
    int base = 10;
    int offset = 0;
    if(s[0] == '0' && s[1] == 'x') {
        base = 16;
        offset = 2;
    }
    return strtoll(s+offset, NULL, base);
}

#define BLOCK_SIZE (1024)
typedef enum {
    CONSOLE,
    TXT_FILE,
    BIN_FILE
} show_mode_t;

int main(int argc, char *argv[])
{
  if(argc < 2) {
    printf("Usage: %s addr [dword_size=1] [mode=0(console)/1(txt)/2(bin)]\n", argv[0]);
    return -1;
  }

  unsigned long long addr = to_long_long(argv[1]);
  unsigned long long dword_size = 1;
  if (argc>2){
      dword_size = to_long_long(argv[2]);
  }

  int dev_id = 0;

  show_mode_t mode = CONSOLE;
  if (argc > 3) {
    mode = (show_mode_t)to_long_long(argv[3]);
  }

  bm_handle_t handle = NULL;
  bm_status_t ret = ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS || handle == NULL) {
    printf("bm_dev_request failed, ret = %d\n", ret);
    return -1;
  }

  FILE* fp = NULL;
  char filename[1024]={0};
  if(mode == BIN_FILE || mode == TXT_FILE){
      snprintf(filename, sizeof(filename), "mem_0x%llx_%lld.%s",
              addr, dword_size, mode==BIN_FILE?"dat":"txt");
      fp = fopen(filename, mode==BIN_FILE?"wb":"w");
      if(fp == NULL){
        printf("create %s failed!\n", filename);
        bm_dev_free(handle);
        return -1;
      }
  }

  unsigned int data[BLOCK_SIZE];
  size_t buffer_size = sizeof(data);
  unsigned long long left_size = dword_size * 4;
  while(left_size>0){
      unsigned long long read_size = left_size>buffer_size? buffer_size: left_size;
      size_t read_dword_size=read_size/sizeof(unsigned int);
      bm_device_mem_t device_mem;
      bm_set_device_mem(&device_mem, read_size, addr);
      bm_memcpy_d2s_partial(handle, data, device_mem, read_size);
      if(mode == BIN_FILE){
          printf("Read addr=0x%llx, dword_size=%ld, data[0]=0x%08x\n", addr, read_dword_size, data[0]);
          fwrite(data, read_size, 1, fp);
      } else if(mode == TXT_FILE){
          printf("Read addr=0x%llx, dword_size=%ld, data[0]=0x%08x\n", addr, read_dword_size, data[0]);
          for(int i=0; i<read_dword_size; i++){
              fprintf(fp, "%08X\n", data[i]);
          }
      } else {
          for(int i=0; i<read_dword_size; i++){
              printf("addr=0x%llx, data=0x%08X\n", addr+i*sizeof(unsigned int), data[i]);
          }
      }
      left_size -= read_size;
      addr += read_size;
  }

  if(mode == BIN_FILE || mode == TXT_FILE){
      printf("Write to file %s\n", filename);
      fclose(fp);
  }

  bm_dev_free(handle);
  return ret;
}
