#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <wchar.h>
#include <locale.h>
#ifdef __linux__
#include <sys/time.h>
#endif
#include <bmcv_api_ext.h>

#define BITMAP_1BIT 1
#define BITMAP_8BIT 0

int main(int argc, char* args[]){

    setlocale(LC_ALL, "");
    bm_status_t ret = BM_SUCCESS;
    wchar_t hexcode[256];
    unsigned char r = 255, g = 255, b = 0;
    int width = 1920, height = 1080, orgx = 0, orgy = 500;
    bm_image_format_ext fmt = FORMAT_RGB_PACKED;
    float fontScale = 2;
    int thick = -1;
    std::string output_path = "out.bmp";
    std::string input_path = "/opt/sophon/libsophon-current/bin/image/vpp_input/rgb24.bin";
    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: %d\n", argc);
        printf("%s text_string r g b fontscale out_name in_name w h fmt x y\n\n", args[0]);
        printf("example:\n");
        printf("%s bitmain.go\n", args[0]);
        printf("%s bitmain.go 255 255 255 2 out.bmp\n", args[0]);
        printf("%s bitmain.go 255 255 255 2 out.bmp /opt/sophon/libsophon-current/bin/image/vpp_input/rgb24.bin 1920 1080 10 0 500\n", args[0]);
        return 0;
    }
    if (argc > 1)
        mbstowcs(hexcode, args[1], sizeof(hexcode) / sizeof(wchar_t));
    else
        mbstowcs(hexcode, "算能sophgo", sizeof(hexcode) / sizeof(wchar_t));
    printf("Received wide character string: %ls\n", hexcode);
    if (argc > 2) r = atoi(args[2]);
    if (argc > 3) g = atoi(args[3]);
    if (argc > 4) b = atoi(args[4]);
    if (argc > 5) fontScale = (float)atof(args[5]);
    if (argc > 6) output_path = args[6];
    if (argc > 7) input_path = args[7];
    if (argc > 8) width = atoi(args[8]);
    if (argc > 9) height = atoi(args[9]);
    if (argc > 10) fmt = (bm_image_format_ext)atoi(args[10]);
    if (argc > 11) orgx = atoi(args[11]);
    if (argc > 12) orgy = atoi(args[12]);
    if (argc > 13) thick = atoi(args[13]);

    bm_image image;
    bm_handle_t handle = NULL;
    bm_dev_request(&handle, 0);
    bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &image, NULL);
    bm_image_alloc_dev_mem(image, BMCV_HEAP1_ID);
    bm_read_bin(image, input_path.c_str());
    bmcv_point_t org;
    org.x = orgx;
    org.y = orgy;
    bmcv_color_t color;
    color.r = r;
    color.g = g;
    color.b = b;
    bm_image watermark;
    bm_device_mem_t watermark_mem;
    bmcv_rect_t rect;
    int stride;
#ifdef __linux__
    int time_single = 0;
    struct timeval tv[2];
    gettimeofday(tv, NULL);
#endif
    if (thick < 0) {
        ret = bmcv_gen_text_watermark(handle, hexcode, color, fontScale, FORMAT_GRAY, &watermark);
        if (ret != BM_SUCCESS) {
            printf("bmcv_gen_text_watermark fail\n");
            goto fail;
        }
    #ifdef __linux__
        gettimeofday(tv + 1, NULL);
        time_single = (unsigned int)((tv[1].tv_sec - tv[0].tv_sec) * 1000000 + tv[1].tv_usec - tv[0].tv_usec);
        printf("bmcv_gen_text_watermark time %d\n", time_single);
    #endif
        rect.start_x = org.x;
        rect.start_y = org.y;
        rect.crop_w = watermark.width;
        rect.crop_h = watermark.height;

        bm_image_get_stride(watermark, &stride);
        bm_image_get_device_mem(watermark, &watermark_mem);
        ret = bmcv_image_watermark_superpose(handle, &image, &watermark_mem, 1, BITMAP_8BIT,
            stride, &rect, color);
        bm_image_destroy(watermark);
        if (ret != BM_SUCCESS) {
            printf("bmcv_image_watermark_superpose fail\n");
            goto fail;
        }
    } else {
        ret = bmcv_image_put_text(handle, image, args[1], org, color, fontScale, thick);
        if (ret != BM_SUCCESS) {
            printf("bmcv_image_put_text fail\n");
            goto fail;
        }
    }
#ifdef __linux__
    gettimeofday(tv + 1, NULL);
    time_single = (unsigned int)((tv[1].tv_sec - tv[0].tv_sec) * 1000000 + tv[1].tv_usec - tv[0].tv_usec);
    printf("all time %d\n", time_single);
#endif
    bm_image_write_to_bmp(image, output_path.c_str());
    printf("bm_image_write_to_bmp path: %s\n", output_path.c_str());

fail:
    bm_image_destroy(image);
    bm_dev_free(handle);
    return ret;
}