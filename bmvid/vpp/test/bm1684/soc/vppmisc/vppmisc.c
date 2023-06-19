#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"
#include <sys/time.h>

#define VPP_CROP_NUM 16
int main(int argc, char *argv[])
{
    int idx = 0;
    int is_src_pa, is_dst_pa;
    vpp_rect rect[VPP_CROP_NUM];
    vpp_mat src[VPP_CROP_NUM];
    vpp_mat dst[VPP_CROP_NUM];
    char *crop_name;
    char  output_name[16];
    int input_width, input_height;
    char *input_name;
    int src_fmt, dst_fmt;

    if (argc != 7)
    {
        printf("usage: %d\n", argc);
        printf("%s input_width input_height src_img_name  src-format dst_img_name dst-format\n", argv[0]);
        printf("example:\n");
        printf("    FMT_NV12  -> FMT_BGR:  %s 1920 1080 img1.nv12  2 crop 3\n", argv[0]);
        return 0;
    }

    input_width = atoi(argv[1]);
    input_height = atoi(argv[2]);
    input_name = argv[3];
    src_fmt    = atoi(argv[4]);
    crop_name = argv[5];
    dst_fmt    = atoi(argv[6]);

    if (vpp_creat_ion_mem(&src[0], FMT_NV12, input_width, input_height) == VPP_ERR) {
        goto err0;
    }

    if (vpp_read_file(&src[0], NULL, input_name) < 0)
    {
        goto err2;
    }
    ion_flush(NULL, src[0].va[0], src[0].ion_len[0]);
    ion_flush(NULL, src[0].va[1], src[0].ion_len[1]);

    for (idx = 0; idx < VPP_CROP_NUM; idx++) {
        src[idx] =src[0];
        rect[idx].x = idx * 2;
        rect[idx].y = idx * 2;
        rect[idx].width = 1020+ idx * 5;
        rect[idx].height= 914 + idx * 2;

        if (vpp_creat_ion_mem(&dst[idx], FMT_BGR, (1640 + idx * 5), (864 + idx * 2)) == VPP_ERR) {
            goto err1;
        }
        ion_invalidate(NULL, dst[idx].va[0], dst[idx].ion_len[0]);
    }

    vpp_misc(src, rect, dst, VPP_CROP_NUM, CSC_MAX, VPP_SCALE_BILINEAR);

    for (idx = 0; idx < VPP_CROP_NUM; idx++) {
        sprintf(output_name,"%s%d.bmp",crop_name,idx);
        vpp_bmp_bgr888(output_name, (unsigned  char *)dst[idx].va[0], dst[idx].cols, dst[idx].rows, dst[idx].stride);
    }

err2:
    for (idx = 0; idx < VPP_CROP_NUM; idx++)
        vpp_free_ion_mem(&dst[idx]);
err1:
    vpp_free_ion_mem(&src[0]);
err0:
    return 0;
}
