bmcv_memory_permute
======================

该接口基于bm_image来实现张量C和H维度的变换

.. math::
    N, C, H, W -> N, H, C, W


**接口形式：**

    .. code-block:: c

      bm_status_t bmcv_memory_permute (
                  bm_handle_t handle,
                  bm_device_mem_t src,
                  bm_device_mem_t dst,
                  int N,
                  int C,
                  int H,
                  int W,
                  int data_size);


**参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t\* src

  输入参数。该结构描述了输入数据的属性。

* bm_device_mem_t\* dst

  输出参数。该结构描述了输出数据的属性。

* int N

  输入参数。输入批次大小。

* int C

  输入参数。输入通道数。

* int H

  输入参数。输入高度。

* int W

  输入参数。输入宽度。

* int data_size

  输入参数。数据所占用的内存字节数。


**数据类型说明：**

    .. code-block:: c
        :linenos:
        :lineno-start: 1
        :force:

        typedef struct bm_mem_desc {
          union {
            struct {
        #ifdef __linux__
              unsigned long device_addr;
        #else
              unsigned long long device_addr;
        #endif
              unsigned int reserved;
              int dmabuf_fd;
            } device;

            struct {
              void *system_addr;
              unsigned int reserved0;
              int reserved1;
            } system;
          } u;

          bm_mem_flags_t flags;
          unsigned int size;
        } bm_mem_desc_t;

        typedef struct bm_mem_desc bm_device_mem_t;

.. list-table:: bm_device_mem_t 参数介绍
    :widths: 15 35

    * - **参数名称**
      - **描述**
    * - device_addr
      - 权重图的物理地址。
    * - size
      - 权重图的字节数大小。

**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**示例代码**

    .. code-block:: c

      #include <assert.h>
      #include <stdint.h>
      #include <stdio.h>
      #include <stdlib.h>
      #include <unistd.h>
      #include <pthread.h>
      #include "bmcv_api_ext_c.h"
      #include "bmcv_internal.h"

      typedef struct {
          int n;
          int c;
          int h;
          int w;
      } bmcv_patch_attr_t;

      typedef struct {
          bm_handle_t handle;
          bmcv_patch_attr_t patch_attr;
          float *input;
      } patch_thread_arg_t;

      void bm_rand_bin(bmcv_patch_attr_t patch_attr, float** input) {
          int total_byte_size = patch_attr.n * patch_attr.c * patch_attr.h * patch_attr.w;
          *input = (float*)malloc(total_byte_size * sizeof(float));
          if (!*input) {
              printf("Failed to allocate memory for file reading\n");
              return;
          }
          for (int i = 0; i < total_byte_size; i++) {
              (*input)[i] = (float)((rand() >> 7) % 256);
          }
      }

      void *test_width_align_patch_thread(void *arg)
      {
          patch_thread_arg_t *thread_arg = (patch_thread_arg_t *)arg;
          bm_handle_t handle = thread_arg->handle;
          bm_status_t ret = bm_dev_request(&handle, 0);
          if (ret != BM_SUCCESS) {
              printf("Create bm handle failed. ret is %d \n", ret);
              exit(-1);
          }

          int N = thread_arg->patch_attr.n;
          int C = thread_arg->patch_attr.c;
          int H = thread_arg->patch_attr.h;
          int W = thread_arg->patch_attr.w;
          int in_h    = 1;
          int in_w    = N * C * H * W;
          int out_h   = 1;
          int out_w   = N * C * H * W;

          float *src_data = (float *)malloc(out_h * out_w * sizeof(float));
          memcpy(src_data, thread_arg->input, sizeof(float) * out_h * out_w);

          bm_image bm_input;
          bm_image_create(handle, in_h, in_w, (bm_image_format_ext)FORMAT_GRAY, (bm_image_data_format_ext)DATA_TYPE_EXT_FLOAT32, &bm_input, NULL);
          bm_image_alloc_dev_mem(bm_input, BMCV_HEAP1_ID);
          bm_image_copy_host_to_device(bm_input, (void **)&src_data);
          bm_image bm_output;
          bm_image_create(handle, out_h, out_w, (bm_image_format_ext)FORMAT_GRAY, (bm_image_data_format_ext)DATA_TYPE_EXT_FLOAT32, &bm_output, NULL);
          bm_image_alloc_dev_mem(bm_output, BMCV_HEAP1_ID);

          ret = bmcv_memory_permute(handle, bm_input.image_private->data[0], bm_output.image_private->data[0], N, C, H, W, sizeof(float));

          float *res_data = (float *)malloc(out_w * out_h * sizeof(float));
          bm_image_copy_device_to_host(bm_output, (void **)&res_data);

          bm_image_destroy(&bm_input);
          bm_image_destroy(&bm_output);
          free(src_data);
          free(res_data);

          return NULL;
      }

      int main(int argc, char **argv) {
          int ret = 0;
          int n = 4;
          int c = 3;
          int h = 416;
          int w = 672;
          int dev_cnt = 1;

          bm_dev_getcount(&dev_cnt);
          bm_handle_t handle[dev_cnt];
          bm_status_t req = bm_dev_request(handle, 0);
          if (req != BM_SUCCESS) {
              printf("Create bm handle for dev 0 failed \n");
              exit(-1);
          }

          pthread_t *pid = (pthread_t *)malloc(sizeof(pthread_t) * dev_cnt);
          patch_thread_arg_t *patch_to_thread_arg = (patch_thread_arg_t *)malloc(sizeof(patch_thread_arg_t) * dev_cnt);
          patch_to_thread_arg[0].handle = handle[0];
          patch_to_thread_arg[0].patch_attr.n = n;
          patch_to_thread_arg[0].patch_attr.c = c;
          patch_to_thread_arg[0].patch_attr.w = w;
          patch_to_thread_arg[0].patch_attr.h = h;

          bm_rand_bin(patch_to_thread_arg[0].patch_attr, &(patch_to_thread_arg[0].input));
          if (pthread_create(&pid[0],
                              NULL,
                              test_width_align_patch_thread,
                              &patch_to_thread_arg[0])) {
              free(pid);
              free(patch_to_thread_arg);
              printf("Create thread failed \n");
              exit(-1);
          }

          bm_dev_free(handle[0]);
          if (patch_to_thread_arg[0].input) {
              free(patch_to_thread_arg[0].input);
              patch_to_thread_arg[0].input = NULL;
          }
          free(pid);
          free(patch_to_thread_arg);
          return ret;
      }


**格式支持:**

1. 该接口支持下列情形data type之间的转换：

* DATA_TYPE_EXT_FLOAT32    ——>    DATA_TYPE_EXT_FLOAT32


**注意事项：**

1. 在调用 bmcv_memory_permute()之前必须确保输入的 image 内存已经申请。

2. 输入图片的批次大小 * 通道数 * 宽 * 高 必须等于输出图片的批次大小 * 通道数 * 宽 * 高。

3. 输入、输出图片的批次大小、通道数、宽、高 必须大于零。

4. 输入、输出图片的最小尺寸为8 * 8，最大尺寸为4096 * 4096。