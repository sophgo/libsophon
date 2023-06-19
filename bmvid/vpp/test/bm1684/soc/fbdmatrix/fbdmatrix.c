#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

struct csc_matrix matrix[9] = {
//YCbCr2RGB,BT601
{0x000004a8, 0x00000000, 0x00000662, 0xfffc8461,
	0x000004a8, 0xfffffe6d, 0xfffffcbf, 0x00021fb9,
	0x000004a8, 0x00000810, 0x00000000, 0xfffbad40 },
//YPbPr2RGB,BT601
{0x00000400, 0x00000000, 0x0000059c, 0xfffd323c,
	0x00000400, 0xfffffe9e, 0xfffffd24, 0x00021f15,
	0x00000400, 0x00000715, 0x00000000, 0xfffc7542 },
//RGB2YCbCr,BT601
{ 0x107, 0x204, 0x65, 0x4000,
	0xffffff68, 0xfffffed6, 0x1c2, 0x20000,
	0x1c2, 0xfffffe88, 0xffffffb7, 0x20000 },
//YUV2RGB,BT709
{ 0x000004a8, 0x00000000, 0x0000072c, 0xfffc1fa4,
	0x000004a8, 0xffffff26, 0xfffffdde, 0x0001338e,
	0x000004a8, 0x00000873, 0x00000000, 0xfffb7bec },
//RGB2YUV,BT709
{ 0x000000bb, 0x00000275, 0x0000003f, 0x00004000,
	0xffffff99, 0xfffffea5, 0x000001c2, 0x00020000,
	0x000001c2, 0xfffffe67, 0xffffffd7, 0x00020000 },

{	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0 },

{	1, 1, 1, 1,
	1, 1, 1, 1,
	1, 1, 1, 1 },

{	0x5a, 0x5a, 0x5a, 0x5a,
	0x5a, 0x5a, 0x5a, 0x5a,
	0x5a, 0x5a, 0x5a, 0x5a },

{	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff }
};

int main(int argc, char *argv[])
{
    int src_fmt, dst_fmt;
    vpp_mat src, dst;
    char *offset_base_y, *comp_base_y, *offset_base_c, *comp_base_c;
    int in_w, in_h = 0, out_w, out_h = 0;
    char *img_name;
    ion_para src_para;
    ion_para dst_para;
    void *src_va = 0;
    void *dst_va = 0;
    int src_stride = 0;    /*src image stride*/
    int dst_stride = 0;    /*dst image stride*/
    int ret, mode = 0;
    struct vpp_rect_s loca;

    if (argc != 13) {
        printf("usage: %d\n", argc);
        printf("%s src_width src_height offset_base_y comp_base_y offset_base_c comp_base_c src-stride dst-format dst_width dst_height dst_img_name csc_mode\n", argv[0]);
        printf("example:\n");
        printf("I420-> BGR:%s 1920 1080 fbc_offset_table_y.bin fbc_comp_data_y.bin fbc_offset_table_c.bin fbc_comp_data_c.bin 1920 3 984 638 out.bmp  0\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);    /*src image w*/
    in_h = atoi(argv[2]);    /*src image h*/
    offset_base_y = argv[3];    /*src image name*/
    comp_base_y = argv[4];    /*src image name*/
    offset_base_c = argv[5];    /*src image name*/
    comp_base_c = argv[6];    /*src image name*/

    src_stride = atoi(argv[7]);    /*src image format w*/
    dst_fmt = atoi(argv[8]);    /*dst image format*/
    out_w = atoi(argv[9]);    /*dst image w*/
    out_h = atoi(argv[10]);    /*dst image h*/
    img_name = argv[11];/*dst image name*/
    mode = atoi(argv[12]);

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

    src.num_comp = 4;
    src.ion_len[0] = len0;
    src.ion_len[1] = len1;
    src.ion_len[2] = len2;
    src.ion_len[3] = len3;
//    printf("len0 %d, len1 %d, len2 %d, len3 %d\n", len0, len1, len2, len3);

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
    src.pa[3] = src_para.pa + src.ion_len[0] + src.ion_len[1] + src.ion_len[2];
    src.va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[1] = src_para.va + src.ion_len[0];
    src.va[2] = src_para.va + src.ion_len[0] + src.ion_len[1];
    src.va[3] = src_para.va + src.ion_len[0] + src.ion_len[1] + src.ion_len[2];
    src.format = FMT_I420;/*data format*/
    src.cols = in_w;    /*image width*/
    src.rows = in_h;    /*image height*/
    src.stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/
    src.is_pa = 1;

    fread(src.va[0], 1, len0, fp0);
    fread(src.va[1], 1, len1, fp1);
    fread(src.va[2], 1, len2, fp2);
    fread(src.va[3], 1, len3, fp3);

    /*ion cache flush*/
    ion_flush(NULL, src_va, src_para.length);//vpp_read_file called ion_flush

    if (vpp_creat_ion_mem(&dst, dst_fmt, out_w, out_h) == VPP_ERR) {
        goto err1;
    }
    dst.is_pa = 1;

    switch (dst_fmt)
    {
        case FMT_BGR:
        case FMT_RGB:
        ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
             break;
        case FMT_I420:
        case FMT_BGRP:
        case FMT_RGBP:
        ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
        ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
        ion_invalidate(NULL, dst.va[2], dst.ion_len[2]);
             break;
        default:
            printf("not supported: dst_fmt = %d\n", dst_fmt);
            break;
    }

    /*Call vpp hardware driver api*/
    fbd_csc_resize(&src, &dst);

    loca.height = src.rows;
    loca.width = src.cols;
    loca.x = 0;
    loca.y = 0;

    fbd_matrix(&src, &loca, &dst, 1, CSC_USER_DEFINED, VPP_SCALE_BILINEAR, &(matrix[mode]));

    ret = output_file(img_name, &dst);

    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_ion_free(&src_para);    /*Release src image memory*/
err0:
    return 0;
}

