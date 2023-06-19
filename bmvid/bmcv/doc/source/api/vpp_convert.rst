bmcv_image_vpp_convert
=========================


该 API 将输入图像格式转化为输出图像格式,并支持 crop + resize 功能,支持从 1 张输入中 crop 多张输出并 resize 到输出图片大小。

    .. code-block:: c

        bm_status_t bmcv_image_vpp_convert(
            bm_handle_t  handle,
            int output_num,
            bm_image input,
            bm_image *output,
            bmcv_rect_t *crop_rect,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR
        );


**传入参数说明:**

* bm_handle_t handle

输入参数。设备环境句柄,通过调用 bm_dev_request 获取

* int output_num

输出参数。输出 bm_image 数量，和src image的crop 数量相等,一个src crop 输出一个dst bm_image

* bm_image input

输入参数。输入 bm_image 对象

* bm_image* output

输出参数。输出 bm_image 对象指针

* bmcv_rect_t * crop_rect

输入参数。具体格式定义如下：

    .. code-block:: c

        typedef struct bmcv_rect {  
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;             
        } bmcv_rect_t;

每个输出 bm_image 对象所对应的在输入图像上 crop 的参数，包括起始点x坐标、起始点y坐标、crop图像的宽度以及crop图像的高度。

* bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR

输入参数。resize 算法选择，包括 BMCV_INTER_NEAREST 和 BMCV_INTER_LINEAR 两种，默认情况下是双线性差值。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项:**

1. 该 API 所需要满足的格式以及部分要求与 bmcv_image_vpp_basic 中的表格相同。

2. 输入输出的寬高（src.width, src.height, dst.widht, dst.height）限制在 16 ～ 4096 之间。

3. 输入必须关联 device memory，否则返回失败。

4. FORMAT_COMPRESSED 是 VPU 解码后内置的一种压缩格式，它包括4个部分：Y compressed table、Y compressed data、CbCr compressed table 以及 CbCr compressed data。请注意 bm_image 中这四部分存储的顺序与 FFMPEG 中 AVFrame 稍有不同，如果需要 attach AVFrame 中 device memory 数据到 bm_image 中时，对应关系如下，关于 AVFrame 详细内容请参考 VPU 的用户手册。

    .. code-block:: c

        bm_device_mem_t src_plane_device[4];
        src_plane_device[0] = bm_mem_from_device((u64)avframe->data[6],
                avframe->linesize[6]);
        src_plane_device[1] = bm_mem_from_device((u64)avframe->data[4],
                avframe->linesize[4] * avframe->h);
        src_plane_device[2] = bm_mem_from_device((u64)avframe->data[7],
                avframe->linesize[7]);
        src_plane_device[3] = bm_mem_from_device((u64)avframe->data[5],
                avframe->linesize[4] * avframe->h / 2);

        bm_image_attach(*compressed_image, src_plane_device);



**代码示例：** 

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "bmlib_utils.h"
        #include "common.h"
        #include <memory>
        #include "stdio.h"
        #include "stdlib.h"
        #include <stdio.h>
        #include <stdlib.h>
        
        int main(int argc, char *argv[]) {
            bm_handle_t handle;
            int            image_h     = 1080;
            int            image_w     = 1920;
            bm_image       src, dst[4];
            bm_dev_request(&handle, 0);
            bm_image_create(handle, image_h, image_w, FORMAT_NV12, 
                    DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_alloc_dev_mem(src, 1);
            for (int i = 0; i < 4; i++) {
                bm_image_create(handle,
                    image_h / 2,
                    image_w / 2,
                    FORMAT_BGR_PACKED,
                    DATA_TYPE_EXT_1N_BYTE,
                    dst + i);
                bm_image_alloc_dev_mem(dst[i]);
            }
            std::unique_ptr<u8 []> y_ptr(new u8[image_h * image_w]);
            std::unique_ptr<u8 []> uv_ptr(new u8[image_h * image_w / 2]);
            memset((void *)(y_ptr.get()), 148, image_h * image_w);
            memset((void *)(uv_ptr.get()), 158, image_h * image_w / 2);
            u8 *host_ptr[] = {y_ptr.get(), uv_ptr.get()};
            bm_image_copy_host_to_device(src, (void **)host_ptr);
        
            bmcv_rect_t rect[] = {{0, 0, image_w / 2, image_h / 2},
                    {0, image_h / 2, image_w / 2, image_h / 2},
                    {image_w / 2, 0, image_w / 2, image_h / 2},
                    {image_w / 2, image_h / 2, image_w / 2, image_h / 2}};
        
            bmcv_image_vpp_convert(handle, 4, src, dst, rect);
        
            for (int i = 0; i < 4; i++) {
                bm_image_destroy(dst[i]);
            }
        
            bm_image_destroy(src);
            bm_dev_free(handle);
            return 0;
        }

   
