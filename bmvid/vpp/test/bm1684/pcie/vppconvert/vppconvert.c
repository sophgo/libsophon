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
    int src_fmt, dst_fmt;
    int convert_fmt,ret = VPP_OK;
    int width , height;
    vpp_mat src;
    vpp_mat dst;
    char *img_name;
    char *input_name;

    if (argc != 7)
    {
        printf("usage: %d\n", argc);
        printf("%s width height src_img_name  src-format dst-format dst_img_name\n", argv[0]);
        printf("format: 0 -- FMT_Y, 1 -- FMT_I420, 2 -- FMT_NV12, 3 -- FMT_BGR, 4 -- FMT_RGB, 5 -- FMT_RGBP, 6 -- FMT_BGRP\n");
        printf("7 -- FMT_YUV444P, 8 -- FMT_YUV422P,9 -- FMT_YUV444, 10 -- FMT_ABGR, 11 -- FMT_ARGB\n");
        printf("support path:\n");
        printf("FMT_Y             -> FMT_Y,FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP\n");
        printf("FMT_YUV444P       -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420,FMT_YUV444P,FMT_YUV422P\n");
        printf("FMT_RGB/FMT_BGR   -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420,FMT_YUV444P,FMT_YUV422P\n");
        printf("FMT_RGBP/FMT_BGRP -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420,FMT_YUV444P,FMT_YUV422P\n");
        printf("FMT_NV12          -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420\n");
        printf("FMT_I420          -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420\n");
        printf("FMT_YUV444        -> FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP\n");
        printf("example:\n");
        printf("FMT_BGR     -> FMT_I420:     %s 1920 1080 bgr-org-opencv.bin 3 1 i420.yuv\n", argv[0]);
        printf("FMT_RGBP    -> FMT_I420:     %s 1920 1080 rgbp.bin 5 1 i420.bin\n", argv[0]);
        printf("FMT_NV12    -> FMT_BGRP:     %s 1920 1080 img1.nv12 2 6 bgrp3.bin\n", argv[0]);
        printf("FMT_I420    -> FMT_BGR :     %s 1920 1080 img2.yuv 1 3 out.bmp\n", argv[0]);
        printf("FMT_BGR     -> FMT_YUV444P:  %s 1920 1080 bgr-org-opencv.bin 3 7 yuv444p \n", argv[0]);
        printf("FMT_YUV444P -> FMT_YUV422P:  %s 1920 1080 yuv444p.bin  7 8 yuv422p.bin \n", argv[0]);
        printf("FMT_YUV444  -> FMT_BGR:      %s 1920 1080  yuv444packed 9  3  bgr5.bmp \n", argv[0]);

        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    input_name = argv[3];
    src_fmt = atoi(argv[4]);
    dst_fmt = atoi(argv[5]);
    img_name = argv[6];

    int idx;
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

    vpp_creat_host_and_device_mem(dev_buffer_src, &src, src_fmt, width, height);
    vpp_creat_host_and_device_mem(dev_buffer_dst, &dst, dst_fmt, width, height);

    vpp_read_file_pcie(&src, dev_buffer_src, input_name);

    vpp_csc(&src, &dst);

    for (idx = 0; idx < dst.num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst.va[idx], dev_buffer_dst[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    ret = output_file(img_name, &dst);
    if(VPP_OK != ret)
        printf("output file wrong: ret = %d\n", ret);

    for (idx = 0; idx < src.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_src[idx]);
        free(src.va[idx]);
    }
    for (idx = 0; idx < dst.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_dst[idx]);
        free(dst.va[idx]);
    }

    return 0;
}
