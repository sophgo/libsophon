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
    int vpp_dev_fd;
    ion_dev_fd_s ion_dev_fd;
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

    VppAssert((is_src_pa == 0) || (is_src_pa == 1));
    VppAssert((is_dst_pa == 0) || (is_dst_pa == 1));

    ion_dev_fd.dev_fd = open("/dev/ion", O_RDWR | O_DSYNC);
    if (ion_dev_fd.dev_fd < 0) {
        VppErr("open /dev/ion failed, errno = %d, msg: %s\n", errno, strerror(errno));
        return VPP_ERR;
    }
    memcpy(ion_dev_fd.name,"ion",sizeof("ion"));

    vpp_dev_fd = open("/dev/bm-vpp", O_RDWR | O_DSYNC);
    if (vpp_dev_fd < 0) {
        VppErr("open /dev/bm-vpp failed, errno = %d, msg: %s\n", errno, strerror(errno));
        return VPP_ERR;
    }

    src.vpp_fd.dev_fd = vpp_dev_fd;
    src.vpp_fd.name = "bm-vpp";

    if (vpp_creat_ion_mem_fd(&src, FMT_BGR, in_w, in_h, &ion_dev_fd) == VPP_ERR) {
        goto err0;
    }
    src.is_pa = is_src_pa;

    if (vpp_creat_ion_mem_fd(&dst, FMT_BGR, in_w + left + right, in_h + top + bottom, &ion_dev_fd) == VPP_ERR) {
        goto err1;
    }
    memset(dst.va[0],0,dst.ion_len[0]);

    src.is_pa = is_src_pa;
    dst.is_pa = is_dst_pa;

    if (vpp_read_file(&src, &ion_dev_fd, input_name) < 0)
    {
        goto err2;
    }

    ion_invalidate(&ion_dev_fd, dst.va[0], dst.ion_len[0]);

    vpp_border(&src, &dst, top, bottom, left, right);

    vpp_bmp_bgr888(img_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

err2:
    vpp_free_ion_mem(&dst);
err1:
    vpp_free_ion_mem(&src);
err0:
    close(ion_dev_fd.dev_fd);
    close(vpp_dev_fd);
    return 0;
}
