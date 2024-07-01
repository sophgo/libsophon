#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <getopt.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#include <pthread.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif

#define MAX_THREAD_NUM (128)
#define NEED_COMPARE (1)
#define NEED_CALC_MEMORY (1)

unsigned int single_frame_count[MAX_THREAD_NUM] = {0};
unsigned int finished_count = 0;
pthread_mutex_t mutex;


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

struct thread_arg{
  int loop_times;
  int devid;
  int thread_id;
  int compare;
  int memory_cacl;
  unsigned int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt,dst_fmt;
  char *src_name, *dst_name;
};

static void user_usage() {
  printf(
    "-N : num,\n"
    "-a : src_name[0],\n"
    "-b : src_name[1],\n"
    "-c : src_name[2],\n"
    "-d : src_name[3],\n"
    "-e : src_w,\n"
    "-f : src_h,\n"
    "-g : src_fmt,     \n"
    "-h : dst_name,    \n"
    "-i : dst_w,  \n"
    "-j : dst_h,\n"
    "-k : dst_fmt,\n"
  );
}

// 读取堆内存使用情况
void read_heap_memory(const char *heap_name, unsigned long *heap_size, unsigned long *used_memory) {
    char filename[100];
    int ret = 0;

    sprintf(filename, "/sys/kernel/debug/ion/%s/summary", heap_name);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }

    // 读取数据
    ret = fscanf(fp, "%*s %*s %*s %*s size:%lu %*s used:%lu bytes", heap_size, used_memory);
    if((0 == ret ) || (EOF == ret))
    {
      printf("read_heap_memory, fscanf heap_size wrong\n");
    }

    fclose(fp);
}

// 获取进程的 RSS 内存大小
unsigned long get_process_rss() {
    FILE *fp;
    char path[100];
    char line[256];
    unsigned long rss = 0;

    sprintf(path, "/proc/%d/status", getpid());
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("Error opening file");
        return 0;
    }

    printf("/proc/%d/status \n", getpid());

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %lu kB", &rss);
            break;
        }
    }

    fclose(fp);
    return rss;
}

int readfile(const char *path, unsigned char* input_data, unsigned int size) {
  int ret = 0;
  unsigned int cnt = 0;

  FILE *fp_src = fopen(path, "rb");
  if (fp_src == NULL) {
    printf("file: %s is  NULL\n", path);
    return -1;
  }
  cnt = fread((void *)input_data, 1, size, fp_src);
  if (cnt < size) {
    printf("file size %d  is less than required bytes %d\n", cnt, size);
    ret = -1;
  };
  fclose(fp_src);
  return ret;
}

static void write_bin(const char *output_path, unsigned char *output_data, unsigned int size) {
  FILE *fp_dst = fopen(output_path, "wb");
  if (fp_dst == NULL) {
    printf("open file: %s failed \n", output_path);
    return;
  }
  fwrite(output_data, 1, size, fp_dst);
  fclose(fp_dst);
}
static int cmodel_compare(bm_handle_t handle, bm_image src, void* src_in_ptr[4], bm_image dst, bmcv_rect_t rect)
{
  bm_image  src_cmodel, dst_cmodel;

  int src_image_byte_size[4] = {0};
  bm_image_get_byte_size(src, src_image_byte_size);

  int dst_image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, dst_image_byte_size);
  int dst_byte_size = dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2] + dst_image_byte_size[3];

  char* dst_input_ptr        = (char*)malloc(dst_byte_size);
  void* dst_in_ptr[4] = {(void*)dst_input_ptr,
    (void*)((char*)dst_input_ptr + dst_image_byte_size[0]),
    (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1]),
    (void*)((char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};

  bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);

  bm_image_create(handle, src.height, src.width, src.image_format, src.data_type, &src_cmodel);
  bm_image_create(handle, dst.height, dst.width, dst.image_format, dst.data_type, &dst_cmodel);

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


  int ret = memcmp(dst_input_ptr, dst_input_ptr_cmodel, dst_byte_size);
  free(dst_input_ptr);
  free(dst_input_ptr_cmodel);

  return ret;
}
void  write_output_bin(char *dst_name, bm_image dst, int thread_id)
{
  int dst_image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, dst_image_byte_size);
  int dst_byte_size = dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2] + dst_image_byte_size[3];
  unsigned char* dst_input_ptr        = (unsigned char*)malloc(dst_byte_size);
  void* dst_in_ptr[4] = {(void*)dst_input_ptr,
    (void*)((unsigned char*)dst_input_ptr + dst_image_byte_size[0]),
    (void*)((unsigned char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1]),
    (void*)((unsigned char*)dst_input_ptr + dst_image_byte_size[0] + dst_image_byte_size[1] + dst_image_byte_size[2])};
  bm_image_copy_device_to_host(dst, (void **)dst_in_ptr);

  char dst_name_id[128]= {0};
  int len = 0;
  len = snprintf(dst_name_id, 128, "%s_%d", dst_name, thread_id);
  if(len >128)
    printf("Output truncated. Required length: %d\n", len+1);

  write_bin(dst_name_id, dst_input_ptr, dst_byte_size);
}


static void *test_vpp_random_thread(void *arg) {

  int i, loop_times, thread_id, dev, ret = 0, compare = 0, memory_cacl = 0;
  bm_handle_t handle;
  unsigned int src_h, src_w, dst_w, dst_h;
  bm_image_format_ext src_fmt,dst_fmt;
  bm_image src, dst, src_cmodel, dst_cmodel;
  bmcv_rect_t rect;
  char *src_name = NULL, *dst_name = NULL;

  unsigned long rss_before, rss_after;
  unsigned long npu_heap_size_before, npu_used_memory_before;
  unsigned long vpp_heap_size_before, vpp_used_memory_before;
  unsigned long vpu_heap_size_before, vpu_used_memory_before;

  unsigned long npu_heap_size_after, npu_used_memory_after;
  unsigned long vpp_heap_size_after, vpp_used_memory_after;
  unsigned long vpu_heap_size_after, vpu_used_memory_after;


  loop_times = ((struct thread_arg *)arg)->loop_times;
  dev = ((struct thread_arg *)arg)->devid;
  thread_id = ((struct thread_arg *)arg)->thread_id;
  compare = ((struct thread_arg *)arg)->compare;
  memory_cacl = ((struct thread_arg *)arg)->memory_cacl;

  src_w = ((struct thread_arg *)arg)->src_w;
  src_h = ((struct thread_arg *)arg)->src_h;
  src_fmt = ((struct thread_arg *)arg)->src_fmt;
  dst_w = ((struct thread_arg *)arg)->dst_w;
  dst_h = ((struct thread_arg *)arg)->dst_h;
  dst_fmt = ((struct thread_arg *)arg)->dst_fmt;
  src_name = ((struct thread_arg *)arg)->src_name;
  dst_name = ((struct thread_arg *)arg)->dst_name;


  single_frame_count[thread_id] = {0};

  srand(thread_id);
  bm_dev_request(&handle, dev);

  rect.start_x = 0;
  rect.start_y = 0;
  rect.crop_w = src_w;
  rect.crop_h = src_h;

  bm_image_create(handle, src_h, src_w, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
  bm_image_create(handle, dst_h, dst_w, dst_fmt, DATA_TYPE_EXT_1N_BYTE, &dst);
  bm_image_alloc_dev_mem(src);
  bm_image_alloc_dev_mem(dst);

  int src_image_byte_size[4] = {0};
  bm_image_get_byte_size(src, src_image_byte_size);

  int src_byte_size = src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2] + src_image_byte_size[3];
  unsigned char* src_input_ptr = (unsigned char*)malloc(src_byte_size);
  void* src_in_ptr[4] = {(void*)src_input_ptr,
                     (void*)((unsigned char*)src_input_ptr + src_image_byte_size[0]),
                     (void*)((unsigned char*)src_input_ptr + src_image_byte_size[0] + src_image_byte_size[1]),
                     (void*)((unsigned char*)src_input_ptr + src_image_byte_size[0] + src_image_byte_size[1] + src_image_byte_size[2])};

  if(NULL == src_name)
  {
    for (i = 0; i < src_byte_size; i++) {
      src_input_ptr[i] = rand() % 255 + 1;
    }
  }
  else
  {
    ret = readfile(src_name, src_input_ptr, src_byte_size);
    if(ret!=0)
    {
      printf("readfile failed, thread id %d\n", thread_id);
      exit(-1);
    }
  }

  bm_image_copy_host_to_device(src, (void **)src_in_ptr);


  for (i = 0; i < loop_times; i++) {

    if(NEED_CALC_MEMORY == memory_cacl)
    {
      rss_before = get_process_rss();
      read_heap_memory("bm_npu_heap_dump", &npu_heap_size_before, &npu_used_memory_before);
      read_heap_memory("bm_vpp_heap_dump", &vpp_heap_size_before, &vpp_used_memory_before);
      read_heap_memory("bm_vpu_heap_dump", &vpu_heap_size_before, &vpu_used_memory_before);
    }

    bmcv_image_vpp_convert(handle, 1, src, &dst, &rect);

    if(NEED_CALC_MEMORY == memory_cacl)
    {
      rss_after = get_process_rss();

      printf("Memory consumption: %lu kB, rss_before %lu kB, rss_after %lu kB\n", rss_after - rss_before, rss_before, rss_after);
      read_heap_memory("bm_npu_heap_dump", &npu_heap_size_after, &npu_used_memory_after);
      read_heap_memory("bm_vpp_heap_dump", &vpp_heap_size_after, &vpp_used_memory_after);
      read_heap_memory("bm_vpu_heap_dump", &vpu_heap_size_after, &vpu_used_memory_after);
      printf("npu_heap size: %lu Byte, npu_heap consumption %lu Byte, npu_used_memory_after %lu Byte, npu_used_memory_before  %lu Byte\n", 
      npu_heap_size_after,npu_used_memory_after-npu_used_memory_before,npu_used_memory_after,npu_used_memory_before);
      printf("vpp_heap size: %lu Byte, vpp_heap consumption %lu Byte, vpp_used_memory_after %lu Byte, vpp_used_memory_before  %lu Byte\n", 
      vpp_heap_size_after,vpp_used_memory_after-vpp_used_memory_before,vpp_used_memory_after,vpp_used_memory_before);
      printf("vpu_heap size: %lu Byte, vpu_heap consumption %lu Byte, vpu_used_memory_after %lu Byte, vpu_used_memory_before  %lu Byte\n", 
      vpu_heap_size_after,vpu_used_memory_after-vpu_used_memory_before,vpu_used_memory_after,vpu_used_memory_before);    
    }


    single_frame_count[thread_id]++;

    if(NEED_COMPARE == compare)
    {
      ret = cmodel_compare(handle, src, src_in_ptr, dst, rect);
      if(0 != ret) {
        printf("asic  and cmode compare error, thread_id %d\n",thread_id);
        exit(-1);
      }
    }
  }

  if(NULL != dst_name)
  {
    write_output_bin(dst_name, dst, thread_id);
  }

  free(src_input_ptr);
  bm_image_destroy(dst);
  bm_image_destroy(src);


  bm_dev_free(handle);

  pthread_mutex_lock(&mutex);
  finished_count++;
  pthread_mutex_unlock(&mutex);

  return NULL;
}

int main(int argc, char **argv) {

  struct option long_options[] = {
    {"sw",       required_argument,   NULL,  'a'},
    {"sh",       required_argument,   NULL,  'b'},
    {"sformat",  required_argument,   NULL,  'k'},
    {"sname",    required_argument,   NULL,  'i'},
    {"dw",       required_argument,   NULL,  'e'},
    {"dh",       required_argument,   NULL,  'f'},
    {"dformat",  required_argument,   NULL,  'g'},
    {"dname",    required_argument,   NULL,  'j'},
  };

  struct timeval  last_time, new_time;
  unsigned int i, loop_times  = 1;
  int devid = 0, ret = 0, total_frames = 0, whole_time = 0;
  unsigned int thread_num = 1, fps = 0, compare = 0, memory_cacl = 0;

  unsigned int src_h = 1080, src_w = 1920, dst_w = 1920, dst_h = 1080;
  bm_image_format_ext src_fmt = FORMAT_YUV444P, dst_fmt = FORMAT_YUV444P;
  char *src_name = NULL, *dst_name = NULL;

  int ch = 0, opt_idx = 0;
  while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f:g:i:j:k:l:m:t:", long_options, &opt_idx)) != -1) {
    switch (ch) {
      case 't':
        thread_num = atoi(optarg);
        break;
      case 'd':
        devid = atoi(optarg);
        break;
      case 'a':
        src_w = atoi(optarg);
        break;
      case 'b':
        src_h = atoi(optarg);
        break;
      case 'k':
        src_fmt = (bm_image_format_ext)atoi(optarg);
        break;
      case 'e':
        dst_w = atoi(optarg);
        break;
      case 'f':
        dst_h = atoi(optarg);
        break;
      case 'g':
        dst_fmt = (bm_image_format_ext)atoi(optarg);
        break;
      case 'l':
        loop_times = atoi(optarg);
        break;
      case 'c':
        compare = atoi(optarg);
        break;
      case 'i':
        src_name = optarg;
        break;
      case 'j':
        dst_name = optarg;
        break;
      case 'm':
        memory_cacl = atoi(optarg);
        break;
      case '?':
        user_usage();
        return 0;
    }
  }

  printf("argc %d, thread_num %d, devid %d, loop_times %d, compare %d, memory_cacl %d\n",argc, thread_num, devid, loop_times, compare, memory_cacl);
  printf("src_w  %d, src_h %d, src_fmt %d\n",src_w,src_h,src_fmt);
  printf("dst_w  %d, dst_h  %d, dst_fmt  %d\n",dst_w,dst_h,dst_fmt);

  pthread_t * pid = new pthread_t[thread_num];
  struct thread_arg arg[thread_num];

  for (i = 0; i < thread_num; i++) {
    arg[i].loop_times = loop_times;
    arg[i].devid = devid;
    arg[i].thread_id = i;
    arg[i].compare = compare;
    arg[i].memory_cacl = memory_cacl;

    arg[i].src_w = src_w;
    arg[i].src_h = src_h;
    arg[i].src_fmt = src_fmt;
    arg[i].dst_w = dst_w;
    arg[i].dst_h = dst_h;
    arg[i].dst_fmt = dst_fmt;
    arg[i].src_name = src_name;
    arg[i].dst_name = dst_name;

    if (pthread_create( &pid[i], NULL, test_vpp_random_thread, (void *)(&arg[i])))
    {
      delete[] pid;
      perror("create thread failed\n");
      exit(-1);
    }
  }

  pthread_mutex_init(&mutex, NULL);


  total_frames = 0;
  gettimeofday(&new_time, NULL);
  last_time = new_time;
  while(1)
  {
    pthread_mutex_lock(&mutex);
    if(finished_count >= thread_num)
    {
      break;
    }
    pthread_mutex_unlock(&mutex);



    gettimeofday(&new_time, NULL);
    whole_time = (new_time.tv_sec - last_time.tv_sec) * 1000000 + (new_time.tv_usec - last_time.tv_usec);
    if(whole_time >= 1000000)
    {
      for(i = 0; i < thread_num; i++)
      {
        total_frames += single_frame_count[i];
        single_frame_count[i] = 0;
      }
      fps = total_frames / (whole_time/1000000);
      printf("total_frames: %u, fps %u,whole_time %us\n",total_frames,fps,whole_time);

      total_frames = 0;
      fps = 0;
      last_time = new_time;
    }
  }

  for (i = 0; i < thread_num; i++) {
    ret = pthread_join(pid[i], NULL);
    if (ret != 0) {
      delete[] pid;
      perror("Thread join failed");
      exit(-1);
    }
  }

  std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;

  pthread_mutex_destroy(&mutex);
  delete[] pid;

  return 0;
}