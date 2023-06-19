//=====================================================================
//  This file is used to test vpp func: fbd+cscc+resize_border in one api
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
#include "test_misc.h"
#ifdef __linux__
  #include <pthread.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

#ifndef USING_CMODEL
#include "vpplib.h"
#endif

#define CROP_NUM_MAX_TEST (18)
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
        bmlib_log("BMCV", BMLIB_LOG_ERROR,
          "%s len not matched input %d but expect %d\n", file_name, len, length);

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
            bmlib_log("BMCV", BMLIB_LOG_ERROR,"CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    return ret;
}

int main(int argc, char **argv) {
#ifndef USING_CMODEL
    int ret = 0;

    if ((argc != 16) && (argc != 15)) {
        printf("usage: %d\n", argc);
        printf("%s width height src_fmt src_crop_stx src_crop_sty src_crop_w src_crop_h \
 dst_fmt dst_crop_stx dst_crop_sty dst_crop_w dst_crop_h crop_num devid img_name\n", argv[0]);

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
            8-FORMAT_RGB_PLANAR\n \
            9-FORMAT_BGR_PLANAR\n \
            10-FORMAT_RGB_PACKED\n \
            11-FORMAT_BGR_PACKED\n \
            12-FORMAT_RGBP_SEPARATE\n \
            13-FORMAT_BGRP_SEPARATE\n");

        printf("example:\n");
        printf("FORMAT_NV12-->FORMAT_BGR_PLANAR:\n");
        printf("        %s 1920 1080 3 11 20 1600 800 9 150 103 1000 600 18 0 ./img1.nv12\n", argv[0]);
        printf("FORMAT_YUV420P-->FORMAT_BGR_PACKED:\n");
        printf("        %s 1920 1080 0 11 20 1600 800 11 150 103 1000 600 18 0 ./i420.yuv\n", argv[0]);
        printf("FORMAT_BGR_PACKED-->FORMAT_BGR_PLANAR:\n");
        printf("        %s 1920 1080 11 11 20 1600 800 9 150 103 1000 600 18 0 ./bgr-org-opencv-bmp3.bin\n", argv[0]);
        printf("FORMAT_BGR_PLANAR-->FORMAT_BGR_PACKED:\n");
        printf("        %s 1920 1080 9 11 20 1600 800 11 150 103 1000 600 18 0 ./bgrp.bin\n", argv[0]);
        printf("FORMAT_COMPRESSED-->FORMAT_BGR_PLANAR:\n");
        printf("        %s 1920 1080 15 11 20 1600 800 9 150 103 1000 600 18 0\n", argv[0]);
        printf("FORMAT_COMPRESSED-->FORMAT_BGR_PACKED:\n");
        printf("        %s 1920 1080 15 11 20 1600 800 11 150 103 1000 600 18 0\n", argv[0]);
        printf("FORMAT_COMPRESSED-->FORMAT_BGRP_SEPARATE:\n");
        printf("You can set a prefix for the name of the output image data when src_fmt is FORMAT_COMPRESSED\n");
        printf("        %s 1920 1080 15 11 20 1600 800 13 150 103 1000 600 18 0 prefix_string\n", argv[0]);
        return 0;
    }

    int in_w = atoi(argv[1]);
    int in_h = atoi(argv[2]);

    int src_fmt = atoi(argv[3]);

    int src_crop_stx = atoi(argv[4]);
    int src_crop_sty = atoi(argv[5]);
    int src_crop_w = atoi(argv[6]);
    int src_crop_h = atoi(argv[7]);

    int dst_fmt = atoi(argv[8]);

    int dst_crop_stx = atoi(argv[9]);
    int dst_crop_sty = atoi(argv[10]);
    int dst_crop_w = atoi(argv[11]);
    int dst_crop_h = atoi(argv[12]);
    int crop_num = atoi(argv[13]);

    char* img_name = NULL;
    int devid = 0;

    devid = atoi(argv[14]);
    img_name = argv[15];

    if((dst_fmt != FORMAT_RGB_PLANAR) && (dst_fmt != FORMAT_BGR_PLANAR) &&
    (dst_fmt != FORMAT_RGB_PACKED) && (dst_fmt != FORMAT_BGR_PACKED) &&
    (dst_fmt != FORMAT_RGBP_SEPARATE) && (dst_fmt != FORMAT_BGRP_SEPARATE)){
        printf("failed!dst format not support!\n");
        return -1;
    }

    if(crop_num > CROP_NUM_MAX_TEST){
        printf("failed!crop num over 18!\n");
        return -1;
    }
    int output_idx;
    int plane_idx;
    int src_plane_num;
    int dst_plane_num;
    bm_image src_img;
    int src_stride[4];
    int src_len[4];
    int src_num_comp;
    void* src_img_buf_va[4] = {NULL};
    bm_device_mem_t fbc_device_mem[4];

    int dst_stride[4];
    int dst_len[4];
    int dst_num_comp;
    #ifdef __linux__
    void* dst_img_buf_va[crop_num][4];
    for (output_idx = 0; output_idx < crop_num; output_idx++) {
        for (plane_idx = 0; plane_idx < 4; plane_idx++) {
            dst_img_buf_va[output_idx][plane_idx] = NULL;
       }
    }
    #else
    void *dst_img_buf_va[CROP_NUM_MAX_TEST][4] = {NULL};
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

    bm_image_create(handle, in_h, in_w,
      (bm_image_format_ext)src_fmt,
      (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
      &src_img, src_stride);

    src_plane_num = bm_image_get_plane_num(src_img);
    if(src_plane_num != src_num_comp){
        printf("failed!src plane num is not equal to src comp num!\n ");
        return -1;
    }
    /*alloc mem for src image*/
    if (src_fmt != FORMAT_COMPRESSED) {
        bm_image_alloc_dev_mem_heap_mask(src_img, 6);
    } else {
        for (plane_idx = 0; plane_idx < src_num_comp; plane_idx++) {
            ret = bm_malloc_device_byte_heap_mask(handle, &fbc_device_mem[plane_idx], 6, src_len[plane_idx]);
            if (ret != BM_SUCCESS) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR,
                  "malloc device memory size = %d failed, ret = %d\n", src_len[plane_idx], ret);
                return -1;
            }
        }

        bm_image_attach(src_img, fbc_device_mem);
    }

    /*read input image file to src_imge device addr*/

    if (src_fmt == FORMAT_COMPRESSED) {
        vpp_read_file_pcie(handle, &src_img, src_img_buf_va);
     } else {
        vpp_read_file_pcie(handle, &src_img, src_img_buf_va, img_name);
    }

    #ifdef __linux__
    bmcv_rect_t src_crop[crop_num];
    bmcv_padding_atrr_t padding[crop_num];
    #else
    std::shared_ptr<bmcv_rect_t> src_crop_(new bmcv_rect_t[crop_num], std::default_delete<bmcv_rect_t[]>());
    bmcv_rect_t*                 src_crop = src_crop_.get();
    std::shared_ptr<bmcv_padding_atrr_t> padding_(new bmcv_padding_atrr_t[crop_num], std::default_delete<bmcv_padding_atrr_t[]>());
    bmcv_padding_atrr_t*                 padding = padding_.get();
    #endif

    /*dst image param*/
    #ifdef __linux__
    bm_image dst_img[crop_num];
    #else
    std::shared_ptr<bm_image> dst_img_(new bm_image[crop_num], std::default_delete<bm_image[]>());
    bm_image*                 dst_img = dst_img_.get();
    #endif
    int dst_w = 1920;
    int dst_h = 1080;
    get_stride_default_align(dst_fmt, dst_w, dst_h, dst_stride, dst_len, &dst_num_comp);

    for (output_idx = 0; output_idx < crop_num; output_idx++) {
        /*src crop param*/
        src_crop[output_idx].start_x = src_crop_stx + output_idx * 5;
        src_crop[output_idx].start_y = src_crop_sty + output_idx * 5;
        src_crop[output_idx].crop_w = src_crop_w + output_idx * 5;
        src_crop[output_idx].crop_h = src_crop_h + output_idx * 5;

        /*dst crop param and padding param*/

        padding[output_idx].dst_crop_stx = dst_crop_stx - output_idx * 5;
        padding[output_idx].dst_crop_sty = dst_crop_sty - output_idx * 5;
        padding[output_idx].dst_crop_w = dst_crop_w - output_idx * 5;
        padding[output_idx].dst_crop_h = dst_crop_h - output_idx * 5;
        padding[output_idx].padding_r = 128;
        padding[output_idx].padding_g = 128;
        padding[output_idx].padding_b = 128;
        padding[output_idx].if_memset = 1;

        /*create bm imge : dst*/
        bm_image_create(handle, dst_h, dst_w,
          (bm_image_format_ext)dst_fmt,
          (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
          &dst_img[output_idx], dst_stride);

        /*alloc mem for dst image*/
        if (bm_image_alloc_dev_mem_heap_mask(dst_img[output_idx], 6) != BM_SUCCESS) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR,
                "bm_image_alloc_dev_mem err:dst img[%d] %s: %s: %d\n", output_idx,
                __FILE__, __func__, __LINE__);
            ret = -1;
            goto exit;
        }
    }

    /*vpp hw start work*/
    bmcv_image_vpp_convert_padding(handle, crop_num, src_img, dst_img,
    padding, src_crop, BMCV_INTER_LINEAR);

    /*dump image data*/
    dst_plane_num = bm_image_get_plane_num(dst_img[0]);

    if(dst_num_comp != dst_plane_num){
        printf("failed!dst plane num is not equal to dst comp num!\n ");
        return -1;
    }


    bm_device_mem_t dst_device_mem[4];
    char out_name[100];

    for (output_idx = 0; output_idx < crop_num; output_idx++) {
        /*get device mem*/
        if (bm_image_get_device_mem(dst_img[output_idx], dst_device_mem) != BM_SUCCESS) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR,
                  "bm_image_get_device_mem err %s: %s: %d\n",
                  __FILE__, __func__, __LINE__);
            ret = -1;
            goto exit;
        }

        /*dump data from device mem to host mem and write file*/
        for (plane_idx = 0; plane_idx < dst_plane_num; plane_idx++) {
            /*dump data from device mem*/
            dst_img_buf_va[output_idx][plane_idx] = malloc(dst_len[plane_idx]);

            ret = bm_memcpy_d2s(handle, dst_img_buf_va[output_idx][plane_idx], dst_device_mem[plane_idx]);
            if (ret != BM_SUCCESS) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR,
                  "CDMA transfer from system to device failed, ret = %d\n", ret);
                ret = -1;
                goto exit;
            }

            /*write output image to file*/
            if (src_fmt == FORMAT_COMPRESSED) {
                if (img_name != NULL) {
                    sprintf(out_name, "%s%s%d%s%d%s", img_name, "_border_output_",
                        output_idx, "_plane_", plane_idx, ".dat");
                } else {
                    sprintf(out_name, "%s%d%s%d%s", "border_output_", output_idx, "_plane_", plane_idx, ".dat");
                }
            } else {
                sprintf(out_name, "%s%d%s%d%s", "border_output_", output_idx, "_plane_", plane_idx, ".dat");
            }

            FILE *fp_dst = fopen(out_name, "wb");
            if (!fp_dst) {
                bmlib_log("BMCV", BMLIB_LOG_ERROR,
                  "ERROR: Unable to open the output file: %s\n", out_name);
                ret = -1;
                goto exit;
            }

            if (fwrite(dst_img_buf_va[output_idx][plane_idx], 1, dst_len[plane_idx], fp_dst) !=
                (unsigned int)(dst_len[plane_idx])) {
                fclose(fp_dst);
                return BM_ERR_FAILURE;
            }
            fclose(fp_dst);
        }
    }
exit:
    bm_image_destroy(src_img);

    for (plane_idx = 0; plane_idx < 4; plane_idx++) {
        if (src_img_buf_va[plane_idx] != NULL)
        free(src_img_buf_va[plane_idx]);
    }

    for (output_idx = 0; output_idx < crop_num; output_idx++) {
        bm_image_destroy(dst_img[output_idx]);

        for (plane_idx = 0; plane_idx < 4; plane_idx++) {
            if (dst_img_buf_va[output_idx][plane_idx] != NULL)
            free(dst_img_buf_va[output_idx][plane_idx]);
       }
    }

#else
    printf("cmodel not supported yet!\n");
    UNUSED(argc);
    UNUSED(argv);
#endif
    return ret;
}

