#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <bmlib_runtime.h>
#include <bmcpu_internal.h>

unsigned  int ipstr2num(char* str)
{
	int i = 0;
	int j = 0;
	char new_str[3];
	int num = 0;
	char ipstr[15];
	unsigned int result[4];
	unsigned  int ip = 0;

	while(*str != '\0')
	{
		while((*str != '.') && (*str != '\0'))
		{
			new_str[i] = *str;
			num = num * 10 + new_str[i] - '0';
			str += 1;
			i += 1;
		}
		result[j] = num;
		num = 0;
		if (*str == '\0')
		{
			break;
		}
		else
		{
			str += 1;
			i = 0;
			j += 1;
		}
	}

    ip |= ((result[3]&0xff)<<24);
    ip |= ((result[2]&0xff)<<16);
    ip |= ((result[1]&0xff)<<8);
    ip |= ((result[0]&0xff)<<0);

    return htonl(ip);
}

int main(int argc, char *argv[]) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    bm_handle_t handle;
    bm_status_t ret;
    u32 ip;

    if (argc < 3) {
        printf("Usage: test_set_ip dev_id ip\n");
        return -1;
    }

    ret = bm_dev_request(&handle, atoi(argv[1]));
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        return -1;
    }

    ip = ipstr2num(argv[2]);
    ret = bm_set_ip(handle, ip);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set ip error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    } else {
        printf("Set ip %s success!\n", argv[2]);
    }

    bm_dev_free(handle);
#else
    argc = argc;
    argv = argv;
    printf("This test case is only valid in PCIe mode!\n");
#endif
    return 0;
}
