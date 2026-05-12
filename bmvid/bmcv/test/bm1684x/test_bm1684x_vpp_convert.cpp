#include <iostream>
#include <vector>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <pthread.h>
#include "md5.h"
#ifdef __linux__
#include <sys/time.h>
#endif

extern void bm_read_bin(bm_image src, const char *input_name);
extern void bm_write_bin(bm_image dst, const char *output_name);
extern void format_to_str(bm_image_format_ext format, char* res);
static void algorithm_to_str(bmcv_resize_algorithm algorithm, char* res)
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

typedef struct {
  int loop_time;
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  bm_image_format_ext src_fmt;
  bm_image_format_ext dst_fmt;
  char *src_name;
  char *dst_name;
  char *dst_csv_name;
  bmcv_rect_t rect;
  bmcv_resize_algorithm algorithm;
  bm_handle_t handle;
} vpp_convert_thread_arg_t;

static int cmpMd5(unsigned char* got, unsigned char* exp){
  char buf[33] = {0};
  for (int i = 0; i < 16; i++) {
      std::sprintf(buf + 2*i, "%02x", got[i]);
  }
  buf[32] = '\0';

  if (std::strcmp((char*)exp, buf) == 0) {
      printf("md5 compare pass!\n");
      return 0;
  } else {
      printf("md5 compare fail!\n");
      return -1;
  }
}

void* vpp_convert(void* args) {
  vpp_convert_thread_arg_t* vpp_convert_thread_arg = (vpp_convert_thread_arg_t*)args;
  int loop_time = vpp_convert_thread_arg->loop_time;
  int src_w = vpp_convert_thread_arg->src_w;
  int src_h = vpp_convert_thread_arg->src_h;
  int dst_w = vpp_convert_thread_arg->dst_w;
  int dst_h = vpp_convert_thread_arg->dst_h;
  bm_image_format_ext src_fmt = vpp_convert_thread_arg->src_fmt;
  bm_image_format_ext dst_fmt = vpp_convert_thread_arg->dst_fmt;
  char *src_name = vpp_convert_thread_arg->src_name;
  char *dst_name = vpp_convert_thread_arg->dst_name;
  char *dst_csv_name = vpp_convert_thread_arg->dst_csv_name;
  bmcv_rect_t rect = vpp_convert_thread_arg->rect;
  bmcv_resize_algorithm algorithm =vpp_convert_thread_arg->algorithm;
  bm_handle_t handle = vpp_convert_thread_arg->handle;

  bm_image       src, dst;
  int i = 0;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_alloc_dev_mem(src,1);
  bm_read_bin(src,src_name);

  bm_image_create(handle, dst_h, dst_w,dst_fmt,DATA_TYPE_EXT_1N_BYTE,&dst);
  bm_image_alloc_dev_mem(dst,1);

  #ifdef __linux__
    struct timeval tv_start;
    struct timeval tv_end;
    struct timeval timediff;
  #endif

  for(i = 0;i < loop_time; i++){
  #ifdef __linux__
      gettimeofday(&tv_start, NULL);
  #endif

      bmcv_image_vpp_csc_matrix_convert(handle, 1, src, &dst, CSC_MAX_ENUM, NULL, algorithm, &rect);

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

  bm_write_bin(dst, dst_name);
  bm_image_destroy(src);
  bm_image_destroy(dst);

  unsigned char exp_md5[] = "005642f7754380372c0c1ab9cf4b79b6";
  unsigned char* md5_tpuOut = new unsigned char[16];
  calculate_md5(dst_name, md5_tpuOut);
  int com_result = cmpMd5(md5_tpuOut, exp_md5);
  if (com_result != 0) {
    exit(-1);
  }

  char src_fmt_str[100],dst_fmt_str[100],algorithm_str[100];
  format_to_str(src.image_format, src_fmt_str);
  format_to_str(dst.image_format, dst_fmt_str);
  algorithm_to_str(algorithm, algorithm_str);

  printf("%d*%d->%d*%d, %s->%s,%s\n",src_w,src_h,dst_w,dst_h,src_fmt_str,dst_fmt_str,algorithm_str);
  printf("bmcv_image_vpp_csc_matrix_convert:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

  bmlib_log("BMCV",BMLIB_LOG_TRACE, "loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);

  FILE *fp_csv = fopen(dst_csv_name, "ab+");
  fprintf(fp_csv, "%d*%d->%d*%d, %s->%s, %s, %lld, %lld, %lldM\n",src_w,src_h,dst_w,dst_h,src_fmt_str,dst_fmt_str,algorithm_str,time_avg, fps, pixel_per_sec);
  fclose(fp_csv);

  return (void*)0;
}

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  int thread_num = 1;
  bm_image_format_ext src_fmt;
  bm_image_format_ext dst_fmt;
  char *src_name, *dst_name, *dst_csv_name;
  bm_image src, dst;
  bmcv_rect_t rect;
  bmcv_resize_algorithm algorithm = BMCV_INTER_NEAREST;
  int loop_time = 0;
  int  dev_id = 0;

  if (argc != 18) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name start_x start_y crop_w crop_h dst_w dst_h dst_fmt dst_name algorithm loop_time dev_id thread_num dst_csv_name\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P-->FORMAT_RGB_PACKED:\n");
    printf("%s 1920 1080 0 i420.yuv 0 0 1920 1080 1920 1080 10 rgb24.bin 0 1 0 1 csc_scale.csv\n", argv[0]);
    return 0;
  }

  if (argc == 18) {
    src_w = atoi(argv[1]);
    src_h = atoi(argv[2]);
    src_fmt = (bm_image_format_ext)atoi(argv[3]);
    src_name = argv[4];
    rect.start_x = atoi(argv[5]);
    rect.start_y = atoi(argv[6]);
    rect.crop_w = atoi(argv[7]);
    rect.crop_h = atoi(argv[8]);
    dst_w = atoi(argv[9]);
    dst_h = atoi(argv[10]);
    dst_fmt = (bm_image_format_ext)atoi(argv[11]);
    dst_name = argv[12];
    algorithm = bmcv_resize_algorithm(atoi(argv[13]));
    loop_time = atoi(argv[14]);
    dev_id = atoi(argv[15]);
    thread_num = atoi(argv[16]);
    dst_csv_name = argv[17];
  }

  // pthread_t pid[thread_num];
  pthread_t* pid = new pthread_t[thread_num];
  bm_status_t ret    = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  // vpp_convert_thread_arg_t vpp_convert_thread_arg[thread_num];
  vpp_convert_thread_arg_t* vpp_convert_thread_arg = new vpp_convert_thread_arg_t[thread_num];
  for (int i = 0; i < thread_num; i++) {
    vpp_convert_thread_arg[i].loop_time = loop_time;
    vpp_convert_thread_arg[i].src_w = src_w;
    vpp_convert_thread_arg[i].src_h = src_h;
    vpp_convert_thread_arg[i].dst_w = dst_w;
    vpp_convert_thread_arg[i].dst_h = dst_h;
    vpp_convert_thread_arg[i].src_fmt = src_fmt;
    vpp_convert_thread_arg[i].dst_fmt = dst_fmt;
    vpp_convert_thread_arg[i].src_name = src_name;
    vpp_convert_thread_arg[i].rect.start_x = rect.start_x;
    vpp_convert_thread_arg[i].rect.start_y = rect.start_y;
    vpp_convert_thread_arg[i].rect.crop_w = rect.crop_w;
    vpp_convert_thread_arg[i].rect.crop_h = rect.crop_h;
    vpp_convert_thread_arg[i].algorithm = algorithm;
    vpp_convert_thread_arg[i].handle = handle;
    char *dst_name_i = (char *)malloc(strlen(dst_name) + 16);
    char *dst_csv_name_i = (char *)malloc(strlen(dst_csv_name) + 16);
    sprintf(dst_name_i, "%s_thread%02d.bin", dst_name, i);
    sprintf(dst_csv_name_i, "%s_thread%02d.bin", dst_csv_name, i);
    vpp_convert_thread_arg[i].dst_name = dst_name_i;
    vpp_convert_thread_arg[i].dst_csv_name = dst_csv_name_i;
    if (pthread_create(pid + i, NULL, vpp_convert, vpp_convert_thread_arg + i) != 0) {
        printf("create thread failed\n");
        delete[] pid;
        delete[] vpp_convert_thread_arg;
        return -1;
    }
  }
  for (int i = 0; i < thread_num; i++) {
    int ret = pthread_join(pid[i], NULL);
    if (ret != 0) {
        printf("Thread join failed\n");
        delete[] pid;
        delete[] vpp_convert_thread_arg;
        exit(-1);
    }
  }

  bm_dev_free(handle);
  delete[] pid;
  delete[] vpp_convert_thread_arg;

  return 0;
}

