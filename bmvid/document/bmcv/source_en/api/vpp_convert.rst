bmcv_image_vpp_convert
=========================

The API converts the input image format into the output image format, and supports the function of crop and resize. It supports the output of multiple crops from one input and resizing to the output image size.

    .. code-block:: c

        bm_status_t bmcv_image_vpp_convert(
            bm_handle_t  handle,
            int output_num,
            bm_image input,
            bm_image *output,
            bmcv_rect_t *crop_rect,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR
        );


**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* int output_num

  Output parameter. The number of output bm_image, which is equal to the number of crop of src image. One src crop outputs one dst bm_image.

* bm_image input

  Input parameters. Input bm_image object.

* bm_image* output

  Output parameter. Output bm_image object pointer.

* bmcv_rect_t * crop_rect

  Input parameter. The specific format is defined as follows:

    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;

    The parameters of the crop on the input image corresponding to each output bm_image object, including the X coordinate of the starting point, the Y coordinate of the starting point, the width of the crop image and the height of the crop image.

* bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR

  Input parameters. Resize algorithm selection, including BMCV_INTER_NEAREST 、BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC, which is bilinear difference by default.

  bm1684 supports BMCV_INTER_NEAREST,BMCV_INTER_LINEAR and BMCV_INTER_BICUBIC.

  bm1684x supports BMCV_INTER_NEAREST and BMCV_INTER_LINEAR.

**Return parameter description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

1. The format and some requirements that the API needs to meet are the same as the table of bmcv_image_vpp_basic.

2. The width and height (src.width, src.height, dst.width, dst.height) of bm1684 input and output are limited to 16 ~ 4096.

   The width and height (src.width, src.height, dst.width, dst.height) of bm1684x input and output are limited to 8 ~ 8192, and zoom 128 times.

3. The input must be associated with device memory, otherwise, a failure will be returned.

4. FORMAT_COMPRESSED is a built-in compression format after VPU decoding. It includes four parts: Y compressed table, Y compressed data, CbCr compressed table and CbCr compressed data. Please note the order of the four parts in bm_image is slightly different from that of the AVFrame in FFMPEG. If you need to attach the device memory data in AVFrame to bm_image, the corresponding relationship is as follows. For details of AVFrame, please refer to "VPU User Manual".

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



**Code example:**

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


