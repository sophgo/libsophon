#include <math.h>
#include "stdio.h"
#include "stdlib.h"
#include "getopt.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#define UNUSED_VARIABLE(x)  ((x) = (x))

extern void bm_write_bin(bm_image dst, const char *output_name);
extern void bm_read_bin(bm_image src, const char *input_name);

static void user_usage() {
  printf(
    "-h xxx : matrix row,     default 8192\n"
    "-w xxx : matrix columns, default 8192\n"
    "-s xxx : input name,     default: NULL, random data\n"
    "-d xxx : output name,    default: NULL, not dump output data\n"
    "-c xxx : csv name,       default: NULL, not save csv file\n"
    "-l xxx : loop time,      default: 1\n"
    "-m xxx : copmare is 1,   default:0\n"
    "-H xxx : user_usage \n"
  );
}


void cpu_matrix_log(float *matrix, int rows, int cols)
{
  for (int i = 0; i < rows; i++)
  {
    for (int j = 0; j < cols; j++)
    {
      matrix[i*cols + j] = logf(matrix[i*cols + j]);
    }
  }

}

int cmp(float* matrix1, float* matrix2, int byte_size)
{
    const float epsilon = 1e-6;

    for (int i = 0; i < byte_size; ++i) {
      if (fabs(matrix1[i] - matrix2[i]) > epsilon)
        return -1;
    }
    return 0;
}

void cpu_matrix_compare(bm_image src, bm_image dst)
{
  int image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, image_byte_size);
  int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
  char* input_ptr = (char *)malloc(byte_size);
  char* bmcv_output_ptr = (char *)malloc(byte_size);

  void* in_ptr[4] = {(void*)input_ptr,
                     (void*)((char*)input_ptr + image_byte_size[0]),
                     (void*)((char*)input_ptr + image_byte_size[0] + image_byte_size[1]),
                     (void*)((char*)input_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};

  bm_image_copy_device_to_host(src, (void **)in_ptr);

  void* out_ptr[4] = {(void*)bmcv_output_ptr,
                     (void*)((char*)bmcv_output_ptr + image_byte_size[0]),
                     (void*)((char*)bmcv_output_ptr + image_byte_size[0] + image_byte_size[1]),
                     (void*)((char*)bmcv_output_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};

  bm_image_copy_device_to_host(dst, (void **)out_ptr);


  cpu_matrix_log((float*)input_ptr,src.height,src.width);

  int ret1 = cmp((float*)input_ptr, (float*)bmcv_output_ptr, src.height*src.width);

  if(ret1 == 0 )
    printf("matrix_log comparison success.\n");
  else
    printf("matrix_log comparison failed.\n");

  free(input_ptr);
  free(bmcv_output_ptr);
}

float randomFloat(float min, float max) {
    float range = max - min;
    float random = ((float)rand()) / RAND_MAX;
    return (random * range) + min;
}

void printMatrix(float **matrix, int row, int col) {
    int i, j;
    for (i = 0; i < row; i++) {
        for (j = 0; j < col; j++) {
            printf("%.2f\t", matrix[i][j]);
        }
        printf("\n");
    }
}

void gen_matrix_random_data(bm_image src)
{
  int image_byte_size[4] = {0};
  bm_image_get_byte_size(src, image_byte_size);
  long long byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
  char* input_ptr = (char *)malloc(byte_size);

  for(long long i = 0; i < src.height * src.width; i++)
    *((float *)input_ptr + i) = randomFloat(0, 60000);

  void* in_ptr[4] = {(void*)input_ptr,
                     (void*)((char*)input_ptr + image_byte_size[0]),
                     (void*)((char*)input_ptr + image_byte_size[0] + image_byte_size[1]),
                     (void*)((char*)input_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};

  bm_image_copy_host_to_device(src, (void **)in_ptr);

  free(input_ptr);
}

int main(int argc, char *argv[]) {

  struct option long_options[] = {
    {"matrix_row", required_argument,   nullptr,  'h'},
    {"matrix_col", required_argument,   nullptr,  'w'},
    {"src_name",   required_argument,   nullptr,  's'},
    {"dst_name",   required_argument,   nullptr,  'd'},
    {"loop_time",  required_argument,   nullptr,  'l'},
    {"csv_name",   required_argument,   nullptr,  'c'},
    {"compare",    required_argument,   nullptr,  'm'},
    {"help",       no_argument,         nullptr,  'H'}
  };

  bm_handle_t handle = NULL;
  int src_w, src_h, dst_w, dst_h;
  dst_h = src_h = 8192;
  dst_w = src_w = 8192;

  const char *src_name = NULL, *dst_name = NULL, *csv_name = NULL;
  bm_image src, dst, dst_cmodel;

  unsigned int i = 0, loop_time = 1,compare = 0;
  unsigned long long time_single, time_total = 0, time_avg = 0;
  unsigned long long time_max = 0, time_min = 1000000;
  int dev_id = 0;
  int ch = 0, opt_idx = 0;
  while ((ch = getopt_long(argc, argv, "h:w:s:d:l:c:m:H", long_options, &opt_idx)) != -1) {
    switch (ch) {
      case 'h':
        dst_h = src_h = atoi(optarg);
        break;
      case 'w':
        dst_w = src_w = atoi(optarg);
        break;
      case 's':
        src_name = optarg;
        break;
      case 'd':
        dst_name = optarg;
        break;
      case 'l':
        loop_time = atoi(optarg);
        break;
      case 'c':
        csv_name = optarg;
        break;
      case 'm':
        compare = atoi(optarg);
        break;
      case 'H':
        user_usage();
        return 0;
    }
  }

  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;

  bm_status_t ret = bm_dev_request(&handle, dev_id);
  if (ret != BM_SUCCESS) {
    printf("Create bm handle failed. ret = %d\n", ret);
    exit(-1);
  }

  printf(" src_h = %d, src_w = %d,loop_time = %d,dst_name = %s,csv_name = %s,compare = %d \n",
    src_h, src_w,loop_time, dst_name,csv_name,compare);

  bm_image_create(handle, src_h, src_w, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &src);
  bm_image_alloc_dev_mem(src,1);

  if(NULL == src_name)
    gen_matrix_random_data(src);
  else
    bm_read_bin(src,src_name);

  bm_image_create(handle, dst_h, dst_w, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &dst);
  bm_image_alloc_dev_mem(dst,1);

  for(i = 0;i < loop_time; i++){

    gettimeofday_(&tv_start);
    bmcv_matrix_log(handle, src, dst);
    gettimeofday_(&tv_end);

    timediff.tv_sec  = tv_end.tv_sec - tv_start.tv_sec;
    timediff.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
    time_single = (unsigned int)(timediff.tv_sec * 1000000 + timediff.tv_usec);
    if(time_single>time_max){time_max = time_single;}
    if(time_single<time_min){time_min = time_single;}
    time_total = time_total + time_single;
  }
  time_avg = time_total / loop_time;

  printf("loop %d cycles, time_max = %llu, time_min = %llu, time_avg = %llu\n",
  loop_time, time_max, time_min, time_avg);

  if(NULL != dst_name)
    bm_write_bin(dst, dst_name);

  if(1 == compare)
    cpu_matrix_compare(src, dst);

  if(NULL != csv_name)
  {
    FILE *fp_csv = fopen(csv_name, "ab+");
    fprintf(fp_csv, "%d*%d->%d*%d,time_avg: %lld us ,time_min: %lld us\n",src_w,src_h,dst_w,dst_h,time_avg,time_min);
    fclose(fp_csv);
  }

  bm_image_destroy(src);
  bm_image_destroy(dst);
  bm_dev_free(handle);

  return 0;
}

