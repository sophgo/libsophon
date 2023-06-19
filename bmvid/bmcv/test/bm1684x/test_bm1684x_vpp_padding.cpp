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

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt, dst_fmt;
  char *src_name, *dst_name, *dst_csv_name;
  bm_image src, dst;
  bmcv_rect_t rect;
  bmcv_padding_atrr_t padding_attr;
  unsigned int i = 0, loop_time = 0;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;
  int dev_id = 0;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 16) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name dst_start_x dst_start_y dst_crop_w dst_crop_h dst_w dst_h dst_fmt dst_name loop_time dst_csv_name dev_id\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_GRAY-->FORMAT_GRAY:\n");
    printf("%s 800 600 14 gray.bin 80 60 800 600 960 720 14 gray.dst 1000 padding.csv 0\n", argv[0]);

    return 0;
  }

  src_w = atoi(argv[1]);
  src_h = atoi(argv[2]);
  src_fmt = (bm_image_format_ext)atoi(argv[3]);
  src_name = argv[4];
  padding_attr.dst_crop_stx = atoi(argv[5]);
  padding_attr.dst_crop_sty = atoi(argv[6]);
  padding_attr.dst_crop_w = atoi(argv[7]);
  padding_attr.dst_crop_h = atoi(argv[8]);
  padding_attr.if_memset = 1;
  padding_attr.padding_b = 255;
  padding_attr.padding_g = 255;
  padding_attr.padding_r = 255;
  dst_w = atoi(argv[9]);
  dst_h = atoi(argv[10]);
  dst_fmt = (bm_image_format_ext)atoi(argv[11]);
  dst_name = argv[12];
  loop_time = atoi(argv[13]);
  dst_csv_name = argv[14];
  dev_id  = atoi(argv[15]);

  rect.start_x = 0;
  rect.start_y = 0;
  rect.crop_w = src_w;
  rect.crop_h = src_h;


  bm_status_t ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
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

    bmcv_image_vpp_convert_padding(handle, 1, src ,&dst, &padding_attr, &rect, BMCV_INTER_LINEAR);

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

  char src_fmt_str[100],dst_fmt_str[100];
  format_to_str(src.image_format, src_fmt_str);
  format_to_str(dst.image_format, dst_fmt_str);


  printf("%d*%d->%d*%d, %s->%s\n",src_w,src_h,dst_w,dst_h,src_fmt_str,dst_fmt_str);
  printf("bmcv_image_vpp_convert_padding:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

  bmlib_log("BMCV",BMLIB_LOG_TRACE, "loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);

  FILE *fp_csv = fopen(dst_csv_name, "ab+");
  fprintf(fp_csv, "%s, %lld, %lld, %lldM\n",src_fmt_str,time_avg, fps, pixel_per_sec);

  fclose(fp_csv);


  return 0;

}

