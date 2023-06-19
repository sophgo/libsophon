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
    int ret = -1;
    int src_format = 0, dst_format = 0;
    char *input_name;
    int in_w;
    int in_h;
    int out_w;
    int out_h;
    char* dst_img_name;
    int is_src_pa, is_dst_pa;
    int idx;
    vpp_mat src;
    vpp_mat dst;

    if (argc != 10)
    {
        printf("usage: %d\n", argc);
        printf("%s in_w in_h input_name src_format is_src_pa is_dst_pa  out_w out_h dst_img_name\n", argv[0]);
        printf("note: 1. The dst format must be the same as the src format\n");
        printf("support path:\n");
        printf("FMT_Y       -> FMT_Y,    FMT_BGR   -> FMT_BGR,  FMT_RGB   -> FMT_RGB\n");
        printf("FMT_RGBP    -> FMT_RGBP, FMT_BGRP  -> FMT_BGRP, FMT_I420   > FMT_I420\n");
        printf("FMT_YUV444P -> FMT_YUV444P \n");
        printf("example:\n");
        printf("%s 1920 1080 img2.yuv 1 1 1 954 488 i420.yuv\n", argv[0]);
        printf("%s 1920 1080 bgr-org-opencv.bin  3 1 1 877 1011 bgr2.bmp \n", argv[0]);
        printf("%s 1920 1080 bgrp.bin 6 1 1 1386 674 bgrp2.bin\n", argv[0]);
        printf("%s 1920 1080 yuv444p.bin 7 1 1 2731 1673 yuv.444p \n", argv[0]);
        return ret;
    }

    in_w         = atoi(argv[1]);
    in_h         = atoi(argv[2]);
    input_name   = argv[3];
    src_format   = atoi(argv[4]);
    is_src_pa    = atoi(argv[5]);
    is_dst_pa    = atoi(argv[6]);
    out_w        = atoi(argv[7]);
    out_h        = atoi(argv[8]);
    dst_img_name = argv[9];
    dst_format = src_format;

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

    switch (src_format)
    {
        case FMT_Y:
        case FMT_BGR:
        case FMT_RGB:
             ion_flush(NULL, src.va[0], src.ion_len[0]);
             ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
             break;
        case FMT_NV12:
             ion_flush(NULL, src.va[0], src.ion_len[0]);
             ion_flush(NULL, src.va[1], src.ion_len[1]);
             ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
             ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
             break;
        case FMT_I420:
        case FMT_BGRP:
        case FMT_RGBP:
        case FMT_YUV444P:
             ion_flush(NULL, src.va[0], src.ion_len[0]);
             ion_flush(NULL, src.va[1], src.ion_len[1]);
             ion_flush(NULL, src.va[2], src.ion_len[2]);
             ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
             ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
             ion_invalidate(NULL, dst.va[2], dst.ion_len[2]);
             break;
        default:
            printf("not supported: src_fmt = %d\n", src_format);
            break;
    }

    vpp_resize(&src, &dst);

    ret = output_file(dst_img_name, &dst);

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0: 
    return ret;
}

