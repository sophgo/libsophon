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
    int input_width , input_height, output_width, output_height;
    vpp_mat src, dst;
    char *output_name;
    char *input_name;
    int src_len, idx;

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

    bm_handle_t handle = NULL;
    bm_device_mem_t dev_buffer_src;
    bm_device_mem_t dev_buffer_dst;

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

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
    src_len = src.ion_len[0] + src.ion_len[1];
    src.vpp_fd.handle = handle;
//  printf("src_len %d\n", src_len);

    ret = bm_malloc_device_byte_heap(handle, &dev_buffer_src, DDR_CH, src_len);
        if (ret != BM_SUCCESS) {
            VppErr("malloc device memory size = %d failed, ret = %d\n", src_len, ret);
    }

    src.pa[0] = dev_buffer_src.u.device.device_addr;
    src.va[0] = malloc(src_len);
    src.va[1] = (char *)src.va[0] + src.ion_len[0];    /*cbcr data Virtual address*/
    src.pa[1] = src.pa[0] + src.ion_len[0];    /*cbcr data Physical address*/

    FILE *fp0 = fopen(input_name, "rb");
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
//    printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);
    fread(src.va[0], 1, len0, fp0);

    dst.format = dst_fmt;                                     /*Must fill*/
    dst.axisX = 0;                                            /*Must fill*/
    dst.axisY = 0;                                            /*Must fill*/
    dst.cols = output_width;                                   /*Must fill*/
    dst.rows = output_height;                                  /*Must fill*/
    dst.is_pa = 1;                                            /*Must fill*/
    dst.stride = VPP_ALIGN(dst.cols * 3, STRIDE_ALIGN);        /*Must fill*/
    dst.uv_stride = 0;
    dst.ion_len[0] = dst.stride * dst.rows;                    /*Selective filling*/
    dst.vpp_fd.handle = handle;                                /*Must fill*/
    ret = bm_malloc_device_byte_heap(handle, &dev_buffer_dst, DDR_CH, dst.ion_len[0]);
        if (ret != BM_SUCCESS) {
            VppErr("malloc device memory size = %d failed, ret = %d\n", dst.ion_len[0], ret);
    }
    dst.pa[0] = dev_buffer_dst.u.device.device_addr;
    dst.va[0] = malloc(dst.ion_len[0]);

    ret = bm_memcpy_s2d(handle, dev_buffer_src, src.va[0]);
    if (ret != BM_SUCCESS) {
        VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
    }

    vpp_resize_csc(&src, &dst);

    ret = bm_memcpy_d2s(handle, (void *)dst.va[0], dev_buffer_dst);
    if (ret != BM_SUCCESS) {
        VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
    }

    vpp_bmp_bgr888(output_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

    bm_free_device(handle, dev_buffer_src);
    free(src.va[0]);

    bm_free_device(handle, dev_buffer_dst);
    free(dst.va[0]);
    return 0;
}
