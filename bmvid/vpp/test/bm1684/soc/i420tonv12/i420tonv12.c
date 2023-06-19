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
    int w = 0, h = 0, stride = 0;
    int stride_uv = 0,w_uv = 0, i = 0;
    FILE *fp_yuv420p, *fp_nv12;
    int len_yuv420p = 0;
    void *p_yuv420p, *p_nv12;

    if (argc != 5) {
        printf("usage: %d\n", argc);
        printf("%s width height src_image dst_image\n", argv[0]);
        printf("example:\n");
        printf("I420-> NV12:%s 1920 1080 img2.yuv  out.nv12\n", argv[0]);
        return 0;
    }

    w = atoi(argv[1]);    /*src image w*/
    h = atoi(argv[2]);    /*src image h*/
    fp_yuv420p = fopen(argv[3], "rb");
    fp_nv12 = fopen(argv[4], "wb");

    fseek(fp_yuv420p, 0, SEEK_END);
    len_yuv420p = ftell(fp_yuv420p);

    if(len_yuv420p != (w * h * 3 / 2))
    {
            VppErr("errno = %d, msg: %s\n", errno, strerror(errno));
    }
    fseek(fp_yuv420p, 0, SEEK_SET);
    stride = VPP_ALIGN(w, STRIDE_ALIGN);

    p_yuv420p = malloc(stride * h * 3 / 2);
    p_nv12 = malloc(stride * h * 3 / 2);


    for (i = 0; i < h; i++)
    {
        fread(p_yuv420p + i*stride, 1, w, fp_yuv420p);
    }

    w_uv = w >> 1;
    stride_uv = stride >> 1;

    for (i = 0; i < h; i++)
    {
        fread(p_yuv420p +stride * h +i*stride_uv, 1, w_uv, fp_yuv420p);
    }

    i420tonv12(p_yuv420p,w,h, p_nv12,stride);

    for (i = 0; i < h * 3 / 2; i++)
    {
        if (fwrite(p_nv12+i*stride, 1, w, fp_nv12) != w)
        {
            VppErr("errno = %d, msg: %s\n", errno, strerror(errno));
        }
    }

    free(p_yuv420p);
    free(p_nv12);
    fclose(fp_yuv420p);
    fclose(fp_nv12);
    return 0;
}

