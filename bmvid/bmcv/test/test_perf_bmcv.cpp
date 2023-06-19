#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <set>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "bmlib_runtime.h"

#define YUV2RGB 1
#define WARP_AFFINE 2
#define RESIZE 3
#define CONVERT_TO 4
#define NMS 5
#define STORAGE_CONVERT 6
#define VPP_CONVERT 7
#define JPEG_ENCODE 8
#define JPEG_DECODE 9
#define COPY_TO 10
#define SORT 11
#define DRAW_RECTANGLE 12
#define FEATURE_MATCH 13

typedef unsigned char u8;
using namespace std;

typedef float bm_gen_proposal_data_type_t;
class gen_prop_comp {
   public:
    vector<face_rect_t> nequal_ref_vec;
    vector<face_rect_t> nequal_res_vec;
    bool compare_face_rect(const face_rect_t &ref, const face_rect_t &res);
    bool result_compare(vector<face_rect_t> &ref, m_proposal_t *p_result);
};

static int img_data_init(
    u8 ***data,
    bm_image_format_ext image_format,
    int h,
    int w
    )
{
    int i;
    int plane_num;
    int plane_len[4];   //these plane size are for bm_image
    int plane_width[4];
    int plane_height[4];

    switch(image_format)
    {
        case FORMAT_YUV420P:
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
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_GRAY:
            plane_num = 1;
            plane_width[0] = w;
            plane_height[0] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_YUV444P:
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
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_RGB_PLANAR:
            plane_num = 1;
            plane_width[0] = w;
            plane_height[0] = h * 3;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_BGR_PLANAR:
            plane_num = 1;
            plane_width[0] = w;
            plane_height[0] = h * 3;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_RGBP_SEPARATE:
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
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_BGRP_SEPARATE:
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
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_BGR_PACKED:
            plane_num = 1;
            plane_width[0] = w * 3;
            plane_height[0] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_RGB_PACKED:
            plane_num = 1;
            plane_width[0] = w * 3;
            plane_height[0] = h;
            plane_len[0] = plane_width[0] * plane_height[0];
            *data = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        case FORMAT_NV12:
            plane_num = 2;
            plane_height[0] = h;
            plane_width[0] = w;
            plane_height[1] = ALIGN(h, 2) / 2;
            plane_width[1] = ALIGN(w, 2);
            plane_len[0] = plane_width[0] * plane_height[0];
            plane_len[1] = plane_width[1] * plane_height[1];
            *data = new u8 *[plane_num];
            for(i = 0; i < plane_num; i++) {
                (*data)[i] = new u8[plane_len[i]];
                memset((*data)[i], 0x0, plane_len[i]);
            }
            break;
        default:
            printf("%s:%d[%s] vpp not support this format.\n",__FILE__, __LINE__, __FUNCTION__);
            return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    int parameter_opt;
    bm_image image_in;
    bm_image image_out;
    bm_image_format_ext image_format_in = FORMAT_RGB_PLANAR;
    bm_image_data_format_ext image_data_format_in = DATA_TYPE_EXT_1N_BYTE;
    bm_image_format_ext image_format_out = FORMAT_RGB_PLANAR;
    bm_image_data_format_ext image_data_format_out = DATA_TYPE_EXT_1N_BYTE;
    bm_handle_t handle;
    u8 **src_data = NULL;
    //u8 **dst_data;
    int w_in = 1920;
    int h_in = 1080;
    int w_out = 1920;
    int h_out = 1080;
    int dev;
    //UNUSED(parameter_opt);

    int img_in_flag = 0;
    int img_out_flag = 0;

    int rect_num;
    bmcv_rect_t rect;
    int line_width;
    unsigned char r, g, b;
    int input_num;
    int output_num;
    int image_num;
    bmcv_resize_t resize_img_attr;
    bmcv_resize_image resize_attr;
    bmcv_affine_image_matrix matrix_image;
    bmcv_convert_to_attr convert_to_attr;
    size_t in_size, out_size;
    //void **p_jpeg_data;
    FILE *fp;
    u8 *jpeg_data = NULL;
    bmcv_copy_to_atrr_t copy_to_attr;
    //bm_gen_proposal_data_type_t *anchor_scales;
    face_rect_t *proposal_rand = NULL;
    nms_proposal_t *nms_output_proposal = NULL;
    int proposal_size;
    float nms_threshold;
    int data_cnt = 1000;
    int sort_cnt = 10;
    float src_data_p[100];
    float src_index_p[100];
    float dst_data_p[50];
    int dst_index_p[50];
    int order = 0;
    int batch_size = 1;
    int feature_size = 512;
    int db_size = 500000;
    unsigned char *feature_src_data_p = NULL;
    unsigned char *db_data_p = NULL;
    short output_val[4];
    int output_index[4];
    bmcv_affine_matrix matrix_data;
    void *jpeg_data_encode = NULL;
    int heap_id = BMCV_HEAP1_ID;
    //std::shared_ptr<bmcv_warp_matrix> matrix_data
    //    = std::make_shared<bmcv_warp_matrix>();


    if((argc == 1) || (argc > 3)) {
        printf("testcase should be used as below!\n");
        printf("test_bmcv_perf testcase\n");
        printf("test_bmcv_perf testcase parameter\n");

        printf("1. yuv2rgb\n2.warp_affine\n3.resize\n");
        printf("4.convert_to\n5.nms\n");
        printf("6.storage_convert\n7.vpp_convert\n");
        printf("8.jpeg_encode\n9.jpeg_decode\n10.copy_to\n");
        printf("11.sort\n12.draw_rectangle\n");
        printf("13.feature_match\n");
        return 0;
    } else if (argc < 3) {
        parameter_opt = 0;
    } else {
        parameter_opt = atoi(argv[2]);
    }
    int testcase = atoi(argv[1]);
    char* img_path = NULL;
    if (testcase == 8 || testcase == 9) {
        img_path = getenv("BMCV_TEST_IMAGE_PATH");
        if (img_path == NULL) {
            printf("please set environment vairable: BMCV_TEST_IMAGE_PATH !\n");
            return 0;
        }
    }

    dev = 0;
    bm_dev_request(&handle, dev);

    image_format_in = FORMAT_BGR_PLANAR;
    image_format_out = FORMAT_BGR_PLANAR;
    image_data_format_in = DATA_TYPE_EXT_1N_BYTE;
    image_data_format_out = DATA_TYPE_EXT_1N_BYTE;
    //w_in = 1920;
    //h_in = 1080;

    switch(testcase)
    {
        case DRAW_RECTANGLE:
            rect_num = 1;
            rect.start_x = 0;
            rect.start_y = 0;
            rect.crop_w = 200;
            rect.crop_h = 300;
            line_width = 3;
            r = (unsigned char)255;
            g = (unsigned char)0;
            b = (unsigned char)0;
            img_in_flag = 1;
            img_out_flag = 1;
            break;
        case VPP_CONVERT:
            output_num = 1;
            rect.start_x = 0;
            rect.start_y = 0;
            rect.crop_w = 1920;
            rect.crop_h = 1080;
            img_in_flag = 1;
            img_out_flag = 1;
            w_out = 1920;
            h_out = 1080;
            image_format_in = FORMAT_RGB_PLANAR;
            image_format_out = FORMAT_RGB_PLANAR;

            switch (parameter_opt) {
                case 0:
                    image_format_in = FORMAT_YUV420P;
                    break;
                case 1:
                    w_out = 640;
                    h_out = 480;
                    break;
                case 2:
                    image_format_out = FORMAT_BGR_PLANAR;
                    break;
                case 3:
                    image_format_in = FORMAT_NV12;
                    w_out = 640;
                    h_out = 480;
                    break;
            }

            heap_id = BMCV_HEAP1_ID;
            break;
        case YUV2RGB:
            image_num = 1;
            img_in_flag = 1;
            img_out_flag = 1;
            image_format_in = FORMAT_NV12;
            image_format_out = FORMAT_BGR_PLANAR;
            break;
        case WARP_AFFINE:
            matrix_image.matrix_num = 1;
            matrix_image.matrix = &matrix_data;
            matrix_image.matrix->m[0] = 1;//3.848430;
            matrix_image.matrix->m[1] = 0;//-0.02484;
            matrix_image.matrix->m[2] = 0;//916.7;
            matrix_image.matrix->m[3] = 0;//0.02;
            matrix_image.matrix->m[4] = 1;//3.8484;
            matrix_image.matrix->m[5] = 0;//56.4748;
            w_out = 112;
            h_out = 112;
            img_in_flag = 1;
            img_out_flag = 1;
            break;
        case RESIZE:
            image_num = 1;
            resize_img_attr.start_x = 0;
            resize_img_attr.start_y = 0;
            resize_img_attr.in_width = 1920;
            resize_img_attr.in_height = 1080;
            resize_img_attr.out_width = 320;
            resize_img_attr.out_height = 180;
            resize_attr.resize_img_attr = &resize_img_attr;
            resize_attr.roi_num = 1;
            resize_attr.stretch_fit = 1;
            resize_attr.interpolation = BMCV_INTER_NEAREST;

            img_in_flag = 1;
            img_out_flag = 1;
            break;
        case CONVERT_TO:
            convert_to_attr.alpha_0 = 1;
            convert_to_attr.beta_0 = 128;
            convert_to_attr.alpha_1 = 1;
            convert_to_attr.beta_1 = 128;
            convert_to_attr.alpha_2 = 1;
            convert_to_attr.beta_2 = 128;
            input_num = 1;
            image_format_in = FORMAT_BGR_PLANAR;
            image_format_out = FORMAT_BGR_PLANAR;
            image_data_format_in = DATA_TYPE_EXT_1N_BYTE;
            image_data_format_out = DATA_TYPE_EXT_1N_BYTE;
            img_in_flag = 1;
            img_out_flag = 1;
            w_in = 1920;
            h_in = 1080;
            w_out = 1920;
            h_out = 1080;
            //heap_id = BMCV_HEAP0_ID;
            break;
        case STORAGE_CONVERT:
            img_in_flag = 1;
            img_out_flag = 1;
            image_num = 1;
            image_format_in = FORMAT_RGB_PLANAR;
            break;
        case JPEG_ENCODE:
            out_size = 0;
            image_num = 1;
            img_in_flag = 1;
            img_out_flag = 0;
            heap_id=BMCV_HEAP0_ID;
            w_in = 1920;
            h_in = 1088;
            image_format_in = FORMAT_YUV444P;
            if (parameter_opt == 1) printf("YUV422 not supprted yet!try parameter_opt 1 or 3/n");
            if (parameter_opt == 2) image_format_in = FORMAT_YUV420P;
            break;
        case JPEG_DECODE:
            image_format_in = FORMAT_BGR_PLANAR;
            image_format_out = FORMAT_BGR_PLANAR;
            in_size = 0;
            image_num = 1;
            fp = fopen((char*)(std::string(img_path) + std::string("/1920_420.jpg")).c_str(), "rb+");
            assert(fp != NULL);
            fseek(fp, 0, SEEK_END);
            in_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            jpeg_data = new u8[1920*1080*3*8];
            memset(jpeg_data, 0x0, 1920*1080*3*8);
            if (fread((void *)jpeg_data, in_size, 1, fp) < 1){
                printf("read %d size failed\n", (int)in_size);
            }
            fclose(fp);
            img_in_flag = 0;
            img_out_flag = 0;
            break;
        case COPY_TO:
            copy_to_attr.start_x = 0;
            copy_to_attr.start_y = 0;
            copy_to_attr.padding_r = 0;
            copy_to_attr.padding_g = 0;
            copy_to_attr.padding_b = 0;
            w_in = 400;
            h_in = 400;
            w_out = 800;
            h_out = 800;
            img_in_flag = 1;
            img_out_flag = 1;
            break;
        case NMS:
            proposal_rand = new face_rect_t[MAX_PROPOSAL_NUM];
            nms_output_proposal = new nms_proposal_t;
            proposal_size =1024;
            nms_threshold = 0.2;
            for (i = 0; i < proposal_size; i++)
            {
                proposal_rand[i].x1 = rand() % 200;
                proposal_rand[i].x2 = rand() % 200;
                proposal_rand[i].y1 = rand() % 200;
                proposal_rand[i].y2 = rand() % 200;
                proposal_rand[i].score = (float)(rand() % 100) / 100.0;
            }
            break;
        case SORT:
            for (int i = 0; i < 100; i++) {
                src_data_p[i] = rand() % 1000;
                src_index_p[i] = 100 - i;
            }
            break;
        case FEATURE_MATCH:
            sort_cnt = 1;
            feature_src_data_p = new unsigned char[batch_size * feature_size];
            db_data_p = new unsigned char[feature_size * db_size];
            for (int i = 0; i < batch_size * feature_size; i++) {
                feature_src_data_p[i] = rand() % 1000;
            }
            for (int i = 0; i < feature_size * db_size; i++) {
                db_data_p[i] = rand() % 1000;
            }
            break;
        default:
            printf("testcase not supported!\n");
            printf("testcase are as below:\n");
            printf("1. yuv2rgb\n2.warp_affine\n3.resize\n");
            printf("4.convert_to\n5.nms\n");
            printf("6.storage_convert\n7.vpp_convert\n");
            printf("8.jpeg_encode\n9.jpeg_decode\n10.copy_to\n");
            printf("11.sort\n12.draw_rectangle\n");
            printf("13.feature_match\n");
            return 0;
    }


    if (img_in_flag == 1) {
        bm_image_create(handle,
                        h_in,
                        w_in,
                        (bm_image_format_ext)image_format_in,
                        (bm_image_data_format_ext)image_data_format_in,
                        &image_in);
        img_data_init(&src_data,
                     image_format_in,
                     h_in,
                     w_in);
        if (testcase == JPEG_ENCODE) {
            switch (parameter_opt) {
                case 0:
                fp = fopen((char*)(std::string(img_path) + std::string("/444_1920x1088.yuv")).c_str(), "rb+");
                break;
                case 1:
                fp = fopen((char*)(std::string(img_path) + std::string("/422_1920x1088.yuv")).c_str(), "rb+");
                break;
                case 2:
                fp = fopen((char*)(std::string(img_path) + std::string("/420_1920x1088.yuv")).c_str(), "rb+");
                break;
                default:
                fp = fopen((char*)(std::string(img_path) + std::string("/444_1920x1088.yuv")).c_str(), "rb+");
                break;
            }
            assert(fp != NULL);
            fseek(fp, 0, SEEK_END);
            in_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            jpeg_data = new u8[1920*1080*3*8];
            memset(jpeg_data, 0x0, 1920*1080*3*8);
            if (fread((void *)jpeg_data, in_size, 1, fp) < 1)
                printf("read %d bytes failed\n", (int)in_size);
            if (parameter_opt == 2) {
                src_data[0] = jpeg_data;
                src_data[1] = jpeg_data + h_in * w_in;
                src_data[2] = jpeg_data + h_in * w_in / 4 * 5;
            } else {
                src_data[0] = jpeg_data;
                src_data[1] = jpeg_data + h_in * w_in;
                src_data[2] = jpeg_data + 2 * h_in * w_in;
            }
            //not support yuv422 yet
        }
        bm_image_alloc_dev_mem(image_in, heap_id);
        bm_image_copy_host_to_device(image_in, (void **)src_data);
    }

    if (img_out_flag == 1) {
        bm_image_create(handle,
                        h_out,
                        w_out,
                        (bm_image_format_ext)image_format_out,
                        (bm_image_data_format_ext)image_data_format_out,
                        &image_out);
        bm_image_alloc_dev_mem(image_out, BMCV_HEAP0_ID);
    }


    struct timeval t1, t2;
    gettimeofday_(&t1);
    switch (testcase)
    {
        case DRAW_RECTANGLE:
            for (int z=0; z<100; z++)
                bmcv_image_draw_rectangle(handle, image_in, rect_num, &rect, line_width, r, g, b);
            break;
        case VPP_CONVERT:
            for (int z=0; z<100; z++)
                #ifndef USING_CMODEL
                    bmcv_image_vpp_convert(handle, output_num, image_in, &image_out, &rect);
                #else
                    printf("vpp_convert is not supported upon cmodel!\n");
                #endif
            break;
        case YUV2RGB:
            for (int z=0; z<100; z++)
                bmcv_image_yuv2bgr_ext(handle, image_num, &image_in, &image_out);
            break;
        case WARP_AFFINE:
            for (int z=0; z<100; z++)
                bmcv_image_warp_affine(handle, 1, &matrix_image, &image_in, &image_out);
            break;
        case RESIZE:
            for (int z=0; z<100; z++)
                bmcv_image_resize(handle, image_num, &resize_attr, &image_in, &image_out);
            break;
        case CONVERT_TO:
            for (int z=0; z<100; z++)
                bmcv_image_convert_to(handle, input_num, convert_to_attr, &image_in, &image_out);
            break;
        case STORAGE_CONVERT:

            for (int z=0; z<100; z++)
                bmcv_image_storage_convert(handle, image_num, &image_in, &image_out);
            break;
        case JPEG_ENCODE:

            #ifndef USING_CMODEL
                for (int z=0; z<100; z++)
                    bmcv_image_jpeg_enc(handle, image_num, &image_in, &jpeg_data_encode, &out_size);
            #else
                printf("jpeg codec not supported upon cmodel!\n");
            #endif

            #ifndef USING_CMODEL
            free(jpeg_data_encode);
            #endif

            break;
        case JPEG_DECODE:
            #ifndef USING_CMODEL
                for (int z=0; z<10; z++)
                    bmcv_image_jpeg_dec(handle, (void **)&jpeg_data, &in_size, image_num, &image_out);
            #else
                printf("jpeg codec not supported upon cmodel!\n");
            #endif

            delete[] jpeg_data;
            break;
        case COPY_TO:
            for (int z = 0; z<100; z++)
                bmcv_image_copy_to(handle, copy_to_attr, image_in, image_out);
            break;
        case NMS:
            for (int z=0; z<100; z++)
                bmcv_nms(handle,
                    bm_mem_from_system(proposal_rand),
                    proposal_size,
                    nms_threshold,
                    bm_mem_from_system(nms_output_proposal));
            delete[] proposal_rand;
            delete nms_output_proposal;
            break;
        case SORT:
            for (int z=0; z<100; z++)
                bmcv_sort(handle,
                    bm_mem_from_system(src_index_p),
                    bm_mem_from_system(src_data_p),
                    data_cnt,
                    bm_mem_from_system(dst_index_p),
                    bm_mem_from_system(dst_data_p),
                    sort_cnt,
                    order,
                    true,
                    false);
            break;
        case FEATURE_MATCH:
            #ifndef USING_CMODEL
                for (int z=0; z<100; z++)
                    bmcv_feature_match(handle,
                        bm_mem_from_system(feature_src_data_p),
                        bm_mem_from_system(db_data_p),
                        bm_mem_from_system(output_val),
                        bm_mem_from_system(output_index),
                        batch_size,
                        feature_size,
                        db_size,
                        sort_cnt,
                        2);
            #else
                printf("feature match not supported on cmodel!\n");
            #endif

            delete[] feature_src_data_p;
            delete[] db_data_p;
            break;
    }
    gettimeofday_(&t2);
    cout << "time spend on api(100 round) is: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

#ifdef USING_CMODEL
    UNUSED(feature_src_data_p);
    UNUSED(db_data_p);
    UNUSED(output_val);
    UNUSED(output_index);
    UNUSED(batch_size);
    UNUSED(feature_size);
    UNUSED(db_size);
    UNUSED(output_num);
    UNUSED(out_size);
    UNUSED(jpeg_data_encode);
#endif




    if (img_in_flag == 1) {
       bm_image_destroy(image_in);
       delete[] src_data;
    }

    if (img_out_flag == 1)
       bm_image_destroy(image_out);
       //delete[] dst_data;
    return 0;
}

/*reference result
default in_w=1920 in_h=1080 out_w=1920 out_h=1080
notice that disturbance can be about 150us
default is from heap1 to heap0
default image format for both in and out is bgr_Planar
|--------------------------------------------------------------------
|item(api)            |res(us)   |parameter specific
|yuv2rgb              |1361      |
|warp_affine          |1524      |
|resize               |1497      |out=320*180
|nms                  |11032     |len=1024
|storage_convert      |1609      |
|vpp_convert          |1236      |
|convert_to           |1764      |1920*1080
|jpeg_encode          |7289      |heap0, yuv420
|jpeg_decode          |3848      |heap0, yuv444
|copy_to              |92        |400*400->800*800
|sort                 |160       |data_cnt =1000,sort_cnt=50
|draw_rectangle       |535       |rect=1
|feature_match        |137416    |sort_cnt=10 db_size=5*10^5
*/
