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
    int width, height, idx;
    int is_src_pa, is_dst_pa;
    vpp_rect rect[3];
    vpp_mat src[3];
    vpp_mat dst[3];
    char *input_name[3];
    char *crop0_name;
    char *crop1_name;
    char *crop2_name;

    if (argc != 11)
    {
        printf("usage: %d\n", argc);
        printf("%s width height in_bgr24_0.bin in_bgr24_1.bin in_bgr24_2.bin is_src_pa is_dst_pa dst_crop0_name dst_crop1_name dst_crop2_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv.bin bgr-org-opencv-10.bin bgr-org-opencv-222.bin 1 1 out0.bmp out1.bmp out2.bmp\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    input_name[0] = argv[3];
    input_name[1] = argv[4];
    input_name[2] = argv[5];

    is_src_pa    = atoi(argv[6]);
    is_dst_pa    = atoi(argv[7]);

    crop0_name = argv[8];
    crop1_name = argv[9];
    crop2_name = argv[10];

    if(!((is_src_pa == 0) || (is_src_pa == 1))) {
        printf("is_src_pa=%d input error.\n", is_src_pa);
        return 0;
    }
    if(!((is_dst_pa == 0) || (is_dst_pa == 1))) {
        printf("is_dst_pa=%d input error.\n", is_dst_pa);
        return 0;
    }

    if (vpp_creat_ion_mem(&src[0], FMT_BGR, width, height) == VPP_ERR)
    {
        goto err0;
    }
    src[0].is_pa = is_src_pa;

    if (vpp_creat_ion_mem(&src[1], FMT_BGR, width, height) == VPP_ERR)
    {
        vpp_free_ion_mem(&src[0]);
        goto err0;
    }
    src[1].is_pa = is_src_pa;

    if (vpp_creat_ion_mem(&src[2], FMT_BGR, width, height) == VPP_ERR)
    {
        vpp_free_ion_mem(&src[0]);
        vpp_free_ion_mem(&src[1]);
        goto err0;
    }
    src[2].is_pa = is_src_pa;

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
    rect[2].width = 712;
    rect[2].height= 688;


    if (vpp_creat_ion_mem(&dst[0], FMT_BGR, rect[0].width, rect[0].height) == VPP_ERR) {
        goto err1;
    }
    dst[0].is_pa = is_dst_pa;

    if (vpp_creat_ion_mem(&dst[1], FMT_BGR, rect[1].width, rect[1].height) == VPP_ERR) {
        vpp_free_ion_mem(&dst[0]);
        goto err1;
    }
    dst[1].is_pa = is_dst_pa;

    if (vpp_creat_ion_mem(&dst[2], FMT_BGR, rect[2].width, rect[2].height) == VPP_ERR) {
        vpp_free_ion_mem(&dst[0]);
        vpp_free_ion_mem(&dst[1]);
        goto err1;
    }
    dst[2].is_pa = is_dst_pa;

    for (idx = 0; idx < 3; idx++)
    {
        if (vpp_read_file(&src[idx], NULL, input_name[idx]) < 0)
        {
            goto err2;
        }
    }

    ion_flush(NULL, src[0].va[0], src[0].ion_len[0]);
    ion_flush(NULL, src[1].va[0], src[1].ion_len[0]);
    ion_flush(NULL, src[2].va[0], src[2].ion_len[0]);

    ion_invalidate(NULL, dst[0].va[0], dst[0].ion_len[0]);
    ion_invalidate(NULL, dst[1].va[0], dst[1].ion_len[0]);
    ion_invalidate(NULL, dst[2].va[0], dst[2].ion_len[0]);

    vpp_crop_multi(src, rect, dst, 3);

    vpp_bmp_bgr888(crop0_name, (unsigned char *)dst[0].va[0], dst[0].cols, dst[0].rows, dst[0].stride);
    vpp_bmp_bgr888(crop1_name, (unsigned char *)dst[1].va[0], dst[1].cols, dst[1].rows, dst[1].stride);
    vpp_bmp_bgr888(crop2_name, (unsigned char *)dst[2].va[0], dst[2].cols, dst[2].rows, dst[2].stride);

err2:
    for (idx = 0; idx < 3; idx++)
        vpp_free_ion_mem(&dst[idx]);
err1:
    for (idx = 0; idx < 3; idx++)
        vpp_free_ion_mem(&src[idx]);
err0:
    return 0;
}
