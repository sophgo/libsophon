/**
 * Copyright (C) 2019 Mingyi Dong
 * Copyright (C) 2014 Carlos Rafael Giani
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef _BM_JPEG_RUN_H_
#define _BM_JPEG_RUN_H_

#define FRAMEBUFFER_ALIGNMENT   16
#define MAX_FILE_PATH           128
#define MAX_NUM_INSTANCE        128
//#define PRINTOUT                1
#define BS_MASK                 (1024*16-1)

typedef enum{
    DEC = 0,
    ENC = 1
}CodecType;

typedef struct
{
    char yuvFileName[MAX_FILE_PATH];
    char bitStreamFileName[MAX_FILE_PATH];
    char cfgFileName[MAX_FILE_PATH];
    char refFileName[MAX_FILE_PATH];
    int picWidth;
    int picHeight;
    int frameFormat;
    int srcFormat;
    unsigned int loopNums;
    int instanceIndex;
    int device_index;
    int rotate;
    int Skip;
} EncConfigParam;

typedef struct
{
    char yuvFileName[MAX_FILE_PATH];
    char bitStreamFileName[MAX_FILE_PATH];
    char refFileName[MAX_FILE_PATH];
    unsigned int loopNums;
    int instanceIndex;
    int device_index;
    int Skip;
    int crop_yuv;
} DecConfigParam;


typedef struct {
    int codecMode;
    int numMulti;
    int saveYuv;
    int multiMode[MAX_NUM_INSTANCE];
    EncConfigParam encConfig[MAX_NUM_INSTANCE];
    DecConfigParam decConfig[MAX_NUM_INSTANCE];
} MultiConfigParam;

#if defined (__cplusplus)
extern "C" {
#endif
    int DecodeTest(DecConfigParam *param);
    int EncodeTest(EncConfigParam *param);
    int MultiInstanceTest(MultiConfigParam *param);
#if defined (__cplusplus)
}
#endif
#endif
