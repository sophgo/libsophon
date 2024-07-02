#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <type_traits>
#include <cstring>
#include "test_misc.h"
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
extern void bm1684x_vpp_read_bin(bm_image src, const char *input_name);
extern void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);
extern void format_to_str(bm_image_format_ext format, char* res);

int main(int argc, char **argv) {
    bm_handle_t handle = NULL;
    int img_num = 1;
    int src_h = 1080, src_w = 1920, dst_w = 640, dst_h = 640;
    bm_image_format_ext src_fmt = FORMAT_YUV420P, dst_fmt = FORMAT_RGB_PACKED;
    char *src_name = NULL, *dst_name = NULL, *dst_csv_name = NULL;
    bm_image src, dst;
    int crop_num_vec = 1;
    bmcv_rect_t rect;
    bmcv_padding_atrr_t padding_attr;
    bmcv_resize_algorithm algorithm;
    csc_type_t csc_type;
    csc_matrix_t matrix;
    bmcv_convert_to_attr * convert_to_attr = NULL;
    int convert_to_need;

    unsigned int i = 0, loop_time = 0;
    unsigned long long time_single, time_total = 0, time_avg = 0;
    unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;
    int dev_id = 0;

    bm_status_t ret = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }

    #ifdef __linux__
        struct timeval tv_start;
        struct timeval tv_end;
        struct timeval timediff;
    #endif

    if (argc != 21) {
        printf("usage: %d\n", argc);
        printf("%s img_num src_w src_h src_fmt src_name \
        dst_start_x dst_start_y dst_crop_w dst_crop_h \
        crop_num_vec algorithm csc_type \
        dst_w dst_h dst_fmt dst_name loop_time dst_csv_name dev_id\n", argv[0]);
        printf("example:\n");
        printf("FORMAT_YUV420P-->FORMAT_RGB_PACKED:\n");
        printf("%s 1 1920 1080 0 i420.yuv 0 0 480 480 1 1 0 640 640 10 output.rgb 0 1 output.csv 0\n", argv[0]);
        return 0;
    }

    img_num = atoi(argv[1]);
    src_w = atoi(argv[2]);
    src_h = atoi(argv[3]);
    src_fmt = (bm_image_format_ext)atoi(argv[4]);
    src_name = argv[5];

    rect.start_x = 0;
    rect.start_y = 0;
    rect.crop_w = src_w;
    rect.crop_h = src_h;

    padding_attr.dst_crop_stx = atoi(argv[6]);
    padding_attr.dst_crop_sty = atoi(argv[7]);
    padding_attr.dst_crop_w = atoi(argv[8]);
    padding_attr.dst_crop_h = atoi(argv[9]);
    padding_attr.if_memset = 1;
    padding_attr.padding_b = 255;
    padding_attr.padding_g = 255;
    padding_attr.padding_r = 255;
    crop_num_vec = atoi(argv[10]);
    algorithm = (bmcv_resize_algorithm)atoi(argv[11]);
    csc_type = (csc_type_t)atoi(argv[12]);

    dst_w = atoi(argv[13]);
    dst_h = atoi(argv[14]);
    dst_fmt = (bm_image_format_ext)atoi(argv[15]);
    dst_name = argv[16];
    convert_to_need = atoi(argv[17]);
    loop_time = atoi(argv[18]);
    dst_csv_name = argv[19];
    dev_id  = atoi(argv[20]);
    memset(&matrix, 0, sizeof(matrix));
    matrix.csc_add0 = 128 << 10;
    matrix.csc_add1 = 128 << 10;
    matrix.csc_add2 = 128 << 10;
    if (convert_to_need != 0) {
        convert_to_attr = new bmcv_convert_to_attr;
        convert_to_attr->alpha_0 = ((float)(rand() % 20)) / 10.0f;
        convert_to_attr->alpha_1 = ((float)(rand() % 20)) / 10.0f;
        convert_to_attr->alpha_2 = ((float)(rand() % 20)) / 10.0f;
        convert_to_attr->beta_0 = ((float)(rand() % 20)) / 10.0f - 1;
        convert_to_attr->beta_1 = ((float)(rand() % 20)) / 10.0f - 1;
        convert_to_attr->beta_2 = ((float)(rand() % 20)) / 10.0f - 1;
    } else {
        convert_to_attr = NULL;
    }

    bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
    bm_image_alloc_dev_mem(src, 1);
    bm1684x_vpp_read_bin(src,src_name);
    bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst);
    bm_image_alloc_dev_mem(dst, 1);

    for(i = 0;i < loop_time; i++){

#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif

    bmcv_image_csc_convert_to(handle, img_num, &src ,&dst, &crop_num_vec, &rect, &padding_attr, algorithm, csc_type, &matrix, convert_to_attr);

#ifdef __linux__
    gettimeofday(&tv_end, NULL);
    timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
    timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
    time_single = (unsigned int)(timediff.tv_sec * 1000000 + timediff.tv_usec);
#endif

    if(time_single>time_max){time_max = time_single;}
    if(time_single<time_min){time_min = time_single;}
    time_total = time_total + time_single;
    }
    time_avg = time_total / loop_time;
    fps = 1000000 *2 / time_avg;
    pixel_per_sec = src_w * src_h * fps/1024/1024;

    bm1684x_vpp_write_bin(dst, dst_name);
    bm_image_destroy(src);
    bm_image_destroy(dst);
    if (convert_to_need != 0) {
        delete convert_to_attr;
        convert_to_attr = NULL;
    }
    bm_dev_free(handle);

    char src_fmt_str[100],dst_fmt_str[100];
    format_to_str(src.image_format, src_fmt_str);
    format_to_str(dst.image_format, dst_fmt_str);


    printf("%d*%d->%d*%d, %s->%s\n",src_w,src_h,dst_w,dst_h,src_fmt_str,dst_fmt_str);
    printf("bmcv_image_csc_convert_to:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

    bmlib_log("BMCV",BMLIB_LOG_TRACE, "loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);

    FILE *fp_csv = fopen(dst_csv_name, "ab+");
    fprintf(fp_csv, "%s, %lld, %lld, %lldM\n",src_fmt_str,time_avg, fps, pixel_per_sec);

    fclose(fp_csv);

    return 0;

}



















































