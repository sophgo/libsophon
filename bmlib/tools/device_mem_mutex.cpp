#if defined(SOC_MODE)
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "bmlib_runtime.h"
#include <unistd.h>
#include <errno.h>

int gnum = 0;

static void *func_1(void *);
static void *func_2(void *);
pthread_mutex_t *p_mutex;

int main(int argc, char *argv[])
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    int ret_p;
    bm_device_mem_t dev_buffer;
    unsigned long long vaddr;
    volatile int i = 0;

    pthread_t pt1 = 0;
    pthread_t pt2 = 0;

    if (argc != 1) {
        printf("run ./device_mem_mutex\n");
        return -1;
    }
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    bm_malloc_device_byte(handle, &dev_buffer, sizeof(pthread_mutex_t));
    printf("mem paddr: 0x%llx\t", bm_mem_get_device_addr(dev_buffer));
    ret = bm_mem_mmap_device_mem(handle, &dev_buffer, &vaddr);
    if (ret != BM_SUCCESS) {
        printf("%s %d map failed!\n", __func__, __LINE__);
        return ret;
    }

    p_mutex = (pthread_mutex_t *)vaddr;
    printf("vaddr: 0x%llx\n", p_mutex);

    pthread_mutex_init(p_mutex, NULL);
    ret_p = pthread_create(&pt1, NULL, func_1, NULL);
    if (ret_p != 0)
        perror("pthread 1 create");
    ret_p = pthread_create(&pt2, NULL, func_2, NULL);
    if (ret_p != 0)
        perror("pthread 2 create");

    pthread_join(pt1, NULL);
    pthread_join(pt2, NULL);
    pthread_mutex_destroy(p_mutex);

    bm_mem_unmap_device_mem(handle, p_mutex, sizeof(pthread_mutex_t));
    printf("test end\n");
    bm_dev_free(handle);

    return 0;
}

static void *func_1(void *)
{
    for (int i = 0; i < 3; i++)
    {
        sleep(1);
        printf("This is pthread 1\n");
        pthread_mutex_lock(p_mutex);
        gnum++;
        printf("thread 1 add 1 to num: %d\n", gnum);
        pthread_mutex_unlock(p_mutex);
    }
}

static void *func_2(void *)
{
    for (int i = 0; i < 5; i++)
    {
        sleep(1);
        printf("This is pthread 2\n");
        pthread_mutex_lock(p_mutex);
        gnum++;
        printf("thread 2 add 1 to num: %d\n", gnum);
        pthread_mutex_unlock(p_mutex);
    }
}
#else
#include <stdio.h>
int main()
{
    printf("only support in soc mode!\n");

    return 0;
}
#endif
