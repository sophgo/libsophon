#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "vpplib.h"
#if !defined _WIN32 
#include <unistd.h>
#include <sys/ioctl.h>
#include "vppion.h"
#endif
int main(int argc, char *argv[])
{
    int ret = -1;
    int src_format = 0, dst_format = 0;
    char *input_name;
    int in_w;
    int in_h;
    int out_w;
    int out_h;
    char* dst_img_name;
    int idx;
    vpp_mat src;
    vpp_mat dst;

    if (argc != 8)
    {
        printf("usage: %d\n", argc);
        printf("%s in_w in_h input_name src_format  out_w out_h dst_img_name\n", argv[0]);
        printf("note: 1. The dst format must be the same as the src format\n");
        printf("support path:\n");
        printf("FMT_Y       -> FMT_Y,    FMT_BGR   -> FMT_BGR,  FMT_RGB   -> FMT_RGB\n");
        printf("FMT_RGBP    -> FMT_RGBP, FMT_BGRP  -> FMT_BGRP, FMT_I420   > FMT_I420\n");
        printf("FMT_YUV444P -> FMT_YUV444P \n");
        printf("example:\n");
        printf("%s 1920 1080 img2.yuv 1 954 488 i420.yuv\n", argv[0]);
        printf("%s 1920 1080 bgr-org-opencv.bin 3  877 1011 bgr2.bmp \n", argv[0]);
        printf("%s 1920 1080 bgrp.bin 6 1386 674 bgrp2.bin\n", argv[0]);
        printf("%s 1920 1080 yuv444p.bin 7  2731 1673 yuv.444p \n", argv[0]);
        return ret;
    }

    in_w         = atoi(argv[1]);
    in_h         = atoi(argv[2]);
    input_name   = argv[3];
    src_format   = atoi(argv[4]);
    out_w        = atoi(argv[5]);
    out_h        = atoi(argv[6]);
    dst_img_name = argv[7];
    dst_format = src_format;

    bm_handle_t handle = NULL;

    bm_device_mem_t dev_buffer_src[3];
    bm_device_mem_t dev_buffer_dst[3];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }
    src.vpp_fd.handle = handle;
    dst.vpp_fd.handle = handle;

    vpp_creat_host_and_device_mem(dev_buffer_src, &src, src_format, in_w, in_h);
    vpp_creat_host_and_device_mem(dev_buffer_dst, &dst, dst_format, out_w, out_h);

    vpp_read_file_pcie(&src, dev_buffer_src, input_name);

    vpp_resize(&src, &dst);

    for (idx = 0; idx < dst.num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst.va[idx], dev_buffer_dst[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }
    ret = output_file(dst_img_name, &dst);

    for (idx = 0; idx < src.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_src[idx]);
        free(src.va[idx]);
    }
    for (idx = 0; idx < dst.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_dst[idx]);
        free(dst.va[idx]);
    }

    return ret;
}

