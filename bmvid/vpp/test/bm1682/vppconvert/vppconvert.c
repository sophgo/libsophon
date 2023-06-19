#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

#define CONV_YUV420P_TO_BGR     ((FMT_I420) | (FMT_BGR << 4))
#define CONV_NV12_TO_BGR        ((FMT_NV12) | (FMT_BGR << 4))
#define CONV_YUV420P_TO_RGB     ((FMT_I420) | (FMT_RGB << 4))
#define CONV_NV12_TO_RGB        ((FMT_NV12) | (FMT_RGB << 4))
#define CONV_NV12_TO_YUV420P    ((FMT_NV12) | (FMT_I420 << 4))

int main(int argc, char *argv[])
{
    int src_fmt, dst_fmt;
    int convert_fmt;
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
        printf("    format: 0 -- yuv420p)\n");
        printf("            7 -- nv12)\n");
        printf("            5 -- bgr)\n");
        printf("            6 -- rgb)\n");
        printf("EX\n");
        printf("    yuv420p -> bgr:     %s 1920 1080 img2.yuv 0 0 0 5 out.bmp\n", argv[0]);
        printf("    nv12    -> bgr:     %s 1920 1080 img1.nv12 0 0 7 5 out.bmp\n", argv[0]);
        printf("    yuv420p -> rgb:     %s 1920 1080 img2.yuv 0 0 0 6 out.bmp\n", argv[0]);
        printf("    nv12    -> rgb:     %s 1920 1080 img1.nv12 0 0 7 6 out.bmp\n", argv[0]);
        printf("    nv12    -> yuv420p: %s 1920 1080 img1.nv12 0 0 7 0 out.yuv\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    input_name = argv[3];
    is_src_pa = atoi(argv[4]);
    is_dst_pa = atoi(argv[5]);

    src_fmt = atoi(argv[6]);
    dst_fmt = atoi(argv[7]);
    VppAssert((FMT_I420 == src_fmt) || (FMT_NV12 == src_fmt));
    VppAssert((FMT_BGR == dst_fmt) || (FMT_RGB == dst_fmt) || (FMT_I420== dst_fmt));

    img_name = argv[8];

    VppAssert((is_src_pa == 0) || (is_src_pa == 1));
    VppAssert((is_dst_pa == 0) || (is_dst_pa == 1));

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

    convert_fmt = src_fmt | (dst_fmt << 4);

    switch (convert_fmt)
    {
        case CONV_YUV420P_TO_BGR:
        case CONV_NV12_TO_BGR:
        case CONV_YUV420P_TO_RGB:
        case CONV_NV12_TO_RGB:
            ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);

            vpp_yuv420_to_rgb(&src, &dst, 0);

            vpp_bmp_bgr888(img_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);
            break;
        case CONV_NV12_TO_YUV420P:
            ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);
            ion_invalidate(NULL, dst.va[1], dst.ion_len[1]);
            ion_invalidate(NULL, dst.va[2], dst.ion_len[2]);

            vpp_nv12_to_yuv420p(&src, &dst, 0);

            vpp_output_mat_to_yuv420(img_name, &dst);
            break;
        default:
            printf("not supported: src_fmt = %d, dest_fmt = %d\n", src_fmt, dst_fmt);
            break;
    }

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0:
    return 0;
}
