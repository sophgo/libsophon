#include <iostream>
#include <vector>
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/time.h>
#endif

extern void bm1684x_vpp_read_bin(bm_image src, const char *input_name);
extern void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);
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

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt,dst_fmt;
  char *src_name, *dst_name, *dst_csv_name;
  bm_image       src, dst;
  bmcv_rect_t rect;
  bmcv_resize_algorithm algorithm = BMCV_INTER_NEAREST;
  unsigned int i = 0, loop_time = 0;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;
  int  dev_id = 0;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 17) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name start_x start_y crop_w crop_h dst_w dst_h dst_fmt dst_name algorithm loop_time dst_csv_name dev_id\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P-->FORMAT_RGB_PACKED:\n");
    printf("%s 1920 1080 0 i420.yuv 0 0 1920 1080 1920 1080 10 rgb24.bin 0 1 csc_scale.csv 0\n", argv[0]);
    return 0;
  }

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
  dst_csv_name = argv[15];
  dev_id = atoi(argv[16]);

  bm_status_t ret    = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_alloc_dev_mem(src,1);
  bm1684x_vpp_read_bin(src,src_name);

  bm_image_create(handle, dst_h, dst_w,dst_fmt,DATA_TYPE_EXT_1N_BYTE,&dst);
  bm_image_alloc_dev_mem(dst,1);

//  printf("src addr = 0x%lx\n", src.image_private->data[0].u.device.device_addr);
//  printf("dst addr = 0x%lx\n", dst.image_private->data[0].u.device.device_addr);

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

  bm1684x_vpp_write_bin(dst, dst_name);
  bm_image_destroy(src);
  bm_image_destroy(dst);

  bm_dev_free(handle);

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

  return 0;
}

