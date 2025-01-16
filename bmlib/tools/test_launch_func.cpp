#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "bmlib_runtime.h"
#include "bmlib_memory.h"
#include "api.h"
#include "string.h"

#ifdef WIN32
#include <io.h>
#include <time.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif


typedef struct
{
	u8 *lib_path;
	void *lib_addr;
	u32 size;
	u8 lib_name[LIB_MAX_NAME_LEN];
	unsigned char md5[MD5SUM_LEN];
	int cur_rec;
} a53lite_load_lib_t;



int main(int argc, char *argv[])
{
  int chip_num = 0;
  int op = 0;
	bm_device_mem_t mem;
  
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;
	char* func_name = NULL;
	a53lite_load_lib_t api;
	memset(&api, 0, sizeof(api));


  // ret = bm_dev_request(&handle, chip_num);
  // if (ret != BM_SUCCESS || handle == NULL) {
  //   printf("bm_dev_request failed, ret = %d\n", ret);
  //   // return -1;
  // }
	//firmware_core.so
	bm_send_api(handle,BM_API_ID_A53LITE_LOAD_LIB,(u8 *)(&api), sizeof(api));


  // bm_dev_free(handle);
  return ret;
}