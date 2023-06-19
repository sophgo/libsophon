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
  bm_image *src, *dst;

#ifdef __linux__
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
#endif

  if (argc != 11) {
    printf("usage: %d\n", argc);
    printf("%s start_x start_y crop_w crop_h dst_w dst_h pat_st pat_sy pat_cw pat_ch\n", argv[0]);
    printf("example:\n");
    printf("FORMAT_YUV420P-->FORMAT_BGR_PACKED:\n");
    printf("input:i420.bin 420_1920x1088.yuv\n");
    printf("output:basic_1.bin basic_2.bin basic_3.bin basic_4.bin\n");
    printf("%s 0 0 200 200 600 600 200 200 200 200\n", argv[0]);

    return 0;
  }
  int input_image_num = 2;
  bmcv_rect_t rect[4];
  int  dev_id = 0;
  bm_status_t ret    = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }
  src = new bm_image [input_image_num];
  dst = new bm_image [4];
  bm_image_create(handle, 1080, 1920, (bm_image_format_ext)0, DATA_TYPE_EXT_1N_BYTE, &src[0]);
  bm_image_alloc_dev_mem(src[0]);
  bm1684x_vpp_read_bin(src[0], "i420.bin");
  bm_image_create(handle, 1080, 1920, (bm_image_format_ext)0, DATA_TYPE_EXT_1N_BYTE, &src[1]);
  bm_image_alloc_dev_mem(src[1]);
  bm1684x_vpp_read_bin(src[1], "420_1920x1088.yuv");

#ifdef __linux__
    gettimeofday(&tv_start, NULL);
#endif
  int crop_num_vec[2]={2,2};
  int out_image_num = 0;
  bmcv_padding_atrr_s padding_attr[4];
  for(int i=0;i<input_image_num;i++){
    for(int j=0;j<crop_num_vec[i];j++){
      padding_attr[out_image_num].dst_crop_stx = atoi(argv[7]);
      padding_attr[out_image_num].dst_crop_sty = atoi(argv[8]);
      padding_attr[out_image_num].dst_crop_w = atoi(argv[9]);
      padding_attr[out_image_num].dst_crop_h = atoi(argv[10]);
      padding_attr[out_image_num].if_memset = 1;
      padding_attr[out_image_num].padding_r = 255;
      padding_attr[out_image_num].padding_g = 255;
      padding_attr[out_image_num].padding_b = 255;
      rect[out_image_num].start_x = atoi(argv[1])+j*atoi(argv[3]);
      rect[out_image_num].start_y = atoi(argv[2]);
      rect[out_image_num].crop_w = atoi(argv[3]);
      rect[out_image_num].crop_h = atoi(argv[4]);
      bm_image_create(handle, atoi(argv[6]), atoi(argv[5]), (bm_image_format_ext)10, DATA_TYPE_EXT_1N_BYTE, &dst[out_image_num]);
      bm_image_alloc_dev_mem(dst[out_image_num]);
      out_image_num++;}}
  bmcv_image_vpp_basic(handle, input_image_num, src, dst, crop_num_vec, rect, padding_attr, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL);
#ifdef __linux__
  gettimeofday(&tv_end, NULL);
  timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
  timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
  printf("bmcv_image_vpp_basic spend %ld us\n" ,(timediff.tv_sec * 1000000 + timediff.tv_usec));
#endif
  bm1684x_vpp_write_bin(dst[0],"basic_1.bin");
  bm1684x_vpp_write_bin(dst[1],"basic_2.bin");
  bm1684x_vpp_write_bin(dst[2],"basic_3.bin");
  bm1684x_vpp_write_bin(dst[3],"basic_4.bin");
  for(int i = 0; i<out_image_num; i++){
  bm_image_destroy(dst[i]);
  }
  bm_image_destroy(src[0]);
  bm_image_destroy(src[1]);
  delete [] src;
  delete [] dst;
  bm_dev_free(handle);

  return 0;
}

