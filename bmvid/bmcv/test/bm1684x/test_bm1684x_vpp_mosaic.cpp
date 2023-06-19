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
extern void format_to_str(bm_image_format_ext format, char* res);
extern void bm1684x_vpp_read_bin(bm_image src, const char *input_name);
extern void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);
int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_w, src_h, dev_id, mosaic_num;
  char *src_name, *dst_name, *dst_csv_name;
  bm_image_format_ext src_fmt;
  bm_image src;
  bmcv_rect_t *rect;
  unsigned int i = 0, loop_time = 100;
  unsigned int is_expand;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc < 15) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name dst_name is_expand loop_time dev_id dst_csv_name mosaic_num stx_1 sty_1 crop_w_1 crop_h_1 stx_2 sty_2 crop_w_2 crop_h_2 ...\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_RGB24PACKED\n");
    printf("%s 1920 1080 10 src.bin dst.bin 0 100 0 dst.csv 2 0 0 64 64 200 200 128 128\n", argv[0]);
    return -1;
  }
  src_w = atoi(argv[1]);
  src_h = atoi(argv[2]);
  src_fmt = (bm_image_format_ext)atoi(argv[3]);
  src_name = argv[4];
  dst_name = argv[5];
  is_expand = atoi(argv[6]);
  loop_time = atoi(argv[7]);
  dev_id = atoi(argv[8]);
  dst_csv_name = argv[9];
  mosaic_num = atoi(argv[10]);
  if (argc != 11 + 4 * mosaic_num) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name dst_name is_expand loop_time dev_id dst_csv_name mosaic_num stx_1 sty_1 crop_w_1 crop_h_1 stx_2 sty_2 crop_w_2 crop_h_2 ...\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_RGB24PACKED\n");
    printf("%s 1920 1080 10 src.bin dst.bin 0 100 0 dst.csv 2 0 0 64 64 200 200 128 128\n", argv[0]);
    return -1;
  }
  rect = new bmcv_rect_t [mosaic_num];
  for(int i = 0; i<mosaic_num; i++){
    rect[i].start_x = atoi(argv[11 + 4 * i]);
    rect[i].start_y = atoi(argv[12 + 4 * i]);
    rect[i].crop_w = atoi(argv[13 + 4 * i]);
    rect[i].crop_h = atoi(argv[14 + 4 * i]);
  }
  bm_status_t ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
    printf("Create bm handle failed. ret = %d\n", ret);
    exit(-1);
  }
  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_alloc_dev_mem(src);
  bm1684x_vpp_read_bin(src, src_name);
  for(i = 0;i < loop_time; i++){

#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif
    bmcv_image_mosaic(handle, mosaic_num, src, rect, is_expand);
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
  bm1684x_vpp_write_bin(src, dst_name);
  bm_image_destroy(src);
  bm_dev_free(handle);
  delete [] rect;

  char fmt_str[100];
  format_to_str(src.image_format, fmt_str);

  printf("bmcv_image_mosaic:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

  printf("loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);

  FILE *fp_csv = fopen(dst_csv_name, "ab+");
  fprintf(fp_csv, "%lld, %lld, %lldM\n",time_avg, fps, pixel_per_sec);
  fclose(fp_csv);

  return 0;
}

