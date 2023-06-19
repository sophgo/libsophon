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
    vpp_mat src[2], dst[2];
    struct vpp_rect_s loca[2];
    char *offset_base_y, *comp_base_y, *offset_base_c, *comp_base_c;
    int in_w, in_h = 0, out_w, out_h = 0;
    char *img_name;
    char  img_name2[16];
    mem_layout src_para;
    mem_layout dst_para;
    void *src_va = 0;
    void *dst_va = 0;
    int src_stride = 0;    /*src image stride*/
    int dst_stride = 0;    /*dst image stride*/
    int ret = VPP_OK, src_len, idx;

    if (argc != 16) {
        printf("usage: %d\n", argc);
        printf("%s src_width src_height offset_base_y comp_base_y offset_base_c comp_base_c x y width height src-stride dst-format dst_width dst_height dst_img_name\n", argv[0]);
        printf("format: 0 -- FMT_Y, 1 -- FMT_I420, 2 -- FMT_NV12, 3 -- FMT_BGR, 4 -- FMT_RGB, 5 -- FMT_RGBP, 6 -- FMT_BGRP)\n");
        printf("support path:\n");
        printf("FMT_NV12 > FMT_BGR,FMT_RGB,FMT_RGBP,FMT_BGRP,FMT_I420\n");
        printf("example:\n");
        printf("nv12-> BGR:%s 1920 1080 fbc_offset_table_y.bin fbc_comp_data_y.bin fbc_offset_table_c.bin fbc_comp_data_c.bin 128 364 1080 586 1920 3 2012 768 out.bmp\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);    /*src image w*/
    in_h = atoi(argv[2]);    /*src image h*/
    offset_base_y = argv[3];    /*src image name*/
    comp_base_y = argv[4];    /*src image name*/
    offset_base_c = argv[5];    /*src image name*/
    comp_base_c = argv[6];    /*src image name*/
    loca[0].x = atoi(argv[7]);    /*offset of crop image on the x axis*/
    loca[0].y = atoi(argv[8]);    /*offset of crop image on the y axis*/
    loca[0].width= atoi(argv[9]);    /*width of crop image*/
    loca[0].height= atoi(argv[10]);    /*height of crop image*/
    src_stride = atoi(argv[11]);    /*src image format w*/
    dst_fmt = atoi(argv[12]);    /*dst image format*/
    out_w = atoi(argv[13]);    /*dst image w*/
    out_h = atoi(argv[14]);    /*dst image h*/
    img_name = argv[15];/*dst image name*/

    FILE *fp0 = fopen(offset_base_y, "rb");
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
//    printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);

    FILE *fp1 = fopen(comp_base_y, "rb");
    fseek(fp1, 0, SEEK_END);
    int len1 = ftell(fp1);
//    printf("len1 : %d\n", len1);
    fseek(fp1, 0, SEEK_SET);

    FILE *fp2 = fopen(offset_base_c, "rb");
    fseek(fp2, 0, SEEK_END);
    int len2 = ftell(fp2);
//    printf("len2 : %d\n", len2);
    fseek(fp2, 0, SEEK_SET);

    FILE *fp3 = fopen(comp_base_c, "rb");
    fseek(fp3, 0, SEEK_END);
    int len3 = ftell(fp3);
//    printf("len3 : %d\n", len3);
    fseek(fp3, 0, SEEK_SET);

    src[0].num_comp = 4;
    src[0].ion_len[0] = len0;
    src[0].ion_len[1] = len1;
    src[0].ion_len[2] = len2;
    src[0].ion_len[3] = len3;
//   printf("len0 %d, len1 %d, len2 %d, len3 %d\n", len0, len1, len2, len3);

    bm_handle_t handle = NULL;

    bm_device_mem_t dev_buffer_src;
    bm_device_mem_t dev_buffer_dst0[3];
    bm_device_mem_t dev_buffer_dst1[3];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    src_len = src[0].ion_len[0] + src[0].ion_len[1] + src[0].ion_len[2] + src[0].ion_len[3];
    printf("src_len %d\n", src_len);

    ret = bm_malloc_device_byte_heap(handle, &dev_buffer_src, DDR_CH, src_len);
        if (ret != BM_SUCCESS) {
            VppErr("malloc device memory size = %d failed, ret = %d\n", src_len, ret);
    }

    src_para.pa = dev_buffer_src.u.device.device_addr;
    src_para.va = malloc(src_len);

    src[0].pa[0] = src_para.pa;    /*data Physical address*/
    src[0].pa[1] = src_para.pa + src[0].ion_len[0];
    src[0].pa[2] = src_para.pa + src[0].ion_len[0] + src[0].ion_len[1];
    src[0].pa[3] = src_para.pa + src[0].ion_len[0] + src[0].ion_len[1] + src[0].ion_len[2];
    src[0].va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src[0].va[1] = (char *)src_para.va + src[0].ion_len[0];
    src[0].va[2] = (char *)src_para.va + src[0].ion_len[0] + src[0].ion_len[1];
    src[0].va[3] = (char *)src_para.va + src[0].ion_len[0] + src[0].ion_len[1] + src[0].ion_len[2];
    src[0].format = FMT_NV12;/*data format*/
    src[0].cols = in_w;    /*image width*/
    src[0].rows = in_h;    /*image height*/
    src[0].stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/
    src[0].is_pa = 1;
    src[0].vpp_fd.handle = handle;


    src[1].num_comp = 4;
    src[1].ion_len[0] = src[0].ion_len[0];
    src[1].ion_len[1] = src[0].ion_len[1];
    src[1].ion_len[2] = src[0].ion_len[2];
    src[1].ion_len[3] = src[0].ion_len[3];
    src[1].pa[0] = src[0].pa[0];    /*data Physical address*/
    src[1].pa[1] = src[0].pa[1];
    src[1].pa[2] = src[0].pa[2];
    src[1].pa[3] = src[0].pa[3];
    src[1].va[0] = src[0].va[0];    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src[1].va[1] = src[0].va[1];
    src[1].va[2] = src[0].va[2];
    src[1].va[3] = src[0].va[3];
    src[1].format = FMT_NV12;/*data format*/
    src[1].cols = src[0].cols;    /*image width*/
    src[1].rows = src[0].rows;    /*image height*/
    src[1].stride = src[0].stride;    /*The number of bytes of memory occupied by one line of image data*/
    src[1].is_pa = 1;
    src[1].vpp_fd.handle = handle;
    loca[1].x = loca[0].x/2;
    loca[1].y = loca[0].y/2;
    loca[1].width= loca[0].width;
    loca[1].height= loca[0].height;

    dst[0].vpp_fd.handle = handle;
    dst[1].vpp_fd.handle = handle;
    vpp_creat_host_and_device_mem(&dev_buffer_dst0[0], &dst[0], dst_fmt, out_w, out_h);
    vpp_creat_host_and_device_mem(&dev_buffer_dst1[0], &dst[1], dst_fmt, out_w, out_h);

    fread(src[0].va[0], 1, len0, fp0);
    fread(src[0].va[1], 1, len1, fp1);
    fread(src[0].va[2], 1, len2, fp2);
    fread(src[0].va[3], 1, len3, fp3);

    ret = bm_memcpy_s2d(handle, dev_buffer_src, src_para.va);
    if (ret != BM_SUCCESS) {
        VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
    }

    ret = fbd_csc_crop_multi_resize_ctype_stype(src, loca, dst,2, CSC_MAX, VPP_SCALE_BILINEAR);

    for (idx = 0; idx < dst[0].num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst[0].va[idx], dev_buffer_dst0[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    for (idx = 0; idx < dst[1].num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst[1].va[idx], dev_buffer_dst1[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    sprintf(img_name2,"%d%s",2,img_name);
    ret = output_file(img_name, &dst[0]);
    ret = output_file(img_name2, &dst[1]);

    bm_free_device(handle, dev_buffer_src);
    free(src[0].va[0]);

    for (idx = 0; idx < dst[0].num_comp; idx++) {
        bm_free_device(handle, dev_buffer_dst0[idx]);
        free(dst[0].va[idx]);
    }
    for (idx = 0; idx < dst[1].num_comp; idx++) {
        bm_free_device(handle, dev_buffer_dst1[idx]);
        free(dst[1].va[idx]);
    }

    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
    return 0;
}

