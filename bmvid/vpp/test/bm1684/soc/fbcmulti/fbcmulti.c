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
#include "md5.h"

#define MAX_THREAD_NUM (16)
char *filename[MAX_THREAD_NUM];
unsigned int flag[MAX_THREAD_NUM] = {0};
unsigned int thread_time[MAX_THREAD_NUM] = {0};
struct timeval thread_tv1[MAX_THREAD_NUM] = {0}, thread_tv0[MAX_THREAD_NUM] = {0};
char *g_string_path;
int g_flag = 1;
int g_compare = 0,g_printtime = 0, g_comparetime = 0;

#define FRAME (512)
char flag_break[MAX_THREAD_NUM] = {0};

int vpp_write_file1(vpp_mat *mat)
{
    FILE *fp1,*fp2,*fp3;
    int status = 0;
    int i = 0, rows = 0, cols = 0, stride = 0;


    switch (mat->format)
    {
        case FMT_RGBP:
            fp1 = fopen("1684-r111.bin", "wb");
            if(!fp1) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            fp2 = fopen("1684-g111.bin", "wb");
            if(!fp2) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            fp3 = fopen("1684-b111.bin", "wb");
            if(!fp3) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                if (fwrite(mat->va[0]+i*stride, 1, cols, fp1) != cols)
                {
                    fclose(fp1);
                    return VPP_ERR;
                }
            }
            for (i = 0; i < rows; i++)
            {
                if (fwrite(mat->va[1]+i*stride, 1, cols, fp2) != cols)
                {
                    fclose(fp2);
                    return VPP_ERR;
                }
            }
            for (i = 0; i < rows; i++)
            {
                if (fwrite(mat->va[2]+i*stride, 1, cols, fp3) != cols)
                {
                    fclose(fp3);
                    return VPP_ERR;
                }
            }
            fclose(fp1);
            fclose(fp2);
            fclose(fp3);
            break;
        default:
            VppErr("format = %d", mat->format);
            return -1;
    }


    return status;
}


void *vpp_test(void * arg)
{
    int ret = -1,g_exit_flag, i = 0,ddd = 0;
    int src_fmt, dst_fmt;
    vpp_mat src, dst;
    struct vpp_rect_s loca;
    char *offset_base_y, *comp_base_y, *offset_base_c, *comp_base_c;
    int in_w, in_h = 0, out_w, out_h = 0;
    char *img_name;
    ion_para src_para;
    ion_para dst_para;
    void *src_va = 0;
    int src_stride = 0;    /*src image stride*/
    int ret1 = 0;
    unsigned char golden_output_md5[16] = {0};
    unsigned char output_expected_md5value[3][16]={
    {0x70,0x49,0xc2,0x84,0xdd,0xef,0x82,0x3d,0xe9,0x30,0x30,0xb1,0xf9,0x2b,0xc7,0x5f,},
    {0x99,0x28,0xde,0x7b,0xd6,0x95,0x82,0x52,0xa7,0x01,0x11,0x6d,0x54,0x0a,0xba,0xfd,},
    {0x00,0xce,0x84,0x04,0xed,0x1f,0xe4,0x3a,0xbd,0x5e,0x4b,0x9e,0x6d,0xfd,0xfa,0xed,},
    };

    int id = *(int *)arg;
    unsigned int frame_count = 0,compare_time, compare_count = 0,compare;
    int idx_h = 0,idx_w = 0;
    unsigned char *pb_golden_mem,*pg_golden_mem,*pr_golden_mem;
    compare_time = g_comparetime;
    compare = g_compare;
    frame_count = g_flag;

    in_w = 1920;    /*src image w*/
    in_h = 1080;    /*src image h*/
    offset_base_y = "fbc_offset_table_y.bin";    /*src image name*/
    comp_base_y = "fbc_comp_data_y.bin";    /*src image name*/
    offset_base_c = "fbc_offset_table_c.bin";    /*src image name*/
    comp_base_c = "fbc_comp_data_c.bin";    /*src image name*/
    loca.x =  0;    /*offset of crop image on the x axis*/
    loca.y = 0;    /*offset of crop image on the y axis*/
    loca.width= 1920;    /*width of crop image*/
    loca.height= 1080;    /*height of crop image*/
    src_stride = 1920;    /*src image format w*/
    dst_fmt = FMT_RGBP;    /*dst image format*/
    out_w = 1920;    /*dst image w*/
    out_h = 1080;    /*dst image h*/
    img_name = "map.bmp";/*dst image name*/

    FILE *fp0 = fopen(offset_base_y, "rb");
    if(!fp0) {
        VppErr("ERROR: Unable to open offset_base_y!\n");
         exit(-1);
    }
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
//    printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);

    FILE *fp1 = fopen(comp_base_y, "rb");
    if(!fp1) {
        VppErr("ERROR: Unable to open comp_base_y!\n");
         exit(-1);
    }
    fseek(fp1, 0, SEEK_END);
    int len1 = ftell(fp1);
//    printf("len1 : %d\n", len1);
    fseek(fp1, 0, SEEK_SET);

    FILE *fp2 = fopen(offset_base_c, "rb");
    if(!fp2) {
        VppErr("ERROR: Unable to open offset_base_c!\n");
         exit(-1);
    }
    fseek(fp2, 0, SEEK_END);
    int len2 = ftell(fp2);
//    printf("len2 : %d\n", len2);
    fseek(fp2, 0, SEEK_SET);

    FILE *fp3 = fopen(comp_base_c, "rb");
    if(!fp3) {
        VppErr("ERROR: Unable to open comp_base_c!\n");
         exit(-1);
    }
    fseek(fp3, 0, SEEK_END);
    int len3 = ftell(fp3);
//    printf("len3 : %d\n", len3);
    fseek(fp3, 0, SEEK_SET);

   if(compare == 1)
   {

        /*channel b*/
        FILE *fb = fopen("1684-b.bin", "rb");
        if(!fb) {
            VppErr("ERROR: Unable to open 1684-b.bin!\n");
             exit(-1);
        }
        fseek(fb, 0, SEEK_END);
        int len_b = ftell(fb);
    
        pb_golden_mem = (unsigned char *)malloc(len_b);
        fseek(fb, 0, SEEK_SET);
        fread(pb_golden_mem, 1, len_b, fb);
    
        /*channel g*/
        FILE *fg = fopen("1684-g.bin", "rb");
        if(!fg) {
            VppErr("ERROR: Unable to open 1684-g.bin!\n");
             exit(-1);
        }
        fseek(fg, 0, SEEK_END);
        int len_g = ftell(fg);
    
        pg_golden_mem = (unsigned char *)malloc(len_g);
        fseek(fg, 0, SEEK_SET);
        fread(pg_golden_mem, 1, len_g, fg);
    
    
    
        /*channel r*/
        FILE *fr = fopen("1684-r.bin", "rb");
        if(!fr) {
            VppErr("ERROR: Unable to open 1684-r.bin!\n");
             exit(-1);
        }
        fseek(fr, 0, SEEK_END);
        int len_r = ftell(fr);
    
        pr_golden_mem = (unsigned char *)malloc(len_r);
        fseek(fr, 0, SEEK_SET);
        fread(pr_golden_mem, 1, len_r, fr);
    
    
        fclose(fb);
        fclose(fg);
        fclose(fr);
    }
    memset(&src_para, 0, sizeof(ion_para));
    memset(&dst_para, 0, sizeof(ion_para));

    memset(&src, 0, sizeof(src));    /*This operation must be done*/
    memset(&dst, 0, sizeof(dst));    /*This operation must be done*/

    src.num_comp = 4;
    src.ion_len[0] = len0;
    src.ion_len[1] = len1;
    src.ion_len[2] = len2;
    src.ion_len[3] = len3;

    /*alloc ion mem.u can use vpp_ion_malloc() or vpp_ion_malloc to get ion mem*/
    src_va = vpp_ion_malloc_len((src.ion_len[0] + src.ion_len[1] + src.ion_len[2] + src.ion_len[3]), &src_para);
    if (src_va == NULL) 
    {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err0;
    }

    src.pa[0] = src_para.pa;    /*data Physical address*/
    src.pa[1] = src_para.pa + src.ion_len[0];
    src.pa[2] = src_para.pa + src.ion_len[0] + src.ion_len[1];
    src.pa[3] = src_para.pa + src.ion_len[0] + src.ion_len[1] +src.ion_len[2];
    src.va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[1] = src_para.va + src.ion_len[0];
    src.va[2] = src_para.va + src.ion_len[0] + src.ion_len[1];
    src.va[3] = src_para.va + src.ion_len[0] + src.ion_len[1] + src.ion_len[2];
    src.format = FMT_NV12;/*data format*/
    src.cols = in_w;    /*image width*/
    src.rows = in_h;    /*image height*/
    src.stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/
    src.is_pa = 1;

    if (vpp_creat_ion_mem(&dst, dst_fmt, 1920, 1080) == VPP_ERR) {
        goto err1;
    }
    dst.is_pa = 1;

//    printf("id %d, dst[0]   0x%p\n",id, dst.pa[0]);
    fread(src.va[0], 1, len0, fp0);
    fread(src.va[1], 1, len1, fp1);
    fread(src.va[2], 1, len2, fp2);
    fread(src.va[3], 1, len3, fp3);

    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
    ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
    ion_invalidate(NULL, dst.va[2], dst.ion_len[2]);

    /*ion cache flush*/
    ion_flush(NULL, src_va, src_para.length);//vpp_read_file called ion_flush

    flag[id] = 0;

    while (frame_count)
    {

        fbd_csc_crop_multi(&src, &loca, &dst, 1);
        flag[id]++;
        frame_count--;
        compare_count++;

        if(compare == 1)
        {
            if(compare_count == compare_time)
            {
                unsigned char *pb_vpp_mem = dst.va[2];
                unsigned char *pg_vpp_mem = dst.va[1];
                unsigned char *pr_vpp_mem = dst.va[0];
#if 0
                            /***/
                                vpp_write_file1(&dst);
                            /***/
#endif
                /*begin to compare*/
                for (idx_h = 0; idx_h < 1080; idx_h++) 
                {
                    for (idx_w = 0; idx_w < 1920; idx_w++)
                    {
                        if (pb_vpp_mem[idx_h * dst.stride + idx_w] != pb_golden_mem[idx_h * dst.stride + idx_w])
                        {
                            printf("nv12 fbd to rgbp test channel b error\n");
                            return ((void*)(-1));
                        }
                        if (pg_vpp_mem[idx_h * dst.stride + idx_w] != pg_golden_mem[idx_h * dst.stride + idx_w])
                        {
                            printf("nv12 fbd to rgbp test channel g error\n");
                            return ((void*)(-1));
                        }
                        if (pr_vpp_mem[idx_h * dst.stride + idx_w] != pr_golden_mem[idx_h * dst.stride + idx_w])
                        {
                            printf("nv12 fbd to rgbp test channel r error\n");
                            return ((void*)(-1));
                        }
                    }
                }
                compare_count = 0;
            }
        }
        if(compare == 2)
        {
            if(compare_count == compare_time)
            {
                unsigned char *pb_vpp_mem = dst.va[2];
                unsigned char *pg_vpp_mem = dst.va[1];
                unsigned char *pr_vpp_mem = dst.va[0];

                md5_get(pb_vpp_mem, 1920*1080, golden_output_md5);
                ret1 = memcmp(output_expected_md5value[0], golden_output_md5, sizeof(golden_output_md5));
                if(ret1 !=0 )
                {
                  printf("vpp slt, 1684-b compare failed\n");
                  return ((void*)(-1));
                }

                md5_get(pg_vpp_mem, 1920*1080, golden_output_md5);
                ret1 = memcmp(output_expected_md5value[1], golden_output_md5, sizeof(golden_output_md5));
                if(ret1 !=0 )
                {
                  printf("vpp slt, 1684-g compare failed\n");
                  return ((void*)(-1));
                }

                md5_get(pr_vpp_mem, 1920*1080, golden_output_md5);
                ret1 = memcmp(output_expected_md5value[2], golden_output_md5, sizeof(golden_output_md5));
                if(ret1 !=0 )
                {
                  printf("vpp slt, 1684-r compare failed\n");
                  return ((void*)(-1));
                }
                compare_count = 0;
            }
        }
    }

if(compare == 1)
{
    free(pb_golden_mem);
    free(pg_golden_mem);
    free(pr_golden_mem);
}
    flag_break[id]=1;

    vpp_free_ion_mem(&dst);
err1:
    vpp_ion_free(&src_para);    /*Release src image memory*/
err0:
    return NULL;
}


int main(int argc, char **argv)
{
    struct timeval tv1, old_time, tv0;
    unsigned int i, mark = 0, fps = 0, mark_break = 0;
    unsigned int whole_time = 0;
    unsigned int thread_time_all = 0;
    unsigned int thread_time_realtime = 0, sig_time = 0;
    void *tret;
    int ret = 0;
    if (argc > 5)
    {
        printf("Usage: thread_num image_file ...");
        return -1;
    }
    int thread_num = 16;

    g_compare = atoi(argv[1]);
    g_printtime = atoi(argv[2]);
    g_flag = atoi(argv[3]);
    g_comparetime = atoi(argv[4]);

    pthread_t vc_thread[thread_num];
    pthread_t stat_h;
    for(i = 0; i < thread_num; i++)
    {
 //       printf("flag_break[%d] %d, ",i,flag_break[i]);
    }
    gettimeofday(&tv0, NULL);
    for (int i = 0; i < thread_num; i++) {
      int ret = pthread_create(&vc_thread[i], NULL, vpp_test, &i);
      if (ret != 0)
      {
        printf("pthread create failed");
        return -1;
      }
      sleep(1);
    }

    mark = 0;
    gettimeofday(&tv0, NULL);
    old_time = tv0;
    while(1)
    {
        gettimeofday(&tv0, NULL);
        whole_time = (tv0.tv_sec - old_time.tv_sec) * 1000000 + (tv0.tv_usec - old_time.tv_usec);
        if(whole_time >= (g_printtime*1000000))
        {
 
            for(i = 0; i < thread_num; i++)
            {

                mark += flag[i];
                flag[i] = 0;
                mark_break += flag_break[i];
            }
            fps = mark / (whole_time/1000000);
            printf("all frame: %u, fps %u,whole_time %us\n",mark,fps,whole_time/1000000);
            if(mark == 0)
            {
                break;
            }

            mark = 0;
            fps = 0;
            old_time = tv0;
            if(mark_break == thread_num)
            {
                break;
            }
            mark_break = 0;
        }
    }
  for (int i = 0; i < thread_num; i++) {
    pthread_join(vc_thread[i], &tret);
    if ((long)tret < 0) {
        ret = -1;
        printf("id  %d, tret   %ld\n", i, (long)tret);
    }
  }

  return ret;
}

