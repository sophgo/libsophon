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

bm_status_t open_binary_to_alpha(
  bm_handle_t           handle,
  char *                src_name,
  int                   src_size,
  int                   transparency,
  bm_device_mem_t *     dst)
{
  bm_status_t ret = BM_SUCCESS;
  unsigned char * src = new unsigned char [src_size];
  unsigned char * dst_addr = new unsigned char [src_size * 8];

  FILE * fp_src = fopen(src_name, "rb+");
  size_t read_size = fread((void *)src, src_size, 1, fp_src);
  printf("fread %ld byte\n", read_size);
  fclose(fp_src);

  for(int i=0; i<(src_size * 8); i++){
    int watermask_idx = i / 8;
    int binary_idx = (i % 8);
    dst_addr[i] = ((src[watermask_idx] >> binary_idx) & 1) * transparency;
  }

  ret = bm_malloc_device_byte(handle, dst, src_size * 8);
  if(ret != BM_SUCCESS){
      printf("bm_malloc_device_byte fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
      goto fail;
  }
  ret = bm_memcpy_s2d(handle, dst[0], dst_addr);
  if(ret != BM_SUCCESS){
      printf("bm_memcpy_s2d fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
  }
fail:
  delete [] src;
  delete [] dst_addr;
  return ret;
}

bm_status_t open_water(
  bm_handle_t           handle,
  char *                src_name,
  int                   src_size,
  bm_device_mem_t *     dst)
{
  bm_status_t ret = BM_SUCCESS;
  unsigned char * src = new unsigned char [src_size];

  FILE * fp_src = fopen(src_name, "rb+");
  size_t read_size = fread((void *)src, src_size, 1, fp_src);
  printf("fread %ld byte\n", read_size);
  fclose(fp_src);

  ret = bm_malloc_device_byte(handle, dst, src_size);
  if(ret != BM_SUCCESS){
      printf("bm_malloc_device_byte fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
      goto fail;
  }
  ret = bm_memcpy_s2d(handle, dst[0], src);
  if(ret != BM_SUCCESS){
      printf("bm_memcpy_s2d fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
  }
fail:
  delete [] src;
  return ret;
}

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_w, src_h, water_h, water_w, font_mode, water_byte, interval, transparency = 255;
  char *src_name, *dst_name, *water_name, *dst_csv_name;
  bm_image_format_ext src_fmt;
  bm_image src;
  unsigned int i = 0, loop_time = 1;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;
  int dev_id = 0;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc < 15) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name dst_name water_type water_name water_byte water_w water_h interval loop_time dev_id dst_csv_name transparency(binary mode)\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_RGB24PACKED\n");
    printf("%s 1920 1080 10 src.bin dst.bin 0 water.bin 4096 64 64 50 100 0 dst.csv 128\n", argv[0]);
    return -1;
  }

  src_w = atoi(argv[1]);
  src_h = atoi(argv[2]);
  src_fmt = (bm_image_format_ext)atoi(argv[3]);
  src_name = argv[4];
  dst_name = argv[5];
  font_mode = atoi(argv[6]);
  water_name = argv[7];
  water_byte = atoi(argv[8]);
  water_w = atoi(argv[9]);
  water_h = atoi(argv[10]);
  interval = atoi(argv[11]);
  loop_time = atoi(argv[12]);
  dev_id = atoi(argv[13]);
  dst_csv_name = argv[14];;
  if(font_mode == 1){
    if (argc != 16) {
      printf("usage: %d\n", argc);
      printf("%s src_w src_h src_fmt src_name dst_name water_type water_name water_byte water_w water_h interval loop_time dev_id dst_csv_name transparency(binary mode)\n", argv[0]);
      printf("example:\n");
      printf("FORMAT_RGB24PACKED\n");
      printf("%s 1920 1080 10 src.bin dst.bin 0 water.bin 4096 64 64 50 100 0 dst.csv 128\n", argv[0]);
      return -1;
    }
    transparency = atoi(argv[15]);
  }
  bm_status_t ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
    exit(-1);
  }

  int font_num = ((src_w - water_w) / (water_w + interval) + 1) * ((src_h - water_h) / (water_h + interval) + 1);
  int font_idx = 0;
  bmcv_rect_t * rect = new bmcv_rect_t [font_num];
  for(int w = 0; w < ((src_w - water_w) / (water_w + interval) + 1); w++){
      for(int h = 0; h < ((src_h - water_h) / (water_h + interval) + 1); h++){
      rect[font_idx].start_x = w * (water_w + interval);
      rect[font_idx].start_y = h * (water_h + interval);
      rect[font_idx].crop_w = water_w;
      rect[font_idx].crop_h = water_h;
      font_idx = font_idx + 1;
      }
  }
  bmcv_color_t color;
  color.r = 128;
  color.g = 128;
  color.b = 128;

  bm_device_mem_t water;
  if(font_mode == 0){
    if(open_water(handle, water_name, water_byte, &water)!=BM_SUCCESS)
      return -1;
  }
  if(font_mode == 1){
    if(open_water(handle, water_name, water_byte, &water)!=BM_SUCCESS)
      return -1;
    water_w = water_w / 8;
  }

  if(font_mode == 2){
    if(open_binary_to_alpha(handle, water_name, water_byte, transparency, &water)!=BM_SUCCESS)
      return -1;
    font_mode = 0;
  }
  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_alloc_dev_mem(src);
  bm1684x_vpp_read_bin(src, src_name);
  for(i = 0;i < loop_time; i++){

#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif
    bmcv_image_watermark_repeat_superpose(handle, src, water, font_num, font_mode, water_w, rect, color);

#ifdef __linux__
    gettimeofday(&tv_end, NULL);
    timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
    timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
    time_single = (unsigned int)(timediff.tv_sec * 1000000 + timediff.tv_usec);
#endif
    if(i == 0)
      bm1684x_vpp_write_bin(src, dst_name);
    if(time_single>time_max){time_max = time_single;}
    if(time_single<time_min){time_min = time_single;}
    time_total = time_total + time_single;
  }
  time_avg = time_total / loop_time;
  fps = 1000000 *2 / time_avg;
  pixel_per_sec = src_w * src_h * fps/1024/1024;

  char fmt_str[100];
  format_to_str(src.image_format, fmt_str);

  printf("bmcv_image_watermark_superpose:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

  bmlib_log("BMCV",BMLIB_LOG_TRACE, "loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);

  FILE *fp_csv = fopen(dst_csv_name, "ab+");
  fprintf(fp_csv, "%lld, %lld, %lldM\n",time_avg, fps, pixel_per_sec);
  fclose(fp_csv);

  bm_image_destroy(src);
  bm_free_device(handle, water);
  bm_dev_free(handle);
  delete [] rect;

  return 0;
}

