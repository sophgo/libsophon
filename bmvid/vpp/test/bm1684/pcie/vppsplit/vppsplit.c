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
    vpp_mat src;
    vpp_mat dst;
    char * ch_b_name;
    char * ch_g_name;
    char * ch_r_name;
    char *input_name;
    int width, height;

    if (argc != 7)
    {
        printf("usage: %d\n", argc);
        printf("usage: %d\n", argc);
        printf("%s width height in_bgr24.bin   ch_r_name ch_g_name ch_b_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv.bin r.bmp g.bmp b.bmp\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    input_name = argv[3];
    ch_r_name = argv[4];
    ch_g_name = argv[5];
    ch_b_name = argv[6];

    int ret;
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

    vpp_creat_host_and_device_mem(dev_buffer_src, &src, FMT_BGR, width, height);
    vpp_creat_host_and_device_mem(dev_buffer_dst, &dst, FMT_RGBP, width, height);

    vpp_read_file_pcie(&src, dev_buffer_src, input_name);

    vpp_split(&src, &dst);

    for (idx = 0; idx < dst.num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst.va[idx], dev_buffer_dst[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }
    vpp_bmp_gray(ch_r_name, (unsigned char *) dst.va[0], dst.cols, dst.rows, dst.stride);
    vpp_bmp_gray(ch_g_name, (unsigned char *) dst.va[1], dst.cols, dst.rows, dst.stride);
    vpp_bmp_gray(ch_b_name, (unsigned char *) dst.va[2], dst.cols, dst.rows, dst.stride);

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
