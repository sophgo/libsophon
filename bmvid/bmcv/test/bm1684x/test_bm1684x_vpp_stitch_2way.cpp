#ifndef SOC_MODE
#define PCIE_MODE
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

extern void bm1684x_vpp_read_bin(bm_image src, const char *input_name);
extern void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);
extern void format_to_str(bm_image_format_ext format, char* res);
void algorithm_to_str(bmcv_resize_algorithm algorithm, char* res)
{
  switch(algorithm)
  {
    case BMCV_INTER_NEAREST:
      strcpy(res, "BMCV_INTER_NEAREST");
      break;
    case BMCV_INTER_LINEAR:
      strcpy(res, "BMCV_INTER_LINEAR");
      break;
    case BMCV_INTER_BICUBIC:
      strcpy(res, "BMCV_INTER_BICUBIC");
      break;
    default:
      printf("%s:%d[%s] Not found such algorithm.\n",__FILE__, __LINE__, __FUNCTION__);
      break;
  }
}

typedef struct convert_ctx_{
    int loop;
    int i;
}convert_ctx;

int test_loop_times  = 1;
int test_threads_num = 1;
int src_h = 1080, src_w = 1920, dst_w = 1920, dst_h = 2160, dev_id = 0;
bm_image_format_ext src_fmt = FORMAT_YUV420P, dst_fmt = FORMAT_YUV420P;
const char *src_name = "/opt/sophon/libsophon-current/bin/image/vpp_input/i420.yuv", *dst_name = "stitch_1920x2160_yuv420.bin";

bmcv_rect_t dst_rect0 = {.start_x = 0, .start_y = 0, .crop_w = 1920, .crop_h = 1080};
bmcv_rect_t dst_rect1 = {.start_x = 0, .start_y = 1080, .crop_w = 1920, .crop_h = 1080};
bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR;
bm_handle_t handle = NULL;

static void * stitch(void* arg) {
    bm_status_t ret;
    convert_ctx ctx = *(convert_ctx*)arg;
    bm_image src, dst;
    unsigned int i = 0, loop_time = 0;
    unsigned long long time_single, time_total = 0, time_avg = 0;
    unsigned long long time_max = 0, time_min = 10000, fps_actual = 0, pixel_per_sec = 0;
#if SLEEP_ON
    int fps = 60;
    int sleep_time = 1000000 / fps;
#endif

    struct timeval tv_start;
    struct timeval tv_end;
    struct timeval timediff;

    loop_time = ctx.loop;

    bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
    bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst, NULL);

    ret = bm_image_alloc_dev_mem(src,BMCV_HEAP_ANY);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem_src. ret = %d\n", ret);
        exit(-1);
    }
    ret = bm_image_alloc_dev_mem(dst,BMCV_HEAP_ANY);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem_dst. ret = %d\n", ret);
        exit(-1);
    }
    bm1684x_vpp_read_bin(src,src_name);
    bmcv_rect_t rect = {.start_x = 0, .start_y = 0, .crop_w = src_w, .crop_h = src_h};
    bmcv_rect_t src_rect[2] = {rect, rect};
    bmcv_rect_t dst_rect[2] = {dst_rect0, dst_rect1};

    bm_image input[2] = {src, src};

    for(i = 0;i < loop_time; i++){
        gettimeofday(&tv_start, NULL);

        bmcv_image_vpp_stitch(handle, 2, input, dst, dst_rect, src_rect, BMCV_INTER_LINEAR);

        gettimeofday(&tv_end, NULL);
        timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
        timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
        time_single = (unsigned int)(timediff.tv_sec * 1000000 + timediff.tv_usec);
#if SLEEP_ON
        if(time_single < sleep_time)
            usleep((sleep_time - time_single));
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
    fps_actual = 1000000 / time_avg;
    pixel_per_sec = src_w * src_h * fps_actual/1024/1024;

    if(ctx.i == 0){
            bm1684x_vpp_write_bin(dst, dst_name);
    }
    bm_image_destroy(src);
    bm_image_destroy(dst);

    char src_fmt_str[100],dst_fmt_str[100],algorithm_str[100];
    format_to_str(src.image_format, src_fmt_str);
    format_to_str(dst.image_format, dst_fmt_str);
    algorithm_to_str(algorithm, algorithm_str);


    printf("idx:%d, %d*%d->%d*%d, %s->%s,%s\n",ctx.i,src_w,src_h,dst_w,dst_h,src_fmt_str,dst_fmt_str,algorithm_str);
    printf("idx:%d, bmcv_image_stitch:loop %d cycles, time_max = %llu, time_avg = %llu, fps %llu, %lluM pps\n",
        ctx.i, loop_time, time_max, time_avg, fps_actual, pixel_per_sec);

    return 0;
}

static void print_help(char **argv){
    printf("please follow this order:\n \
        %s src_w src_h src_fmt src_name start_x0 start_y0 crop_w0 crop_h0 start_x1 start_y1 crop_w1 crop_h1 dst_w dst_h dst_name dev_id thread_num loop_num\n \
        %s thread_num loop_num\n", argv[0], argv[0]);
};

int main(int argc, char **argv) {
    if (argc >= 19) {
        test_threads_num = atoi(argv[17]);
        test_loop_times  = atoi(argv[18]);
    }
    if (argc >= 17) {
        src_w = atoi(argv[1]);
        src_h = atoi(argv[2]);
        src_fmt = (bm_image_format_ext)atoi(argv[3]);
        src_name = argv[4];
        dst_rect0.start_x = atoi(argv[5]);
        dst_rect0.start_y = atoi(argv[6]);
        dst_rect0.crop_w = atoi(argv[7]);
        dst_rect0.crop_h = atoi(argv[8]);
        dst_rect1.start_x = atoi(argv[9]);
        dst_rect1.start_y = atoi(argv[10]);
        dst_rect1.crop_w = atoi(argv[11]);
        dst_rect1.crop_h = atoi(argv[12]);
        dst_w = atoi(argv[13]);
        dst_h = atoi(argv[14]);
        dst_fmt = src_fmt;
        dst_name = argv[15];
        dev_id = atoi(argv[16]);
    }
    if (argc == 2){
        if (atoi(argv[1]) < 0){
            print_help(argv);
            exit(-1);
        } else
            test_threads_num  = atoi(argv[1]);
    }
    else if (argc == 3){
        test_threads_num = atoi(argv[1]);
        test_loop_times  = atoi(argv[2]);
    } else if (argc > 3 && argc < 17) {
        printf("command input error\n");
        print_help(argv);
        exit(-1);
    }
    int ret = (int)bm_dev_request(&handle, dev_id);
    if (ret != 0) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    convert_ctx ctx[test_threads_num];
    #ifdef __linux__
    pthread_t *          pid = (pthread_t *)malloc(sizeof(pthread_t)*test_threads_num);
    for (int i = 0; i < test_threads_num; i++) {
        ctx[i].i = i;
        ctx[i].loop = test_loop_times;
        if (pthread_create(
                &pid[i], NULL, stitch, (void *)(ctx + i))) {
            free(pid);
            perror("create thread failed\n");
            exit(-1);
        }
    }
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            free(pid);
            perror("Thread join failed");
            exit(-1);
        }
    }
    bm_dev_free(handle);
    printf("--------ALL THREADS TEST OVER---------\n");
    free(pid);
    #endif

    return 0;
}
