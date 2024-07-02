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
  bm_image_format_ext src_fmt,dst_fmt;
  bm_image_data_format_ext src_type,dst_type;
  char *src_name, *dst_name;
  bm_image src, dst;
  bmcv_copy_to_atrr_t copy_to_attr;
  unsigned int i = 0, loop_time = 0;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;
  int dev_id = 0;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 19 && argc != 16) {
    printf("usage: %d\n", argc);
    printf(
        "%s src_w src_h src_fmt src_type src_name dst_w dst_h dst_fmt dst_type dst_name start_x "
        "start_y if_padding (padding_r padding_g padding_b) loop_time dev_id\n",
        argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P-->FORMAT_YUV420P, DATA_TYPE_EXT_1N_BYTE-->DATA_TYPE_EXT_1N_BYTE:\n");
    printf("%s 1920 1080 0 1 src.bin 3840 2160 0 1 dst.bin 960 540 1 153 204 102 1 0\n", argv[0]);
    printf("%s 1920 1080 0 1 src.bin 3840 2160 0 1 dst.bin 960 540 0 1 0\n", argv[0]);
    return 0;
  }

  src_w = atoi(argv[1]);
  src_h = atoi(argv[2]);
  src_fmt = (bm_image_format_ext)atoi(argv[3]);
  src_type = (bm_image_data_format_ext)atoi(argv[4]);
  src_name = argv[5];
  dst_w = atoi(argv[6]);
  dst_h = atoi(argv[7]);
  dst_fmt = (bm_image_format_ext)atoi(argv[8]);
  dst_type = (bm_image_data_format_ext)atoi(argv[9]);
  dst_name = argv[10];
  copy_to_attr.start_x = atoi(argv[11]);
  copy_to_attr.start_y = atoi(argv[12]);
  copy_to_attr.if_padding = atoi(argv[13]);

  if (copy_to_attr.if_padding) {
    copy_to_attr.padding_r = atoi(argv[14]);
    copy_to_attr.padding_g = atoi(argv[15]);
    copy_to_attr.padding_b = atoi(argv[16]);
    loop_time = atoi(argv[17]);
    dev_id = atoi(argv[18]);
  } else {
    loop_time = atoi(argv[14]);
    dev_id = atoi(argv[15]);
  }

  if (src_type == DATA_TYPE_EXT_1N_BYTE) {
      if (dst_type != DATA_TYPE_EXT_FLOAT32 && dst_type != DATA_TYPE_EXT_1N_BYTE &&
          dst_type != DATA_TYPE_EXT_1N_BYTE_SIGNED && dst_type != DATA_TYPE_EXT_FP16 &&
          dst_type != DATA_TYPE_EXT_BF16) {
          printf("Invalid output datatype.\n");
          return -1;
      }
  } else if (src_type == DATA_TYPE_EXT_FLOAT32) {
      if (dst_type != DATA_TYPE_EXT_FLOAT32) {
          printf("Invalid output datatype.\n");
          return -1;
      }
  } else {
      printf("Invalid input datatype.\n");
      return -1;
  }

  bm_status_t ret    = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  bm_image_create(handle, src_h, src_w, src_fmt, src_type, &src);
  bm_image_alloc_dev_mem(src,1);
  bm1684x_vpp_read_bin(src,src_name);

  bm_image_create(handle, dst_h, dst_w, dst_fmt, dst_type, &dst);
  bm_image_alloc_dev_mem(dst,1);

//  printf("src addr = 0x%lx\n", src.image_private->data[0].u.device.device_addr);
//  printf("dst addr = 0x%lx\n", dst.image_private->data[0].u.device.device_addr);

  for(i = 0;i < loop_time; i++){
#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif


    ret = bmcv_image_copy_to(handle, copy_to_attr, src, dst);

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


  printf("%d*%d->%d*%d, %s->%s,%s\n",src_w,src_h,dst_w,dst_h,src_fmt_str,dst_fmt_str,algorithm_str);
  printf("bmcv_image_copy_to:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

  bmlib_log("BMCV",BMLIB_LOG_TRACE, "loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);


  return 0;
}

