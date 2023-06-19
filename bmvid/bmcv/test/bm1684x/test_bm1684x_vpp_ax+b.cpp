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

void data_type_to_string(bm_image_data_format_ext format, char* res)
{
  switch(format)
  {
    case DATA_TYPE_EXT_FLOAT32:
      strcpy(res, "fp32");
      break;
    case DATA_TYPE_EXT_1N_BYTE:
      strcpy(res, "u8");
      break;
    case DATA_TYPE_EXT_1N_BYTE_SIGNED:
      strcpy(res, "s8");
      break;
    case DATA_TYPE_EXT_FP16:
      strcpy(res, "fp16");
      break;
    case DATA_TYPE_EXT_BF16:
      strcpy(res, "bpf16");
      break;
    default:
      bmlib_log("BMCV", BMLIB_LOG_WARNING, "Not found such data type %s: %s: %d\n",
        __FILE__, __func__, __LINE__);
      break;
    }
}

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt, dst_fmt;
  char *src_name, *dst_name, *dst_csv_name;
  bm_image src, dst;
  bm_image_data_format_ext dst_data_type = DATA_TYPE_EXT_1N_BYTE;
  unsigned int i = 0, loop_time = 0;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 10000, fps = 0, pixel_per_sec = 0;
  int dev_id = 0;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 13) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h fmt src_name dst_w dst_h dst_fmt dst_name dst_data_type loop_time dst.csv dev_id\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P:\n");
    printf("%s 1920 1080 0 i420.yuv 1920 1080  0 i420.dst  1 1000 linear.csv 0\n", argv[0]);
    return 0;
  }

  src_w = atoi(argv[1]);
  src_h = atoi(argv[2]);
  src_fmt = (bm_image_format_ext)atoi(argv[3]);
  src_name = argv[4];
  dst_w = atoi(argv[5]);
  dst_h = atoi(argv[6]);
  dst_fmt = (bm_image_format_ext)atoi(argv[7]);
  dst_name = argv[8];
  dst_data_type = bm_image_data_format_ext(atoi(argv[9]));
  loop_time = atoi(argv[10]);
  dst_csv_name = argv[11];
  dev_id = atoi(argv[12]);

  bmcv_convert_to_attr convert_to_attr;
  convert_to_attr.alpha_0 = 0.8;
  convert_to_attr.alpha_1 = 0.7;
  convert_to_attr.alpha_2 = 1.2;
  convert_to_attr.beta_0 = 2.5;
  convert_to_attr.beta_1 = 3.5;
  convert_to_attr.beta_2 = 4.6;


  bm_status_t ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_alloc_dev_mem(src, 1);
  bm1684x_vpp_read_bin(src,src_name);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, dst_data_type, &dst);
  bm_image_alloc_dev_mem(dst, 1);

  for(i = 0;i < loop_time; i++){

#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif

    bmcv_image_convert_to(handle, 1, convert_to_attr, &src, &dst);

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


  char fmt_str[100],dst_datatype_str[100], src_datatype_str[100];
  format_to_str(src.image_format, fmt_str);
  data_type_to_string(src.data_type, src_datatype_str);
  data_type_to_string(dst.data_type, dst_datatype_str);

  printf("%d*%d, %s, u8 -> %s\n",src_w,src_h,fmt_str,dst_datatype_str);
  printf("bmcv_image_convert_to:loop %d cycles, time_avg = %llu, fps %llu, %lluM pps\n\n",loop_time, time_avg, fps, pixel_per_sec);

  bmlib_log("BMCV",BMLIB_LOG_TRACE, "loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
    loop_time, time_max, time_min, time_avg);


  FILE *fp_csv = fopen(dst_csv_name, "ab+");
//fprintf(fp_csv, "format, width and height, input data type, output data type, time,fps, pixel per sec\n");
  fprintf(fp_csv, "%s, %s, %lld, %lld, %lldM\n",fmt_str,dst_datatype_str,time_avg, fps, pixel_per_sec);

  fclose(fp_csv);

  return 0;
}

