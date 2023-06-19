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
    vpp_mat src;
    vpp_mat dst;
    char * ch_b_name;
    char * ch_g_name;
    char * ch_r_name;
    char *input_name;

    int width, height;
    int is_src_pa, is_dst_pa;

    if (argc != 9)
    {
        printf("usage: %d\n", argc);
        printf("usage: %d\n", argc);
        printf("%s width height in_bgr24.bin is_src_pa is_dst_pa ch_r_name ch_g_name ch_b_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv.bin 0 0 r.bmp g.bmp b.bmp\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    input_name = argv[3];
    is_src_pa = atoi(argv[4]);
    is_dst_pa = atoi(argv[5]);
    ch_b_name = argv[6];
    ch_g_name = argv[7];
    ch_r_name = argv[8];

    VppAssert((is_src_pa == 0) || (is_src_pa == 1));
    VppAssert((is_dst_pa == 0) || (is_dst_pa == 1));

    if (vpp_creat_ion_mem(&src, FMT_BGR, width, height) == VPP_ERR) {
        goto err0;
    }
    if (vpp_creat_ion_mem(&dst, FMT_RGBP, width, height) == VPP_ERR) {
        goto err1;
    }

    src.is_pa = is_src_pa; 
    dst.is_pa = is_dst_pa;

    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        printf("vpp_read_file failed, src\n");
        goto err2;
    }

    ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
    ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
    ion_invalidate(NULL, dst.va[2], dst.ion_len[2]);

    vpp_split_to_rgb(&src, &dst);

    vpp_bmp_gray(ch_b_name, (unsigned char *) dst.va[0], dst.cols, dst.rows, dst.stride);
    vpp_bmp_gray(ch_g_name, (unsigned char *) dst.va[1], dst.cols, dst.rows, dst.stride);
    vpp_bmp_gray(ch_r_name, (unsigned char *) dst.va[2], dst.cols, dst.rows, dst.stride);

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0:  
    return 0;
}
