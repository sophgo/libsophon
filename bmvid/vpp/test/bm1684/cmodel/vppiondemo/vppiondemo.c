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
    int input_width , input_height, output_width, output_height;
    vpp_mat src, dst;
    char *output_name;
    char *input_name;

    if (argc != 9)
    {
        printf("usage: %d\n", argc);
        printf("%s input_width input_height src_img_name  src-format  output_width output_height dst_img_name dst-format\n", argv[0]);
        printf("example:\n");
        printf("    FMT_NV12  -> FMT_BGR:  %s 1920 1080 img1.nv12  2 1540 864 out.bmp 3\n", argv[0]);
        return 0;
    }

    input_width   = atoi(argv[1]);
    input_height  = atoi(argv[2]);
    input_name    = argv[3];
    src_fmt      = atoi(argv[4]);
    output_width  = atoi(argv[5]);
    output_height = atoi(argv[6]);
    output_name  = argv[7];
    dst_fmt      = atoi(argv[8]);
    if(dst_fmt != 3) {
        printf("dst_fmt=%d input error.\n", dst_fmt);
        return 0;
    }

    memset(&src,0,sizeof(src));
    memset(&dst,0,sizeof(dst));
    src.format = src_fmt;/*data format*/
    src.axisX = 0;/*X-axis of cropping source starting point , if not crop ,set 0*/
    src.axisY = 0;/*Y-axis of cropping source starting point, if not crop ,set 0*/
    src.cols = input_width;    /*image width*/
    src.rows = input_height;    /*image height*/
    src.is_pa = 1; /*use physical addresses*/
    src.stride = VPP_ALIGN(src.cols, STRIDE_ALIGN); /*stride must 64B align for vpp hw*/
    src.uv_stride = 0;
    src.ion_len[0] = src.stride * src.rows;    /*Y data length*/
    src.ion_len[1] = src.stride * (src.rows >> 1);    /*cbcr data length*/

    /*alloc ion mem.u can use vpp_ion_malloc() or vpp_ion_malloc to get ion mem*/
    src.va[0] = malloc((src.ion_len[0] + src.ion_len[1])); /*Y data Virtual address*/
    if (src.va[0] == NULL) {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err0;
    }
    src.va[1] = src.va[0] + src.ion_len[0];    /*cbcr data Virtual address*/

    dst.format = dst_fmt;                                     /*Must fill*/
    dst.axisX = 0;                                            /*Must fill*/
    dst.axisY = 0;                                            /*Must fill*/
    dst.cols = output_width;                                   /*Must fill*/
    dst.rows = output_height;                                  /*Must fill*/
    dst.is_pa = 1;                                            /*Must fill*/
    dst.stride = VPP_ALIGN(dst.cols * 3, STRIDE_ALIGN);        /*Must fill*/
    dst.uv_stride = 0;
    dst.ion_len[0] = dst.stride * dst.rows;                    /*Selective filling*/
    dst.va[0] = malloc(dst.ion_len[0]);  /*Selective filling*/
    if (dst.va[0] == NULL) {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err1;
    }

    if (vpp_read_file(&src, NULL, input_name) < 0)
    {
        goto err2;
    }

    vpp_resize_csc(&src, &dst);

    vpp_bmp_bgr888(output_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

err2:
    free(dst.va[0]);
err1:
    free(src.va[0]);
err0:
    return 0;
}
