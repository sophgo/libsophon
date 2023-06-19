#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/time.h>
#include "vppion.h"
#include "vpplib.h"

#define MAX_CROP_NUM (16)
char *filename[MAX_CROP_NUM];
unsigned int g_thread_frame[MAX_CROP_NUM] = {0};
unsigned int thread_time[MAX_CROP_NUM] = {0};
struct timeval thread_tv1[MAX_CROP_NUM] = {0}, thread_tv0[MAX_CROP_NUM] = {0};
int g_flag = 1;
#define FRAME (512)
void *vpp_test(void * arg)
{
    int vpp_dev_fd, i;
    vpp_mat src;
    vpp_mat src1[MAX_CROP_NUM], dst[MAX_CROP_NUM], dst_bgr,dst_i420;
    struct vpp_rect_s loca[MAX_CROP_NUM];
    char *offset_base_y, *comp_base_y, *offset_base_c, *comp_base_c;
    int in_w, in_h = 0;
    char *img_name;
    ion_para src_para;
    ion_para dst_para;
    void *src_va = 0;
    void *dst_va = 0;
    int src_stride = 0;    /*src image stride*/
    int dst_stride = 0;    /*dst image stride*/
    int ret, src_len, idx;
    struct timeval tv1, tv0, tv2, tv3;
    unsigned int run_time = 0;
    int id = *(int *)arg;

    in_w = 1920;    /*src image w*/
    in_h = 1080;    /*src image h*/
    offset_base_y = "fbc_offset_table_y.bin";    /*src image name*/
    comp_base_y = "fbc_comp_data_y.bin";    /*src image name*/
    offset_base_c = "fbc_offset_table_c.bin";    /*src image name*/
    comp_base_c = "fbc_comp_data_c.bin";    /*src image name*/
    src_stride = 1920;    /*src image format w*/

    FILE *fp0 = fopen(offset_base_y, "rb");
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
//  printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);

    FILE *fp1 = fopen(comp_base_y, "rb");
    fseek(fp1, 0, SEEK_END);
    int len1 = ftell(fp1);
//  printf("len1 : %d\n", len1);
    fseek(fp1, 0, SEEK_SET);

    FILE *fp2 = fopen(offset_base_c, "rb");
    fseek(fp2, 0, SEEK_END);
    int len2 = ftell(fp2);
//  printf("len2 : %d\n", len2);
    fseek(fp2, 0, SEEK_SET);

    FILE *fp3 = fopen(comp_base_c, "rb");
    fseek(fp3, 0, SEEK_END);
    int len3 = ftell(fp3);
//  printf("len3 : %d\n", len3);
    fseek(fp3, 0, SEEK_SET);

    src_va = vpp_ion_malloc_len((len0 + len1 + len2 + len3), &src_para);
    if (src_va == NULL) 
    {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
    }

    fread(src_para.va, 1, len0, fp0);
    fread((src_para.va + len0), 1, len1, fp1);
    fread((src_para.va + len0 + len1), 1, len2, fp2);
    fread((src_para.va + len0 + len1 + len2), 1, len3, fp3);
    ion_flush(NULL, src_para.va,(len0 + len1 + len2 + len3));

    src.format = FMT_NV12;
    src.pa[0] = src_para.pa;
    src.pa[1] = src_para.pa + len0;
    src.pa[2] = src_para.pa + len0 + len1;
    src.pa[3] = src_para.pa + len0 + len1 + len2;
    src.cols = in_w;
    src.rows = in_h;
    src.stride = src_stride;
    src.is_pa = 1;
//  printf("fbd src.pa[0] 0x%p,src.pa[1] 0x%p,src.pa[2] 0x%p,src.pa[3] 0x%p\n", src.pa[0],src.pa[1],src.pa[2],src.pa[3]);

    if (vpp_creat_ion_mem(&dst_i420, FMT_I420, 1920, 1080) == VPP_ERR) {
        printf("vpp_creat_ion_mem error \n");
    }
//  printf("fbd dst_i420.pa[0]   0x%p,dst_i420.va[0]   0x%p\n", dst_i420.pa[0],dst_i420.va[0]);
    ion_invalidate(NULL, dst_i420.va[0], dst_i420.ion_len[0]);
    ion_invalidate(NULL, dst_i420.va[1], dst_i420.ion_len[1]);
    ion_invalidate(NULL, dst_i420.va[2], dst_i420.ion_len[2]);

    if (vpp_creat_ion_mem(&dst_bgr, FMT_BGR, 800, 600) == VPP_ERR) {
        printf("vpp_creat_ion_mem error \n");
    }
//  printf("fbd dst_bgr.pa[0]  0x%p,dst_bgr.va[0]   0x%p\n", dst_bgr.pa[0],dst_bgr.va[0]);
    ion_invalidate(NULL, dst_bgr.va[0], dst_bgr.ion_len[0]);

    for(i = 0; i < MAX_CROP_NUM; i++)
    {
        if (vpp_creat_ion_mem(&dst[i], FMT_BGR, 256, 256) == VPP_ERR) {
            printf("vpp_creat_ion_mem error \n");
        }
        dst[i].is_pa = 1;
        loca[i].width= 256;
        loca[i].height= 256;
        loca[i].x = i*32;
        loca[i].y = i*22;
        ion_invalidate(NULL, dst[i].va[0], dst[i].ion_len[0]);
//      printf("fbd dst[%d].pa[0]  0x%p,dst[%d].va[0]   0x%p\n",i, dst[i].pa[0],i, dst[i].va[0]);
    }

    g_thread_frame[id] = 0;
    while (1)
    {
        gettimeofday(&tv0, NULL);
        fbd_csc_resize(&src,&dst_i420);
        gettimeofday(&tv1, NULL);
        vpp_resize_csc(&dst_i420, &dst_bgr);
        gettimeofday(&tv2, NULL);
        for(i = 0; i < MAX_CROP_NUM; i++)
        {
            src1[i] = dst_i420;
        }
        vpp_misc(&src1[0], &loca[0], &dst[0], MAX_CROP_NUM, CSC_MAX, VPP_SCALE_BILINEAR);
        gettimeofday(&tv3, NULL);
        g_thread_frame[id]++;
#if 0
        run_time = (tv1.tv_sec - tv0.tv_sec) * 1000000 + (tv1.tv_usec - tv0.tv_usec);
        printf("fbd_csc_resize  %u, id  %d\n",run_time,id);
        run_time = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
        printf("vpp_resize_csc  %u, id  %d\n",run_time,id);
        run_time = (tv3.tv_sec - tv2.tv_sec) * 1000000 + (tv3.tv_usec - tv2.tv_usec);
        printf("vpp_misc  %u, id  %d\n",run_time,id);
#endif
#if 0
        vpp_bmp_bgr888("800*600.bmp", (unsigned  char *)dst_bgr.va[0], dst_bgr.cols, dst_bgr.rows, dst_bgr.stride);
        for (idx = 0; idx < MAX_CROP_NUM; idx++) {
            sprintf(output_name,"%s%d.bmp",crop,idx);
            vpp_bmp_bgr888(output_name, (unsigned  char *)dst[idx].va[0], dst[idx].cols, dst[idx].rows, dst[idx].stride);
        }
#endif
    }

    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
    vpp_free_ion_mem(&dst_i420);
    vpp_free_ion_mem(&dst_bgr);
    for(i = 0; i < MAX_CROP_NUM; i++)
    {
        vpp_free_ion_mem(&dst[i]);
    }
    vpp_ion_free(&src_para);    /*Release src image memory*/
    return NULL;
}

int main(int argc, char **argv)
{
    struct timeval tv1, old_time, tv0;
    unsigned int i, all_thread_frame = 0, fps = 0;
    unsigned int whole_time = 0;
    unsigned int thread_time_all = 0;
    unsigned int thread_time_realtime = 0, sig_time = 0;

    if (argc != 2) {
        printf("usage: %d\n", argc);
        printf("%s thread_num\n", argv[0]);
        printf("example:\n");
        printf("%s 16\n", argv[0]);
        return 0;
    }

    int thread_num = atoi(argv[1]);

    pthread_t vc_thread[thread_num];
    pthread_t stat_h;

    for (int i = 0; i < thread_num; i++) {
      int ret = pthread_create(&vc_thread[i], NULL, vpp_test, &i);
      if (ret != 0)
      {
        printf("pthread create failed");
        return -1;
      }
      sleep(1);
    }

    all_thread_frame = 0;
    gettimeofday(&tv0, NULL);
    old_time = tv0;
    while(1)
    {
        gettimeofday(&tv0, NULL);
        whole_time = (tv0.tv_sec - old_time.tv_sec) * 1000000 + (tv0.tv_usec - old_time.tv_usec);
        if(whole_time >= 10000000)
        {
            for(i = 0; i < thread_num; i++)
            {
                all_thread_frame += g_thread_frame[i];
                g_thread_frame[i] = 0;
            }
            fps = all_thread_frame / (whole_time/1000000);
            printf("all frame: %u, fps %u,spend time %u\n",all_thread_frame,fps,whole_time/1000000);

            all_thread_frame = 0;
            fps = 0;
            old_time = tv0;
        }
    }

    for (int i = 0; i < thread_num; i++) {
      pthread_join(vc_thread[i], NULL);
    }
  return 0;
}

