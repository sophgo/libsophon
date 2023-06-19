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

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt,dst_fmt;
  char *src_name, *dst_name;
  bm_image       src, dst;
  bmcv_rect_t rect;
  bmcv_padding_atrr_t padding_param;
#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 13) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name start_x start_y crop_w crop_h dst_w dst_h dst_fmt dst_name\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P-->FORMAT_BGR_PACKED:\n");
    printf("%s 1920 1080 0 i420.bin 0 0 1920 1080 1920 1080 11 bgr24.bin\n", argv[0]);

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

  int  dev_id = 0;
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

  if(rect.crop_w > src_w)
    rect.crop_w = src_w;

  if(rect.crop_h > src_h)
    rect.crop_h = src_h;

  padding_param.dst_crop_stx = (dst_w - rect.crop_w)/2;
  padding_param.dst_crop_sty = (dst_h - rect.crop_h)/2;
  padding_param.dst_crop_w = rect.crop_w;
  padding_param.dst_crop_h = rect.crop_h;
  padding_param.padding_r = 0;
  padding_param.padding_g = 0;
  padding_param.padding_b = 0;
  if((dst_w ==  rect.crop_w) && (dst_h ==  rect.crop_h))
    padding_param.if_memset = 0;
  else
    padding_param.if_memset = 1;

#ifdef __linux__
  gettimeofday(&tv_start, NULL);
#endif

  bmcv_image_vpp_convert_padding(handle, 1, src ,&dst, &padding_param, &rect, BMCV_INTER_LINEAR);

#ifdef __linux__
  gettimeofday(&tv_end, NULL);
  timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
  timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
 // printf("src_w %d,src_h %d,src_fmt %d,src_name %s,rect.start_x %d,rect.start_y %d,rect.crop_w %d,rect.crop_h %d,dst_w %d,dst_h %d,dst_fmt %d,dst_name %s\n",src_w,src_h,src_fmt,src_name,rect.start_x,rect.start_y,rect.crop_w,rect.crop_h,dst_w,dst_h,dst_fmt,dst_name);
//  printf("dst_w %d,dst_h %d,dst_fmt %d,dst_name %s,dst_crop_stx %d,dst_crop_sty %d,dst_crop_w %d,dst_crop_h %d\n",dst_w,dst_h,dst_fmt,dst_name,padding_param.dst_crop_stx,padding_param.dst_crop_sty,padding_param.dst_crop_w,padding_param.dst_crop_h);
  printf("bmcv_image_vpp_convert_padding spend %ld us\n" ,(timediff.tv_sec * 1000000 + timediff.tv_usec));
#endif

  bm1684x_vpp_write_bin(dst,dst_name);

  bm_image_destroy(dst);
  bm_image_destroy(src);
  bm_dev_free(handle);

  return 0;
}

