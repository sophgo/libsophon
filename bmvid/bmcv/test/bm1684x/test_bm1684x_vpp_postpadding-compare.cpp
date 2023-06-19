#include <iostream>
#include <vector>
#include "stdio.h"
#include "stdlib.h"
#include <sstream>
#include <string.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "md5.h"

#define CROPNUM 2
static bm_handle_t handle;
char opencvFile_path[200] = "./";
#define UNUSED_VARIABLE(x)  ((x) = (x))

using namespace std;

void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);

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

int test_vpp_postpadding(void)
{

  unsigned int src_h, src_w;
  bm_image_format_ext src_fmt;
  bm_image       src, dst[CROPNUM];
  unsigned char *input_ptr = NULL;
  int src_len;

  read_image(&input_ptr, &src_len,"girl_1920x1080_nv12.bin");

  void *buf[4] = {(void *)input_ptr, (void *)(input_ptr + 1920*1080), 0, 0};

  bmcv_rect_t rect[CROPNUM];
  bmcv_padding_atrr_t padding_param[CROPNUM];
  int dst_w[CROPNUM], dst_h[CROPNUM];
  bm_image_format_ext dst_fmt[CROPNUM];
  bm_image_data_format_ext dst_data_type[CROPNUM];

  unsigned char postpadding_output_expected_value[CROPNUM][16]={
    {0xc0,0x34,0x8,0x15,0x98,0xcb,0x7d,0x13,0x48,0x94,0xfa,0x10,0xac,0x92,0xf5,0xe2,},
    {0x2f,0x65,0xce,0xbb,0xa3,0x6f,0x65,0x3e,0x36,0xae,0x4a,0x8b,0x4d,0x92,0x4,0xac,},
  };
  unsigned char postpadding_output[CROPNUM][16] = {0};

  src_w = 1920;
  src_h = 1080;
  src_fmt = FORMAT_NV12;

  rect[0].start_x = 703;
  rect[0].start_y = 51;
  rect[0].crop_w = 1213;
  rect[0].crop_h = 1027;
  rect[1]=rect[0];

  dst_w[0] = 1920;
  dst_h[0] = 2160;
  dst_fmt[0] = FORMAT_RGB_PACKED;
  dst_data_type[0] = DATA_TYPE_EXT_1N_BYTE;

  dst_w[1] = 1314;
  dst_h[1] = 1280;
  dst_fmt[1] = FORMAT_RGB_PACKED;
  dst_data_type[1] = DATA_TYPE_EXT_1N_BYTE;

  padding_param[0].dst_crop_stx = (dst_w[0] - rect[0].crop_w)/2;
  padding_param[0].dst_crop_sty = (dst_h[0] - rect[0].crop_h)/2;
  padding_param[0].dst_crop_w = rect[0].crop_w;
  padding_param[0].dst_crop_h = rect[0].crop_h;
  padding_param[0].padding_r = 255;
  padding_param[0].padding_g = 0;
  padding_param[0].padding_b = 0;
  padding_param[0].if_memset = 1;

  padding_param[1].dst_crop_stx = (dst_w[1] - rect[1].crop_w)/2;
  padding_param[1].dst_crop_sty = (dst_h[1] - rect[1].crop_h)/2;
  padding_param[1].dst_crop_w = rect[1].crop_w;
  padding_param[1].dst_crop_h = rect[1].crop_h;
  padding_param[1].padding_r = 0;
  padding_param[1].padding_g = 255;
  padding_param[1].padding_b = 0;
  padding_param[1].if_memset = 1;

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_create(handle, dst_h[0], dst_w[0], dst_fmt[0], dst_data_type[0], &dst[0]);
  bm_image_create(handle, dst_h[1], dst_w[1], dst_fmt[1], dst_data_type[1], &dst[1]);

  bm_image_alloc_dev_mem(src);
  bm_image_alloc_dev_mem(dst[0]);
  bm_image_alloc_dev_mem(dst[1]);

  bm_image_copy_host_to_device(src, buf);

  struct timeval t1, t2;
  gettimeofday_(&t1);
  bmcv_image_vpp_convert_padding(handle, CROPNUM, src ,dst, padding_param, rect, BMCV_INTER_LINEAR);
  gettimeofday_(&t2);
  cout << "vpp convert padding using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

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

    md5_get(output_ptr,byte_size,postpadding_output[j]);

#if 0
    printf("{");
    for(int i=0;i<16;i++)
      printf("0x%x,",postpadding_output[j][i]);
    printf("},");
    printf("\n");
#endif

    std::ostringstream os;
    os << "vpp";
    os << "+postpadding+";
    os << j;
    os << ".bin";
    bm1684x_vpp_write_bin(dst[j], os.str().c_str());

    free(output_ptr);
  }

  int ret1 = memcmp(postpadding_output_expected_value, postpadding_output, sizeof(postpadding_output));

  if(ret1 ==0 )
    printf("test_vpp_postpadding, compare succeed\n");
  else
    printf("test_vpp_postpadding, compare failed\n");


  bm_image_destroy(dst[1]);
  bm_image_destroy(dst[0]);
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
        std::cout << "[test vpp postpadding] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    std::cout << "[test vpp postpadding] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[test vpp postpadding] LOOP " << loop_idx << "------"
                  << std::endl;
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }

        printf("nv12->rgbpacked,crop 2 number, color red and green\n");
        if(0 != test_vpp_postpadding())
          return -1;

        bm_dev_free(handle);
    }
    std::cout << "------[test vpp postpadding] ALL TEST PASSED!" << std::endl;

    return 0;
}


