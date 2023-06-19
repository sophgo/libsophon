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
    int width, height, idx;
    vpp_rect rect[3];
    vpp_mat src[3];
    vpp_mat dst[3];
    char *input_name[3];
    char *crop0_name;
    char *crop1_name;
    char *crop2_name;

    if (argc != 9)
    {
        printf("usage: %d\n", argc);
        printf("%s width height in_bgr24_0.bin in_bgr24_1.bin in_bgr24_2.bin  dst_crop0_name dst_crop1_name dst_crop2_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv.bin bgr-org-opencv-10.bin bgr-org-opencv-222.bin out0.bmp out1.bmp out2.bmp\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    input_name[0] = argv[3];
    input_name[1] = argv[4];
    input_name[2] = argv[5];

    crop0_name = argv[6];
    crop1_name = argv[7];
    crop2_name = argv[8];

    int ret;
    int num;

    bm_handle_t handle = NULL;

    bm_device_mem_t dev_buffer_src[3][3];
    bm_device_mem_t dev_buffer_dst[3][3];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    rect[0].x = 0;
    rect[0].y = 0;
    rect[0].width = 1000;
    rect[0].height= 1000;
    rect[1].x = 128;
    rect[1].y = 128;
    rect[1].width = 870;
    rect[1].height= 300;
    rect[2].x = 256;
    rect[2].y = 256;
    rect[2].width = 712;
    rect[2].height= 688;

    for (idx = 0; idx < 3; idx++) {
        src[idx].vpp_fd.handle = handle;
        dst[idx].vpp_fd.handle = handle;

        vpp_creat_host_and_device_mem(dev_buffer_src[idx], &src[idx], FMT_BGR, width, height);
        vpp_creat_host_and_device_mem(dev_buffer_dst[idx], &dst[idx], FMT_BGR, rect[idx].width, rect[idx].height);
    }

    for (idx = 0; idx < 3; idx++)
    {
        vpp_read_file_pcie(&src[idx], dev_buffer_src[idx], input_name[idx]);
    }

    vpp_crop_multi(src, rect, dst, 3);

    for (num = 0; num < 3; num++) {
        for (idx = 0; idx < dst[num].num_comp; idx++) {
            ret = bm_memcpy_d2s(handle, (void *)dst[num].va[idx], dev_buffer_dst[num][idx]);
            if (ret != BM_SUCCESS) {
                VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
            }
        }
    }

    vpp_bmp_bgr888(crop0_name, (unsigned char *)dst[0].va[0], dst[0].cols, dst[0].rows, dst[0].stride);
    vpp_bmp_bgr888(crop1_name, (unsigned char *)dst[1].va[0], dst[1].cols, dst[1].rows, dst[1].stride);
    vpp_bmp_bgr888(crop2_name, (unsigned char *)dst[2].va[0], dst[2].cols, dst[2].rows, dst[2].stride);

    for (num = 0; num < 3; num++) {
        for (idx = 0; idx < src[num].num_comp; idx++) {
            bm_free_device(handle, dev_buffer_src[num][idx]);
            free(src[num].va[idx]);
        }
    }

    for (num = 0; num < 3; num++) {
        for (idx = 0; idx < dst[num].num_comp; idx++) {
            bm_free_device(handle, dev_buffer_dst[num][idx]);
            free(dst[num].va[idx]);
        }
    }

    return 0;
}
