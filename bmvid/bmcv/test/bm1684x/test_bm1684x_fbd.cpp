#include <iostream>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "md5.h"

#define CROPNUM 32
static bm_handle_t handle;
char opencvFile_path[200] = "./";
#define UNUSED_VARIABLE(x)  ((x) = (x))

using namespace std;

unsigned char fbd_yuv_output_expected_value[CROPNUM][16]={
  {0x99,0x90,0xbe,0x41,0xb2,0xa1,0x21,0x76,0xa3,0x8,0x4d,0x92,0xe3,0xb1,0x23,0x23,},
  {0x84,0xdc,0x8d,0xe7,0xe4,0x25,0x1d,0x23,0x83,0x13,0xef,0xe2,0xe4,0x31,0x35,0xd5,},
  {0xaf,0x96,0x21,0x6b,0x37,0x14,0x28,0x53,0x52,0xdc,0xf8,0x20,0x5b,0xc2,0xff,0x69,},
  {0xc4,0xfc,0x29,0x0,0x9c,0xf9,0x73,0x1e,0x51,0x92,0xd,0x9e,0x9,0xb,0x2,0xb1,},
  {0xf9,0x70,0xd5,0xab,0xf3,0x55,0x64,0x22,0x83,0x72,0x9,0x42,0x15,0x1d,0xfb,0x96,},
  {0x49,0xe4,0xe,0xe5,0xdf,0x61,0x76,0x96,0x35,0x81,0xb2,0x26,0x74,0x74,0x96,0x29,},
  {0x77,0x87,0x35,0xf4,0x1a,0xd6,0x82,0xf1,0x7d,0x93,0xb9,0xa6,0xb2,0xb5,0xcb,0xdb,},
  {0xfe,0xeb,0x91,0xc3,0x98,0x1f,0x6a,0xf4,0x96,0x0,0x94,0x67,0xb3,0x7,0x77,0x6a,},
  {0x31,0x4e,0x8c,0x79,0xe2,0x4b,0xbf,0xc4,0x96,0x67,0x72,0x22,0x97,0x63,0xb3,0x4,},
  {0x9b,0x3b,0x1,0x1d,0x9d,0x66,0xb4,0xd0,0xd2,0x7a,0x4c,0x93,0xb6,0x81,0x15,0xa8,},
  {0xe0,0xf8,0x4,0x3a,0x1,0x56,0xc4,0x78,0xc3,0x5a,0x44,0x15,0x7e,0x6b,0xed,0xe1,},
  {0x6e,0xbb,0xb8,0x65,0xa,0x4b,0x46,0x33,0x40,0x75,0xf,0x91,0xff,0x49,0x8b,0x65,},
  {0x4a,0xfb,0x57,0x7c,0xc,0xf3,0x68,0x3f,0x59,0xf3,0xd5,0xa1,0xfd,0x95,0x94,0x1d,},
  {0x17,0xe3,0x36,0xe1,0x10,0xcd,0x87,0xbd,0xb9,0x7a,0x95,0x47,0x5f,0x73,0x0,0xd,},
  {0x60,0x1a,0x3d,0x15,0x4,0x4d,0x47,0x44,0x8e,0x3f,0x11,0x46,0x7,0xd5,0x63,0x21,},
  {0xda,0xac,0xa8,0x4e,0xaf,0x54,0xd3,0x26,0x80,0x46,0x60,0x5e,0xc8,0xb4,0x56,0xbb,},
  {0x8a,0x55,0x44,0x5d,0xa5,0x17,0xe1,0xbe,0xfc,0x67,0xd8,0x1a,0x64,0x66,0xd8,0x6f,},
  {0xe0,0x1,0x7b,0x80,0xa6,0x87,0x3c,0xaf,0x7,0x34,0x5e,0x17,0x96,0x7d,0x3d,0x61,},
  {0x3d,0xb4,0xc9,0x7c,0x26,0xe5,0x89,0xde,0x76,0x20,0xf,0x18,0xd7,0xaf,0xc4,0x40,},
  {0x39,0x9d,0x57,0xb5,0x7b,0xb8,0x29,0x68,0xd5,0xfd,0x6d,0xca,0xa6,0x8b,0x23,0x36,},
  {0xab,0x13,0xba,0x90,0xfe,0xfb,0x4e,0x63,0x74,0x54,0x34,0xea,0xd1,0x25,0xdf,0x67,},
  {0xf4,0xf2,0x94,0xf1,0xaa,0x8a,0xee,0xec,0x28,0x78,0xec,0x5f,0xd7,0x26,0xb9,0xdb,},
  {0x80,0x0,0xfb,0x9e,0x8f,0xab,0x9d,0x4b,0x6d,0xb,0x1b,0x25,0x90,0xcf,0x7,0xc9,},
  {0xd7,0x29,0x8b,0x71,0x31,0xd5,0xf7,0x62,0x9c,0xee,0x8,0xa1,0x39,0x2c,0x16,0xdf,},
  {0xb1,0x94,0xc5,0xdd,0x52,0xe6,0xca,0x85,0x52,0xd8,0x3d,0x1f,0x94,0xd1,0x77,0x25,},
  {0xaa,0x48,0xf8,0x49,0x7,0x75,0xdd,0x16,0x94,0x62,0x46,0xdc,0x35,0xfc,0xed,0xab,},
  {0xcd,0xf1,0x1f,0xe7,0xfc,0xf5,0xf0,0xa8,0x4d,0xa7,0x4b,0xf,0xbc,0x37,0x78,0x16,},
  {0xf7,0x4d,0xaf,0x58,0x9c,0x47,0xb7,0x7a,0x22,0x48,0x60,0x10,0xcc,0xb,0x44,0x5e,},
  {0xb5,0xc4,0xc8,0x66,0x18,0x62,0x45,0x11,0xa,0x5c,0x55,0xd9,0x73,0xcb,0x54,0x73,},
  {0xb,0x11,0x40,0x52,0x66,0xd0,0x60,0x36,0xcf,0xd1,0xcc,0xb4,0x6d,0x92,0x8d,0x6,},
  {0x3a,0xc2,0x19,0xd,0x50,0x0,0x5a,0xb5,0x5a,0x20,0x27,0x1,0x16,0x6b,0x10,0xeb,},
  {0xd7,0x16,0x0,0xde,0xee,0x55,0xdc,0xee,0xd4,0x61,0xa0,0x7c,0x5f,0xa9,0x31,0x93,},
};

unsigned char fbd_rgb_output_expected_value[CROPNUM][16]={
  {0xd0,0x71,0x9c,0x93,0x12,0xe5,0xd7,0x8c,0x8e,0x41,0xfc,0xc5,0xa8,0xf7,0x81,0x40,},
  {0x43,0x52,0x26,0xaa,0x0,0x69,0x85,0x8e,0xaf,0xe1,0xde,0xfb,0x6b,0xc0,0xaa,0x7e,},
  {0xd1,0x92,0x24,0x9d,0x2a,0x7,0x62,0x10,0x54,0x3e,0xa0,0x25,0x6,0xa5,0x69,0xd5,},
  {0x8d,0xd5,0x86,0xc,0x44,0x90,0x4b,0x15,0xc2,0x56,0xb1,0x71,0x34,0xe9,0x3e,0xcb,},
  {0xee,0x7a,0xe,0x68,0x4d,0x79,0x4d,0x8c,0x2a,0xe5,0xe1,0xbb,0xca,0x18,0xbb,0xfb,},
  {0xba,0xf8,0x1e,0xf0,0x84,0xd,0xbb,0xc1,0xde,0xa7,0x44,0x33,0x72,0xce,0x3e,0x43,},
  {0xbc,0x68,0x3c,0x1a,0xc4,0x4d,0x3a,0xcd,0xa7,0xc1,0xc,0x57,0xc3,0xbb,0xe5,0x29,},
  {0xa,0x58,0x90,0x80,0x56,0xd4,0xff,0xb3,0x2a,0xf8,0x18,0xc6,0x81,0x6e,0x45,0x6d,},
  {0x8d,0x5d,0xc,0x48,0xb3,0xc,0x33,0x40,0x2b,0x2a,0x7e,0x89,0x6c,0x9e,0x8c,0xa0,},
  {0xf3,0xaf,0x2b,0x5b,0xcf,0xb9,0x68,0xd9,0x53,0x7d,0x9f,0xdd,0x9d,0x1d,0x1,0xd9,},
  {0x4,0xee,0xbd,0x90,0x9b,0x69,0x7,0xdf,0x98,0x12,0x96,0x34,0x86,0x5c,0xa6,0x98,},
  {0xc4,0x6f,0x28,0xdd,0x51,0x67,0x10,0x7c,0x2c,0x99,0x7,0xcb,0x2a,0xb2,0x27,0x2,},
  {0xc1,0xbe,0x73,0x6,0x89,0xba,0x2,0xce,0x37,0xcc,0x2d,0xd2,0x89,0xd1,0x59,0x20,},
  {0x3c,0xed,0xd5,0x6,0xf7,0xa1,0x2a,0x65,0x4a,0xbd,0xcf,0x4a,0xb8,0x34,0xef,0x6d,},
  {0xb,0xa1,0x48,0x7,0x66,0xef,0x6f,0x19,0x21,0xc3,0x7b,0xe5,0x24,0x2e,0xc7,0x72,},
  {0x23,0x74,0x58,0xb2,0x95,0x6e,0xa1,0x2f,0xf9,0xd6,0x7e,0xba,0x29,0xb9,0x16,0x79,},
  {0xa0,0x89,0xc2,0xdc,0x26,0xad,0x7c,0x56,0xb1,0xb,0xf3,0xc6,0xaf,0xdd,0x4b,0x7d,},
  {0xd8,0xbe,0x2a,0x6a,0x8c,0x1b,0x16,0xe2,0xd4,0x25,0x9f,0xcb,0x8a,0xc,0x33,0xbd,},
  {0x84,0x5c,0xb0,0xb8,0x7,0x1a,0x9e,0xe0,0x78,0x40,0xf4,0x99,0x94,0xea,0xf7,0xd6,},
  {0xbb,0x47,0x21,0xea,0xb8,0x62,0x57,0x1c,0x28,0x66,0x3a,0x4d,0x77,0x6a,0xdf,0xe0,},
  {0x8b,0x1f,0xbf,0x12,0xd3,0x1,0x8e,0x80,0x86,0xe7,0x2d,0x4a,0xca,0xdd,0x79,0xe4,},
  {0x57,0x77,0xce,0x5c,0xa0,0x50,0x19,0x2c,0x70,0x9c,0x37,0xcf,0x2b,0x7e,0x7,0xe7,},
  {0x63,0x3,0xf1,0xab,0xbe,0xfb,0xd4,0x56,0xac,0x38,0x13,0x11,0xd7,0xbd,0x3e,0xea,},
  {0x2,0x66,0x1e,0x99,0xbb,0xa7,0x42,0x22,0x2c,0x6,0xe9,0x6e,0x5,0xdc,0xe3,0x94,},
  {0x35,0x8a,0x3c,0x20,0x3b,0xbd,0x2c,0xf5,0xbb,0xde,0x6b,0x98,0x27,0xf1,0xd7,0xad,},
  {0x8c,0x7d,0xd4,0x9,0xf2,0x68,0x82,0x11,0xc7,0xb6,0xc3,0x4a,0x78,0xee,0xdf,0x12,},
  {0xf6,0x97,0x3a,0x5,0x71,0x50,0x1a,0x57,0x2c,0x20,0xc1,0x3f,0x53,0x28,0xf,0x4e,},
  {0xef,0xf4,0x35,0x1c,0x8b,0xc7,0xcc,0x38,0x6,0xf9,0x47,0xe5,0x25,0x21,0x13,0x55,},
  {0x6c,0xf2,0xd6,0xfb,0xcf,0xb2,0xf7,0x25,0x76,0x19,0x39,0x54,0x98,0xd,0x24,0x77,},
  {0xd8,0x9,0xe9,0xe9,0x4a,0x4c,0x65,0x54,0xa9,0xdd,0xca,0x23,0x23,0x30,0xe,0x43,},
  {0x24,0x8a,0x96,0xef,0xe,0xbc,0x17,0xda,0x63,0x71,0x69,0x3c,0x6f,0x21,0xa0,0x12,},
  {0xc1,0x72,0x1,0x9c,0x8a,0x28,0x32,0x59,0x61,0x4,0xcd,0x75,0x95,0x5f,0xfc,0x8f,},
};

void read_image(unsigned char **input_ptr, int *src_len, const char * src_name)
{
    char input_name[200] = {0};
    int len = strlen(opencvFile_path);
    if(opencvFile_path[len-1] != '/')
      opencvFile_path[len] = '/';
    snprintf(input_name, 200,"%s%s", opencvFile_path,src_name);

    FILE *fp_src = fopen(input_name, "rb+");
    fseek(fp_src, 0, SEEK_END);
    *src_len = ftell(fp_src);
    *input_ptr   = (unsigned char *)malloc(*src_len);
    fseek(fp_src, 0, SEEK_SET);
    size_t cnt = fread((void *)*input_ptr, 1, *src_len, fp_src);
    fclose(fp_src);
    UNUSED_VARIABLE(cnt);

}

void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);

#ifndef USING_CMODEL
static int test_vpp_fbd_compressed_to_yuv() {
  int IMAGE_H = 1080;
  int IMAGE_W = 1920;
  int ret = 0;

  bm_image_format_ext dst_format_0[] = {
    FORMAT_GRAY,
    FORMAT_YUV420P,
    FORMAT_YUV444P,
    FORMAT_NV12,
    FORMAT_NV21,
  };

  bm_image_data_format_ext datatype[] = {
    DATA_TYPE_EXT_FP16,
    DATA_TYPE_EXT_BF16,
    DATA_TYPE_EXT_FLOAT32,
  };

  bm_image_format_ext dst_fmt;
  bm_image_data_format_ext dst_datatpye;
  unsigned char fbd_output[CROPNUM][16];

  bm_image src, dst[CROPNUM];
  bm_image_create(handle,
                  IMAGE_H,
                  IMAGE_W,
                  FORMAT_COMPRESSED,
                  DATA_TYPE_EXT_1N_BYTE,
                  &src);
  bm_device_mem_t mem[4];
  memset(mem, 0, sizeof(bm_device_mem_t) * 4);

  unsigned char * buf[4] = {NULL};
  int plane_size[4] = {0};

  read_image(&buf[0], &plane_size[0],"offset_base_y.bin");
  read_image(&buf[1], &plane_size[1],"offset_comp_y.bin");
  read_image(&buf[2], &plane_size[2],"offset_base_c.bin");
  read_image(&buf[3], &plane_size[3],"offset_comp_c.bin");

  for (int i = 0; i < 4; i++) {
    bm_malloc_device_byte(handle, mem + i, plane_size[i]);
    bm_memcpy_s2d(handle, mem[i], (void *)buf[i]);
  }
  bm_image_attach(src, mem);

  std::vector<bmcv_rect_t> rects;
  int x = IMAGE_W / 2;
  int y = IMAGE_H / 2;
  for (int i = 0; i < CROPNUM; i++) {
    rects.push_back({i/16 *32, i*2, x, y});
  }
  bmcv_rect_t *rect = &rects[0];
  int crop_num = rects.size();

  for (int i = 0; i < CROPNUM/2; i++) {
    dst_fmt = dst_format_0[i % (sizeof(dst_format_0)/sizeof(bm_image_format_ext))];
    dst_datatpye = (i%2 == 0) ? DATA_TYPE_EXT_1N_BYTE_SIGNED : DATA_TYPE_EXT_1N_BYTE;
 //   printf("i %d, dst_fmt  0x%x,dst_datatpye  0x%x\n",i,dst_fmt,dst_datatpye);

    bm_image_create(handle,
      IMAGE_H / 2 + 2*i,
      IMAGE_W / 2 + 2*i,
      dst_fmt,
      dst_datatpye,
      dst + i);
    bm_image_alloc_dev_mem(dst[i]);
  }

  for (int i = CROPNUM/2; i < CROPNUM; i++) {
    dst_fmt = FORMAT_YUV444P;
    dst_datatpye = datatype[i % (sizeof(datatype)/sizeof(bm_image_data_format_ext))];
   // printf("i %d, dst_fmt  0x%x,dst_datatpye  0x%x\n",i,dst_fmt,dst_datatpye);

    bm_image_create(handle,
      IMAGE_H / 2 + 2*i,
      IMAGE_W / 2 + 2*i,
      dst_fmt,
      dst_datatpye,
      dst + i);
    bm_image_alloc_dev_mem(dst[i]);
  }

  struct timeval t1, t2;
  gettimeofday_(&t1);
  if(BM_SUCCESS != bmcv_image_vpp_convert(handle, crop_num, src, dst, rect, BMCV_INTER_LINEAR)){
      ret = -1;
      goto exit;
  }
  gettimeofday_(&t2);
  cout << "VPP convert using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

#if 0
  for (int i = 0; i < CROPNUM; i++) {
    std::ostringstream os;
    os << "fbd";
    os << "_BT601_";
    os << i;
    os << ".bin";
    bm1684x_vpp_write_bin(dst[i], os.str().c_str());
  }
#endif

  for(int j=0;j<CROPNUM;j++)
  {
    int image_byte_size[4] = {0};
    bm_image_get_byte_size(dst[j], image_byte_size);

    int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
    unsigned char* output_ptr = (unsigned char *)malloc(byte_size);

    void* out_ptr[4] = {(void*)output_ptr,
      (void*)((unsigned char*)output_ptr + image_byte_size[0]),
      (void*)((unsigned char*)output_ptr + image_byte_size[0] + image_byte_size[1]),
      (void*)((unsigned char*)output_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};

    bm_image_copy_device_to_host(dst[j], (void **)out_ptr);

    md5_get(output_ptr,byte_size,fbd_output[j]);

#if 0
    printf("{");
    for(int i=0;i<16;i++)
      printf("0x%x,",fbd_output[j][i]);
    printf("},");
    printf("\n");
#endif

  free(output_ptr);
  }

  ret = memcmp(fbd_yuv_output_expected_value, fbd_output, sizeof(fbd_output));

  if(ret ==0 ){
    printf("Fbd to yuv, comparison accuracy succeed\n");
  }
  else{
    printf("Fbd to yuv, comparison accuracy failed\n");
  }

exit:
  for (int i = 0; i < CROPNUM; i++)
  {
    bm_image_destroy(dst[i]);
  }

  for (int i = 0; i < 4; i++) {
    bm_free_device(handle, mem[i]);
  }
  bm_image_destroy(src);

  return ret;
}

static int test_vpp_fbd_compressed_to_rgb() {
  int IMAGE_H = 1080;
  int IMAGE_W = 1920;
  int ret = 0;

  bm_image_format_ext dst_format_1[] = {
    FORMAT_RGB_PLANAR,
    FORMAT_BGR_PLANAR,
    FORMAT_RGB_PACKED,
    FORMAT_BGR_PACKED,
    FORMAT_RGBP_SEPARATE,
    FORMAT_BGRP_SEPARATE,
  };

  bm_image_data_format_ext datatype[] = {
    DATA_TYPE_EXT_FP16,
    DATA_TYPE_EXT_BF16,
    DATA_TYPE_EXT_FLOAT32,
  };

  bm_image_format_ext dst_fmt;
  bm_image_data_format_ext dst_datatpye;
  unsigned char fbd_output[CROPNUM][16];

  bm_image src, dst[CROPNUM];
  bm_image_create(handle,
                  IMAGE_H,
                  IMAGE_W,
                  FORMAT_COMPRESSED,
                  DATA_TYPE_EXT_1N_BYTE,
                  &src);
  bm_device_mem_t mem[4];
  memset(mem, 0, sizeof(bm_device_mem_t) * 4);

  unsigned char * buf[4] = {NULL};
  int plane_size[4] = {0};

  read_image(&buf[0], &plane_size[0],"offset_base_y.bin");
  read_image(&buf[1], &plane_size[1],"offset_comp_y.bin");
  read_image(&buf[2], &plane_size[2],"offset_base_c.bin");
  read_image(&buf[3], &plane_size[3],"offset_comp_c.bin");

  for (int i = 0; i < 4; i++) {
    bm_malloc_device_byte(handle, mem + i, plane_size[i]);
    bm_memcpy_s2d(handle, mem[i], (void *)buf[i]);
  }
  bm_image_attach(src, mem);

  std::vector<bmcv_rect_t> rects;
  int x = IMAGE_W / 2;
  int y = IMAGE_H / 2;
  for (int i = 0; i < CROPNUM; i++) {
    rects.push_back({i/16 *32, i*2, x, y});
  }
  bmcv_rect_t *rect = &rects[0];
  int crop_num = rects.size();

  for (int i = 0; i < CROPNUM/2; i++) {
    dst_fmt = dst_format_1[i % (sizeof(dst_format_1)/sizeof(bm_image_format_ext))];
    dst_datatpye = (i%2 == 0) ? DATA_TYPE_EXT_1N_BYTE_SIGNED : DATA_TYPE_EXT_1N_BYTE;
 //   printf("i %d, dst_fmt  0x%x,dst_datatpye  0x%x\n",i,dst_fmt,dst_datatpye);

    bm_image_create(handle,
      IMAGE_H / 2 + 2*i,
      IMAGE_W / 2 + 2*i,
      dst_fmt,
      dst_datatpye,
      dst + i);
    bm_image_alloc_dev_mem(dst[i]);
  }

  for (int i = CROPNUM/2; i < CROPNUM; i++) {
    dst_fmt = FORMAT_RGBP_SEPARATE;
    dst_datatpye = datatype[i % (sizeof(datatype)/sizeof(bm_image_data_format_ext))];
   // printf("i %d, dst_fmt  0x%x,dst_datatpye  0x%x\n",i,dst_fmt,dst_datatpye);

    bm_image_create(handle,
      IMAGE_H / 2 + 2*i,
      IMAGE_W / 2 + 2*i,
      dst_fmt,
      dst_datatpye,
      dst + i);
    bm_image_alloc_dev_mem(dst[i]);
  }

  struct timeval t1, t2;
  gettimeofday_(&t1);
  if(BM_SUCCESS != bmcv_image_vpp_convert(handle, crop_num, src, dst, rect, BMCV_INTER_LINEAR)){
    ret = -1;
    goto exit;
  }
  gettimeofday_(&t2);
  cout << "VPP convert using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

#if 0
  for (int i = 0; i < CROPNUM; i++) {
    std::ostringstream os;
    os << "fbd";
    os << "_BT601_";
    os << i;
    os << ".bin";
    bm1684x_vpp_write_bin(dst[i], os.str().c_str());
  }
#endif

  for(int j=0;j<CROPNUM;j++)
  {
    int image_byte_size[4] = {0};
    bm_image_get_byte_size(dst[j], image_byte_size);

    int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
    unsigned char* output_ptr = (unsigned char *)malloc(byte_size);

    void* out_ptr[4] = {(void*)output_ptr,
      (void*)((unsigned char*)output_ptr + image_byte_size[0]),
      (void*)((unsigned char*)output_ptr + image_byte_size[0] + image_byte_size[1]),
      (void*)((unsigned char*)output_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};

    bm_image_copy_device_to_host(dst[j], (void **)out_ptr);

    md5_get(output_ptr,byte_size,fbd_output[j]);

#if 0
    printf("{");
    for(int i=0;i<16;i++)
      printf("0x%x,",fbd_output[j][i]);
    printf("},");
    printf("\n");
#endif

  free(output_ptr);
  }

  ret = memcmp(fbd_rgb_output_expected_value, fbd_output, sizeof(fbd_output));

  if(ret ==0 ){
    printf("Fbd to rgb, comparison accuracy succeed\n");
  }
  else{
    printf("Fbd to rgb, comparison accuracy failed\n");
  }

exit:
  for (int i = 0; i < CROPNUM; i++)
  {
    bm_image_destroy(dst[i]);
  }

  for (int i = 0; i < 4; i++) {
    bm_free_device(handle, mem[i]);
  }
  bm_image_destroy(src);

  free(buf[0]);
  free(buf[1]);
  free(buf[2]);
  free(buf[3]);

  return ret ;
}

#endif

int main(int argc, char *argv[]) {
    int test_loop_times = 0;

    if (NULL != getenv("BMCV_TEST_FILE_PATH")) {
        strcpy(opencvFile_path, getenv("BMCV_TEST_FILE_PATH"));
    }

    if (argc == 1) {
        test_loop_times = 1;
    } else {
        test_loop_times = atoi(argv[1]);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST VPP FBD] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    std::cout << "[TEST VPP FBD] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST VPP FBD] LOOP " << loop_idx << "------"
                  << std::endl;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
#ifndef USING_CMODEL
        if(0 != test_vpp_fbd_compressed_to_yuv())
          return -1;
        if(0 != test_vpp_fbd_compressed_to_rgb())
          return -1;
#endif
        bm_dev_free(handle);
    }
    std::cout << "------[TEST VPP FBD] ALL TEST PASSED!" << std::endl;

    return 0;
}