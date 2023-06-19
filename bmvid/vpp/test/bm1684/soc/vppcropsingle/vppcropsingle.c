#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

#define VPP_CROP_NUM 256
int main(int argc, char *argv[])
{
    int idx = 0,src_fmt = 3;
    int is_src_pa, is_dst_pa;
    vpp_rect rect[VPP_CROP_NUM];
    vpp_mat src;
    vpp_mat dst[VPP_CROP_NUM];
    char *crop_name;
    char  output_name[16];
    int width, height;
    char *input_name;

    if (argc != 8) {
        printf("usage: %d\n", argc);
        printf("%s width height src_img_name src_fmt is_src_pa is_dst_pa dst_crop_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv-bmp3.bin 3 1 1  crop \n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    input_name = argv[3];
    src_fmt = atoi(argv[4]);
    is_src_pa    = atoi(argv[5]);
    is_dst_pa    = atoi(argv[6]);
    crop_name = argv[7];

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
    src.is_pa = is_src_pa;
    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        goto err2;
    }
    ion_flush(NULL, src.va[0], src.ion_len[0]);

    for (idx = 0; idx < VPP_CROP_NUM; idx++) {
        rect[idx].x = idx * 2;
        rect[idx].y = idx * 2;
        rect[idx].width = 108 + idx * 5;
        rect[idx].height= 54 + idx * 2;

        if (vpp_creat_ion_mem(&dst[idx], FMT_BGR, rect[idx].width, rect[idx].height) == VPP_ERR) {
            goto err1;
        }
        dst[idx].is_pa = is_dst_pa;
        ion_invalidate(NULL, dst[idx].va[0], dst[idx].ion_len[0]);
    }

    vpp_crop_single(&src, rect, dst, VPP_CROP_NUM);

    for (idx = 0; idx < VPP_CROP_NUM; idx++) {
        sprintf(output_name,"%s%d.bmp",crop_name,idx);
        vpp_bmp_bgr888(output_name, (unsigned  char *)dst[idx].va[0], dst[idx].cols, dst[idx].rows, dst[idx].stride);
    }

err2:
    for (idx = 0; idx < VPP_CROP_NUM; idx++)
        vpp_free_ion_mem(&dst[idx]);
err1:
    vpp_free_ion_mem(&src);
err0:

    return 0;
}
