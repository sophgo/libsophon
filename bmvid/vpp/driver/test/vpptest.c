#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

int vpp_write_file1(vpp_mat *mat, char *file_name1, char *file_name2, char *file_name3)
{
    FILE *fp1,*fp2,*fp3;
    int status = 0;
    int i = 0, rows = 0, cols = 0, stride = 0;

    VppAssert(file_name1!= NULL);

    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
            fp1 = fopen(file_name1, "wb");
            if(!fp1) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }

            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                if (fwrite(mat->va[0]+i*stride, 1, cols, fp1) != cols)
                {
                    fclose(fp1);
                    return VPP_ERR;
                }
            }
            fclose(fp1);
            break;

        case FMT_RGBP:
            fp1 = fopen(file_name1, "wb");
            if(!fp1) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            fp2 = fopen(file_name2, "wb");
            if(!fp2) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            fp3 = fopen(file_name3, "wb");
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

        case FMT_I420:
            fp1 = fopen(file_name1, "wb");
            if(!fp1) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            fp2 = fopen(file_name2, "wb");
            if(!fp2) {
                VppErr("ERROR: Unable to open the output file!\n");
                exit(-1);
            }
            fp3 = fopen(file_name3, "wb");
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

            rows = mat->rows / 2;
            cols = mat->cols / 2;
            stride = mat->stride >> 1;

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

//#define WRITE_BIN
#define FREQUENCY 1000
int main(int argc, char *argv[])
{

    int idx_h ,idx_w, frequency = 0;
    vpp_mat src_fbd, temp_i420, temp_bgr24, dst_rgbp;
    int in_w, in_h = 0, out_w, out_h = 0;
    char *offset_base_y, *comp_base_y, *offset_base_c, *comp_base_c;
    int src_stride = 0;    /*src image stride*/
    ion_para src_para;
    ion_para dst_para;
    void *src_va = 0;


    if (argc != 1)
    {
        printf("usage: %d\n", argc);
        printf("%s");
        return -10;
    }

    in_w = 1920;    /*src image w*/
    in_h = 1080;    /*src image h*/
    offset_base_y = "fbc_offset_table_y.bin";    /*src image name*/
    comp_base_y = "fbc_comp_data_y.bin";    /*src image name*/
    offset_base_c = "fbc_offset_table_c.bin";    /*src image name*/
    comp_base_c = "fbc_comp_data_c.bin";    /*src image name*/

    src_stride = 1920;    /*src image format w*/
    out_w = 1920;    /*dst image w*/
    out_h = 1080;    /*dst image h*/

    FILE *fp0 = fopen(offset_base_y, "rb");
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
//    printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);

    FILE *fp1 = fopen(comp_base_y, "rb");
    fseek(fp1, 0, SEEK_END);
    int len1 = ftell(fp1);
//    printf("len1 : %d\n", len1);
    fseek(fp1, 0, SEEK_SET);

    FILE *fp2 = fopen(offset_base_c, "rb");
    fseek(fp2, 0, SEEK_END);
    int len2 = ftell(fp2);
//    printf("len2 : %d\n", len2);
    fseek(fp2, 0, SEEK_SET);

    FILE *fp3 = fopen(comp_base_c, "rb");
    fseek(fp3, 0, SEEK_END);
    int len3 = ftell(fp3);
//    printf("len3 : %d\n", len3);
    fseek(fp3, 0, SEEK_SET);

    memset(&src_para, 0, sizeof(ion_para));
    memset(&dst_para, 0, sizeof(ion_para));

    memset(&src_fbd, 0, sizeof(src_fbd));    /*This operation must be done*/
    memset(&temp_i420, 0, sizeof(temp_i420));    /*This operation must be done*/
    memset(&temp_bgr24, 0, sizeof(temp_bgr24));    /*This operation must be done*/
    memset(&dst_rgbp, 0, sizeof(dst_rgbp));    /*This operation must be done*/

    src_fbd.num_comp = 4;
    src_fbd.ion_len[0] = len0;
    src_fbd.ion_len[1] = len1;
    src_fbd.ion_len[2] = len2;
    src_fbd.ion_len[3] = len3;

    /*alloc ion mem.u can use vpp_ion_malloc() or vpp_ion_malloc to get ion mem*/
    src_va = vpp_ion_malloc_len((src_fbd.ion_len[0] + src_fbd.ion_len[1] + src_fbd.ion_len[2] +src_fbd.ion_len[3]), &src_para);
    if (src_va == NULL) 
    {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        return -2;
    }

    src_fbd.pa[0] = src_para.pa;    /*data Physical address*/
    src_fbd.pa[1] = src_para.pa + src_fbd.ion_len[0];
    src_fbd.pa[2] = src_para.pa + src_fbd.ion_len[0] + src_fbd.ion_len[1];
    src_fbd.pa[3] = src_para.pa + src_fbd.ion_len[0] + src_fbd.ion_len[1] + src_fbd.ion_len[2];
    src_fbd.va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src_fbd.va[1] = src_para.va + src_fbd.ion_len[0];
    src_fbd.va[2] = src_para.va + src_fbd.ion_len[0] + src_fbd.ion_len[1];
    src_fbd.va[3] = src_para.va + src_fbd.ion_len[0] + src_fbd.ion_len[1] + src_fbd.ion_len[2];
    src_fbd.format = FMT_NV12;/*data format*/
    src_fbd.cols = in_w;    /*image width*/
    src_fbd.rows = in_h;    /*image height*/
    src_fbd.stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/
    src_fbd.is_pa = 1;

    if (vpp_creat_ion_mem(&temp_i420, FMT_I420, out_w, out_h) == VPP_ERR) {
        vpp_ion_free(&src_para);
        return -2;
    }
    temp_i420.is_pa = 1;

    fread(src_fbd.va[0], 1, len0, fp0);
    fread(src_fbd.va[1], 1, len1, fp1);
    fread(src_fbd.va[2], 1, len2, fp2);
    fread(src_fbd.va[3], 1, len3, fp3);

    ion_invalidate(NULL, temp_i420.va[0], temp_i420.ion_len[0]);
    ion_invalidate(NULL, temp_i420.va[1], temp_i420.ion_len[1]);
    ion_invalidate(NULL, temp_i420.va[2], temp_i420.ion_len[2]);

    /*ion cache flush*/
    ion_flush(NULL, src_va, src_para.length);//vpp_read_file called ion_flush

    frequency = FREQUENCY;
    while(frequency) {
    /*Call vpp hardware driver api*/
    fbd_csc_resize(&src_fbd, &temp_i420, 0);
    frequency--;
    }

#if defined  WRITE_BIN
/***/
    char *write_name1, *write_name2, *write_name3,*write_name4;
    write_name1 = "1684-y.bin";
    write_name2 = "1684-u.bin";
    write_name3 = "1684-v.bin";
    write_name4 = "1684-I420.bin";
    vpp_write_file1(&temp_i420, write_name1, write_name2, write_name3);
    vpp_write_file(write_name4, &temp_i420);
/***/
#endif

    /*channel b*/
    FILE *fy = fopen("1684-y.bin", "rb");
    fseek(fy, 0, SEEK_END);
    int len_y = ftell(fy);

    unsigned char *py_golden_mem = (unsigned char *)malloc(len_y);
    fseek(fy, 0, SEEK_SET);
    fread(py_golden_mem, 1, len_y, fy);

    unsigned char *py_vpp_mem = temp_i420.va[0];

    /*channel g*/
    FILE *fu = fopen("1684-u.bin", "rb");
    fseek(fu, 0, SEEK_END);
    int len_u = ftell(fu);

    unsigned char *pu_golden_mem = (unsigned char *)malloc(len_u);
    fseek(fu, 0, SEEK_SET);
    fread(pu_golden_mem, 1, len_u, fu);

    unsigned char *pu_vpp_mem = temp_i420.va[1];

    /*channel r*/
    FILE *fv = fopen("1684-v.bin", "rb");
    fseek(fv, 0, SEEK_END);
    int len_v = ftell(fv);

    unsigned char *pv_golden_mem = (unsigned char *)malloc(len_v);
    fseek(fv, 0, SEEK_SET);
    fread(pv_golden_mem, 1, len_v, fv);

    unsigned char *pv_vpp_mem = temp_i420.va[2];

    /*begin to compare*/
    for (idx_h = 0; idx_h < 1080; idx_h++) {
        for (idx_w = 0; idx_w < 1920; idx_w++) {
            if (py_vpp_mem[idx_h * dst_rgbp.stride + idx_w] != py_golden_mem[idx_h * dst_rgbp.stride + idx_w]) {
                printf("NV12 fbd to I420 test channel y error\n");
                return -1;
            }
        }
    }

    for (idx_h = 0; idx_h < 540; idx_h++) {
        for (idx_w = 0; idx_w < 960; idx_w++) {
            if (pu_vpp_mem[idx_h * dst_rgbp.stride + idx_w] != pu_golden_mem[idx_h * dst_rgbp.stride + idx_w]) {
                printf("NV12 fbd to I420 test channel u error\n");
                return -1;
            }
        }
    }

    for (idx_h = 0; idx_h < 540; idx_h++) {
        for (idx_w = 0; idx_w < 960; idx_w++) {
            if (pv_vpp_mem[idx_h * dst_rgbp.stride + idx_w] != pv_golden_mem[idx_h * dst_rgbp.stride + idx_w]) {
                printf("NV12 fbd to I420 test channel v error\n");
                return -1;
            }
        }
    }
    free(py_golden_mem);
    free(pu_golden_mem);
    free(pv_golden_mem);

    vpp_ion_free(&src_para);    /*Release src image memory*/
    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
/*NV12 fbd-> I420 done*/
/*****************************************************************************/

    if (vpp_creat_ion_mem(&temp_bgr24, FMT_BGR, 1920, 1080) == VPP_ERR) {
        vpp_free_ion_mem(&temp_i420);
        return -2;
    }

    temp_bgr24.is_pa = 1;

    ion_invalidate(NULL, temp_bgr24.va[0], temp_bgr24.ion_len[0]);
#ifdef PCIE_MODE
#else
    frequency = FREQUENCY;
    while(frequency) {
    /*Call vpp hardware driver api*/
    vpp_yuv420_to_rgb(&temp_i420, &temp_bgr24, 0);
    frequency--;
    }
#endif

#if defined  WRITE_BIN
/***/
    write_name1 = "1684bgr24.bin";
    vpp_write_file(write_name1, &temp_bgr24);
/***/
#endif
    FILE *fbgr = fopen("1684bgr24.bin", "rb");
    fseek(fbgr, 0, SEEK_END);
    int len_bgr = ftell(fbgr);

    unsigned char *pbgr_golden_mem = (unsigned char *)malloc(len_bgr);
    fseek(fbgr, 0, SEEK_SET);

    fread(pbgr_golden_mem, 1, len_bgr, fbgr);
    unsigned char *pbgr_vpp_mem = temp_bgr24.va[0];

    for (idx_h = 0; idx_h < 1080; idx_h++) {
        for(idx_w = 0; idx_w < 1920; idx_w++) {
            if (((pbgr_vpp_mem[idx_h * temp_bgr24.stride + idx_w * 3 + 0]) != pbgr_golden_mem[idx_h * temp_bgr24.stride + idx_w * 3 + 0]) || \
                 ((pbgr_vpp_mem[idx_h * temp_bgr24.stride + idx_w * 3 + 1]) != pbgr_golden_mem[idx_h * temp_bgr24.stride + idx_w * 3 + 1]) || \
                ((pbgr_vpp_mem[idx_h * temp_bgr24.stride + idx_w * 3 + 2]) != pbgr_golden_mem[idx_h * temp_bgr24.stride + idx_w * 3 + 2]))
             {
                 printf("I420to bgr24 test error\n");
                 return -1;
            }
        }
    }

    free(pbgr_golden_mem);
    vpp_free_ion_mem(&temp_i420);
    fclose(fbgr); 
/******************I420 -> bgr24 done**************************/
    if (vpp_creat_ion_mem(&dst_rgbp, FMT_RGBP, 1920, 1080) == VPP_ERR) {
        vpp_free_ion_mem(&temp_bgr24);
        return -2;
    }

    dst_rgbp.is_pa = 1;
    ion_invalidate(NULL, dst_rgbp.va[0], dst_rgbp.ion_len[0]);
    ion_invalidate(NULL, dst_rgbp.va[1], dst_rgbp.ion_len[1]);
    ion_invalidate(NULL, dst_rgbp.va[2], dst_rgbp.ion_len[2]);
#ifdef PCIE_MODE
#else
    frequency = FREQUENCY;
    while(frequency) {
    /*Call vpp hardware driver api*/
    vpp_split_to_rgb(&temp_bgr24, &dst_rgbp);
    frequency--;
    }
#endif

#if defined  WRITE_BIN
/***/
    write_name1 = "1684-r.bin";
    write_name2 = "1684-g.bin";
    write_name3 = "1684-b.bin";
    write_name4 = "1684-rgbp.bin";
    vpp_write_file1(&dst_rgbp, write_name1, write_name2, write_name3);
    vpp_write_file(write_name4, &dst_rgbp);
/***/
#endif

    /*channel b*/
    FILE *fb = fopen("1684-b.bin", "rb");
    fseek(fb, 0, SEEK_END);
    int len_b = ftell(fb);

    unsigned char *pb_golden_mem = (unsigned char *)malloc(len_b);
    fseek(fb, 0, SEEK_SET);
    fread(pb_golden_mem, 1, len_b, fb);

    unsigned char *pb_vpp_mem = dst_rgbp.va[2];

    /*channel g*/
    FILE *fg = fopen("1684-g.bin", "rb");
    fseek(fg, 0, SEEK_END);
    int len_g = ftell(fg);

    unsigned char *pg_golden_mem = (unsigned char *)malloc(len_g);
    fseek(fg, 0, SEEK_SET);
    fread(pg_golden_mem, 1, len_g, fg);

    unsigned char *pg_vpp_mem = dst_rgbp.va[1];

    /*channel r*/
    FILE *fr = fopen("1684-r.bin", "rb");
    fseek(fr, 0, SEEK_END);
    int len_r = ftell(fr);

    unsigned char *pr_golden_mem = (unsigned char *)malloc(len_r);
    fseek(fr, 0, SEEK_SET);
    fread(pr_golden_mem, 1, len_r, fr);

    unsigned char *pr_vpp_mem = dst_rgbp.va[0];

    /*begin to compare*/
    for (idx_h = 0; idx_h < 1080; idx_h++) {
        for (idx_w = 0; idx_w < 1920; idx_w++) {
            if (pb_vpp_mem[idx_h * dst_rgbp.stride + idx_w] != pb_golden_mem[idx_h * dst_rgbp.stride + idx_w]) {
                printf("bgr24 to rgbp test channel b error\n");
                return -1;
            }
            if (pg_vpp_mem[idx_h * dst_rgbp.stride + idx_w] != pg_golden_mem[idx_h * dst_rgbp.stride + idx_w]) {
                printf("bgr24 to rgbp test channel g error\n");
                return -1;
            }
            if (pr_vpp_mem[idx_h * dst_rgbp.stride + idx_w] != pr_golden_mem[idx_h * dst_rgbp.stride + idx_w]) {
                printf("bgr24 to rgbp test channel r error\n");
                return -1;
            }
        }
    }
    //cout << "bgr24 to rgbp test ok" << endl;
    free(pb_golden_mem);
    free(pg_golden_mem);
    free(pr_golden_mem);

    vpp_free_ion_mem(&dst_rgbp);
    vpp_free_ion_mem(&temp_bgr24);

    return 0;
}
#endif

#if 0
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
    printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);

    FILE *fp1 = fopen(comp_base_y, "rb");
    if(!fp1) {
        VppErr("ERROR: Unable to open comp_base_y!\n");
         exit(-1);
    }
    fseek(fp1, 0, SEEK_END);
    int len1 = ftell(fp1);
    printf("len1 : %d\n", len1);
    fseek(fp1, 0, SEEK_SET);

    FILE *fp2 = fopen(offset_base_c, "rb");
    if(!fp2) {
        VppErr("ERROR: Unable to open offset_base_c!\n");
         exit(-1);
    }
    fseek(fp2, 0, SEEK_END);
    int len2 = ftell(fp2);
    printf("len2 : %d\n", len2);
    fseek(fp2, 0, SEEK_SET);

    FILE *fp3 = fopen(comp_base_c, "rb");
    if(!fp3) {
        VppErr("ERROR: Unable to open comp_base_c!\n");
         exit(-1);
    }
    fseek(fp3, 0, SEEK_END);
    int len3 = ftell(fp3);
    printf("len3 : %d\n", len3);
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

    printf("id %d, dst[0]   0x%p\n",id, dst.pa[0]);
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
    if((int)tret < 0) {
        ret = -1;
        printf("id  %d, tret   %d\n",i,(int)tret);
    }
  }

  return ret;
}
#endif

