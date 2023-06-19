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
    int is_src_pa, is_dst_pa;
    vpp_mat src;
    vpp_mat dst;
    char *input_name;
    int in_w, in_h;
    int top;
    int bottom;
    int left;
    int right;
    char *img_name;

    if (argc != 11) {
        printf("usage: %d\n", argc);
        printf("%s width height bgr-org-opencv-bmp3.bin top bottom left right is_src_pa is_dst_pa dst_img_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv-bmp3.bin 32 32 32 32 0 0 out.bmp\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);
    in_h = atoi(argv[2]);

    input_name = argv[3];
    top = atoi(argv[4]);
    bottom = atoi(argv[5]);
    left = atoi(argv[6]);
    right = atoi(argv[7]);
    is_src_pa    = atoi(argv[8]);
    is_dst_pa    = atoi(argv[9]);
    img_name = argv[10];

    if(!((is_src_pa == 0) || (is_src_pa == 1))) {
        printf("is_src_pa=%d input error.\n", is_src_pa);
        return 0;
    }
    if(!((is_dst_pa == 0) || (is_dst_pa == 1))) {
        printf("is_dst_pa=%d input error.\n", is_dst_pa);
        return 0;
    }

    if (vpp_creat_ion_mem(&src, FMT_BGR, in_w, in_h) == VPP_ERR) {
        goto err0;
    }
    src.is_pa = is_src_pa;

    if (vpp_creat_ion_mem(&dst, FMT_BGR, in_w + left + right, in_h + top + bottom) == VPP_ERR) {
        goto err1;
    }
    dst.is_pa = is_dst_pa;
    memset(dst.va[0], 0, dst.ion_len[0]);

    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        goto err2;
    }

    ion_flush(NULL, src.va[0], src.ion_len[0]);
    ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);

    vpp_border(&src, &dst, top, bottom, left, right);

    vpp_bmp_bgr888(img_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0:
    return 0;
}
