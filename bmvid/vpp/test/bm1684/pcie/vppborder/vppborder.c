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
#else
#pragma comment(lib, "libbmlib.lib")
#endif

int main(int argc, char *argv[])
{
    vpp_mat src;
    vpp_mat dst;
    char *input_name;
    int in_w, in_h;
    int top;
    int bottom;
    int left;
    int right;
    char *img_name;

    if (argc != 9) {
        printf("usage: %d\n", argc);
        printf("%s width height bgr-org-opencv-bmp3.bin top bottom left right  dst_img_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv-bmp3.bin 32 32 32 32  out.bmp\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);
    in_h = atoi(argv[2]);

    input_name = argv[3];
    top = atoi(argv[4]);
    bottom = atoi(argv[5]);
    left = atoi(argv[6]);
    right = atoi(argv[7]);
    img_name = argv[8];

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

    vpp_creat_host_and_device_mem(dev_buffer_src, &src, FMT_BGR, in_w, in_h);
    vpp_creat_host_and_device_mem(dev_buffer_dst, &dst, FMT_BGR, in_w + left + right, in_h + top + bottom);

    vpp_read_file_pcie(&src, dev_buffer_src, input_name);

    for (idx = 0; idx < dst.num_comp; idx++) {
        bm_memset_device(handle, 0, dev_buffer_dst[idx]);
    }

    vpp_border(&src, &dst, top, bottom, left, right);

    for (idx = 0; idx < dst.num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst.va[idx], dev_buffer_dst[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }
    vpp_bmp_bgr888(img_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

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

