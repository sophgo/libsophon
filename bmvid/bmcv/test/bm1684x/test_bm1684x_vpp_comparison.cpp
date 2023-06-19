#include <iostream>
#include <vector>
#include "test_misc.h"
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

extern bm_status_t bm1684x_vpp_cmodel_calc(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr,
  border_t*               border_param,
  font_t*                 font_param);

int main(int argc, char **argv) {

  bm_handle_t handle = NULL;
  int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt,dst_fmt;
  char *src_name, *dst_name;
  bm_image       src, dst, src_cmodel, dst_cmodel;
  bmcv_rect_t rect;
  int  dev_id = 0;

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


  bm_status_t ret    = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
      printf("Create bm handle failed. ret = %d\n", ret);
      exit(-1);
  }

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst);
  bm_image_alloc_dev_mem(src);
  bm_image_alloc_dev_mem(dst);

  int src_image_byte_size[4] = {0};
  bm_image_get_byte_size(src, src_image_byte_size);
  printf("src plane0 size: %d\n", src_image_byte_size[0]);
  printf("src plane1 size: %d\n", src_image_byte_size[1]);
  printf("src plane2 size: %d\n", src_image_byte_size[2]);
  printf("src plane3 size: %d\n", src_image_byte_size[3]);
  int src_byte_size = src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2] + src_image_byte_size[3];
  char* src_input_ptr = (char*)malloc(src_byte_size);
  void* src_in_ptr[4] = {(void*)src_input_ptr,
                     (void*)((char*)src_input_ptr + src_image_byte_size[0]),
                     (void*)((char*)src_input_ptr + src_image_byte_size[0] + src_image_byte_size[1]),
                     (void*)((char*)src_input_ptr + src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2])};

  FILE *fp_src = fopen(src_name, "rb+");

  if(fread((void *)src_input_ptr, 1, src_byte_size, fp_src) != (unsigned int)src_byte_size)
    printf("%s %s %d,fread %s failed\n",filename(__FILE__), __func__, __LINE__,src_name);


  fclose(fp_src);

  bm_image_copy_host_to_device(src, (void **)src_in_ptr);


  int dst_image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, dst_image_byte_size);
  printf("dst plane0 size: %d\n", dst_image_byte_size[0]);
  printf("dst plane1 size: %d\n", dst_image_byte_size[1]);
  printf("dst plane2 size: %d\n", dst_image_byte_size[2]);
  printf("dst plane3 size: %d\n", dst_image_byte_size[3]);
  int dst_byte_size = dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2] + dst_image_byte_size[3];


  char* dst_input_ptr        = (char*)malloc(dst_byte_size);


  void* dst_in_ptr[4] = {(void*)dst_input_ptr,
                     (void*)((char*)dst_input_ptr + dst_image_byte_size[0]),
                     (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1]),
                     (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};

  bmcv_image_vpp_convert(handle, 1, src, &dst, &rect);

  bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);

  FILE *fp_dst = fopen(dst_name, "wb");

  fwrite((void *)dst_input_ptr, 1, dst_byte_size, fp_dst);

  fclose(fp_dst);
/***************************************************************************************************/
  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src_cmodel);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst_cmodel);


  bm_device_mem_t src_cmodel_mem[4];
  bm_device_mem_t dst_cmodel_mem[4];

  int src_cmodel_plane_num = bm_image_get_plane_num(src_cmodel);
  for(int i = 0; i< src_cmodel_plane_num; i++)
  {
    src_cmodel_mem[i].u.device.device_addr = (unsigned long)src_in_ptr[i];
    src_cmodel_mem[i].size = src_image_byte_size[i];
    src_cmodel_mem[i].flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
  }

  bm_image_attach(src_cmodel, src_cmodel_mem);

  char* dst_input_ptr_cmodel = (char*)malloc(dst_byte_size);
  void* dst_in_ptr_cmodel[4] = {(void*)dst_input_ptr_cmodel,
    (void*)((char*)dst_input_ptr_cmodel + dst_image_byte_size[0]),
    (void*)((char*)dst_input_ptr_cmodel + dst_image_byte_size[0] + dst_image_byte_size[1]),
    (void*)((char*)dst_input_ptr_cmodel + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};

  int dst_cmodel_plane_num = bm_image_get_plane_num(dst_cmodel);
  for(int i = 0; i< dst_cmodel_plane_num; i++)
  {
    dst_cmodel_mem[i].u.device.device_addr = (unsigned long)dst_in_ptr_cmodel[i];
    dst_cmodel_mem[i].size = dst_image_byte_size[i];
    dst_cmodel_mem[i].flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
  }

  bm_image_attach(dst_cmodel, dst_cmodel_mem);

  bm1684x_vpp_cmodel_calc(handle, 1, &src_cmodel, &dst_cmodel, &rect,NULL,BMCV_INTER_LINEAR,CSC_MAX_ENUM,NULL,NULL,NULL,NULL);

  FILE *fp_dst_cmodel = fopen("cmodel.bin", "wb");

  fwrite((void *)dst_input_ptr_cmodel, 1, dst_byte_size, fp_dst_cmodel);

  fclose(fp_dst_cmodel);

  int ret1 = memcmp(dst_input_ptr, dst_input_ptr_cmodel, dst_byte_size);

  if(ret1 ==0 )
    printf("asic dst and cmode dst Consistent comparison,success.\n");
  else
    printf("asic dst and cmode dst error.\n");



  delete [] src_input_ptr;
  delete [] dst_input_ptr;
  delete [] dst_input_ptr_cmodel;

  bm_image_destroy(dst_cmodel);
  bm_image_destroy(src_cmodel);
  bm_image_destroy(dst);
  bm_image_destroy(src);
  bm_dev_free(handle);

  return 0;
}

