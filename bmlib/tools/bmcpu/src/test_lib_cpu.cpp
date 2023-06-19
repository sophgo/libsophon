#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <bmlib_runtime.h>

int g_devid = 0;
char g_library_file[100];
char g_function_name[100];
char g_function_param[100];

void *test_thread(void *arg) {
    bm_handle_t handle;
    int ret;
    int process_handle;
    bm_device_mem_t dev_mem;
    char *buffer;
    int thread_no = *(int *)arg;
    void *map_vaddr = NULL;
    struct timeval start;
    struct timeval end;

    ret = bm_dev_request(&handle, g_devid);
    if ((ret != 0) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        exit(-1);
    }

    printf("***thread %d test create process\n", thread_no);
    process_handle = bmcpu_open_process(handle, 0, -1);
    if (process_handle < 0) {
        printf("ERROR!!! thread %d open process ret: %d\n", thread_no, process_handle);
        exit(-1);
    }
    printf("thread %d open process ret: %d\n", thread_no, process_handle);
    printf("***thread %d test create process end\n", thread_no);

    printf("***thread %d test load library\n", thread_no);
    ret = bmcpu_load_library(handle, process_handle, (char *)g_library_file, -1);
    if (ret != 0) {
        printf("ERROR!!! thread %d load library ret: %d\n", thread_no, ret);
        exit(-1);
    }
    printf("thread %d load library ret: %d\n", thread_no, ret);
    printf("***thread %d test load library end\n", thread_no);

    printf("***thread %d test exec function\n", thread_no);

    gettimeofday(&start, NULL);
    ret = bmcpu_exec_function(handle,
                           process_handle,
                           (char *)g_function_name,
                           g_function_param,
                           strlen(g_function_param)+1,
                           -1);
    if (ret != 0) {
        printf("ERROR!!! thread %d test exec function error!\n", thread_no);
        exit(ret);
    }
    gettimeofday(&end, NULL);
    printf("thread %d exec function ret: %d, time : %ld\n",
            thread_no,
            ret,
            1000000 * (end.tv_sec-start.tv_sec) + end.tv_usec -  start.tv_usec);
    printf("***thread %d test exec function end\n", thread_no);


    printf("***thread %d test map phyaddr function\n", thread_no);
    ret = bm_malloc_device_byte(handle, &dev_mem, 8192);
    if (ret != 0) {
        printf("ERROR!!! thread %d malloc device mem error!\n", thread_no);
        exit(ret);
    }
    buffer = (char *)malloc(8192);
    if (buffer == NULL) {
        printf("ERROR!!! thread %d malloc buffer error!\n", thread_no);
        exit(-1);
    }
    memset(buffer, 0x56, 8192);
    ret = bm_memcpy_s2d(handle, dev_mem, buffer);
    if (ret != 0) {
        printf("ERROR!!! thread %d copy data to device mem error!\n", thread_no);
        exit(ret);
    }

    map_vaddr = bmcpu_map_phys_addr(handle, process_handle, (void *)(dev_mem.u.device.device_addr), 8192, -1);
    if (map_vaddr == NULL) {
        printf("ERROR!!! thread %d map phys addr error!\n", thread_no);
        exit(-1);
    }
    printf("thread %d map phys addr ret: %llx\n", thread_no, (unsigned long long)map_vaddr);
    bm_free_device(handle, dev_mem);
    free(buffer);
    printf("***thread %d test map phyaddr function end\n", thread_no);


    printf("***thread %d test close subprocess function\n", thread_no);
    ret = bmcpu_close_process(handle, process_handle, -1);
    if (ret < 0) {
        printf("ERROR!!! thread %d close process error! ret %d\n", thread_no, ret);
        exit(-1);
    }
    printf("thread %d query api BM_API_ID_CLOSE_PROCESS ret: %d\n", thread_no, ret);
    printf("***thread %d test close subprocess end function\n", thread_no);

    bm_dev_free(handle);

    return (void *)0;
}

int main(int argc, char *argv[]) {
    pthread_t thread[100];
    int arg[100];
    int thread_num;
    int i;
    int ret;

    if (argc < 5) {
        printf("Usage: test_lib_cpu dev_id library_file function_name function_param [thread_num]\n");
        return -1;
    }

    g_devid = atoi(argv[1]);
    strcpy(g_library_file, argv[2]);
    strcpy(g_function_name, argv[3]);
    strcpy(g_function_param, argv[4]);

    if (argc == 5)
        thread_num = 1;
    else
        thread_num = atoi(argv[5]);
    if (thread_num > 100)
        thread_num = 100;

    for (i = 0; i < thread_num; i++) {
        arg[i] = i;
        ret = pthread_create(&thread[i], NULL, test_thread, &arg[i]);
        if (ret != 0) {
            printf("create thread %d error!\n", i);
            return -1;
        }
    }

    for (i = 0; i < thread_num; i++) {
        ret = pthread_join(thread[i], NULL);
        if (ret != 0) {
            printf("join thread %d error!\n", i);
            return -1;
        }
    }
    return 0;
}