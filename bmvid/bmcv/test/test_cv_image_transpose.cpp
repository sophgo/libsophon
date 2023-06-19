#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string.h>
#include <time.h>
#include <math.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include "bmcv_api.h"
#include "test_misc.h"

using namespace std;

int main(int argc, char **argv)
{
  bm_handle_t handle;
  int dev = 0;
  int h = 1080;
  int w = 1920;
  uint8_t *src;
  uint8_t *dst;
  uint8_t *ref;
  bool random;
  bm_image image_in;
  bm_image image_out;
  bmcv_image bmcv_image_in;
  bmcv_image bmcv_image_out;

  if (argc == 1) {
    random = true;
  } else if (argc == 2) {
    if (!atoi(argv[1]))
      random = false;
    else
      random = true;
  } else {
    printf("too much parameters\n");
    printf("please just input one parameter as switch to random\n");
    exit(-1);
  }

  if (random) {
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    std::cout << "seed: " << seed << std::endl;
    srand(seed);
    h = rand() % 2000;
    w = rand() % 2000;
  }

  std::cout << "w = " << w << std::endl;
  std::cout << "h = " << h << std::endl;
  bm_dev_request(&handle, dev);
  bm_image_create(handle,
                  h,
                  w,
                  FORMAT_RGB_PLANAR,
                  DATA_TYPE_EXT_1N_BYTE,
                  &image_in);
  bm_image_create(handle,
                  w,
                  h,
                  FORMAT_RGB_PLANAR,
                  DATA_TYPE_EXT_1N_BYTE,
                  &image_out);

  bm_image_alloc_dev_mem(image_in);
  bm_image_alloc_dev_mem(image_out);
  bm_image_to_bmcv_image(&image_in, &bmcv_image_in);
  bm_image_to_bmcv_image(&image_out, &bmcv_image_out);

  src = new uint8_t[h * w * 3];
  dst = new uint8_t[h * w * 3];
  ref = new uint8_t[h * w * 3];
  for (int i = 0; i < h * w * 3; i++)
    src[i] = rand() % 256;

  bm_image_copy_host_to_device(image_in, (void **)&src);

  struct timeval t1, t2;
  gettimeofday_(&t1);
  bmcv_img_transpose(handle,
                    bmcv_image_in,
                    bmcv_image_out);
  gettimeofday_(&t2);
  cout << "img transpose using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

  bm_image_copy_device_to_host(image_out, (void **)&dst);

  for (int k = 0; k < 3; k++)
    for (int i = 0; i < h; i++)
      for (int j = 0; j < w; j++)
        ref[k * h * w + j * h + i] = src[k * h * w + i * w + j];

  for (int i = 0; i < h * w * 3; i++) {
    if (ref[i] != dst[i]) {
      std::cout << "not match at "
      << i << " as ref is " << ref[i]
      << " dst is " << dst[i] << std::endl;
      return -1;
    }
  }
  std::cout << "success!\n" <<std::endl;
  delete[] ref;
  delete[] src;
  delete[] dst;
  bm_image_destroy(image_in);
  bm_image_destroy(image_out);
  return 0;
}
