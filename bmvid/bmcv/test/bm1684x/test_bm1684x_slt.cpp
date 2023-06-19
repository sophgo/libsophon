#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/time.h>
#include "bmcv_api_ext.h"
#include "md5.h"

bm_handle_t handle = NULL;
char opencvFile_path[200] = "./";
#define UNUSED_VARIABLE(x)  ((x) = (x))

#define MAX_THREAD_NUM (16)
char *filename[MAX_THREAD_NUM];
unsigned int flag[MAX_THREAD_NUM] = {0};
unsigned int thread_time[MAX_THREAD_NUM] = {0};
char *g_string_path;
int g_flag = 1;
unsigned int g_compare = 0,g_printtime = 0, g_comparetime = 0;

char flag_break[MAX_THREAD_NUM] = {0};

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

void *vpp_test(void * arg)
{

    int id = *(int *)arg;
    unsigned int frame_count = 0,compare_time, compare_count = 0,compare;
    int idx_h = 0;
    int ret1 = 0;
    unsigned char *pb_golden_mem=NULL;
    unsigned char golden_output_md5[16] = {0};
    unsigned char output_expected_md5value[16]={
      0x19,0xac,0x8e,0x35,0x87,0x3e,0x37,0xa8,0x72,0x89,0x2b,0xd3,0x17,0x46,0xce,0x1e
    };

    compare_time = g_comparetime;
    compare = g_compare;
    frame_count = g_flag;

   int IMAGE_H = 1080;
   int IMAGE_W = 1920;
   int image_byte_size[4] = {0};

   bm_image src, dst;
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

   bm_image_create(handle,
                  IMAGE_H,
                  IMAGE_W,
                  FORMAT_BGR_PACKED,
                  DATA_TYPE_EXT_1N_BYTE,
                  &dst);
   bm_image_alloc_dev_mem(dst);

   bm_image_get_byte_size(dst, image_byte_size);

   int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
   unsigned char* output_ptr = (unsigned char*)malloc(byte_size);

   unsigned char* out_ptr[4] = {(unsigned char*)output_ptr,
                       ((unsigned char*)output_ptr + image_byte_size[0]),
                       ((unsigned char*)output_ptr + image_byte_size[0] + image_byte_size[1]),
                       ((unsigned char*)output_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};


   if(compare == 1)
   {

        /*channel b*/
        FILE *fb = fopen("1686-bgr.bin", "rb");
        if(!fb) {
            printf("ERROR: Unable to open 1686-bgr.bin!\n");
             exit(-1);
        }
        fseek(fb, 0, SEEK_END);
        int len_b = ftell(fb);

        pb_golden_mem = (unsigned char *)malloc(len_b);
        fseek(fb, 0, SEEK_SET);
        size_t cnt = fread(pb_golden_mem, 1, len_b, fb);
        cnt = cnt;

        fclose(fb);
    }


    flag[id] = 0;

    while (frame_count)
    {

        bmcv_image_vpp_convert(handle, 1, src, &dst);
        bm_image_copy_device_to_host(dst, (void **)out_ptr);

        flag[id]++;
        frame_count--;
        compare_count++;

        if(compare == 1)
        {
          if(compare_count == compare_time)
          {
            /*begin to compare*/
            for (idx_h = 0; idx_h < 1080*1920*3; idx_h++)
            {

              if (output_ptr[idx_h] != pb_golden_mem[idx_h])
              {
               printf("nv12 fbd to rgbpacked test error, thread id %d\n",id);
               printf("idx_h %d,output_ptr 0x%x ,pb_golden_mem 0x%x ",idx_h,output_ptr[idx_h],pb_golden_mem[idx_h]);

               FILE *fb_dst = fopen("debug-bgr.bin", "wb");
               fwrite((void *)output_ptr, 1, 1920*1080*3, fb_dst);
               fclose(fb_dst);

               return ((void*)(-1));
              }

            }
                compare_count = 0;
          }
        }

        if(compare == 2)
        {
          if(compare_count == compare_time)
          {

            md5_get(output_ptr, byte_size, golden_output_md5);

            ret1 = memcmp(output_expected_md5value, golden_output_md5, sizeof(golden_output_md5));

            if(ret1 !=0 )
            {
              printf("1684x vpp_slt, compare failed\n");
              return ((void*)(-1));
            }

            compare_count = 0;
          }
        }

    }

if(compare == 1)
{
    free(pb_golden_mem);
}
    free(output_ptr);
    flag_break[id]=1;


    bm_image_destroy(dst);
    bm_image_destroy(src);
    for (int i = 0; i < 4; i++) {
        bm_free_device(handle, mem[i]);
    }

    free(buf[0]);
    free(buf[1]);
    free(buf[2]);
    free(buf[3]);

    return NULL;
}


int main(int argc, char **argv)
{
    struct timeval  old_time, tv0;
    unsigned int i, mark = 0, fps = 0, mark_break = 0;
    unsigned int whole_time = 0;
    int dev_id = 0;
    void *tret;
    int ret = 0;
    if (NULL != getenv("BMCV_TEST_FILE_PATH")) {
        strcpy(opencvFile_path, getenv("BMCV_TEST_FILE_PATH"));
    }

    unsigned int thread_num = 16;

    if (argc == 6) {
      g_compare = atoi(argv[1]);
      g_printtime = atoi(argv[2]);
      g_flag = atoi(argv[3]);
      g_comparetime = atoi(argv[4]);
      thread_num = atoi(argv[5]);
      dev_id = 0;

    } else if (argc == 7) {
      g_compare = atoi(argv[1]);
      g_printtime = atoi(argv[2]);
      g_flag = atoi(argv[3]);
      g_comparetime = atoi(argv[4]);
      thread_num = atoi(argv[5]);
      dev_id = atoi(argv[6]);
    } else {
      printf("%s parameters should be 6 or 7\n",argv[0]);
      exit(-1);
    }

    bm_status_t ret1    = bm_dev_request(&handle, dev_id);
    if (ret1 != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }





    pthread_t vc_thread[thread_num];

    gettimeofday(&tv0, NULL);
    for (i = 0; i < thread_num; i++) {
      int ret = pthread_create(&vc_thread[i], NULL, vpp_test, &i);
      if (ret != 0)
      {
        printf("pthread create failed");
        return -1;
      }
      sleep(1);
    }

    mark = 0;
    gettimeofday(&tv0, NULL);
    old_time = tv0;
    while(1)
    {
        gettimeofday(&tv0, NULL);
        whole_time = (tv0.tv_sec - old_time.tv_sec) * 1000000 + (tv0.tv_usec - old_time.tv_usec);
        if(whole_time >= (g_printtime*1000000))
        {

            for(i = 0; i < thread_num; i++)
            {

                mark += flag[i];
                flag[i] = 0;
                mark_break += flag_break[i];
            }
            fps = mark / (whole_time/1000000);
            printf("all frame: %u, fps %u,whole_time %us\n",mark,fps,whole_time/1000000);
            if(mark == 0)
            {
                break;
            }

            mark = 0;
            fps = 0;
            old_time = tv0;
            if(mark_break == thread_num)
            {
                break;
            }
            mark_break = 0;
        }
    }


  for (i = 0; i < thread_num; i++) {
    pthread_join(vc_thread[i], &tret);
    if((long long)tret < 0) {
        ret = -1;
        printf("id  %d, tret   %lld\n",i,(long long)tret);
    }
  }

  return ret;
}

