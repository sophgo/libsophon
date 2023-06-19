#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

int main(int argc, char *argv[])
{
    int src_fmt, dst_fmt;
    int convert_fmt,ret = VPP_OK;
    int width , height;
    int is_src_pa, is_dst_pa;
    vpp_mat src;
    vpp_mat dst;
    char *img_name;
    char *input_name;

    if (argc != 9)
    {
        printf("usage: %d\n", argc);
        printf("%s width height src_img_name is_src_pa is_dst_pa src-format dst-format dst_img_name\n", argv[0]);
        printf("format: 0 -- FMT_Y, 1 -- FMT_I420, 2 -- FMT_NV12, 3 -- FMT_BGR, 4 -- FMT_RGB, 5 -- FMT_RGBP, 6 -- FMT_BGRP\n");
        printf("7 -- FMT_YUV444P, 8 -- FMT_YUV422P,9 -- FMT_YUV444\n");
        printf("support path:\n");
        printf("FMT_Y             -> FMT_Y\n");
        printf("FMT_YUV444P       -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420,FMT_YUV444P,FMT_YUV422P\n");
        printf("FMT_RGB/FMT_BGR   -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420,FMT_YUV444P,FMT_YUV422P\n");
        printf("FMT_RGBP/FMT_BGRP -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420,FMT_YUV444P,FMT_YUV422P\n");
        printf("FMT_NV12          -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420\n");
        printf("FMT_I420          -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420\n");
        printf("FMT_YUV444        -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP\n");
        printf("example:\n");
        printf("FMT_BGR     -> FMT_I420:     %s 1920 1080 bgr-org-opencv.bin 1 1 3 1 i420.yuv\n", argv[0]);
        printf("FMT_RGBP    -> FMT_I420:     %s 1920 1080 rgbp.bin 1 1  5 1 i420.bin\n", argv[0]);
        printf("FMT_NV12    -> FMT_BGRP:     %s 1920 1080 img1.nv12 1 1 2 6 bgrp3.bin\n", argv[0]);
        printf("FMT_I420    -> FMT_BGR :     %s 1920 1080 img2.yuv 0 0 1 3 out.bmp\n", argv[0]);
        printf("FMT_BGR     -> FMT_YUV444P:  %s 1920 1080 bgr-org-opencv.bin 1 1 3 7 yuv444p \n", argv[0]);
        printf("FMT_YUV444P -> FMT_YUV422P:  %s 1920 1080 yuv444p.bin  1 1 7 8 yuv422p.bin \n", argv[0]);
        printf("FMT_YUV444  -> FMT_BGR:      %s 1920 1080 yuv444packed   1 1  9  3  bgr5.bmp \n", argv[0]);

        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    input_name = argv[3];
    is_src_pa = atoi(argv[4]);
    is_dst_pa = atoi(argv[5]);
    src_fmt = atoi(argv[6]);
    dst_fmt = atoi(argv[7]);
    img_name = argv[8];

    if(!((is_src_pa == 0) || (is_src_pa == 1))) {
        printf("is_src_pa=%d input error.\n", is_src_pa);
        return 0;
    }
    if(!((is_dst_pa == 0) || (is_dst_pa == 1))) {
        printf("is_dst_pa=%d input error.\n", is_dst_pa);
        return 0;
    }

    if (vpp_creat_ion_mem(&src, src_fmt, width, height) == VPP_ERR) {
        goto err0;
    }
    if (vpp_creat_ion_mem(&dst, dst_fmt, width, height) == VPP_ERR) {
        goto err1;
    }
    src.is_pa = is_src_pa;
    dst.is_pa = is_dst_pa;

    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        goto err2;
    }

    vpp_csc(&src, &dst);
    ret = output_file(img_name, &dst);
    if(VPP_OK != ret)
        printf("output file wrong: ret = %d\n", ret);

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0:
    return 0;
}
