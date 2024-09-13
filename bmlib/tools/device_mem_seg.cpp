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
#include <semaphore.h>

int gnum = 0;
pthread_mutex_t *p_mutex;
static void *func_1(void *);
static void *func_2(void *);
sem_t *p_sem;

int main(int argc, char *argv[])
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    int ret_p;
    bm_device_mem_t dev_buffer;
    unsigned long long vaddr;

    pthread_t pt1 = 0;
    pthread_t pt2 = 0;

    if (argc != 1) {
        printf("run ./device_mem_seg\n");
        return -1;
    }

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    bm_malloc_device_byte(handle, &dev_buffer, sizeof(sem_t));
    printf("mem paddr: 0x%llx\t", bm_mem_get_device_addr(dev_buffer));

    ret = bm_mem_mmap_device_mem(handle, &dev_buffer, &vaddr);
    if (ret != BM_SUCCESS) {
        printf("%s %d map failed!\n", __func__, __LINE__);
        return ret;
    }

    p_sem = (sem_t *)vaddr;
    printf("vaddr is 0x%llx\n", p_sem);
    sem_init(p_sem, 0, 0);
    ret_p = pthread_create(&pt1, NULL, func_1, NULL);
    if (ret_p != 0)
        perror("pthread 1 create");
    printf("create pthread 1\n");
    sleep(7);
    ret_p = pthread_create(&pt2, NULL, func_2, NULL);
    if (ret_p != 0)
        perror("pthread 2 create");
    printf("create pthread 2\n");
    pthread_join(pt1, NULL);
    pthread_join(pt2, NULL);

    printf("test end\n");
    sem_destroy(p_sem);
    bm_dev_free(handle);

    return 0;
}

static void *func_1(void *)
{
    printf("%s wait sem\n", __func__);
    sem_wait(p_sem);
    printf("thread 1 running\n");
}

static void *func_2(void *)
{
    printf("thread 2 running\n");
    sem_post(p_sem);
    printf("thread 2 end\n");
}
#else
#include <stdio.h>
int main()
{
    printf("only support in soc mode!\n");

    return 0;
}
#endif
