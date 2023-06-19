
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>
#else
#include <sys/time.h>
#include <pthread.h>
#endif
#include <stdlib.h>
#include "bm_ion.h"


#ifndef BM_PCIE_MODE
//#define TEST_MULTI_THREAD
#ifdef TEST_MULTI_THREAD
static void * FnMemory_flush(void * arg)
{
    int i = 0;
    bm_ion_buffer_t* buf0 = NULL;
    int buf0_size =  0;
    uint8_t * ddr1_mem  = NULL;
    int count = 0;
    int thread_num = *(int*)arg;
    int r1 = rand() % 100000;
    int r2 = rand() % 5;
    buf0_size = r2*0x100000+ r1;
    ddr1_mem = malloc(buf0_size);
    for (i = 0; i < buf0_size; i++)
    {
        *(uint8_t *)(ddr1_mem+i) = i%255;
    }
    while(1)
    {
        buf0 = bm_ion_allocate_buffer(0, buf0_size, (BM_ION_FLAG_VPP<<4) | BM_ION_FLAG_CACHED);
        bm_ion_map_buffer(buf0, BM_ION_MAPPING_FLAG_WRITE);

        memcpy(buf0->vaddr, ddr1_mem, buf0_size);
        bm_ion_flush_buffer(buf0);
        bm_ion_unmap_buffer(buf0);
        bm_ion_free_buffer(buf0);

        count++;
        if(count % 50 == 0)
        {
            printf("thread_num = %d,count = %d , buf0_size=%d\n",thread_num,count,buf0_size);
        }
        usleep(1000);
    }
    free(ddr1_mem);
}
#endif

int main(int argc, char* argv[])
{
    int i, j;
    struct timeval start;
    struct timeval end;
    double difftime = 0;
    bm_ion_buffer_t* buf0 = NULL;
    bm_ion_buffer_t* buf1 = NULL;
    int buf0_size =  2*1024*1024;
    int buf1_size =  2*1024*1024;
    int countsize = 0;

    bm_ion_allocator_open(0);

#ifdef TEST_MULTI_THREAD
    pthread_t thread_id[100];
    void *ret[100] = {NULL};
    int parameter[100]={0};
    int thread_nums = 0;
    thread_nums =  atoi(argv[1]);
    printf("thread_nums = %d\n",thread_nums);
    for(i = 0; i < thread_nums; i++)
    {
        parameter[i] = i;
        pthread_create(&thread_id[i], NULL, FnMemory_flush, (void *)&parameter[i]);
        printf("creat thread  %d, thread_nums= %d\n",i,thread_nums);
    }
    for(i=0;i<thread_nums; i++)
    {
        pthread_join(thread_id[i], &ret[i]);
    }
#else

    uint8_t * ddr1_mem = malloc(buf0_size);
    uint8_t * ddr1_mem2 = malloc(buf0_size);

    buf0 = bm_ion_allocate_buffer(0, buf0_size, (BM_ION_FLAG_VPP << 4) | BM_ION_FLAG_CACHED);
    buf1 = bm_ion_allocate_buffer(0, buf1_size, (BM_ION_FLAG_VPP << 4) | BM_ION_FLAG_CACHED);  // BM_ION_FLAG_WRITECOMBINE

    bm_ion_map_buffer(buf0,  (BM_ION_MAPPING_FLAG_READ|BM_ION_MAPPING_FLAG_WRITE));
    bm_ion_map_buffer(buf1, BM_ION_MAPPING_FLAG_WRITE|BM_ION_MAPPING_FLAG_READ);

    printf("[%s:%d]  buf0: pa=0x%0lx, CACHED, va=%p, size=%dMB\n",
           __func__, __LINE__, buf0->paddr, buf0->vaddr, buf0->size>>20);

    printf("[%s:%d]  buf1: pa=0x%0lx, CACHED, va=%p, size=%dMB\n",
           __func__, __LINE__, buf1->paddr, buf1->vaddr, buf1->size>>20);

    printf("[%s:%d]  malloc ddr1_mem:  va=%p, ddr1_mem2: va=%p, size=%dMB\n",
           __func__, __LINE__,  ddr1_mem, ddr1_mem2, buf0_size>>20);

    printf("start comparing test...\n");
    for (i = 0; i < buf0_size; i++)
    {
        *(uint8_t *)(ddr1_mem+i) = i%255;
        *(uint8_t *)(buf0->vaddr+i) = i%255;
    }

    printf("[%s:%d] copy data from vpp_ion buf0 to vpp_ion buf1\n", __func__, __LINE__);
    memcpy(buf1->vaddr, buf0->vaddr, buf0_size);

    if (memcmp(buf0->vaddr, buf1->vaddr, buf1_size))
        fprintf(stderr, "compare failed!\n");
    else
        printf("memcmp compare successful!\n\n");

    printf("start copying test...\n");

    // test  read and write  from vpp_ion to vpp_ion
    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memcpy(buf0->vaddr, buf1->vaddr, buf1_size);
        countsize += (buf1_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data from vpp_ion to vpp_ion (buf1 ->  buf0), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;

    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memcpy(buf1->vaddr, buf0->vaddr, buf1_size);
        countsize += (buf1_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data from vpp_ion to vpp_ion (buf0 ->  buf1), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;

    // test  write to vpp_ion from ddr1
    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memcpy(buf0->vaddr, ddr1_mem, buf0_size);
        countsize += (buf0_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data to vpp_ion from ddr1 (ddr1_mem -> buf0), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;

    // test  read  from vpp_ion to ddr1
    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memcpy(ddr1_mem, buf0->vaddr, buf0_size);
        countsize += (buf0_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data to ddr1 from vpp_ion (buf0 -> ddr1_mem), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;

    // test  read  from ddr1 to ddr1
    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memcpy(ddr1_mem2, ddr1_mem, buf0_size);
        countsize += (buf0_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data to ddr1 from ddr1 (ddr1_mem -> ddr1_mem2), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;

    // test  write to vpp_ion from ddr1
    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memset(buf0->vaddr, 80, buf0_size);
        countsize += (buf0_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memset data to vpp_ion(buf0), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;

    // test  write to vpp_ion from ddr1
    gettimeofday(&start, NULL);
    for(j = 0; j < 1000; j++)
    {
        memset(ddr1_mem2, 0xab, buf0_size);
        countsize += (buf0_size >> 20);
    }
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memset data to ddr1_mem2 , times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n", difftime, countsize, (float)(countsize/difftime));
    countsize = 0;


    bm_ion_unmap_buffer(buf1);
    bm_ion_unmap_buffer(buf0);

    bm_ion_free_buffer(buf1);
    bm_ion_free_buffer(buf0);

    free(ddr1_mem);
    free(ddr1_mem2);
#endif
    bm_ion_allocator_close(0);
    return 0;
}
#endif

#ifdef BM_PCIE_MODE
int main(int argc, char* argv[])
{
#ifdef WIN32
    clock_t start;
    clock_t end;
#else
    struct timeval start, end;
#endif
    double difftime = 0;
    bm_ion_buffer_t* dev_buf = NULL;
    uint8_t* host_buf = NULL;
    int buf_size = 2*1024*1024;
    int countsize = 0;
    int soc_idx = 0;
    int ret;
    int i, j;

    if (argc != 1 && argc != 2) {
        printf("Usage:\n\t%s\n", argv[0]);
        printf("Usage:\n\t%s <sophon device index>\n", argv[0]);
        return -1;
    }

    if (argc == 2)
        soc_idx = atoi(argv[1]);


    ret = bm_ion_allocator_open(soc_idx);
    if (ret < 0) {
        fprintf(stderr, "bm_ion_allocator_open failed\n");
        return -1;
    }

    host_buf = malloc(buf_size);
    if (host_buf==NULL) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    dev_buf = bm_ion_allocate_buffer(soc_idx, buf_size, (BM_ION_FLAG_VPP << 4));
    if (dev_buf == NULL) {
        fprintf(stderr, "bm_ion_allocate_buffer failed\n");
    }

    printf("[%s:%d]  dev_buf:  pa=0x%0lx, size=%dMB\n",
           __func__, __LINE__, dev_buf->paddr, dev_buf->size>>20);

    printf("[%s:%d]  host_buf: va=%p, size=%dMB\n",
           __func__, __LINE__,  host_buf, buf_size>>20);

    printf("start copying test...\n");

    for (i = 0; i < buf_size; i++)
        *(uint8_t *)(host_buf+i) = i%255;

    /* write to vpp_ion from host memory */
#ifdef WIN32
    start = clock();
#else
    gettimeofday(&start, NULL);
#endif
    countsize = 0;
    for(j = 0; j < 1000; j++)
    {
        bm_ion_upload_data(host_buf, dev_buf, buf_size);
        countsize += (buf_size >> 20);
    }

#ifdef WIN32
    end = clock();
    difftime = (double)(end - start) / CLOCKS_PER_SEC;
    printf("CPU memcpy data to vpp_ion from host (host_buf -> dev_buf), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n",
        difftime, countsize, (float)(countsize / difftime));
#else
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data to vpp_ion from host (host_buf -> dev_buf), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n",
        difftime, countsize, (float)(countsize / difftime));
#endif

    /* read  from vpp_ion to host memory */
#ifdef WIN32
    start = clock();
#else
    gettimeofday(&start, NULL);
#endif
    countsize = 0;
    for(j = 0; j < 1000; j++)
    {
        bm_ion_download_data(host_buf, dev_buf, buf_size);
        countsize += (buf_size >> 20);
    }

#ifdef WIN32
    end = clock();
    difftime = (double)(end - start)/CLOCKS_PER_SEC;
    printf("CPU memcpy data to vpp_ion from host (host_buf -> dev_buf), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n",
        difftime, countsize, (float)(countsize / difftime));
#else
    gettimeofday(&end, NULL);
    difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
    printf("CPU memcpy data to vpp_ion from host (host_buf -> dev_buf), times : %2.1fs, size = %dMB, BW = %4.1fMB/s\n",
        difftime, countsize, (float)(countsize / difftime));
#endif

    if (dev_buf)
        bm_ion_free_buffer(dev_buf);
    if (host_buf)
        free(host_buf);

    bm_ion_allocator_close(soc_idx);

    return 0;
}
#endif
