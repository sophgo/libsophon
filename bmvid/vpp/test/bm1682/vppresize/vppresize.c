#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

static int output_file(int format, char *file_name, vpp_mat *mat);

int main(int argc, char *argv[])
{
    int ret = -1;
    int src_format = 0, dst_format = 0;
    char *input_name;
    int in_w;
    int in_h;
    int out_w;
    int out_h;
    char* dst_img_name;
    int is_src_pa, is_dst_pa;
    vpp_mat src;
    vpp_mat dst;

    if (argc != 11)
    {
        printf("usage: %d\n", argc);
        printf("%s in_w in_h input_name is_src_pa is_dst_pa src-format dst-format out_w out_h dst_img_name\n", argv[0]);
        printf("    src-format: 0 -- yuv420p             \n");
        printf("                2 -- rgbp                 \n");
        printf("                5 -- bgr                 \n");
        printf("                7 -- nv12                 \n");
        printf("    dst-format: 0 -- yuv420p             \n");
        printf("                2 -- rgbp                 \n");
        printf("                5 -- bgr                 \n");
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv.bin 0 0 5 5 1920 1080 resize.bmp\n", argv[0]);
        printf("        %s 1920 1080 img2.yuv           1 0 0 5 1920 1080 resize.bmp\n", argv[0]);
        printf("        %s 1920 1080 img1.nv12          0 1 7 5 1920 1080 resize.bmp\n", argv[0]);
        printf("        %s 1920 1080 img2.yuv           1 1 0 0 1920 1080 resize.i420\n", argv[0]);
        printf("        %s 1920 1080 img1.nv12          0 0 7 0 1920 1080 resize.i420\n", argv[0]);
        printf("        %s 1920 1080 img2.yuv           0 1 0 2 1920 1080 resize.rgbp\n", argv[0]);
        printf("        %s 1920 1080 img2.rgbp          1 0 2 5 1920 1080 resize.bmp\n", argv[0]);
        printf("        %s 1920 1080 img1.nv12          1 1 7 2 1920 1080 resize.rgbp\n", argv[0]);
        return ret;
    }

    in_w         = atoi(argv[1]);
    in_h         = atoi(argv[2]);
    input_name   = argv[3];
    is_src_pa    = atoi(argv[4]);
    is_dst_pa    = atoi(argv[5]);
    src_format       = atoi(argv[6]);
    dst_format       = atoi(argv[7]);
    out_w        = atoi(argv[8]);
    out_h        = atoi(argv[9]);
    dst_img_name = argv[10];

    if(!((is_src_pa == 0) || (is_src_pa == 1))) {
        printf("is_src_pa=%d input error.\n", is_src_pa);
        return 0;
    }
    if(!((is_dst_pa == 0) || (is_dst_pa == 1))) {
        printf("is_dst_pa=%d input error.\n", is_dst_pa);
        return 0;
    }

    if (vpp_creat_ion_mem(&src, src_format, in_w, in_h) == VPP_ERR) {
        goto err0;
    }
    if (vpp_creat_ion_mem(&dst, dst_format, out_w, out_h) == VPP_ERR) {
        goto err1;
    }

    src.is_pa = is_src_pa;
    dst.is_pa = is_dst_pa;

    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        goto err2;
    }

    if (dst_format == FMT_BGR)
    {
        ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
    }
    else
    {
        ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
        ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
        ion_invalidate(NULL, dst.va[2], dst.ion_len[2]);
    }

    vpp_resize(&src, &dst);

    ret = output_file(dst_format, dst_img_name, &dst);

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0: 
    return ret;
}

static int output_file(int format, char *file_name, vpp_mat *mat)
{
    int ret = 0;
    switch (format) 
    {
    case FMT_BGR:
        ret = vpp_bmp_bgr888(file_name, mat->va[0], mat->cols, mat->rows, mat->stride);
        break;
    case FMT_RGBP:
        ret |= vpp_bmp_gray("r.bmp", (unsigned char *) mat->va[0], mat->cols, mat->rows, mat->stride);
        ret |= vpp_bmp_gray("g.bmp", (unsigned char *) mat->va[1], mat->cols, mat->rows, mat->stride);
        ret |= vpp_bmp_gray("b.bmp", (unsigned char *) mat->va[2], mat->cols, mat->rows, mat->stride);
    default:
        ret = vpp_write_file(file_name, mat);
        break;
    }
    return ret;
}
