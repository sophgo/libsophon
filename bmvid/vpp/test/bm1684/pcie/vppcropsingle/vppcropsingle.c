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
#define VPP_CROP_NUM 256
int main(int argc, char *argv[])
{
    int idx = 0,src_fmt = 3;
    vpp_rect rect[VPP_CROP_NUM];
    vpp_mat src;
    vpp_mat dst[VPP_CROP_NUM];
    char *crop_name;
    char  output_name[16];
    int width, height;
    char *input_name;

    if (argc != 6) {
        printf("usage: %d\n", argc);
        printf("%s width height src_img_name src_fmt  dst_crop_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv-bmp3.bin 3 crop \n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    input_name = argv[3];
    src_fmt = atoi(argv[4]);
    crop_name = argv[5];

    int ret;
    int  num;
    bm_handle_t handle = NULL;
    bm_device_mem_t dev_buffer_src[3];
    bm_device_mem_t dev_buffer_dst[VPP_CROP_NUM][3];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    src.vpp_fd.handle = handle;
    vpp_creat_host_and_device_mem(dev_buffer_src, &src, FMT_BGR, width, height);

    vpp_read_file_pcie(&src, dev_buffer_src, input_name);

    for (idx = 0; idx < VPP_CROP_NUM; idx++) {
        rect[idx].x = idx * 2;
        rect[idx].y = idx * 2;
        rect[idx].width = 108 + idx * 5;
        rect[idx].height= 54 + idx * 2;

        dst[idx].vpp_fd.handle = handle;
        vpp_creat_host_and_device_mem(dev_buffer_dst[idx], &dst[idx], FMT_BGR, rect[idx].width, rect[idx].height);
    }

    vpp_crop_single(&src, rect, dst, VPP_CROP_NUM);

    for (num = 0; num < VPP_CROP_NUM; num++) {
        for (idx = 0; idx < dst[num].num_comp; idx++) {
            ret = bm_memcpy_d2s(handle, (void *)dst[num].va[idx], dev_buffer_dst[num][idx]);
            if (ret != BM_SUCCESS) {
                VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
            }
        }
    }

    for (idx = 0; idx < VPP_CROP_NUM; idx++) {
        sprintf(output_name,"%s%d.bmp",crop_name,idx);
        vpp_bmp_bgr888(output_name, (unsigned  char *)dst[idx].va[0], dst[idx].cols, dst[idx].rows, dst[idx].stride);
    }

    for (idx = 0; idx < src.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_src[idx]);
        free(src.va[idx]);
    }

    for (num = 0; num < VPP_CROP_NUM; num++) {
        for (idx = 0; idx < dst[num].num_comp; idx++) {
            bm_free_device(handle, dev_buffer_dst[num][idx]);
            free(dst[num].va[idx]);
        }
    }

    return 0;
}
