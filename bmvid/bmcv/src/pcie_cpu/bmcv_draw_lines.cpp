#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <vector>
#include <math.h>
#include <float.h>
#include "bmcv_cpu_func.h"
#include "bmcv_util.h"

using namespace std;

#define SATURATE(a) ((a) > 255 ? 255 : ((a) < 0 ? 0 : (a)))

struct bmPoint2l {
    int64 x;
    int64 y;
};

static const int ITUR_BT_601_SHIFT = 20;
static const int ITUR_BT_601_CRY =  269484;
static const int ITUR_BT_601_CGY =  528482;
static const int ITUR_BT_601_CBY =  102760;
static const int ITUR_BT_601_CRU = -155188;
static const int ITUR_BT_601_CGU = -305135;
static const int ITUR_BT_601_CBU =  460324;
static const int ITUR_BT_601_CGV = -385875;
static const int ITUR_BT_601_CBV = -74448;

static inline u8 rgbToY42x(u8 r, u8 g, u8 b)
{
    const int shifted16 = (16 << ITUR_BT_601_SHIFT);
    const int halfShift = (1 << (ITUR_BT_601_SHIFT - 1));
    int yy = ITUR_BT_601_CRY * r + ITUR_BT_601_CGY * g + ITUR_BT_601_CBY * b + halfShift + shifted16;

    return SATURATE(yy >> ITUR_BT_601_SHIFT);
}

static inline void rgbToUV42x(u8 r, u8 g, u8 b, u8& u, u8& v)
{
    const int halfShift = (1 << (ITUR_BT_601_SHIFT - 1));
    const int shifted128 = (128 << ITUR_BT_601_SHIFT);
    int uu = ITUR_BT_601_CRU * r + ITUR_BT_601_CGU * g + ITUR_BT_601_CBU * b + halfShift + shifted128;
    int vv = ITUR_BT_601_CBU * r + ITUR_BT_601_CGV * g + ITUR_BT_601_CBV * b + halfShift + shifted128;

    u = SATURATE(uu >> ITUR_BT_601_SHIFT);
    v = SATURATE(vv >> ITUR_BT_601_SHIFT);
}

void inline fill16bit(u8* buffer, u8 u,u8 v, int size)
{
   for(int i = 0; i < size; i++)
   {
       *(buffer+i*2)=u;
       *(buffer+i*2+1)=v;
   }
}

static void fillYuvRow(
        bmMat &img,
        u8 y,
        u8 u,
        u8 v,
        int starty,
        int startx,
        int filllen) {
    starty = starty < 0 ? 0 : starty;
    startx = startx < 0 ? 0 : startx;
    starty = starty >= img.height ? img.height - 1 : starty;
    startx = startx >= img.width ? img.width - 1 : startx;
    filllen = (startx + filllen > img.width - 1) ? (img.width - 1 - startx) : filllen;
    if (img.format == FORMAT_GRAY) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
    }
    else if (img.format == FORMAT_YUV444P) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((u8 *)img.data[1] + starty * img.step[1] + startx, u, filllen);
        memset((u8 *)img.data[2] + starty * img.step[2] + startx, v, filllen);
    }
    else if (img.format == FORMAT_YUV422P) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((u8 *)img.data[1] + starty * img.step[1] + startx / 2, u, filllen/2);
        memset((u8 *)img.data[2] + starty * img.step[2] + startx / 2, v, filllen/2);
    }
    else if (img.format == FORMAT_YUV420P) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((u8 *)img.data[1] + starty / 2 * img.step[1] + startx / 2, u, filllen / 2);
        memset((u8 *)img.data[2] + starty / 2 * img.step[2] + startx / 2, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV12) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((u8 *)img.data[1] + (starty / 2) * img.step[1] + (startx / 2) * 2, u, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV21) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((u8 *)img.data[1] + (starty / 2) * img.step[1] + (startx / 2) * 2, v, u, filllen / 2);
    }
    else if (img.format == FORMAT_NV16) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((u8 *)img.data[1] + starty * img.step[1] + (startx / 2) * 2, u, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV61) {
        memset((u8 *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((u8 *)img.data[1] + starty * img.step[1] + (startx / 2) * 2, v, u, filllen / 2);
    }
    else {
        printf("rectangle function can't support  format=%d\n", img.format);
        return;
    }
    return;
}

static void draw_line(
        bmMat &inout,
        bmPoint start,
        bmPoint end,
        const unsigned char color[3],
        int thickness) {
    u8 rc = color[0];
    u8 gc = color[1];
    u8 bc = color[2];
    u8 yc = rgbToY42x(rc, gc, bc);
    u8 uc=128, vc=128;
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

int bmcv_cpu_draw_line(
        void* addr,
        int size
        ) {
    bmcv_draw_line_param_t* param = reinterpret_cast<bmcv_draw_line_param_t*>(*UINT64_PTR(addr));
    bmcpu_dev_flush_and_invalidate_dcache(
            VOID_PTR(*UINT64_PTR(addr)),
            VOID_PTR(*UINT64_PTR(addr) + sizeof(bmcv_draw_line_param_t) + param->line_num * sizeof(bmPoint)));
    unsigned char* ptr[3] = {UINT8_PTR(param->input_addr[0]),
                             UINT8_PTR(param->input_addr[1]),
                             UINT8_PTR(param->input_addr[2])};
    bmMat mat(param->height, param->width, (bm_image_format_ext)param->format, ptr);
    int * point = (int*)(param + 1);
    unsigned char color[3] = {static_cast<unsigned char>(param->rval),
                              static_cast<unsigned char>(param->gval),
                              static_cast<unsigned char>(param->bval)};
    for (int i = 0; i < mat.chan; i++) {
        bmcpu_dev_flush_and_invalidate_dcache(ptr[i], ptr[i] + mat.size[i]);
    }
    for (int i = 0; i < param->line_num; i++) {
        bmPoint p1 = {point[i * 2], point[i * 2 + 1]};
        bmPoint p2 = {point[param->line_num * 2 + i * 2],
                      point[param->line_num * 2 + i * 2 + 1]};
        draw_line(mat, p1, p2, color, param->thick);
    }
    for (int i = 0; i < mat.chan; i++) {
        bmcpu_dev_flush_dcache(ptr[i], ptr[i] + mat.size[i]);
    }
    return 0;
}

