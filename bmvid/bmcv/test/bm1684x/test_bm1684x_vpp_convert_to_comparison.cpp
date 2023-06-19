#include <iostream>
#include <vector>
#include "test_misc.h"
#include "bmcv_api_ext.h"
#include "stdio.h"
#include "stdlib.h"
#include <sstream>
#include <string.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif

static bm_handle_t handle;
char opencvFile_path[200] = "./";
#define UNUSED_VARIABLE(x)  ((x) = (x))

using namespace std;

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

int test_vpp_convert_to(
  unsigned int dst_w,
  unsigned int dst_h,
  bm_image_format_ext dst_fmt,
  bmcv_convert_to_attr convert_to_attr,
  bm_image_data_format_ext dst_data_type)
{
  unsigned int src_h, src_w;
  bm_image_format_ext src_fmt;
  bm_image       src, dst, src_cmodel, dst_cmodel;
  unsigned char *input_ptr = NULL;
  int src_len;

  read_image(&input_ptr, &src_len,"girl_1920x1080_nv12.bin");

  void *buf[4] = {(void *)input_ptr, (void *)(input_ptr + 1920*1080), 0, 0};

  src_w = 1920;
  src_h = 1080;
  src_fmt = FORMAT_NV12;

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, dst_data_type, &dst);
  bm_image_alloc_dev_mem(src);
  bm_image_alloc_dev_mem(dst);

  bm_image_copy_host_to_device(src, buf);

  int dst_image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, dst_image_byte_size);
#if 0
  printf("dst plane0 size: %d\n", dst_image_byte_size[0]);
  printf("dst plane1 size: %d\n", dst_image_byte_size[1]);
  printf("dst plane2 size: %d\n", dst_image_byte_size[2]);
  printf("dst plane3 size: %d\n", dst_image_byte_size[3]);
#endif
  int dst_byte_size = dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2] + dst_image_byte_size[3];

  char* dst_input_ptr        = (char*)malloc(dst_byte_size);

  void* dst_in_ptr[4] = {(void*)dst_input_ptr,
                     (void*)((char*)dst_input_ptr + dst_image_byte_size[0]),
                     (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1]),
                     (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};

  struct timeval t1, t2;
  gettimeofday_(&t1);
  bmcv_image_convert_to(handle, 1, convert_to_attr, &src, &dst);
  gettimeofday_(&t2);
  cout << "convert to using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

  bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);

#if 0
  FILE *fp_dst = fopen("ax+b.bin", "wb");

  fwrite((void *)dst_input_ptr, 1, dst_byte_size, fp_dst);

  fclose(fp_dst);
#endif
/***************************************************************************************************/
  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src_cmodel);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, dst_data_type, &dst_cmodel);


  bm_device_mem_t src_cmodel_mem[4];
  bm_device_mem_t dst_cmodel_mem[4];
  int src_size[4] = {0};
  bm_image_get_byte_size(src_cmodel, src_size);

  src_cmodel_mem[0].u.device.device_addr = (unsigned long)buf[0];
  src_cmodel_mem[0].size = src_size[0];
  src_cmodel_mem[0].flags.u.mem_type     = BM_MEM_TYPE_DEVICE;

  src_cmodel_mem[1].u.device.device_addr = (unsigned long)buf[1];
  src_cmodel_mem[1].size = src_size[1];
  src_cmodel_mem[1].flags.u.mem_type     = BM_MEM_TYPE_DEVICE;

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

  bm1684x_vpp_cmodel_calc(handle, 1, &src_cmodel, &dst_cmodel, NULL ,NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, &convert_to_attr, NULL, NULL);

#if 0
  FILE *fp_dst_cmodel = fopen("cmodel.bin", "wb");
  fwrite((void *)dst_input_ptr_cmodel, 1, dst_byte_size, fp_dst_cmodel);
  fclose(fp_dst_cmodel);
#endif

  int ret1 = memcmp(dst_input_ptr, dst_input_ptr_cmodel, dst_byte_size);

  if(ret1 == 0)
    printf("test_vpp_convert_to success\n");
  else
    printf("test_vpp_convert_to failed\n");

  delete [] dst_input_ptr;
  delete [] dst_input_ptr_cmodel;

  bm_image_destroy(dst_cmodel);
  bm_image_destroy(src_cmodel);
  bm_image_destroy(dst);
  bm_image_destroy(src);

  free(input_ptr);

  return ret1;
}


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
        std::cout << "[test bm1684x vpp convert to] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    std::cout << "[test bm1684x vpp convert to]] test starts... LOOP times will be "
              << test_loop_times << std::endl;


    unsigned int dst_w,dst_h;
    bm_image_format_ext dst_fmt;
    bmcv_convert_to_attr convert_to_attr;
    bm_image_data_format_ext dst_data_type;

    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[test bm1684x vpp convert to] LOOP " << loop_idx << "------"
                  << std::endl;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
/********************************************************************************************/

        dst_w = 1920;
        dst_h = 1080;
        dst_fmt = FORMAT_RGBYP_PLANAR;
        convert_to_attr.alpha_0 = 1.1245612365478;
        convert_to_attr.alpha_1 = 0.9356426654128;
        convert_to_attr.alpha_2 = 0.8765426547789;
        convert_to_attr.beta_0 =  3.12563147895621;
        convert_to_attr.beta_1 = -4.23145874126695;
        convert_to_attr.beta_2 = -5.21463987451264;
        dst_data_type = DATA_TYPE_EXT_1N_BYTE_SIGNED;

        printf("dst_data_type: DATA_TYPE_EXT_1N_BYTE_SIGNED, dst_fmt: FORMAT_RGBYP_PLANAR\n");
        if(0 != test_vpp_convert_to(dst_w, dst_h, dst_fmt, convert_to_attr, dst_data_type))
          return -1;
/********************************************************************************************/
#if 1
        dst_w = 8192;
        dst_h = 8192;
        dst_fmt = FORMAT_HSV180_PACKED;
        convert_to_attr.alpha_0 = 1.1245612365478;
        convert_to_attr.alpha_1 = 0.9356426654128;
        convert_to_attr.alpha_2 = 0.8765426547789;
        convert_to_attr.beta_0 =  3.12563147895621;
        convert_to_attr.beta_1 = -4.23145874126695;
        convert_to_attr.beta_2 = -5.21463987451264;
        dst_data_type = DATA_TYPE_EXT_1N_BYTE;

        printf("dst_data_type: DATA_TYPE_EXT_1N_BYTE, dst_fmt: FORMAT_HSV180_PACKED\n");
        if(0 != test_vpp_convert_to(dst_w, dst_h, dst_fmt, convert_to_attr, dst_data_type))
          return -1;
#endif
/********************************************************************************************/
#if 1
        dst_w = 4096;
        dst_h = 2160;
        dst_fmt = FORMAT_YUV444P;
        convert_to_attr.alpha_0 = 1.1245612365478;
        convert_to_attr.alpha_1 = 0.9356426654128;
        convert_to_attr.alpha_2 = 0.8765426547789;
        convert_to_attr.beta_0 =  3.12563147895621;
        convert_to_attr.beta_1 = -4.23145874126695;
        convert_to_attr.beta_2 = -5.21463987451264;
        dst_data_type = DATA_TYPE_EXT_FP16;

        printf("dst_data_type: DATA_TYPE_EXT_FP16, dst_fmt: FORMAT_YUV444P\n");
        if(0 != test_vpp_convert_to(dst_w, dst_h, dst_fmt, convert_to_attr, dst_data_type))
          return -1;
#endif
/********************************************************************************************/
#if 1
        dst_w = 3840;
        dst_h = 1024;
        dst_fmt = FORMAT_BGR_PLANAR;
        convert_to_attr.alpha_0 = 1.1245612365478;
        convert_to_attr.alpha_1 = 0.9356426654128;
        convert_to_attr.alpha_2 = 0.8765426547789;
        convert_to_attr.beta_0 =  3.12563147895621;
        convert_to_attr.beta_1 = -4.23145874126695;
        convert_to_attr.beta_2 = -5.21463987451264;
        dst_data_type = DATA_TYPE_EXT_BF16;

        printf("dst_data_type: DATA_TYPE_EXT_BF16, dst_fmt: FORMAT_BGR_PLANAR\n");
        if(0 != test_vpp_convert_to(dst_w, dst_h, dst_fmt, convert_to_attr, dst_data_type))
          return -1;
/********************************************************************************************/
        dst_w = 800;
        dst_h = 600;
        dst_fmt = FORMAT_RGBP_SEPARATE;
        convert_to_attr.alpha_0 = 1.1245612365478;
        convert_to_attr.alpha_1 = 0.9356426654128;
        convert_to_attr.alpha_2 = 0.8765426547789;
        convert_to_attr.beta_0 =  3.12563147895621;
        convert_to_attr.beta_1 = -4.23145874126695;
        convert_to_attr.beta_2 = -5.21463987451264;
        dst_data_type = DATA_TYPE_EXT_FLOAT32;

        printf("dst_data_type: DATA_TYPE_EXT_FLOAT32, dst_fmt: FORMAT_RGBP_SEPARATE\n");
        if(0 != test_vpp_convert_to(dst_w, dst_h, dst_fmt, convert_to_attr, dst_data_type))
          return -1;
/********************************************************************************************/
#endif

        bm_dev_free(handle);
    }
    std::cout << "------[[test bm1684x vpp convert to]] ALL TEST PASSED!" << std::endl;

    return 0;
}

