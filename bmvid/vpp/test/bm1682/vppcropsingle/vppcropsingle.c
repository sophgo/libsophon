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
    int idx = 0;
    int is_src_pa, is_dst_pa;
    vpp_rect rect[3];
    vpp_mat src;
    vpp_mat dst[3];
    char *crop0_name;
    char *crop1_name;
    char *crop2_name;
    int width, height;
    char *input_name;
    int src_fmt, dst_fmt;

    if (argc != 11) {
        printf("usage: %d\n", argc);
        printf("%s width height src_fmt in_bgr24.bin is_src_pa is_dst_pa dst fmt dst_crop0_name dst_crop1_name dst_crop2_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 5 bgr-org-opencv-bmp3.bin 1 1  5 crop0.bmp crop1.bmp crop2.bmp\n", argv[0]);
        printf("        %s 1920 1080 5 bgr-org-opencv-bmp3.bin 1 1  9 crop0.bmp crop1.bmp crop2.bmp\n", argv[0]);
        printf("        %s 1920 1080 0 img2.yuv 1 1  9 crop0.bmp crop1.bmp crop2.bmp\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    src_fmt = atoi(argv[3]);
    input_name = argv[4];

    is_src_pa    = atoi(argv[5]);
    is_dst_pa    = atoi(argv[6]);
    dst_fmt = atoi(argv[7]);
    crop0_name = argv[8];
    crop1_name = argv[9];
    crop2_name = argv[10];

    VppAssert((is_src_pa == 0) || (is_src_pa == 1));
    VppAssert((is_dst_pa == 0) || (is_dst_pa == 1));

    if (vpp_creat_ion_mem(&src, src_fmt, width, height) == VPP_ERR) {
        goto err0;
    }
    src.is_pa = is_src_pa;

    rect[0].x = 0;
    rect[0].y = 0;
    rect[0].width = 1000;
    rect[0].height= 1000;

    rect[1].x = 128;
    rect[1].y = 128;
    rect[1].width = 870;
    rect[1].height= 300;

    rect[2].x = 256;
    rect[2].y = 256;
    rect[2].width = 724;
    rect[2].height= 812;

    if (vpp_creat_ion_mem(&dst[0], dst_fmt, rect[0].width, rect[0].height) == VPP_ERR) {
        goto err1;
    }
    dst[0].is_pa = is_dst_pa;

    if (vpp_creat_ion_mem(&dst[1], dst_fmt, rect[1].width, rect[1].height) == VPP_ERR) {
        vpp_free_ion_mem(&dst[0]);
        goto err1;
    }
    dst[1].is_pa = is_dst_pa;

    if (vpp_creat_ion_mem(&dst[2], dst_fmt, rect[2].width, rect[2].height) == VPP_ERR) {
        vpp_free_ion_mem(&dst[0]);
        vpp_free_ion_mem(&dst[1]);
        goto err1;
    }
    dst[2].is_pa = is_dst_pa;

    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        goto err2;
    }
    if ((dst_fmt == FMT_BGRP) || (dst_fmt == FMT_RGBP))
    {
        ion_invalidate(NULL, dst[0].va[0], dst[0].ion_len[0]);
        ion_invalidate(NULL, dst[0].va[1], dst[0].ion_len[1]);
        ion_invalidate(NULL, dst[0].va[2], dst[0].ion_len[2]);
        ion_invalidate(NULL, dst[1].va[0], dst[1].ion_len[0]);
        ion_invalidate(NULL, dst[1].va[1], dst[1].ion_len[1]);
        ion_invalidate(NULL, dst[1].va[2], dst[1].ion_len[2]);
        ion_invalidate(NULL, dst[2].va[0], dst[2].ion_len[0]);
        ion_invalidate(NULL, dst[2].va[1], dst[2].ion_len[1]);
        ion_invalidate(NULL, dst[2].va[2], dst[2].ion_len[2]);
    }
    if ((dst_fmt == FMT_RGB) || (dst_fmt == FMT_BGR))
    {
        ion_invalidate(NULL, dst[0].va[0], dst[0].ion_len[0]);
        ion_invalidate(NULL, dst[1].va[0], dst[1].ion_len[0]);
        ion_invalidate(NULL, dst[2].va[0], dst[2].ion_len[0]);
    }

    vpp_crop_single(&src, rect, dst, 3);

    if (dst_fmt == FMT_BGR)
    {
    vpp_bmp_bgr888(crop0_name, (unsigned  char *)dst[0].va[0], dst[0].cols, dst[0].rows, dst[0].stride);
    vpp_bmp_bgr888(crop1_name, (unsigned  char *)dst[1].va[0], dst[1].cols, dst[1].rows, dst[1].stride);
    vpp_bmp_bgr888(crop2_name, (unsigned  char *)dst[2].va[0], dst[2].cols, dst[2].rows, dst[2].stride);
    }
    if ((dst_fmt == FMT_BGRP) || (dst_fmt == FMT_RGBP))
    {
    vpp_bmp_gray(crop0_name, (unsigned  char *)dst[0].va[0], dst[0].cols, dst[0].rows, dst[0].stride);
    vpp_bmp_gray(crop1_name, (unsigned  char *)dst[1].va[0], dst[1].cols, dst[1].rows, dst[1].stride);
    vpp_bmp_gray(crop2_name, (unsigned  char *)dst[2].va[0], dst[2].cols, dst[2].rows, dst[2].stride);
    }

err2:
    for (idx = 0; idx < 3; idx++)
        vpp_free_ion_mem(&dst[idx]);
err1:
    vpp_free_ion_mem(&src);
err0:
    return 0;
}
