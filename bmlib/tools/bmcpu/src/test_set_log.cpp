#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <bmlib_runtime.h>
#include <ctype.h>
#ifdef __linux__
#include <sys/syscall.h>
#else
#include <windows.h>
#endif

#define BM_LOGLEVEL_DEBUG  0
#define BM_LOGLEVEL_INFO   1
#define BM_LOGLEVEL_WARN   2
#define BM_LOGLEVEL_ERROR  3
#define BM_LOGLEVEL_FATAL  4
#define BM_LOGLEVEL_OFF    5

#define LOG_TO_CONSOLE_ON  1
#define LOG_TO_CONSOLE_OFF 0

static int isdigitstr(char *str)
{
    int len = strlen(str);
    int i = 0;

    for (i = 0; i < len; i++)
    {
        if (!(isdigit(str[i])))
            return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    bm_handle_t handle;
    bm_status_t ret;
    int i;

    if (argc < 4) {
        printf("Usage: test_set_log dev_id log_level log_to_console\n");
        return -1;
    }

    for(i = 1; i <= 3; i++) {
        if (isdigitstr(argv[i])) {
            printf("bmcpu_set_log error, input is not number!\n");
            return -1;
        }
    }

    ret = bm_dev_request(&handle, atoi(argv[1]));
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        return -1;
    }

    ret = bmcpu_set_log(handle, atoi(argv[2]), atoi(argv[3]), -1);
    if (ret != BM_SUCCESS) {
        printf("ERROR!!! set log error!\n");
        bm_dev_free(handle);
        return -1;
    }

    bm_dev_free(handle);

    printf("set log success!\n");
#else
    argc = argc;
    argv = argv;
    printf("This test case is only valid in PCIe mode!\n");
#endif
    return 0;
}
