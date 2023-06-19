//=====================================================================
//  This file is used to test vpp func: From each large image, a small picture is buckled,
//  and then stitched into a large one
//---------------------------------------------------------------------
//  This confidential and proprietary software may be used only
//  as authorized by a licensing agreement from BITMAIN Inc.
//  In the event of publication, the following notice is applicable:
//
//  (C) COPYRIGHT 2015 - 2025  BITMAIN INC. ALL RIGHTS RESERVED
//
//  The entire notice above must be reproduced on all authorized copies.
//
//=====================================================================

#ifndef SOC_MODE
#define PCIE_MODE
#endif

#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#ifdef __linux__
  #include <pthread.h>
  #include <sys/time.h>
#endif

#ifndef USING_CMODEL
#include "vpplib.h"
#endif

#define INPUT_NUM_MAX_TEST (256)
#define STRIDE_ALIGN    (64)
#define VPP_ALIGN(x, mask)  (((x) + ((mask)-1)) & ~((mask)-1))
#define UNUSED_VARIABLE(x)  ((x) = (x))
extern void format_to_str(bm_image_format_ext format, char* res);
void get_stride_default_align(int img_fmt, int img_w, int img_h, int* stride, int *len, int *num_comp) {
    switch (img_fmt) {
        case FORMAT_RGB_PACKED:
        case FORMAT_BGR_PACKED:
            num_comp[0] = 1;
            stride[0] = VPP_ALIGN(img_w * 3, STRIDE_ALIGN);
            len[0] = stride[0] * img_h;
            break;
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
            num_comp[0] = 1;
            stride[0] = VPP_ALIGN(img_w, STRIDE_ALIGN);
            len[0] = stride[0] * img_h * 3;
            break;
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_YUV444P:
            num_comp[0] = 3;
            stride[0] = VPP_ALIGN(img_w, STRIDE_ALIGN);
            stride[1] = stride[0];
            stride[2] = stride[0];
            len[0] = stride[0] * img_h;
            len[1] = stride[1] * img_h;
            len[2] = stride[2] * img_h;
            break;
        case FORMAT_YUV422P:
            num_comp[0] = 3;
            stride[0] = VPP_ALIGN(img_w, STRIDE_ALIGN);
            stride[1] = stride[0] >> 1;
            stride[2] = stride[1];
            len[0] = stride[0] * img_h;
            len[1] = stride[1] * img_h;
            len[2] = stride[2] * img_h;
            break;
        case FORMAT_YUV420P:
            num_comp[0] = 3;
            stride[0] = VPP_ALIGN(img_w, STRIDE_ALIGN);
            stride[1] = stride[0] >> 1;
            stride[2] = stride[0] >> 1;
            len[0] = stride[0] * img_h;
            len[1] = stride[1] * img_h >> 1;
            len[2] = stride[2] * img_h >> 1;
            break;
        case FORMAT_NV12:
            num_comp[0] = 2;
            stride[0] = VPP_ALIGN(img_w, STRIDE_ALIGN);
            stride[1] = stride[0];
            len[0] = stride[0] * img_h;
            len[1] = stride[1] * (img_h >> 1);
            break;
        case FORMAT_GRAY:
            num_comp[0] = 1;
            stride[0] = VPP_ALIGN(img_w, STRIDE_ALIGN);
            len[0] = stride[0] * img_h;
            break;
        case FORMAT_COMPRESSED:
            num_comp[0] = 4;

            stride[0] = 1920;
            stride[1] = 1920;
            stride[2] = 0;
            stride[3] = 0;
            break;

        default:
            bmlib_log("BMCV", BMLIB_LOG_ERROR,
              "vpp hw not support this format %d, %s: %s: %d\n", img_fmt,
              __FILE__, __func__, __LINE__);
            break;
    }

    if (img_fmt == FORMAT_COMPRESSED) {
        FILE *fp0 = fopen("fbc_offset_table_y.bin", "rb");
        fseek(fp0, 0, SEEK_END);
        len[0] = ftell(fp0);
        fseek(fp0, 0, SEEK_SET);
        fclose(fp0);

        FILE *fp1 = fopen("fbc_comp_data_y.bin", "rb");
        fseek(fp1, 0, SEEK_END);
        len[1] = ftell(fp1);
        fseek(fp1, 0, SEEK_SET);
        fclose(fp1);

        FILE *fp2 = fopen("fbc_offset_table_c.bin", "rb");
        fseek(fp2, 0, SEEK_END);
        len[2] = ftell(fp2);
        fseek(fp2, 0, SEEK_SET);
        fclose(fp2);

        FILE *fp3 = fopen("fbc_comp_data_c.bin", "rb");
        fseek(fp3, 0, SEEK_END);
        len[3] = ftell(fp3);
        fseek(fp3, 0, SEEK_SET);
        fclose(fp3);
    }
}

static int check_file_size(char *file_name, FILE **fp, int length) {
    int len;

    fseek(*fp, 0, SEEK_END);
    len = ftell(*fp);

    if (len != length) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "%s len not matched input %d but expect %d\n", file_name, len, length);
        fclose(*fp);
        return -1;
    }

    fseek(*fp, 0, SEEK_SET);

    return 0;
}

int vpp_read_file_pcie(bm_handle_t handle, bm_image* img, void** img_buf_va, char *file_name = NULL) {
    FILE *fp;
    int ret = 0;
    int size = 0, idx = 0, i = 0, rows = 0, cols = 0;

    int stride[4];
    int len[4];
    int num_comp = 0;

    char fmt_str[100];
    format_to_str(img->image_format, fmt_str);

    int img_w = img->width;
    int img_h = img->height;

    get_stride_default_align(img->image_format, img_w, img_h, stride, len, &num_comp);

    for (idx = 0; idx < num_comp; idx++) {
        img_buf_va[idx] = malloc(len[idx]);
        size += len[idx];
    }

    /*default: img data stride 64 Bytes align*/
    if (img->image_format != FORMAT_COMPRESSED) {
        fp = fopen(file_name, "rb");
        if (fp == NULL) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR, "%s open failed\n", file_name);
            return -1;
        }

        if (check_file_size(file_name, &fp, size) < 0) {
            fclose(fp);
            return -1;
        }

        switch (img->image_format) {
            case FORMAT_BGR_PACKED: case FORMAT_RGB_PACKED:
                rows = img->height;
                cols = img->width * 3;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[0] + i * stride[0], 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }
                break;

            case FORMAT_RGB_PLANAR: case FORMAT_BGR_PLANAR:
                rows = img->height;
                cols = img->width * 3;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[0] + i * stride[0] * 3, 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }
                break;

            case FORMAT_RGBP_SEPARATE: case FORMAT_BGRP_SEPARATE:
            case FORMAT_YUV444P:
                rows = img->height;
                cols = img->width;
                for (idx = 0; idx < num_comp; idx++) {
                    for (i = 0; i < rows; i++) {
                        size_t cnt = fread((unsigned char *)img_buf_va[idx] + i * stride[idx], 1, cols, fp);
                        UNUSED_VARIABLE(cnt);
                    }
                }
                break;

            case FORMAT_YUV422P:
                rows = img->height;
                cols = img->width;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[0] + i * stride[0], 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }
                rows = img->height;
                cols = img->width / 2;
                for (idx = 1; idx < 3; idx++) {
                    for (i = 0; i < rows; i++) {
                        size_t cnt = fread((unsigned char *)img_buf_va[idx] + i * stride[1], 1, cols, fp);
                        UNUSED_VARIABLE(cnt);
                    }
                }
                break;

            case FORMAT_YUV420P:
                rows = img->height;
                cols = img->width;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[0] + i * stride[0], 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }
                rows = img->height / 2;
                cols = img->width / 2;
                for (idx = 1; idx < 3; idx++) {
                    for (i = 0; i < rows; i++) {
                        size_t cnt = fread((unsigned char *)img_buf_va[idx] + i * stride[idx], 1, cols, fp);
                        UNUSED_VARIABLE(cnt);
                    }
                }
                break;

            case FORMAT_NV12:
                rows = img->height;
                cols = img->width;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[0] + i * stride[0], 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }

                rows = img->height >> 1;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[1] + i * stride[1], 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }
                break;

            case FORMAT_GRAY:
                rows = img->height;
                cols = img->width;
                for (i = 0; i < rows; i++) {
                    size_t cnt = fread((unsigned char *)img_buf_va[0] + i * stride[0], 1, cols, fp);
                    UNUSED_VARIABLE(cnt);
                }
                break;

            case FORMAT_COMPRESSED:
                break;
            default:
                bmlib_log("BMCV", BMLIB_LOG_ERROR,
                  "vpp hw not support this format %s, %s: %s: %d\n", fmt_str,
                  __FILE__, __func__, __LINE__);
                break;
        }

        fclose(fp);
    }

    if (img->image_format == FORMAT_COMPRESSED) {
        FILE *fp0 = fopen("fbc_offset_table_y.bin", "rb");
        size_t cnt = fread(img_buf_va[0], 1, len[0], fp0);
        fclose(fp0);

        FILE *fp1 = fopen("fbc_comp_data_y.bin", "rb");
        cnt = fread(img_buf_va[1], 1, len[1], fp1);
        fclose(fp1);

        FILE *fp2 = fopen("fbc_offset_table_c.bin", "rb");
        cnt = fread(img_buf_va[2], 1, len[2], fp2);
        fclose(fp2);

        FILE *fp3 = fopen("fbc_comp_data_c.bin", "rb");
        cnt = fread(img_buf_va[3], 1, len[3], fp3);
        UNUSED_VARIABLE(cnt);
        fclose(fp3);
    }

    bm_device_mem_t device_mem[4];
    if (bm_image_get_device_mem(*img, device_mem) != BM_SUCCESS) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR,
              "bm_image_get_device_mem err %s: %s: %d\n",
              __FILE__, __func__, __LINE__);
    }

    for (idx = 0; idx < num_comp; idx++) {
        ret = bm_memcpy_s2d(handle, device_mem[idx], img_buf_va[idx]);
        if (ret != BM_SUCCESS) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR,
              "CDMA transfer from system to device failed, ret = %d\n", ret);

        }
    }

    return ret;
}

int main(int argc, char **argv) {
#ifndef USING_CMODEL
    int ret;

    if (argc != 16) {
        printf("usage: %d\n", argc);
        printf("%s in_w in_h src_fmt src_crop_stx src_crop_sty src_crop_w src_crop_h \
 dst_w dst_h dst_fmt dst_crop_w dst_crop_h input_num devid img_name\n", argv[0]);

        printf("src_fmt:\n \
            0-FORMAT_YUV420P\n \
            1-FORMAT_YUV422P\n \
            2-FORMAT_YUV444P\n \
            3-FORMAT_NV12\n \
            8-FORMAT_RGB_PLANAR\n \
            9-FORMAT_BGR_PLANAR\n \
            10-FORMAT_RGB_PACKED\n \
            11-FORMAT_BGR_PACKED\n \
            12-FORMAT_RGBP_SEPARATE\n \
            13-FORMAT_BGRP_SEPARATE\n \
            14-FORMAT_GRAY\n \
            15-FORMAT_COMPRESSED\n");

        printf("dst_fmt:\n \
            0-FORMAT_YUV420P\n \
            3-FORMAT_NV12\n \
            8-FORMAT_RGB_PLANAR\n \
            9-FORMAT_BGR_PLANAR\n \
            10-FORMAT_RGB_PACKED\n \
            11-FORMAT_BGR_PACKED\n \
            12-FORMAT_RGBP_SEPARATE\n \
            13-FORMAT_BGRP_SEPARATE\n");

        printf("example:\n");
        printf("FORMAT_NV12-->FORMAT_BGR_PLANAR:\n");
        printf("        %s 1920 1080 3 11 20 1600 800 1920 1080 9 480 270 16 0 ./img1.nv12\n", argv[0]);
        printf("FORMAT_YUV420P-->FORMAT_BGR_PACKED:\n");
        printf("        %s 1920 1080 0 11 20 1600 800 1920 1080 11 480 270 16 0 ./i420.yuv\n", argv[0]);
        printf("FORMAT_BGR_PACKED-->FORMAT_BGR_PLANAR:\n");
        printf("        %s 1920 1080 11 11 20 1600 800 3840 2016 9 480 270 16 0 ./bgr-org-opencv-bmp3.bin\n", argv[0]);
        printf("FORMAT_BGR_PLANAR-->FORMAT_BGR_PACKED:\n");
        printf("        %s 1920 1080 9 11 20 1600 800 3840 2160 11 480 270 16 0 ./bgrp.bin\n", argv[0]);
        printf("FORMAT_YUV420P-->FORMAT_YUV420P:\n");
        printf("        %s 1920 1080 0 12 20 1600 800 1920 1080 0 480 270 16 0 ./i420.yuv\n", argv[0]);
        return 0;
    }

    int in_w = atoi(argv[1]);
    int in_h = atoi(argv[2]);

    int src_fmt = atoi(argv[3]);
    int src_crop_stx = atoi(argv[4]);
    int src_crop_sty = atoi(argv[5]);
    int src_crop_w = atoi(argv[6]);
    int src_crop_h = atoi(argv[7]);

    int dst_w = atoi(argv[8]);
    int dst_h = atoi(argv[9]);

    int dst_fmt = atoi(argv[10]);
    int dst_crop_w = atoi(argv[11]);
    int dst_crop_h = atoi(argv[12]);

    int input_num = atoi(argv[13]);
    char* img_name = argv[15];
    int devid = atoi(argv[14]);

    if(input_num > INPUT_NUM_MAX_TEST){
        printf("failed!!input num over 256!\n");
        return -1;
    }

    if(src_fmt == FORMAT_COMPRESSED){
        printf("failed! Input images do not support compression types \n");
        return -1;
    }

    int input_idx;
    int plane_idx;
    int src_plane_num;
    int dst_plane_num;
    #ifdef __linux__
    bm_image src_img[input_num];
    #else
    std::shared_ptr<bm_image> src_img_(new bm_image[input_num], std::default_delete<bm_image[]>());
    bm_image*                 src_img = src_img_.get();
    #endif
    int src_stride[4];
    int src_len[4];
    int src_num_comp;
    void* src_img_buf_va[4] = {NULL};

    int dst_stride[4];
    int dst_len[4];
    int dst_num_comp;
    void* dst_img_buf_va[4] = {NULL};
    #ifdef __linux__
    bmcv_rect_t src_crop[input_num];
    bmcv_rect_t dst_crop[input_num];
    #else
    std::shared_ptr<bmcv_rect_t> src_crop_(new bmcv_rect_t[input_num], std::default_delete<bmcv_rect_t[]>());
    bmcv_rect_t*                 src_crop = src_crop_.get();
    std::shared_ptr<bmcv_rect_t> dst_crop_(new bmcv_rect_t[input_num], std::default_delete<bmcv_rect_t[]>());
    bmcv_rect_t*                 dst_crop = dst_crop_.get();
    #endif

    /*set bmlib log level*/
    bmlib_log_set_level(BMLIB_LOG_INFO);

    /*request handle*/
    bm_handle_t handle = NULL;
    ret = bm_dev_request(&handle, devid);
    if (ret != BM_SUCCESS || handle == NULL) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR,
          "bm_dev_request err (may be devid err!)%s: %s: %d\n", __FILE__, __func__, __LINE__);
        return -1;
    }

    /*creat bm image: src*/
    get_stride_default_align(src_fmt, in_w, in_h, src_stride, src_len, &src_num_comp);

    for (input_idx = 0; input_idx < input_num; input_idx++) {
        bm_image_create(handle, in_h, in_w,
        (bm_image_format_ext)src_fmt,
        (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
        &src_img[input_idx], src_stride);

        /*alloc mem for src image*/
        bm_image_alloc_dev_mem_heap_mask(src_img[input_idx], 6);

        /*read input image file to src_imge device addr*/  /*TODO  img_name*/
        vpp_read_file_pcie(handle, &src_img[input_idx], src_img_buf_va, img_name);

        /*src crop param*/
        src_crop[input_idx].start_x = src_crop_stx + input_idx * 2;
        src_crop[input_idx].start_y = src_crop_sty + input_idx * 4;
        src_crop[input_idx].crop_w = src_crop_w + input_idx * 6;
        src_crop[input_idx].crop_h = src_crop_h + input_idx * 6;

        for (plane_idx = 0; plane_idx < 4; plane_idx++) {
            if (src_img_buf_va[plane_idx] != NULL) {
                free(src_img_buf_va[plane_idx]);
                src_img_buf_va[plane_idx] = NULL;
             }
        }
    }

    src_plane_num = bm_image_get_plane_num(src_img[0]);
    if(src_plane_num != src_num_comp){
        printf("failed!src plane num is not equal to src comp num!\n ");
        return -1;
    }

    /*dst image param*/
    bm_image dst_img;
    get_stride_default_align(dst_fmt, dst_w, dst_h, dst_stride, dst_len, &dst_num_comp);

    /*create bm imge : dst*/
    bm_image_create(handle, dst_h, dst_w,
        (bm_image_format_ext)dst_fmt,
        (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
        &dst_img, dst_stride);

    /*alloc mem for dst image*/
    if (bm_image_alloc_dev_mem_heap_mask(dst_img, 6) != BM_SUCCESS) {
         bmlib_log("BMCV", BMLIB_LOG_ERROR,
            "bm_image_alloc_dev_mem err:dst img %s: %s: %d\n", __FILE__, __func__, __LINE__);
    }

    /*set dst crop rect param*/
    int max_img_num_dim_w = dst_w / dst_crop_w;
    int max_img_num_dim_h = dst_h / dst_crop_h;
    int row_num_in_dst_img;  // start frm 0
    int col_num_in_dst_img;  // start frm 0

    for (input_idx = 0; input_idx < input_num; input_idx++) {
         row_num_in_dst_img = input_idx / max_img_num_dim_w;
         col_num_in_dst_img = (input_idx % max_img_num_dim_w);
         dst_crop[input_idx].start_x = col_num_in_dst_img * dst_crop_w;
         dst_crop[input_idx].start_y = row_num_in_dst_img * dst_crop_h;

         dst_crop[input_idx].crop_w = dst_crop_w;
         dst_crop[input_idx].crop_h = dst_crop_h;
    }

    if (input_num != 17) {
        if(max_img_num_dim_w * max_img_num_dim_h < input_num){
            printf("%s:%d[%s]Error!crop param error.\n",__FILE__, __LINE__, __FUNCTION__);
        }
    }

    if (input_num == 17) {
        dst_crop[0].start_x = 0;
        dst_crop[0].start_y = 0;
        dst_crop[0].crop_w = 320;
        dst_crop[0].crop_h = 180;

        dst_crop[1].start_x = 320;
        dst_crop[1].start_y = 0;
        dst_crop[1].crop_w = 640;
        dst_crop[1].crop_h = 180;

        dst_crop[11].start_x = 640;
        dst_crop[11].start_y = 0;
        dst_crop[11].crop_w = 320;
        dst_crop[11].crop_h = 180;
        dst_crop[12].start_x = 960;
        dst_crop[12].start_y = 0;
        dst_crop[12].crop_w = 320;
        dst_crop[12].crop_h = 180;
        dst_crop[13].start_x = 1280;
        dst_crop[13].start_y = 0;
        dst_crop[13].crop_w = 320;
        dst_crop[13].crop_h = 180;

        dst_crop[2].start_x = 1600;
        dst_crop[2].start_y = 0;
        dst_crop[2].crop_w = 320;
        dst_crop[2].crop_h = 180;

        dst_crop[3].start_x = 0;
        dst_crop[3].start_y = 180;
        dst_crop[3].crop_w = 320;
        dst_crop[3].crop_h = 360;

        dst_crop[9].start_x = 0;
        dst_crop[9].start_y = 540;
        dst_crop[9].crop_w = 320;
        dst_crop[9].crop_h = 360;

        dst_crop[4].start_x = 320;
        dst_crop[4].start_y = 180;
        dst_crop[4].crop_w = 1280;
        dst_crop[4].crop_h = 720;

        dst_crop[5].start_x = 1600;
        dst_crop[5].start_y = 180;
        dst_crop[5].crop_w = 320;
        dst_crop[5].crop_h = 360;

        dst_crop[10].start_x = 1600;
        dst_crop[10].start_y = 540;
        dst_crop[10].crop_w = 320;
        dst_crop[10].crop_h = 360;

        dst_crop[6].start_x = 0;
        dst_crop[6].start_y = 900;
        dst_crop[6].crop_w = 320;
        dst_crop[6].crop_h = 180;

        dst_crop[7].start_x = 320;
        dst_crop[7].start_y = 900;
        dst_crop[7].crop_w = 320;
        dst_crop[7].crop_h = 180;
        dst_crop[14].start_x = 640;
        dst_crop[14].start_y = 900;
        dst_crop[14].crop_w = 320;
        dst_crop[14].crop_h = 180;
        dst_crop[15].start_x = 960;
        dst_crop[15].start_y = 900;
        dst_crop[15].crop_w = 320;
        dst_crop[15].crop_h = 180;
        dst_crop[16].start_x = 1280;
        dst_crop[16].start_y = 900;
        dst_crop[16].crop_w = 320;
        dst_crop[16].crop_h = 180;


        dst_crop[8].start_x = 1600;
        dst_crop[8].start_y = 900;
        dst_crop[8].crop_w = 320;
        dst_crop[8].crop_h = 180;
    }

    /*get dst image plane num*/
    dst_plane_num = bm_image_get_plane_num(dst_img);
    if(dst_num_comp != dst_plane_num){
        printf("failed!dst plane num is not equal to dst comp num!\n ");
        return -1;
    }

    bm_device_mem_t dst_device_mem[4];

    /*get device mem*/
    if (bm_image_get_device_mem(dst_img, dst_device_mem) != BM_SUCCESS) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR,
              "bm_image_get_device_mem err %s: %s: %d\n",
              __FILE__, __func__, __LINE__);
    }

    /*memset dst image device mem*/
//    for (plane_idx = 0; plane_idx < dst_plane_num; plane_idx++) {
//        bm_memset_device(handle, 0x80808080, dst_device_mem[plane_idx]);
//    }

    /*vpp hw start work*/
    bmcv_image_vpp_stitch(handle, input_num, src_img, dst_img, dst_crop, src_crop);

    /*dump data from device mem to host mem and write file*/
    char out_name[100];
    char out_name_merge[100];
    sprintf(out_name_merge, "%s", "vpp_stitch_merge_file.dat");
    FILE *fp_merge = fopen(out_name_merge, "wb");

    for (plane_idx = 0; plane_idx < dst_plane_num; plane_idx++) {
        /*dump data from device mem*/
        dst_img_buf_va[plane_idx] = malloc(dst_len[plane_idx]);

        ret = bm_memcpy_d2s(handle, dst_img_buf_va[plane_idx], dst_device_mem[plane_idx]);
        if (ret != BM_SUCCESS) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR,
              "CDMA transfer from system to device failed, ret = %d\n", ret);
        }

        /*write output image to file*/

        sprintf(out_name, "%s%d%s", "vpp_stitch_plane_", plane_idx, ".dat");

        FILE *fp_dst = fopen(out_name, "wb");
        if (!fp_dst) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR,
              "ERROR: Unable to open the output file: %s\n", out_name);
            exit(-1);
        }

        if (fwrite(dst_img_buf_va[plane_idx], 1, dst_len[plane_idx], fp_dst) !=
            (unsigned int)(dst_len[plane_idx])) {
            fclose(fp_dst);
            return BM_ERR_FAILURE;
        }

        fclose(fp_dst);

        if (dst_plane_num > 1) {
            if (fwrite(dst_img_buf_va[plane_idx], 1, dst_len[plane_idx], fp_merge) !=
                (unsigned int)(dst_len[plane_idx])) {
                fclose(fp_merge);
                return BM_ERR_FAILURE;
            }
        }
    }

    /*free va mem and dextory bm image*/
    bm_image_destroy(dst_img);

    for (plane_idx = 0; plane_idx < 4; plane_idx++) {
        if (dst_img_buf_va[plane_idx] != NULL)
        free(dst_img_buf_va[plane_idx]);

        if (src_img_buf_va[plane_idx] != NULL)
            free(src_img_buf_va[plane_idx]);
    }

    for (input_idx = 0; input_idx < input_num; input_idx++) {
        bm_image_destroy(src_img[input_idx]);
    }

    fclose(fp_merge);

#else
    printf("cmodel not supported yet!\n");
    UNUSED(argc);
    UNUSED(argv);
#endif
    return 0;
}

