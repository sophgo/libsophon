#include <memory>
#include <vector>
#include <stdio.h>
#include <iostream>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <time.h>
#endif
#include <float.h>
#include <math.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "device_mem_allocator.h"
#include "pcie_cpu/bmcv_api_struct.h"

using namespace std;

#define SATURATE(a, s, e) ((a) > (e) ? (e) : ((a) < (s) ? (s) : (a)))
#define IS_CS_YUV(a) (a == FORMAT_NV12 || a == FORMAT_NV21 || a == FORMAT_NV16 \
                   || a == FORMAT_NV61 || a == FORMAT_NV24 || a == FORMAT_YUV420P \
                   || a == FORMAT_YUV422P || a == FORMAT_YUV444P)
typedef long long int64;
typedef unsigned char uchar;

typedef struct {
    int64 x;
    int64 y;
} bmPoint2l;

static const int ITUR_BT_601_SHIFT = 20;
static const int ITUR_BT_601_CRY =  269484;
static const int ITUR_BT_601_CGY =  528482;
static const int ITUR_BT_601_CBY =  102760;
static const int ITUR_BT_601_CRU = -155188;
static const int ITUR_BT_601_CGU = -305135;
static const int ITUR_BT_601_CBU =  460324;
static const int ITUR_BT_601_CGV = -385875;
static const int ITUR_BT_601_CBV = -74448;

static inline uchar rgbToY42x(uchar r, uchar g, uchar b)
{
    const int shifted16 = (16 << ITUR_BT_601_SHIFT);
    const int halfShift = (1 << (ITUR_BT_601_SHIFT - 1));
    int yy = ITUR_BT_601_CRY * r + ITUR_BT_601_CGY * g + ITUR_BT_601_CBY * b + halfShift + shifted16;

    return SATURATE(yy >> ITUR_BT_601_SHIFT, 0, 255);
}

static inline void rgbToUV42x(uchar r, uchar g, uchar b, uchar& u, uchar& v)
{
    const int halfShift = (1 << (ITUR_BT_601_SHIFT - 1));
    const int shifted128 = (128 << ITUR_BT_601_SHIFT);
    int uu = ITUR_BT_601_CRU * r + ITUR_BT_601_CGU * g + ITUR_BT_601_CBU * b + halfShift + shifted128;
    int vv = ITUR_BT_601_CBU * r + ITUR_BT_601_CGV * g + ITUR_BT_601_CBV * b + halfShift + shifted128;

    u = SATURATE(uu >> ITUR_BT_601_SHIFT, 0, 255);
    v = SATURATE(vv >> ITUR_BT_601_SHIFT, 0, 255);
}

void inline fill16bit(
        uchar* buffer,
        uchar u,
        uchar v,
        int size) {
   for(int i = 0; i < size; i++)
   {
       *(buffer+i*2)=u;
       *(buffer+i*2+1)=v;
   }
}

static void fillYuvRow(
        bmMat &img,
        uchar y,
        uchar u,
        uchar v,
        int starty,
        int startx,
        int filllen) {
    starty = starty < 0 ? 0 : starty;
    startx = startx < 0 ? 0 : startx;
    starty = starty >= img.height ? img.height - 1 : starty;
    startx = startx >= img.width ? img.width - 1 : startx;
    filllen = (startx + filllen > img.width - 1) ? (img.width - 1 - startx) : filllen;
    if (img.format == FORMAT_GRAY) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
    }
    else if (img.format == FORMAT_YUV444P) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((uchar *)img.data[1] + starty * img.step[1] + startx, u, filllen);
        memset((uchar *)img.data[2] + starty * img.step[2] + startx, v, filllen);
    }
    else if (img.format == FORMAT_YUV422P) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((uchar *)img.data[1] + starty * img.step[1] + startx / 2, u, filllen/2);
        memset((uchar *)img.data[2] + starty * img.step[2] + startx / 2, v, filllen/2);
    }
    else if (img.format == FORMAT_YUV420P) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((uchar *)img.data[1] + starty / 2 * img.step[1] + startx / 2, u, filllen / 2);
        memset((uchar *)img.data[2] + starty / 2 * img.step[2] + startx / 2, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV12) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + (starty / 2) * img.step[1] + (startx / 2) * 2, u, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV21) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + (starty / 2) * img.step[1] + (startx / 2) * 2, v, u, filllen / 2);
    }
    else if (img.format == FORMAT_NV16) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + starty * img.step[1] + (startx / 2) * 2, u, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV61) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + starty * img.step[1] + (startx / 2) * 2, v, u, filllen / 2);
    }
    else {
        printf("rectangle function can't support  format=%d\n", img.format);
        return;
    }
    return;
}

void draw_line(
        bmMat &inout,
        bmcv_point_t start,
        bmcv_point_t end,
        bmcv_color_t color,
        int thickness) {
    uchar rc = color.r;
    uchar gc = color.g;
    uchar bc = color.b;
    uchar yc = rgbToY42x(rc, gc, bc);
    uchar uc = 128, vc = 128;
    rgbToUV42x(rc, gc, bc, uc, vc);

    if (thickness <= 0){
        printf("thickness should be greater than 0\n");
        return;
    }

    int dx = end.x-start.x, dy = end.y-start.y;
    int e, j;
    int sx = dx > 0;
    int sy = dy > 0;

    if (!sx) dx = -dx;
    if (!sy) dy = -dy;

    int s = dx>dy;
    int w1 = thickness>>1;
    int w2 = thickness>>1;
    if (w1 + w2 < thickness) w2++;

    if (thickness == 1){
        thickness = 2;
    }

    if (s){
        for (j = -w1; j < w2; j++){
            fillYuvRow(inout, yc, uc, vc, start.y+j, start.x, thickness);
        }
        e = (dy<<1) - dx;
        while (start.x != end.x){
            if (e < 0){
                sx?start.x++:start.x--;
                e+=dy<<1;
                for (j = -w1; j < w2; j++){
                    fillYuvRow(inout, yc, uc, vc, start.y+j, start.x, thickness);
                }
            } else {
                sy?start.y++:start.y--;
                e-=dx<<1;
            }
        }
    } else {
        for (j = -w1; j < w2; j++){
            fillYuvRow(inout, yc, uc, vc, start.y,start.x+j, thickness);
        }
        e = (dx<<1)-dy;
        while (start.y != end.y){
            if (e < 0){
                sy?start.y++:start.y--;
                e+=dx<<1;
                 for (j = -w1; j < w2; j++){
                    fillYuvRow(inout, yc, uc, vc, start.y, start.x+j, thickness);
                 }
            } else {
                sx?start.x++:start.x--;
                e-=dy<<1;
            }
        }
    }
}

static bm_status_t bmcv_draw_line_check(
        bm_handle_t handle,
        bm_image image,
        int thickness) {
    if (handle == NULL) {
        bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_DEVNOTREADY;
    }
    if (thickness <= 0) {
        bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "thickness should greater than 0!\r\n");
        return BM_ERR_PARAM;
    }
    if (!IS_CS_YUV(image.image_format) && image.image_format != FORMAT_GRAY) {
        bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "image format not supported %d !\r\n", image.image_format);
        return BM_ERR_DATA;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_draw_lines(
        bm_handle_t handle,
        bm_image image,
        const bmcv_point_t* start,
        const bmcv_point_t* end,
        int line_num,
        bmcv_color_t color,
        int thickness) {
    bm_status_t ret = BM_SUCCESS;
    bm_handle_check_1(handle, image);
    if (BM_SUCCESS != bmcv_draw_line_check(handle, image, thickness)) {
        return BM_ERR_FAILURE;
    }
    // clip point
    bmcv_point_t* sp = new bmcv_point_t [line_num];
    bmcv_point_t* ep = new bmcv_point_t [line_num];
    for (int i = 0; i < line_num; i++) {
        sp[i].x = SATURATE(start[i].x, 0, image.width - 1);
        sp[i].y = SATURATE(start[i].y, 0, image.height - 1);
        ep[i].x = SATURATE(end[i].x, 0, image.width - 1);
        ep[i].y = SATURATE(end[i].y, 0, image.height - 1);
        if (sp[i].y > ep[i].y) {
            swap(sp[i].x, ep[i].x);
            swap(sp[i].y, ep[i].y);
        }
    }
    int timeout = -1;
    int process_id = get_cpu_process_id(handle);
    if (process_id != -1) {
        // construct param
        int channel = bm_image_get_plane_num(image);
        int input_str[3];
        bm_image_get_stride(image, input_str);
        bm_device_mem_t input_mem[3];
        bm_image_get_device_mem(image, input_mem);
        int param_len = sizeof(bmcv_draw_line_param_t) + line_num * sizeof(bmcv_point_t) * 2;
        char *param = new char [param_len];
        bmcv_draw_line_param_t* param_ptr = (bmcv_draw_line_param_t*) param;
        param_ptr->format = image.image_format;
        param_ptr->width = input_str[0];
        param_ptr->height = image.height;
        param_ptr->rval = color.r;
        param_ptr->gval = color.g;
        param_ptr->bval = color.b;
        param_ptr->thick = thickness;
        param_ptr->line_num = line_num;
        for (int i = 0; i < channel; i++) {
            param_ptr->input_addr[i] = get_mapped_addr(handle, input_mem + i);
        }
        memcpy(param + sizeof(bmcv_draw_line_param_t),
               sp,
               line_num * sizeof(bmcv_point_t));
        memcpy(param + sizeof(bmcv_draw_line_param_t) + line_num * sizeof(bmcv_point_t),
               ep,
               line_num * sizeof(bmcv_point_t));
        // copy param to device memory
        DeviceMemAllocator allocator(handle);
        bm_device_mem_t param_mem;
        try {
            param_mem = allocator.map_input_to_device<char>(
                              bm_mem_from_system(param),
                              param_len);
        } catch (bm_status_t ret) {
            goto fail;
        }
        u64 param_addr_mapped = get_mapped_addr(handle, &param_mem);
        ret = (bm_status_t)bmcpu_exec_function_ext(handle,
                                      process_id,
                                      (char*)"bmcv_cpu_draw_line",
                                      (void*)&param_addr_mapped,
                                      sizeof(void*),
                                      1,
                                      timeout);
        if (ret != BM_SUCCESS) {
            bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "exec function failed! return %d\r\n", ret);
            goto fail;
        }
        delete [] param;
    } else {
#ifdef SOC_MODE
        bm_device_mem_t dmem;
        unsigned char *in_ptr[3];
        unsigned long long virt_addr  = 0;
        unsigned long long size[3] = {0};
        unsigned long long total_size = 0;

        for (int i = 0; i < image.image_private->plane_num; i++) {
            size[i] = image.image_private->memory_layout[i].size;
            total_size += size[i];
        }

        dmem = image.image_private->data[0];
        bm_set_device_mem(&dmem, total_size, dmem.u.device.device_addr);
        ret = bm_mem_mmap_device_mem_no_cache(image.image_private->handle, &dmem, &virt_addr);
        if (ret != BM_SUCCESS) {
            bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "bm_mem_mmap_device_mem failed with error code %d\r\n", ret);
            goto fail;
        }

        in_ptr[0] = (unsigned char *)virt_addr;
        in_ptr[1] = in_ptr[0] + size[0];
        in_ptr[2] = in_ptr[1] + size[1];
#else
        int w = image.width;
        int h = image.height;
        unsigned char* host_buf = new unsigned char [w * h *3];
        unsigned char* in_ptr[3] = {host_buf, host_buf + w * h, host_buf + w * h * 2};
        bm_image_copy_device_to_host(image, (void **)in_ptr);
#endif
        int strides[3];
        bm_image_get_stride(image, strides);
        bmMat mat;
        mat.width = image.width;
        mat.height = image.height;
        mat.format = image.image_format;
        mat.step = &strides[0];
        mat.data = (void**)in_ptr;
        for (int i = 0; i < line_num; i++) {
            draw_line(mat, sp[i], ep[i], color, thickness);
        }
#ifdef SOC_MODE
        ret = bm_mem_unmap_device_mem(image.image_private->handle, (void *)&virt_addr, total_size);
        if (ret != BM_SUCCESS) {
            bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "bm_mem_unmap_device_mem failed with error code %d\r\n", ret);
            goto fail;
        }
#else
        bm_image_copy_host_to_device(image, (void **)in_ptr);
        delete [] host_buf;
#endif
    }

fail:
    delete [] sp;
    delete [] ep;
    return ret;
}

