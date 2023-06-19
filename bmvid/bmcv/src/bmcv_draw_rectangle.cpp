#include <math.h>
#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

static void check_rectangle_parameter(int rect_num,
    bmcv_rect *rects,
    int line_width)
{
    int i;

    for (i = 0; i < rect_num; i++) {
        if ( (2 * line_width > rects[i].crop_w)
            || 2 * line_width > rects[i].crop_h) {
            bmlib_log("DRAW RECTANGLE", BMLIB_LOG_INFO,
                "rectangle mighr be too small!\n");
        }
    }
}

static bm_status_t bmcv_draw_solid_rectangle(bm_handle_t handle,
                                u64 base_addr,
                                int pixel_stride,
                                int row_stride,
                                int height,
                                int width,
                                bmcv_rect_t rect,
                                u8          val)
{
    int up_left_x = bm_max(0, rect.start_x);
    int up_left_y = bm_max(0, rect.start_y);
    int right_down_x = bm_min(width - 1, rect.start_x + rect.crop_w - 1);
    int right_down_y = bm_min(height - 1, rect.start_y + rect.crop_h - 1);

    int n         = right_down_y - up_left_y + 1;
    int w = right_down_x - up_left_x + 1;
    u64 offset = up_left_x * pixel_stride + up_left_y * row_stride;
    base_addr  = base_addr + offset;
    if(n <= 0 || w <= 0)
    {
        //This rect is out of range, ignore it
        return BM_SUCCESS;
    }
    bm_api_memset_byte_t api = {base_addr, n, 1, 1, w, row_stride, 1, 1, pixel_stride, val};

    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_MEMSET_BYTE, (uint8_t *)&api, sizeof(api))) {
        BMCV_ERR_LOG("draw rectangle send api error\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("draw rectangle sync api error\r\n");
        return BM_ERR_FAILURE;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_draw_rectangle_(bm_handle_t handle,
                                      bm_image    image,
                                      bmcv_rect_t rect,
                                      int         line_width,
                                      uint8_t     r,
                                      uint8_t     g,
                                      uint8_t     b)
{
    if(image.data_type != DATA_TYPE_EXT_1N_BYTE)
    {
        return BM_ERR_FAILURE;
    }
    if(rect.crop_h <= 0 || rect.crop_w <= 0)
        return BM_SUCCESS;

    bm_status_t ret = BM_SUCCESS;
    int inner_pixel_num = line_width / 2;
    int outer_pixel_num = (line_width - 1) / 2;
    u8  fill_val[3]     = {r, g, b};
    bm_device_mem_t mem[3];
    bm_image_get_device_mem(image, mem);
    int stride[3] = {0};
    bm_image_get_stride(image, stride);

    if(image.image_format == FORMAT_BGR_PLANAR || image.image_format == FORMAT_BGR_PACKED)
    {
        std::swap(fill_val[0], fill_val[2]);
    }
    if(image.image_format == FORMAT_BGR_PLANAR || image.image_format == FORMAT_RGB_PLANAR)
    {
        int plane_size = stride[0] * image.height;

        u64 base       = bm_mem_get_device_addr(mem[0]);
        for (int i = 0; i < 3; i++) {
            u64 plane_base = base + i * plane_size;
            bmcv_rect_t rect_;
            rect_.start_x = rect.start_x - outer_pixel_num;
            rect_.start_y = rect.start_y - outer_pixel_num;
            rect_.crop_w  = rect.crop_w + 2 * outer_pixel_num;
            rect_.crop_h  = line_width;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 1, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = rect.start_x - outer_pixel_num;
            rect_.start_y = rect.start_y - outer_pixel_num;
            rect_.crop_w  = line_width;
            rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 1, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = rect.start_x - outer_pixel_num;
            rect_.start_y = rect.start_y + rect.crop_h - 1 - inner_pixel_num;
            rect_.crop_w  = rect.crop_w + 2 * outer_pixel_num;
            rect_.crop_h  = line_width;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 1, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = rect.start_x + rect.crop_w - 1 - inner_pixel_num;
            rect_.start_y = rect.start_y - outer_pixel_num;
            rect_.crop_w  = line_width;
            rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 1, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;
        };
    }
    else if(image.image_format == FORMAT_BGR_PACKED || image.image_format == FORMAT_RGB_PACKED)
    {
        u64 base       = bm_mem_get_device_addr(mem[0]);
        for (int i = 0; i < 3; i++) {
            u64 plane_base = base + i;
            bmcv_rect_t rect_;
            rect_.start_x = rect.start_x - outer_pixel_num;
            rect_.start_y = rect.start_y - outer_pixel_num;
            rect_.crop_w  = rect.crop_w + 2 * outer_pixel_num;
            rect_.crop_h  = line_width;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 3, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = rect.start_x - outer_pixel_num;
            rect_.start_y = rect.start_y - outer_pixel_num;
            rect_.crop_w  = line_width;
            rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 3, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = rect.start_x - outer_pixel_num;
            rect_.start_y = rect.start_y + rect.crop_h - 1 - inner_pixel_num;
            rect_.crop_w  = rect.crop_w + 2 * outer_pixel_num;
            rect_.crop_h  = line_width;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 3, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = rect.start_x + rect.crop_w - 1 - inner_pixel_num;
            rect_.start_y = rect.start_y - outer_pixel_num;
            rect_.crop_w  = line_width;
            rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
            ret = bmcv_draw_solid_rectangle(
                handle, plane_base, 3, stride[0], image.height, image.width, rect_, fill_val[i]);
            if (ret != BM_SUCCESS) return ret;
        };
    }
    else if(image.image_format == FORMAT_NV12 || image.image_format == FORMAT_NV21 || image.image_format == FORMAT_NV16 || image.image_format == FORMAT_NV61 || image.image_format == FORMAT_YUV420P)
    {
        calculate_yuv(r, g, b, fill_val, fill_val + 1, fill_val + 2);

        if (image.image_format == FORMAT_NV21 ||
            image.image_format == FORMAT_NV61) {
            std::swap(fill_val[1], fill_val[2]);
        }

        // y plane
        u64 base       = bm_mem_get_device_addr(mem[0]);
        u64 plane_base = base;
        bmcv_rect_t rect_;
        rect_.start_x = rect.start_x - outer_pixel_num;
        rect_.start_y = rect.start_y - outer_pixel_num;
        rect_.crop_w  = rect.crop_w + 2 * outer_pixel_num;
        rect_.crop_h  = line_width;
        ret = bmcv_draw_solid_rectangle(handle,
                                  plane_base,
                                  1,
                                  stride[0],
                                  image.height,
                                  image.width,
                                  rect_,
                                  fill_val[0]);
        if (ret != BM_SUCCESS) return ret;

        rect_.start_x = rect.start_x - outer_pixel_num;
        rect_.start_y = rect.start_y - outer_pixel_num;
        rect_.crop_w  = line_width;
        rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
        ret = bmcv_draw_solid_rectangle(handle,
                                  plane_base,
                                  1,
                                  stride[0],
                                  image.height,
                                  image.width,
                                  rect_,
                                  fill_val[0]);
        if (ret != BM_SUCCESS) return ret;

        rect_.start_x = rect.start_x - outer_pixel_num;
        rect_.start_y = rect.start_y + rect.crop_h - 1 - inner_pixel_num;
        rect_.crop_w  = rect.crop_w + 2 * outer_pixel_num;
        rect_.crop_h  = line_width;
        ret = bmcv_draw_solid_rectangle(handle,
                                  plane_base,
                                  1,
                                  stride[0],
                                  image.height,
                                  image.width,
                                  rect_,
                                  fill_val[0]);
        if (ret != BM_SUCCESS) return ret;

        rect_.start_x = rect.start_x + rect.crop_w - 1 - inner_pixel_num;
        rect_.start_y = rect.start_y - outer_pixel_num;
        rect_.crop_w  = line_width;
        rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
        ret = bmcv_draw_solid_rectangle(handle,
                                  plane_base,
                                  1,
                                  stride[0],
                                  image.height,
                                  image.width,
                                  rect_,
                                  fill_val[0]);
        if (ret != BM_SUCCESS) return ret;

        if(image.image_format == FORMAT_NV16 || image.image_format == FORMAT_NV61)
        {
            base       = bm_mem_get_device_addr(mem[1]);
            for (int i = 0; i < 2; i++)
            {
                plane_base = base + i;
                bmcv_rect_t rect_;
                rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
                rect_.start_y = rect.start_y - outer_pixel_num;
                rect_.crop_w  = ALIGN(rect.crop_w + 2 * outer_pixel_num, 2) / 2;
                rect_.crop_h  = line_width;
                ret = bmcv_draw_solid_rectangle(handle,
                                          plane_base,
                                          2,
                                          stride[1],
                                          image.height,
                                          image.width,
                                          rect_,
                                          fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;

                rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
                rect_.start_y = (rect.start_y - outer_pixel_num);
                rect_.crop_w  = ALIGN(line_width, 2) / 2;
                rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
                ret = bmcv_draw_solid_rectangle(handle,
                                          plane_base,
                                          2,
                                          stride[1],
                                          image.height,
                                          image.width,
                                          rect_,
                                          fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;

                rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
                rect_.start_y =
                    rect.start_y + rect.crop_h - 1 - inner_pixel_num;
                rect_.crop_w  = ALIGN(rect.crop_w + 2 * outer_pixel_num, 2) / 2;
                rect_.crop_h  = line_width;
                ret = bmcv_draw_solid_rectangle(handle,
                                          plane_base,
                                          2,
                                          stride[1],
                                          image.height,
                                          image.width,
                                          rect_,
                                          fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;

                rect_.start_x = (rect.start_x + rect.crop_w - 1 - inner_pixel_num) / 2;
                rect_.start_y = rect.start_y - outer_pixel_num;
                rect_.crop_w  = ALIGN(line_width, 2) / 2;
                rect_.crop_h  = rect.crop_h + 2 * outer_pixel_num;
                ret = bmcv_draw_solid_rectangle(handle,
                                          plane_base,
                                          2,
                                          stride[1],
                                          image.height,
                                          image.width,
                                          rect_,
                                          fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;
            }
        }
        else if(image.image_format == FORMAT_NV12 || image.image_format == FORMAT_NV21)
        {
            base       = bm_mem_get_device_addr(mem[1]);
            for (int i = 0; i < 2; i++)
            {
                plane_base = base + i;
                bmcv_rect_t rect_;
                rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
                rect_.start_y = (rect.start_y - outer_pixel_num) / 2;
                rect_.crop_w  = ALIGN(rect.crop_w + 2 * outer_pixel_num, 2) / 2;
                rect_.crop_h  = ALIGN(line_width, 2) / 2;
                ret = bmcv_draw_solid_rectangle(handle,
                                        plane_base,
                                        2,
                                        stride[1],
                                        image.height / 2,
                                        image.width,
                                        rect_,
                                        fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;

                rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
                rect_.start_y = (rect.start_y - outer_pixel_num) / 2;
                rect_.crop_w  = ALIGN(line_width, 2) / 2;
                rect_.crop_h  = ALIGN(rect.crop_h + 2 * outer_pixel_num, 2) / 2;
                ret = bmcv_draw_solid_rectangle(handle,
                                        plane_base,
                                        2,
                                        stride[1],
                                        image.height / 2,
                                        image.width,
                                        rect_,
                                        fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;

                rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
                rect_.start_y = (rect.start_y + rect.crop_h - 1 - inner_pixel_num) / 2;
                rect_.crop_w  = ALIGN(rect.crop_w + 2 * outer_pixel_num, 2) / 2;
                rect_.crop_h  = ALIGN(line_width, 2) / 2;
                ret = bmcv_draw_solid_rectangle(handle,
                                        plane_base,
                                        2,
                                        stride[1],
                                        image.height / 2,
                                        image.width,
                                        rect_,
                                        fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;

                rect_.start_x = (rect.start_x + rect.crop_w - 1 - inner_pixel_num) / 2;
                rect_.start_y = (rect.start_y - outer_pixel_num) / 2;
                rect_.crop_w  = ALIGN(line_width, 2) / 2;
                rect_.crop_h  = ALIGN(rect.crop_h + 2 * outer_pixel_num, 2) / 2;
                ret = bmcv_draw_solid_rectangle(handle,
                                        plane_base,
                                        2,
                                        stride[1],
                                        image.height / 2,
                                        image.width,
                                        rect_,
                                        fill_val[i + 1]);
                if (ret != BM_SUCCESS) return ret;
            }
        }
        else if(image.image_format == FORMAT_YUV420P)
        {
            u64 base1       = bm_mem_get_device_addr(mem[1]);
            u64 base2       = bm_mem_get_device_addr(mem[2]);

            bmcv_rect_t rect_;
            rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
            rect_.start_y = (rect.start_y - outer_pixel_num) / 2;
            rect_.crop_w  = ALIGN(rect.crop_w + 2 * outer_pixel_num, 2) / 2;
            rect_.crop_h  = ALIGN(line_width, 2) / 2;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base1,
                                    1,
                                    stride[1],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[1]);
            if (ret != BM_SUCCESS) return ret;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base2,
                                    1,
                                    stride[2],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[2]);
            if (ret != BM_SUCCESS) return ret;
            rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
            rect_.start_y = (rect.start_y - outer_pixel_num) / 2;
            rect_.crop_w  = ALIGN(line_width, 2) / 2;
            rect_.crop_h  = ALIGN(rect.crop_h + 2 * outer_pixel_num, 2) / 2;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base1,
                                    1,
                                    stride[1],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[1]);
            if (ret != BM_SUCCESS) return ret;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base2,
                                    1,
                                    stride[2],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[2]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = (rect.start_x - outer_pixel_num) / 2;
            rect_.start_y = (rect.start_y + rect.crop_h - 1 - inner_pixel_num) / 2;
            rect_.crop_w  = ALIGN(rect.crop_w + 2 * outer_pixel_num, 2) / 2;
            rect_.crop_h  = ALIGN(line_width, 2) / 2;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base1,
                                    1,
                                    stride[1],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[1]);
            if (ret != BM_SUCCESS) return ret;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base2,
                                    1,
                                    stride[2],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[2]);
            if (ret != BM_SUCCESS) return ret;

            rect_.start_x = (rect.start_x + rect.crop_w - 1 - inner_pixel_num) / 2;
            rect_.start_y = (rect.start_y - outer_pixel_num) / 2;
            rect_.crop_w  = ALIGN(line_width, 2) / 2;
            rect_.crop_h  = ALIGN(rect.crop_h + 2 * outer_pixel_num, 2) / 2;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base1,
                                    1,
                                    stride[1],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[1]);
            if (ret != BM_SUCCESS) return ret;
            ret = bmcv_draw_solid_rectangle(handle,
                                    base2,
                                    1,
                                    stride[2],
                                    image.height / 2,
                                    image.width / 2,
                                    rect_,
                                    fill_val[2]);
            if (ret != BM_SUCCESS) return ret;
        }
    }
    else
    {
        BMCV_ERR_LOG("error currently not support this format draw rectangle\n");
        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

static bmcv_rect_t refine_rect(
    bmcv_rect_t rect,
    int height,
    int width,
    int line_width
)
{
    int outer_pixel_num = (line_width - 1) / 2;
    int up_left_x = bm_max(outer_pixel_num, rect.start_x);
    int up_left_y = bm_max(outer_pixel_num, rect.start_y);
    int right_down_x = bm_min(width - 1 - outer_pixel_num, rect.start_x + rect.crop_w - 1);
    int right_down_y = bm_min(height - 1 - outer_pixel_num, rect.start_y + rect.crop_h - 1);
    bmcv_rect_t res    = {0, 0, 0, 0};
    if (right_down_y - up_left_y <= 0 || right_down_x - up_left_x <= 0) {
        return res;
    }
    res.start_x = up_left_x;
    res.start_y = up_left_y;
    res.crop_w  = right_down_x - up_left_x + 1;
    res.crop_h = right_down_y - up_left_y + 1;
    return res;
}

bm_status_t bmcv_image_draw_rectangle_(bm_handle_t   handle,
                                      bm_image      image,
                                      int           rect_num,
                                      bmcv_rect_t * rects,
                                      int           line_width,
                                      unsigned char r,
                                      unsigned char g,
                                      unsigned char b)
{
    if(rect_num == 0)
        return BM_SUCCESS;
    if(rect_num < 0)
    {
        BMCV_ERR_LOG("rect num less than 0\n");
        return BM_ERR_PARAM;
    }
    if(!image.image_private)
    {
        BMCV_ERR_LOG("invalidate image, not created\n");
        return BM_ERR_PARAM;
    }
    if(image.data_type != DATA_TYPE_EXT_1N_BYTE)
    {
        BMCV_ERR_LOG("invalidate image, data type should be DATA_TYPE_EXT_1N_BYTE\n");
        return BM_ERR_PARAM;
    }
    if(!bm_image_is_attached(image))
    {
        BMCV_ERR_LOG("invalidate image, please attach device memory\n");
        return BM_ERR_PARAM;
    }

    if(image.height >= (1 << 16) || image.width >= (1 << 16))
    {
        BMCV_ERR_LOG("Not support such big size image\n");
        return BM_NOT_SUPPORTED;
    }

    check_rectangle_parameter(rect_num, rects, line_width);

    for (int i = 0; i < rect_num; i++)
    {
        bmcv_rect_t rect =
            refine_rect(rects[i], image.height, image.width, line_width);
        if (bmcv_image_draw_rectangle_(handle, image, rect, line_width, r, g, b) != BM_SUCCESS)
        {
            BMCV_ERR_LOG("error call draw rectangle\n");
            return BM_ERR_FAILURE;
        }
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_draw_rectangle(
    bm_handle_t   handle,
    bm_image      image,
    int           rect_num,
    bmcv_rect_t * rects,
    int           line_width,
    unsigned char r,
    unsigned char g,
    unsigned char b)
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    u8 draw_val[3] = {r, g, b};
    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_draw_rectangle_(handle, image, rect_num, rects, line_width, r, g, b);
        break;

      case BM1684X:
        if(is_yuv_or_rgb(image.image_format) == COLOR_SPACE_YUV)
            calculate_yuv(r, g, b, draw_val, draw_val + 1, draw_val + 2);
        ret = bm1684x_vpp_draw_rectangle(handle, image, rect_num, rects, line_width, draw_val[0], draw_val[1], draw_val[2]);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;

}

