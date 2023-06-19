#ifndef SOC_MODE
#define PCIE_MODE
#endif

#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
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

#define MAX_RECT_NUM_TEST 4
#define MAT_ALIGN 64
#define u8 uint8_t
#define CMODEL_NOT_SUPPORT -8

using namespace std;
typedef unsigned char u8;

struct thread_arg{
    int test_loop_times;
    int devid;
    int thread_id;
};

int test_seed = -1;

static int data_init(vpp_mat_s* mat,
    u8 ***data,
    u8 ***data_expand,
    bm_image_format_ext image_format,
    int h,
    int w,
    int &plane_num,
    int *plane_len,   //these plane size are for bm_image
    int *plane_width,
    int *plane_height,
    int *mat_stride
    )
{
    int i;
    switch(image_format)
    {
        case FORMAT_YUV420P:
            mat->format = FMT_I420;
            mat->num_comp = 3;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = (h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0] >> 1;
            mat_stride[2] = mat_stride[1];
            plane_num = 3;
            plane_width[0] = w;
            plane_height[0] = h;
            plane_width[1] = (w >> 1);
            plane_height[1] = (h >> 1);
            plane_width[2] = (w >> 1);
            plane_height[2] = (h >> 1);
            plane_len[0] = plane_width[0] * plane_height[0];
            plane_len[1] = plane_width[1] * plane_height[1];
            plane_len[2] = plane_width[2] * plane_height[2];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (*data_expand)[1];
            mat->va[2] = (*data_expand)[2];
            break;
        case FORMAT_GRAY:
            mat->format = FMT_Y;
            mat->num_comp = 1;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat_stride[0] = mat->stride;
            plane_num = 1;
            plane_width[0] = w;
            plane_height[0] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            break;
        case FORMAT_YUV444P:
            mat->format = FMT_YUV444P;
            mat->num_comp = 3;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = mat->stride * h;
            mat->ion_len[2] = mat->stride * h;
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0];
            mat_stride[2] = mat_stride[1];
            plane_num = 3;
            plane_width[0] = w;
            plane_height[0] = h;
            plane_width[1] = w;
            plane_height[1] = h;
            plane_width[2] = w;
            plane_height[2] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            plane_len[1] = plane_width[1] * plane_height[1];
            plane_len[2] = plane_width[2] * plane_height[2];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (*data_expand)[1];
            mat->va[2] = (*data_expand)[2];
            break;
        case FORMAT_RGB_PLANAR:
            mat->format = FMT_RGBP;
            mat->num_comp = 3;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = mat->stride * h;
            mat->ion_len[2] = mat->stride * h;
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0];
            mat_stride[2] = mat_stride[1];
            plane_num = 1;
            plane_width[0] = w;
            plane_height[0] = h * 3;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[3 * (mat->ion_len[i])];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (void *)((unsigned long long)mat->va[0]
                         + mat->ion_len[0]);
            mat->va[2] = (void *)((unsigned long long)mat->va[1]
                         + mat->ion_len[1]);
            break;
        case FORMAT_BGR_PLANAR:
            mat->format = FMT_BGRP;
            mat->num_comp = 3;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = mat->stride * h;
            mat->ion_len[2] = mat->stride * h;
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0];
            mat_stride[2] = mat_stride[1];
            plane_num = 1;
            plane_width[0] = w;
            plane_height[0] = h * 3;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[3 * (mat->ion_len[i])];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (void *)((unsigned long long)mat->va[0]
                         + mat->ion_len[0]);
            mat->va[2] = (void *)((unsigned long long)mat->va[1]
                         + mat->ion_len[1]);
            break;
        case FORMAT_RGBP_SEPARATE:
            mat->format = FMT_RGBP;
            mat->num_comp = 3;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = mat->stride * h;
            mat->ion_len[2] = mat->stride * h;
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0];
            mat_stride[2] = mat_stride[1];
            plane_num = 3;
            plane_width[0] = w;
            plane_height[0] = h;
            plane_width[1] = w;
            plane_height[1] = h;
            plane_width[2] = w;
            plane_height[2] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            plane_len[1] = plane_width[1] * plane_height[1];
            plane_len[2] = plane_width[2] * plane_height[2];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[(mat->ion_len[i])];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (*data_expand)[1];
            mat->va[2] = (*data_expand)[2];
            break;
        case FORMAT_BGRP_SEPARATE:
            mat->format = FMT_BGRP;
            mat->num_comp = 3;
            mat->stride = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = mat->stride * h;
            mat->ion_len[2] = mat->stride * h;
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0];
            mat_stride[2] = mat_stride[1];
            plane_num = 3;
            plane_width[0] = w;
            plane_height[0] = h;
            plane_width[1] = w;
            plane_height[1] = h;
            plane_width[2] = w;
            plane_height[2] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            plane_len[1] = plane_width[1] * plane_height[1];
            plane_len[2] = plane_width[2] * plane_height[2];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (*data_expand)[1];
            mat->va[2] = (*data_expand)[2];
            break;
        case FORMAT_BGR_PACKED:
            mat->format = FMT_BGR;
            mat->num_comp = 1;
            mat->stride = ALIGN(w * 3, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat_stride[0] = mat->stride;
            plane_num = 1;
            plane_width[0] = w * 3;
            plane_height[0] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            break;
        case FORMAT_RGB_PACKED:
            mat->format = FMT_RGB;
            mat->num_comp = 1;
            mat->stride = ALIGN(w * 3, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat_stride[0] = mat->stride;
            plane_num = 1;
            plane_width[0] = w * 3;
            plane_height[0] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            break;
        case FORMAT_NV12:
            mat->format = FMT_NV12;
            mat->num_comp = 2;
            mat->stride     = ALIGN(w, MAT_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = mat->stride * (h >> 1);
            mat_stride[0] = mat->stride;
            mat_stride[1] = mat_stride[0];
            plane_num = 2;
            plane_height[0] = h;
            plane_width[0] = w;
            plane_height[1] = ALIGN(h, 2) / 2;
            plane_width[1] = ALIGN(w, 2);
            plane_len[0] = plane_width[0] * plane_height[0];
            plane_len[1] = plane_width[1] * plane_height[1];
            *data = new u8 *[plane_num];
            *data_expand = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                (*data_expand)[i] = new u8[mat->ion_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
                memset((*data_expand)[i], 0x0, mat->ion_len[i]);
            }
            mat->va[0] = (*data_expand)[0];
            mat->va[1] = (*data_expand)[1];
            break;
        default:
            printf("%s:%d[%s] vpp not support this format.\n",__FILE__, __LINE__, __FUNCTION__);
            return -1;
    }
    return 0;
}

static int test_vpp_random_cmp(u8 ***hw_rslt,
                               u8 ***ref_rslt,
                               int rect_num,
                               int plane_num,
                               int **plane_len,
                               int alg)
{
    int i;
    int j;
    int k;
    for (i = 0; i < rect_num; i++) {
        for (j = 0; j < plane_num; j++) {
            for (k = 0; k < plane_len[i][j]; k++) {
                if (alg == 1) {
                    if (hw_rslt[i][j][k] != ref_rslt[i][j][k]) {
                        cout << "cmp error at image "
                            << i << " plane "
                            << j << " element "
                            << k << " plane_len "
                            << plane_len[i][j] << " exp "<<hex
                            << (int)ref_rslt[i][j][k] << " got "<<hex
                            << (int)hw_rslt[i][j][k] << endl;
                            exit(-1);
                    }
                } else if (alg == 0) {
                     if (((int)hw_rslt[i][j][k] - (int)ref_rslt[i][j][k]) > 1
                        || ((int)ref_rslt[i][j][k] - (int)ref_rslt[i][j][k] > 1)) {
                        cout << "cmp error at image "
                            << i << " plane "
                            << j << " element "
                            << k << " plane_len "
                            << plane_len[i][j] << " exp "<<hex
                            << (int)ref_rslt[i][j][k] << " got "<<hex
                            << (int)hw_rslt[i][j][k] << endl;
                            exit(-1);
                     }
                } else {
                     cout << "alg not supported!" << endl;
                }
            }
        }
    }
    cout << "cmp success!" << endl;
    return 1;
}

static int test_vpp_random(bm_handle_t handle,
    bm_image_format_ext image_format_in,
    bm_image_format_ext image_format_out,
    unsigned seed, int thread_id) {

    int i;
    int j;
    int k;
    //loop cnt above
    int h_in;
    int w_in;
    int *h_out;
    int *w_out;
    bm_image image_in;
    bm_image *image_out;
    u8 **org_img;
    u8 **org_img_expand;
    u8 ***ref_rslt;
    u8 ***ref_rslt_expand;
    u8 ***hw_rslt;
    int rect_num;
    bmcv_rect_t *rect;
    vpp_rect_s *rect_vpp;
    int plane_height_in[3];
    int plane_width_in[3];
    int plane_len_in[3];
    int mat_stride_in[3];
    int **mat_stride_out;
    int **plane_height_out;
    int **plane_width_out;
    int **plane_len_out;
    int plane_num_in;
    int plane_num_out;
    vpp_mat_s mat_in;
    vpp_mat_s* mat_out;
    int *w_scale;
    int *h_scale;
    bm_status_t ret;
    int alg;
    int cmodel_ret;
    (void)thread_id;

    srand(seed);
    alg = rand() % 2;
    h_in = (rand() % 540 + 28) * 2;
    w_in = (rand() % 960 + 28) * 2;
    cout << "draw_rect_test start! " <<endl;
    cout << "image_format_in is " << image_format_in <<endl;
    cout << "image_format_out is " << image_format_out << endl;
    cout << "h_in = " << h_in << endl;
    cout << "w_in = " << w_in << endl;

    memset(&mat_in, 0x0, sizeof(vpp_mat_s));
    mat_in.cols = w_in;
    mat_in.rows = h_in;
    mat_in.uv_stride = 0;
    mat_in.axisX = 0;
    mat_in.axisY = 0;
    mat_in.is_pa = 0;
    data_init(&mat_in, &org_img, &org_img_expand,
                  image_format_in, h_in, w_in, plane_num_in,
                  plane_len_in, plane_width_in,
                  plane_height_in, mat_stride_in);


    for (i = 0; i < plane_num_in; i++) {
        for (j = 0; j < plane_len_in[i]; j++) {
            org_img[i][j] = rand() % 255 + 1;
        }

        for (k = 0; k < plane_height_in[i]; k++) {
            memcpy((void *)(org_img_expand[i]
                + k * mat_stride_in[i]),
                (void *)(org_img[i] + k * plane_width_in[i]),
                (unsigned)plane_width_in[i]);
        }
    }

    rect_num = rand() % MAX_RECT_NUM_TEST + 1;
    image_out = new bm_image[rect_num];
    ref_rslt = new u8 **[rect_num];
    ref_rslt_expand = new u8 **[rect_num];
    hw_rslt = new u8 **[rect_num];
    mat_out = new vpp_mat_s[rect_num];
    rect = new bmcv_rect_t[rect_num];
    rect_vpp = new vpp_rect_s[rect_num];
    w_out = new int[rect_num];
    h_out = new int[rect_num];
    mat_stride_out = new int *[rect_num];
    plane_height_out = new int *[rect_num];
    plane_width_out = new int *[rect_num];
    plane_len_out = new int *[rect_num];
    for (i = 0; i < rect_num; i++) {
        mat_stride_out[i] = new int[3];
        plane_height_out[i] = new int[3];
        plane_width_out[i] =new int[3];
        plane_len_out[i] = new int[3];
    }
    memset(rect, 0x0, sizeof(bmcv_rect_t) * rect_num);
    memset(rect_vpp, 0x0, sizeof(vpp_rect_s) * rect_num);
    for (i = 0; i < rect_num; i++) {
        rect[i].start_x = rand() % (w_in - 54) + 18;
        rect[i].start_y = rand() % (h_in - 54) + 18;
        rect[i].crop_w = rand() % (w_in - rect[i].start_x - 36) + 18;
        rect[i].crop_h = rand() % (h_in - rect[i].start_y - 36) + 18;
        rect[i].start_x = ALIGN(rect[i].start_x, 2);
        rect[i].start_y = ALIGN(rect[i].start_y, 2);
        rect[i].crop_w = ALIGN(rect[i].crop_w, 2);
        rect[i].crop_h = ALIGN(rect[i].crop_h, 2);
        if (rect[i].crop_w < 16)
            rect[i].crop_w = 16;
        if (rect[i].crop_w > 4096)
            rect[i].crop_w = 4096;
        if (rect[i].crop_h < 16)
            rect[i].crop_h =16;
        if (rect[i].crop_h > 4096)
            rect[i].crop_h =4096;
        cout << "rect[" << i << "].start_x = "
            << rect[i].start_x << endl;
        cout << "rect[" << i << "].start_y = "
            << rect[i].start_y << endl;
        cout << "rect[" << i << "].crop_w = "
            << rect[i].crop_w << endl;
        cout << "rect[" << i << "].crop_h = "
            << rect[i].crop_h << endl;

        rect_vpp[i].x = rect[i].start_x;
        rect_vpp[i].y = rect[i].start_y;
        rect_vpp[i].width = rect[i].crop_w;
        rect_vpp[i].height = rect[i].crop_h;
    }

    w_scale = new int[rect_num];
    h_scale = new int[rect_num];
    for (i = 0; i < rect_num; i++) {
        w_scale[i] = rand() % 64 + 1;
        h_scale[i] = rand() % 64 + 1;
        w_out[i] = rect[i].crop_w * w_scale[i] / 8;
        h_out[i] = rect[i].crop_h * h_scale[i] / 8;
        w_out[i] = ALIGN(w_out[i], 2);
        h_out[i] = ALIGN(h_out[i], 2);
        if (w_out[i] > 4096)
            w_out[i] = 4096;
        if (w_out[i] < 16)
            w_out[i] = 16;
        if (h_out[i] > 4096)
            h_out[i] = 4096;
        if (h_out[i] < 16)
            h_out[i] = 16;
        cout << "w_out[" << i << "] = "
            << w_out[i] << endl;
        cout << "h_out[" << i << "] = "
            << h_out[i] << endl;
    }

    for (i = 0; i < rect_num; i++) {
        memset(&(mat_out[i]), 0x0, sizeof(vpp_mat_s));
        mat_out[i].cols = w_out[i];
        mat_out[i].rows = h_out[i];
        mat_out[i].is_pa = 0;
        mat_out[i].axisX = 0;
        mat_out[i].axisY = 0;
        mat_out[i].uv_stride = 0;
        data_init(&(mat_out[i]), &(ref_rslt[i]), &(ref_rslt_expand[i]),
                      image_format_out, h_out[i], w_out[i],
                      plane_num_out, plane_len_out[i],
                      plane_width_out[i], plane_height_out[i],
                      mat_stride_out[i]);
        cout << "mat_out.startx[" << i << "] = "
            << mat_out[i].axisX << endl;
        cout << "mat_out.starty[" << i << "] = "
            << mat_out[i].axisY << endl;
    }

    for (i = 0; i < rect_num; i++) {
        hw_rslt[i] = new u8 *[plane_num_out];
        for (j = 0; j < plane_num_out; j++) {
            hw_rslt[i][j] = new u8[plane_len_out[i][j]];
        }
    }
    bm_image_create(handle,
                    h_in,
                    w_in,
                    (bm_image_format_ext)image_format_in,
                    (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
                    &image_in);
    for (i = 0; i < rect_num; i++) {
        bm_image_create(handle,
                        h_out[i],
                        w_out[i],
                        (bm_image_format_ext)image_format_out,
                        (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
                        &image_out[i]);
        /*alloc mem for image out*/
        bm_image_alloc_dev_mem(image_out[i], BMCV_HEAP_ANY);  //out ddr0 first
        //bm_image_alloc_dev_mem_heap_mask(image_out[i], 6);  //out ddr2
    }

    bm_image_alloc_dev_mem(image_in);
    bm_image_copy_host_to_device(image_in, (void **)org_img);

    if (alg == 1) {
        ret = bmcv_image_vpp_convert(handle, rect_num,
            image_in, image_out, rect, BMCV_INTER_NEAREST);
    } else {
        ret = bmcv_image_vpp_convert(handle, rect_num,
            image_in, image_out, rect, BMCV_INTER_LINEAR);
    }

    if(BM_SUCCESS != ret) {
        delete[] rect;
        delete[] rect_vpp;
        delete[] w_scale;
        delete[] h_scale;
        delete[] w_out;
        delete[] h_out;
        delete[] mat_out;
        for (i = 0; i < rect_num; i++) {
            for (j = 0; j < plane_num_out; j++) {
                delete[] ref_rslt[i][j];
                delete[] ref_rslt_expand[i][j];
                delete[] hw_rslt[i][j];
            }
            delete[] ref_rslt[i];
            delete[] ref_rslt_expand[i];
            delete[] hw_rslt[i];
            delete[] mat_stride_out[i];
            delete[] plane_height_out[i];
            delete[] plane_width_out[i];
            delete[] plane_len_out[i];
        }
        for (i = 0; i < plane_num_in; i++) {
            delete[] org_img[i];
            delete[] org_img_expand[i];
        }
        delete[] org_img;
        delete[] org_img_expand;
        delete[] ref_rslt;
        delete[] ref_rslt_expand;
        delete[] hw_rslt;
        delete[] mat_stride_out;
        delete[] plane_height_out;
        delete[] plane_width_out;
        delete[] plane_len_out;
        bm_image_destroy(image_in);
        for (i = 0; i <rect_num; i++) {
            bm_image_destroy(image_out[i]);
        }
        delete[] image_out;
        if (ret != BM_NOT_SUPPORTED) {
            std::cout << "bmcv_api running failed!" << std::endl;
            exit(-1);
        }
        std::cout << "random input not support! nerver mind, just do it again!"
                 << endl;
        seed = seed + 1;
        return 0;
    }

    for (i = 0; i < rect_num; i++) {
        bm_image_copy_device_to_host(image_out[i], (void **)hw_rslt[i]);
    }

    for (i = 0; i < rect_num; i++) {
        if (alg == 1) {
            cmodel_ret = vpp_misc_cmodel(&mat_in, rect_vpp + i, &(mat_out[i]), 1,
                CSC_MAX, VPP_SCALE_NEAREST);
        } else {
            cmodel_ret = vpp_misc_cmodel(&mat_in, rect_vpp + i, &(mat_out[i]), 1,
                CSC_MAX, VPP_SCALE_BILINEAR);
        }
        if (CMODEL_NOT_SUPPORT == cmodel_ret) {
            std::cout << "cmodel running failed!" << std::endl;
            delete[] rect;
            delete[] rect_vpp;
            delete[] w_scale;
            delete[] h_scale;
            delete[] w_out;
            delete[] h_out;
            delete[] mat_out;
            for (i = 0; i < rect_num; i++) {
                for (j = 0; j < plane_num_out; j++) {
                    delete[] ref_rslt[i][j];
                    delete[] ref_rslt_expand[i][j];
                    delete[] hw_rslt[i][j];
                }
                delete[] ref_rslt[i];
                delete[] ref_rslt_expand[i];
                delete[] hw_rslt[i];
                delete[] mat_stride_out[i];
                delete[] plane_height_out[i];
                delete[] plane_width_out[i];
                delete[] plane_len_out[i];
            }
            for (i = 0; i < plane_num_in; i++) {
                delete[] org_img[i];
                delete[] org_img_expand[i];
            }
            delete[] org_img;
            delete[] org_img_expand;
            delete[] ref_rslt;
            delete[] ref_rslt_expand;
            delete[] hw_rslt;
            delete[] mat_stride_out;
            delete[] plane_height_out;
            delete[] plane_width_out;
            bm_image_destroy(image_in);
            for (i = 0; i <rect_num; i++) {
                bm_image_destroy(image_out[i]);
            }
            delete[] image_out;
            delete[] plane_len_out;
            std::cout << "random input not support! nerver mind, just do it again!"
                     << endl;
            seed = seed + 1;
            return 0;
        }
        for (j = 0; j < plane_num_out; j++) {
            for (k = 0; k < plane_height_out[i][j]; k++) {
                memcpy((void *)(ref_rslt[i][j] + plane_width_out[i][j] * k),
                    (void *)(ref_rslt_expand[i][j] + mat_stride_out[i][j] * k),
                    (unsigned)plane_width_out[i][j]);
            }
        }
    }

    test_vpp_random_cmp(hw_rslt, ref_rslt, rect_num,
                        plane_num_out, plane_len_out, alg);

    delete[] rect;
    delete[] rect_vpp;
    delete[] w_scale;
    delete[] h_scale;
    delete[] w_out;
    delete[] h_out;
    delete[] mat_out;
    for (i = 0; i < rect_num; i++) {
        for (j = 0; j < plane_num_out; j++) {
            delete[] ref_rslt[i][j];
            delete[] ref_rslt_expand[i][j];
            delete[] hw_rslt[i][j];
        }
        delete[] ref_rslt[i];
        delete[] ref_rslt_expand[i];
        delete[] hw_rslt[i];
        delete[] mat_stride_out[i];
        delete[] plane_height_out[i];
        delete[] plane_width_out[i];
        delete[] plane_len_out[i];
    }
    for (i = 0; i < plane_num_in; i++) {
        delete[] org_img[i];
        delete[] org_img_expand[i];
    }
    delete[] org_img;
    delete[] org_img_expand;
    delete[] ref_rslt;
    delete[] ref_rslt_expand;
    delete[] hw_rslt;
    delete[] mat_stride_out;
    delete[] plane_height_out;
    delete[] plane_width_out;
    delete[] plane_len_out;
    bm_image_destroy(image_in);
    for (i = 0; i <rect_num; i++) {
        bm_image_destroy(image_out[i]);
    }
    delete[] image_out;

    return 0;
}

#ifdef __linux__
static void *test_vpp_random_thread(void *arg) {
#else
DWORD WINAPI test_vpp_random_thread(LPVOID arg) {
#endif

    int i;
    int loop_times;
    unsigned int seed;
    bm_handle_t handle;
    int dev;
    int thread_id = 0;

    loop_times = ((struct thread_arg *)arg)->test_loop_times;
    dev = ((struct thread_arg *)arg)->devid;
    thread_id = ((struct thread_arg *)arg)->thread_id;

    std::cout << "dev_id is " << dev <<endl;
    bm_dev_request(&handle, dev);
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST CV VPP RANDOM] test starts... LOOP times will be "
              << loop_times << std::endl;

    for(i = 0; i < loop_times; i++) {
        std::cout << "------[TEST CV VPP RANDOM] LOOP " << i << "------"
                  << std::endl;
        if (test_seed == -1) {
            seed = (unsigned)time(NULL);
        } else {
            seed = test_seed;
        }
        std::cout << "seed: " << seed << std::endl;
        srand(seed);

        cout << "start of loop test " << i << endl;
        test_vpp_random(handle, FORMAT_BGR_PLANAR, FORMAT_RGB_PACKED, seed, thread_id);
        test_vpp_random(handle, FORMAT_RGBP_SEPARATE, FORMAT_RGB_PACKED, seed, thread_id);
        test_vpp_random(handle, FORMAT_NV12, FORMAT_YUV420P, seed, thread_id);

    }
    bm_dev_free(handle);
    return 0;
}
#endif

int main(int argc, char **argv) {
#ifndef USING_CMODEL
    int test_loop_times  = 0;
    int devid = 0;
    int test_threads_num = 1;
    char * pEnd;

    if (argc == 1) {
        test_loop_times  = 1;
        test_threads_num = 1;
    } else if (argc == 2) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = 1;
    } else if (argc == 3) {
        test_loop_times  = atoi(argv[1]);
        devid = atoi(argv[2]);
    } else if (argc == 4) {
        test_loop_times  = atoi(argv[1]);
        devid = atoi(argv[2]);
        test_threads_num = atoi(argv[3]);
    } else if (argc == 5) {
        test_loop_times  = atoi(argv[1]);
        devid = atoi(argv[2]);
        test_threads_num = atoi(argv[3]);
        test_seed = strtol(argv[4],&pEnd,0);
    } else {
        std::cout << "command input error, please follow this "
                     "order:test_cv_vpp_random loop_num devid multi_thread_num seed"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500000 || test_loop_times < 1) {
        std::cout << "[TEST CV VPP RANDOM] loop times should be 1~15000" << std::endl;
        exit(-1);
    }
    if (devid > 255 || devid < 0) {
        std::cout << "[TEST CV VPP RANDOM] devid times should be 0~255" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 32 || test_threads_num < 1) {
        std::cout << "[TEST CV VPP RANDOM] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    if (devid < 0 || devid > 255) {
        std::cout << "[TEST CV VPP RANDOM] devid should be 0~255";
    }
    #ifdef __linux__
    pthread_t *          pid = new pthread_t[test_threads_num];
        struct thread_arg arg[test_threads_num];

    for (int i = 0; i < test_threads_num; i++) {
         arg[i].test_loop_times = test_loop_times;
         arg[i].devid = devid;
         arg[i].thread_id = i;
        if (pthread_create(
                &pid[i], NULL, test_vpp_random_thread, (void *)(&arg[i]))) {
            delete[] pid;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    #else
#define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    struct thread_arg*  arg = new thread_arg[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        arg[i].test_loop_times    = test_loop_times;
        arg[i].devid              = devid;
        arg[i].thread_id          = i;
        hThreadArray[i] = CreateThread(
            NULL,                    // default security attributes
            0,                       // use default stack size
            test_vpp_random_thread,  // thread function name
            (void *)(&arg[i]),       // argument to thread function
            0,                       // use default creation flags
            &dwThreadIdArray[i]);    // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int   ret = 0;
    DWORD dwWaitResult =
        WaitForMultipleObjects(test_threads_num, hThreadArray, TRUE, INFINITE);
    // DWORD dwWaitResult = WaitForSingleObject(hThreadArray[i], INFINITE);
    switch (dwWaitResult) {
        // case WAIT_OBJECT_0:
        //    ret = 0;
        //    break;
        case WAIT_FAILED:
            ret = -1;
            break;
        case WAIT_ABANDONED:
            ret = -2;
            break;
        case WAIT_TIMEOUT:
            ret = -3;
            break;
        default:
            ret = 0;
            break;
    }
    if (ret < 0) {
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    delete[] arg;
    #endif
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
#else
    printf("cmodel not supported yet!\n");
    UNUSED(argc);
    UNUSED(argv);
#endif
    return 0;
}

