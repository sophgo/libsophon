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

#define UNUSED_VARIABLE(x)  ((x) = (x))

// extern void bm1684x_vpp_read_bin(bm_image src, const char *input_name);
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

void read_image(unsigned char **input_ptr, int *src_len, const char * src_name)
{
    // char input_name[200] = {0};
    // int len = strlen(opencvFile_path);
    // if(opencvFile_path[len-1] != '/')
    //   opencvFile_path[len] = '/';
    // snprintf(input_name, 200,"%s%s", opencvFile_path,src_name);

    FILE *fp_src = fopen(src_name, "rb+");
    fseek(fp_src, 0, SEEK_END);
    *src_len = ftell(fp_src);
    *input_ptr   = (unsigned char *)malloc(*src_len);
    fseek(fp_src, 0, SEEK_SET);
    size_t cnt = fread((void *)*input_ptr, 1, *src_len, fp_src);
    fclose(fp_src);
    UNUSED_VARIABLE(cnt);
}

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt = FORMAT_COMPRESSED, dst_fmt;
  char *table_y, *data_y, *table_c, *data_c, *dst_name;
  bm_image src, dst;
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

  if (argc != 11) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h table_y data_y table_c data_c dst_fmt dst_name loop_time dev_id\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_COMPRESSED-->FORMAT_YUV420P:\n");
    printf("%s 1920 1080 offset_base_y.bin offset_comp_y.bin offset_base_c.bin offset_comp_c.bin 0 fbd_1080p.bin 1 1\n", argv[0]);
    return 0;
  }

  src_w = atoi(argv[1]);
  src_h = atoi(argv[2]);
  table_y = argv[3];
  data_y = argv[4];
  table_c = argv[5];
  data_c = argv[6];
  dst_w = src_w;
  dst_h = src_h;
  dst_fmt = (bm_image_format_ext)atoi(argv[7]);
  dst_name = argv[8];
  loop_time = atoi(argv[9]);
  dev_id = atoi(argv[10]);

  rect.start_x = 0;
  rect.start_y = 0;
  rect.crop_w = dst_w;
  rect.crop_h = dst_h;

  bm_status_t ret    = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_device_mem_t mem[4];
  memset(mem, 0, sizeof(bm_device_mem_t) * 4);

  unsigned char * buf[4] = {NULL};
  int plane_size[4] = {0};

  read_image(&buf[0], &plane_size[0], table_y);
  read_image(&buf[1], &plane_size[1], data_y);
  read_image(&buf[2], &plane_size[2], table_c);
  read_image(&buf[3], &plane_size[3], data_c);

  for (int i = 0; i < 4; i++) {
    bm_malloc_device_byte(handle, mem + i, plane_size[i]);
    bm_memcpy_s2d(handle, mem[i], (void *)buf[i]);
  }
  bm_image_attach(src, mem);

  bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst);
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

  return 0;
}

