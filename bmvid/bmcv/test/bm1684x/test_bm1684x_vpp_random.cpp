#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#include <pthread.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif
using namespace std;

struct thread_arg{
    int test_loop_times;
    int devid;
    int thread_id;
};

int test_seed = -1;
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

static int test_vpp_random(
  bm_handle_t handle,
  unsigned seed)
{

  unsigned int src_h, src_w, dst_w, dst_h, minimum_w, minimum_h, maximum_w, maximum_h;
  bm_image_format_ext src_fmt,dst_fmt;
  bm_image       src, dst, src_cmodel, dst_cmodel;
  bmcv_rect_t rect;
  int i, ret = 0;
  bm_image_format_ext src_format[] = {
    FORMAT_YUV420P,
    FORMAT_YUV422P,
    FORMAT_YUV444P,
    FORMAT_NV12,
    FORMAT_NV21,
    FORMAT_NV16,
    FORMAT_NV61,FORMAT_RGB_PLANAR,
    FORMAT_BGR_PLANAR,
    FORMAT_RGB_PACKED,
    FORMAT_BGR_PACKED,
    FORMAT_RGBP_SEPARATE,
    FORMAT_BGRP_SEPARATE,
    FORMAT_GRAY, FORMAT_YUV444_PACKED,
    FORMAT_YVU444_PACKED,
    FORMAT_YUV422_YUYV,
    FORMAT_YUV422_YVYU,
    FORMAT_YUV422_UYVY,
    FORMAT_YUV422_VYUY};
  bm_image_format_ext dst_format[] = {
    FORMAT_YUV420P,
    FORMAT_YUV444P,
    FORMAT_NV12,
    FORMAT_NV21,FORMAT_RGB_PLANAR,
    FORMAT_BGR_PLANAR,
    FORMAT_RGB_PACKED,
    FORMAT_BGR_PACKED,
    FORMAT_RGBP_SEPARATE,
    FORMAT_BGRP_SEPARATE,
    FORMAT_GRAY,FORMAT_RGBYP_PLANAR,
    FORMAT_HSV180_PACKED,
    FORMAT_HSV256_PACKED};

  srand(seed);

  src_w = rand() % 8185 + 8;
  src_h = rand() % 8185 + 8;
  src_fmt = src_format[rand() % (sizeof(src_format)/sizeof(bm_image_format_ext))];
  dst_fmt = dst_format[rand() % (sizeof(dst_format)/sizeof(bm_image_format_ext))];

  rect.crop_w = rand() % (src_w - 7) + 8;
  rect.crop_h = rand() % (src_h - 7) + 8;

  rect.start_x = rand() % (src_w - rect.crop_w + 1);
  rect.start_y = rand() % (src_h - rect.crop_h + 1);

  if(rect.crop_w < 8 * 128)
    minimum_w = 8;
  else
    minimum_w = rect.crop_w / 128 + 1;

  if(rect.crop_w * 128 > 8192)
    maximum_w = 8192;
  else
    maximum_w = rect.crop_w * 128;

  dst_w = rand() % (maximum_w - minimum_w +1) + minimum_w;

  if(rect.crop_h < 8 * 128)
    minimum_h = 8;
  else
    minimum_h = rect.crop_h / 128 + 1;

  if(rect.crop_h * 128 > 8192)
    maximum_h = 8192;
  else
    maximum_h = rect.crop_h * 128;

  dst_h = rand() % (maximum_h - minimum_h +1) + minimum_h;

  std::cout << "seed: " << seed << std::endl;
  printf("src_w 0x%x, src_h 0x%x, rect.start_x 0x%x, rect.start_y 0x%x, rect.crop_w  0x%x,rect.crop_h 0x%x,src_fmt  0x%x\n",src_w,src_h,rect.start_x, rect.start_y, rect.crop_w ,rect.crop_h,src_fmt);
  printf("dst_w 0x%x, dst_h 0x%x, dst_fmt 0x%x\n",dst_w,dst_h,dst_fmt);

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst);
  bm_image_alloc_dev_mem(src);
  bm_image_alloc_dev_mem(dst);

  int src_image_byte_size[4] = {0};
  bm_image_get_byte_size(src, src_image_byte_size);

  int src_byte_size = src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2] + src_image_byte_size[3];
  char* src_input_ptr = (char*)malloc(src_byte_size);
  void* src_in_ptr[4] = {(void*)src_input_ptr,
                     (void*)((char*)src_input_ptr + src_image_byte_size[0]),
                     (void*)((char*)src_input_ptr + src_image_byte_size[0] + src_image_byte_size[1]),
                     (void*)((char*)src_input_ptr + src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2])};


  for (i = 0; i < src_byte_size; i++) {
    src_input_ptr[i] = rand() % 255 + 1;
  }

  bm_image_copy_host_to_device(src, (void **)src_in_ptr);

  int dst_image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, dst_image_byte_size);

  int dst_byte_size = dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2] + dst_image_byte_size[3];

  char* dst_input_ptr        = (char*)malloc(dst_byte_size);


  void* dst_in_ptr[4] = {(void*)dst_input_ptr,
    (void*)((char*)dst_input_ptr + dst_image_byte_size[0]),
    (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1]),
    (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};

  struct timeval t1, t2;
  gettimeofday_(&t1);
  bmcv_image_vpp_convert(handle, 1, src, &dst, &rect);
  gettimeofday_(&t2);
  cout << "vpp convert using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;


  bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);

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


  int ret1 = memcmp(dst_input_ptr, dst_input_ptr_cmodel, dst_byte_size);
  if(ret1 ==0 ) {
    printf("asic  and cmode compare success\n");
    ret = 0;
  } else {
    printf("asic  and cmode compare error\n");
    ret = -1;
  }

  delete [] src_input_ptr;
  delete [] dst_input_ptr;
  delete [] dst_input_ptr_cmodel;

  bm_image_destroy(dst_cmodel);
  bm_image_destroy(src_cmodel);
  bm_image_destroy(dst);
  bm_image_destroy(src);

  return ret;
}

static void *test_vpp_random_thread(void *arg) {


    int i;
    int loop_times;
    unsigned int seed;
    bm_handle_t handle;
    int dev, ret = 0;
 //   int thread_id = 0;

    loop_times = ((struct thread_arg *)arg)->test_loop_times;
    dev = ((struct thread_arg *)arg)->devid;
  //  thread_id = ((struct thread_arg *)arg)->thread_id;

    std::cout << "dev_id is " << dev <<endl;
    bm_dev_request(&handle, dev);

    printf("MULTI THREAD TEST STARTING----thread id is 0x%lx\n",pthread_self());
    std::cout << "[TEST CV VPP RANDOM] test starts... LOOP times will be "
              << loop_times << std::endl;

    for(i = 0; i < loop_times; i++) {
        std::cout << "------[TEST CV VPP RANDOM] LOOP " << i << "------"
                  << std::endl;
        if (test_seed == -1) {
            seed = (unsigned)time(NULL);
        } else {
            seed = test_seed;
        }

        cout << "start of loop test " << i << endl;
        ret = test_vpp_random(handle, seed);
        if(ret != 0){
          break;
          exit(-1);
        }
    }
    bm_dev_free(handle);
    return NULL;
}

int main(int argc, char **argv) {

    int test_loop_times  = 0;
    int devid = 0;
    int test_threads_num = 1;
    char * pEnd;

    if (argc == 1) {
        test_loop_times  = 1;
        test_threads_num = 1;
    } else if (argc == 2) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = 1;
    } else if (argc == 3) {
        test_loop_times  = atoi(argv[1]);
        devid = atoi(argv[2]);
    } else if (argc == 4) {
        test_loop_times  = atoi(argv[1]);
        devid = atoi(argv[2]);
        test_threads_num = atoi(argv[3]);
    } else if (argc == 5) {
        test_loop_times  = atoi(argv[1]);
        devid = atoi(argv[2]);
        test_threads_num = atoi(argv[3]);
        test_seed = strtol(argv[4],&pEnd,0);
    } else {
        std::cout << "command input error, please follow this "
                     "order:test_cv_vpp_random loop_num devid multi_thread_num seed"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500000 || test_loop_times < 1) {
        std::cout << "[TEST CV VPP RANDOM] loop times should be 1~1500000" << std::endl;
        exit(-1);
    }
    if (devid > 255 || devid < 0) {
        std::cout << "[TEST CV VPP RANDOM] devid should be 0~255" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 32 || test_threads_num < 1) {
        std::cout << "[TEST CV VPP RANDOM] thread nums should be 1~32 " << std::endl;
        exit(-1);
    }

    pthread_t * pid = new pthread_t[test_threads_num];
    struct thread_arg arg[test_threads_num];

  for (int i = 0; i < test_threads_num; i++) {
    arg[i].test_loop_times = test_loop_times;
    arg[i].devid = devid;
    arg[i].thread_id = i;
    if (pthread_create( &pid[i], NULL, test_vpp_random_thread, (void *)(&arg[i])))
        {
            delete[] pid;
            perror("create thread failed\n");
            exit(-1);
    }
  }
  int ret = 0;
  for (int i = 0; i < test_threads_num; i++) {
    ret = pthread_join(pid[i], NULL);
    if (ret != 0) {
      delete[] pid;
      perror("Thread join failed");
      exit(-1);
    }
  }
  std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
  delete[] pid;

  return 0;
}


