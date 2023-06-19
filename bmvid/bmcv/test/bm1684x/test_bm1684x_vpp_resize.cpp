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
  bm_image src, dst;
  bmcv_rect_t rect;
#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 13) {
    printf("usage: %d\n", argc);
    printf("%s src_w src_h src_fmt src_name start_x start_y crop_w crop_h dst_w dst_h dst_fmt dst_name\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P\n");
    printf("%s 1920 1080 0 i420.bin 200 200 500 800 1920 1080 0 i420_resize.bin\n", argv[0]);

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

  int dev_id = 0;
  bm_status_t ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_alloc_dev_mem(src);
  bm1684x_vpp_read_bin(src,src_name);

  bm_image_create(handle, dst_h, dst_w, dst_fmt,DATA_TYPE_EXT_1N_BYTE,&dst);
  bm_image_alloc_dev_mem(dst);

#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif
  bmcv_resize_image resize_attr;
  bmcv_resize_t resize_img_attr;
  resize_img_attr.start_x = rect.start_x;
  resize_img_attr.start_y = rect.start_y;
  resize_img_attr.in_width = rect.crop_w;
  resize_img_attr.in_height = rect.crop_h;
  resize_img_attr.out_width = 600;
  resize_img_attr.out_width = 600;
  resize_attr.resize_img_attr = &resize_img_attr;
  resize_attr.roi_num = 1;
  resize_attr.stretch_fit = 0;
  resize_attr.padding_b = 255;
  resize_attr.padding_g = 255;
  resize_attr.padding_r = 255;
  resize_attr.interpolation = 1;
  bmcv_image_resize(handle, 1, &resize_attr, &src, &dst);
#ifdef __linux__
  gettimeofday(&tv_end, NULL);
  timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
  timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
  printf("bmcv_image_vpp_resize spend %ld us\n" ,(timediff.tv_sec * 1000000 + timediff.tv_usec));
#endif

  bm1684x_vpp_write_bin(dst,dst_name);

  bm_image_destroy(dst);
  bm_image_destroy(src);
  bm_dev_free(handle);

  return 0;
}

