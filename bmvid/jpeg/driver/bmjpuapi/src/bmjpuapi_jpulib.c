/* bmjpuapi implementation on top of the BitMain bm-jpu library
 * Copyright (C) 2018 Solan Shang
 * Copyright (C) 2015 Carlos Rafael Giani
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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#ifdef  _WIN32
#include <windows.h>
#else
//#include <stdatomic.h>
#include <unistd.h>
#endif

#include <time.h> /* nanosleep */
#include "jpu_lib.h"

#include "bm_jpeg_internal.h"
#include "bmjpuapi_priv.h"
#include "bmjpuapi_parse_jpeg.h"


/***********************************************/
/******* COMMON STRUCTURES AND FUNCTIONS *******/
/***********************************************/


#ifndef TRUE
#define TRUE (1)
#endif


#ifndef FALSE
#define FALSE (0)
#endif


#ifndef BOOL
#define BOOL int
#endif


/* This catches fringe cases where somebody passes a
 * non-null value as TRUE that is not the same as TRUE */
#define TO_BOOL(X) ((X) ? TRUE : FALSE)


#ifdef CHIP_BM1684
#define FRAME_WIDTH_ALIGN  64
#else
#define FRAME_WIDTH_ALIGN  32
#endif
#define FRAME_HEIGHT_ALIGN 16

#define JPU_DEC_BITSTREAM_BUFFER_SIZE (1024*1024*5)
#define JPU_ENC_BITSTREAM_BUFFER_SIZE (1024*1024*5)

#define JPU_WAIT_TIMEOUT             2000 /* milliseconds to wait for frame completion */
#define JPU_MAX_TIMEOUT_COUNTS       5   /* how many timeouts are allowed in series */

#define MJPEG_ENC_HEADER_DATA_MAX_SIZE  2048

#define MAX_INST_HANDLE_SIZE          (12*1024)

/* Component tables, used by bm-jpu to fill in JPEG SOF headers as described
 * by the JPEG specification section B.2.2 and to pick the correct quantization
 * tables. There are 5 tables, one for each supported pixel format. Each table
 * contains 4 row, each row is made up of 6 byte.
 *
 * The row structure goes as follows:
 * - The first byte is the number of the component.
 * - The second and third bytes contain, respectively, the vertical and
 *   horizontal sampling factors, which are either 1 or 2. Chroma components
 *   always use sampling factors of 1, while the luma component can contain
 *   factors of value 2. For example, a horizontal luma factor of 2 means
 *   that horizontally, for a pair of 2 Y pixels there is one U and one
 *   V pixel.
 * - The fourth byte is the index of the quantization table to use. The
 *   luma component uses the table with index 0, the chroma components use
 *   the table with index 1. These indices correspond to the DC_TABLE_INDEX0
 *   and DC_TABLE_INDEX1 constants.
 *
 * Note that the fourth component row is currently unused. So are the fifth
 * and sixth bytes in each row. Still, the bm-jpu library API requires them
 * (possibly for future extensions?).
 */

static uint8_t const mjpeg_enc_component_info_tables[5][4 * 6] = {
    /* YUV 4:2:0 table. For each 2x2 patch of Y pixels,
     * there is one U and one V pixel. */
    { 0x00, 0x02, 0x02, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x02, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },

    /* YUV 4:2:2 horizontal table. For each horizontal pair
     * of 2 Y pixels, there is one U and one V pixel. */
    { 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x02, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },

    /* YUV 4:2:2 vertical table. For each vertical pair
     * of 2 Y pixels, there is one U and one V pixel. */
    { 0x00, 0x01, 0x02, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x02, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },

    /* YUV 4:4:4 table. For each Y pixel, there is one
     * U and one V pixel. */
    { 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x02, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },

    /* YUV 4:0:0 table. There are only Y pixels, no U or
     * V ones. This is essentially grayscale. */
    { 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00 }
};


/* These quantization tables are from the JPEG specification, section K.1 */
static uint8_t const mjpeg_enc_quantization_luma[64] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
};

static uint8_t const mjpeg_enc_quantization_chroma[64] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
};

#if 0
static uint8_t const old_lumaQ2[64] = {
    0x06, 0x04, 0x04, 0x04, 0x05, 0x04, 0x06, 0x05,
    0x05, 0x06, 0x09, 0x06, 0x05, 0x06, 0x09, 0x0B,
    0x08, 0x06, 0x06, 0x08, 0x0B, 0x0C, 0x0A, 0x0A,
    0x0B, 0x0A, 0x0A, 0x0C, 0x10, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x10, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
static uint8_t const old_chromaBQ2[64] = {
    0x07, 0x07, 0x07, 0x0D, 0x0C, 0x0D, 0x18, 0x10,
    0x10, 0x18, 0x14, 0x0E, 0x0E, 0x0E, 0x14, 0x14,
    0x0E, 0x0E, 0x0E, 0x0E, 0x14, 0x11, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x11, 0x11, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x11, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
#endif

/* Natural order -> zig zag order
 * The quantization tables above are in natural order
 * but should be applied in zig zag order.
 */
static uint8_t const zigzag[64] = {
        0,   1,  8, 16,  9,  2,  3, 10,
        17, 24, 32, 25, 18, 11,  4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13,  6,  7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
};
/* These Huffman tables correspond to the default tables inside the JPU library */
static uint8_t const mjpeg_enc_huffman_bits_luma_dc[16] = {
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static uint8_t const mjpeg_enc_huffman_bits_luma_ac[16] = {
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
    0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D
};
static uint8_t const mjpeg_enc_huffman_bits_chroma_dc[16] = {
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};
static uint8_t const mjpeg_enc_huffman_bits_chroma_ac[16] = {
    0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
    0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
};
static uint8_t const mjpeg_enc_huffman_value_luma_dc[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B
};
static uint8_t const mjpeg_enc_huffman_value_luma_ac[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};
static uint8_t const mjpeg_enc_huffman_value_chroma_dc[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B
};
static uint8_t const mjpeg_enc_huffman_value_chroma_ac[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
    0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
    0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
    0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
    0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};

#ifdef _WIN32
static unsigned int bmjpu_init_done = 0;
#else
//static atomic_flag bmjpu_init_done = ATOMIC_FLAG_INIT;
static int bmjpu_init_done = 0;
#endif
static int bmjpulib_lock(int sleep_us)
{
    struct timespec req = {0, sleep_us*1000};
#ifdef _WIN32
    while (InterlockedCompareExchange(&bmjpu_init_done, 1, 0))
    {
        Sleep(1);
    }

#else
//    while (atomic_flag_test_and_set(&(bmjpu_init_done)))
    while (__atomic_test_and_set(&bmjpu_init_done, __ATOMIC_SEQ_CST))
    {
        int ret = nanosleep(&req, NULL);
        if (ret < 0) /* interrupted by a signal handler */
        {
//            atomic_flag_clear(&bmjpu_init_done);
            __atomic_clear(&bmjpu_init_done, __ATOMIC_SEQ_CST);
            return -1;
        }
    }
#endif
    return 0;
}
static void bmjpulib_unlock()
{
#ifdef _WIN32
    InterlockedExchange(&bmjpu_init_done, 0);
#else
//    atomic_flag_clear(&bmjpu_init_done);
    __atomic_clear(&bmjpu_init_done, __ATOMIC_SEQ_CST);
#endif
}

#if 0
/* Version 1 */
static unsigned long long jpu_init_inst_counter = 0;
static BOOL bm_jpu_load(void)
{
    int ret;

    ret = bmjpulib_lock(1000); // 1ms
    if (ret < 0)
        return FALSE;

    BM_JPU_LOG("JPU init instance counter: %lu", jpu_init_inst_counter);

    if (jpu_init_inst_counter != 0)
    {
        ++jpu_init_inst_counter;
        bmjpulib_unlock();
        return TRUE;
    }

    ret = jpu_Init(NULL);
    if (ret != JPG_RET_SUCCESS)
    {
        BM_JPU_ERROR("loading JPU failed");
        bmjpulib_unlock();
        return FALSE;
    }

    BM_JPU_DEBUG("loaded JPU");
    ++jpu_init_inst_counter;

    bmjpulib_unlock();
    return TRUE;
}
static BOOL bm_jpu_unload(void)
{
    int ret;

    ret = bmjpulib_lock(1000); // 1ms
    if (ret < 0)
        return FALSE;

    BM_JPU_LOG("JPU init instance counter: %lu", jpu_init_inst_counter);

    if (jpu_init_inst_counter != 0)
    {
        --jpu_init_inst_counter;

        if (jpu_init_inst_counter == 0)
        {
            jpu_UnInit();
            BM_JPU_DEBUG("unloaded JPU");
        }
    }

    bmjpulib_unlock();
    return TRUE;
}

#else

/* Version 2 */
#define MAX_NUM_DEV  64
static volatile int g_jpu_inited[MAX_NUM_DEV] = {0};
static bm_handle_t g_bm_jpu_handle[MAX_NUM_DEV] = { 0 };

bm_handle_t bm_jpu_get_handle(int device_index) {
    int ret = 0;
    bm_handle_t handle = 0;
#ifdef USING_SOC
    device_index = 0;
#endif
    ret = bmjpulib_lock(100); // 100us
    if (ret < 0) {
        return 0;
    }

    if (g_bm_jpu_handle[device_index] != 0) {
        handle = g_bm_jpu_handle[device_index];
    }

    bmjpulib_unlock();
    return handle;
}

static BOOL bm_jpu_load(int device_index)
{
    int ret, index = device_index;

    ret = bmjpulib_lock(100); // 100us
    if (ret < 0)
        return FALSE;

    if (g_bm_jpu_handle[device_index] == 0) {
        bm_handle_t handle;
        bm_status_t ret2 = bm_dev_request(&handle, index);
        if (ret2 != BM_SUCCESS) {
            bmjpulib_unlock();
            BM_JPU_ERROR("request dev %d failed, ret2 = %d\n", index, ret2);
            return FALSE;
        }
        g_bm_jpu_handle[index] = handle;
    }

    bmjpulib_unlock();

#ifdef BM_PCIE_MODE
    if((unsigned int)index >= MAX_NUM_DEV)
    {
        BM_JPU_ERROR("The value of parameter device_index(%d) should be in the range of 0 ~ %d\n",
                     index, MAX_NUM_DEV);
        return FALSE;
    }
#else
    index = 0;
#endif
    if(g_jpu_inited[index] == 0)
    {
        ret = bmjpulib_lock(100); // 100us
        if (ret < 0)
            return FALSE;

        if(g_jpu_inited[index] == 0)
        {
            ret = jpu_Init(index);
            if (ret != JPG_RET_SUCCESS)
            {
                BM_JPU_ERROR("loading JPU failed");
                bmjpulib_unlock();
                return FALSE;
            }

            ret = vpp_Init(index);
            if (ret != JPG_RET_SUCCESS )
            {
                BM_JPU_ERROR("loading VPP failed");
                bmjpulib_unlock();
                return FALSE;
            }

            g_jpu_inited[index] = 1;
        }
        bmjpulib_unlock();
    }
    BM_JPU_DEBUG("loaded JPU");
    return TRUE;
}
static BOOL bm_jpu_unload(int device_index)
{
    return TRUE;
}
#endif


/******************************************************/
/******* MISCELLANEOUS STRUCTURES AND FUNCTIONS *******/
/******************************************************/


BmJpuDecReturnCodes bm_jpu_calc_framebuffer_sizes(unsigned int frame_width,
                                   unsigned int frame_height,
                                   unsigned int framebuffer_alignment,
                                   BmJpuImageFormat image_format,
                                   BmJpuFramebufferSizes *calculated_sizes)
{
    int alignment;
    int is_interleave = 0;

    if ((calculated_sizes == NULL) || (frame_width <= 0) || ((frame_height <= 0))) {
        BM_JPU_ERROR("bm_jpu_calc_framebuffer_sizes params err: calculated_sizes(0X%lx), frame_width(%d), frame_height(%d).", calculated_sizes, frame_width, frame_height);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    calculated_sizes->aligned_frame_width  = BM_JPU_ALIGN_VAL_TO(frame_width, FRAME_WIDTH_ALIGN);
    calculated_sizes->aligned_frame_height = BM_JPU_ALIGN_VAL_TO(frame_height, FRAME_HEIGHT_ALIGN);

    calculated_sizes->y_stride = calculated_sizes->aligned_frame_width;
    calculated_sizes->y_size = calculated_sizes->y_stride * calculated_sizes->aligned_frame_height;

    switch (image_format)
    {
        case BM_JPU_IMAGE_FORMAT_YUV420P:
            calculated_sizes->cbcr_stride = calculated_sizes->y_stride / 2;
            calculated_sizes->cbcr_size = calculated_sizes->y_size / 4;
            break;
        case BM_JPU_IMAGE_FORMAT_NV12:
        case BM_JPU_IMAGE_FORMAT_NV21:
            calculated_sizes->cbcr_stride = calculated_sizes->y_stride;
            calculated_sizes->cbcr_size = calculated_sizes->y_size / 2;
            is_interleave = 1;
            break;
        case BM_JPU_IMAGE_FORMAT_YUV422P:
            calculated_sizes->cbcr_stride = calculated_sizes->y_stride / 2;
            calculated_sizes->cbcr_size = calculated_sizes->y_size / 2;
            break;
        case BM_JPU_IMAGE_FORMAT_NV16:
        case BM_JPU_IMAGE_FORMAT_NV61:
            calculated_sizes->cbcr_stride = calculated_sizes->y_stride;
            calculated_sizes->cbcr_size = calculated_sizes->y_size;
            is_interleave = 1;
            break;
        case BM_JPU_IMAGE_FORMAT_YUV444P:
            calculated_sizes->cbcr_stride = calculated_sizes->y_stride;
            calculated_sizes->cbcr_size = calculated_sizes->y_size;
            break;
        case BM_JPU_IMAGE_FORMAT_GRAY:
            calculated_sizes->cbcr_stride = 0;
            calculated_sizes->cbcr_size = 0;
            break;
        default:
            BM_JPU_ERROR("bm_jpu_calc_framebuffer_sizes image_format(%d) err.", image_format);
            return BM_JPU_DEC_RETURN_CODE_ERROR;
    }

    alignment = framebuffer_alignment;
    if (alignment > 1)
    {
        calculated_sizes->y_size    = BM_JPU_ALIGN_VAL_TO(calculated_sizes->y_size, alignment);
        calculated_sizes->cbcr_size = BM_JPU_ALIGN_VAL_TO(calculated_sizes->cbcr_size, alignment);
    }

    /* cbcr_size is added twice if is_interleave is 0, since in that case,
     * there are *two* separate planes for Cb and Cr, each one with cbcr_size bytes,
     * while in the is_interleave != 0 case, there is one shared chroma plane
     * for both Cb and Cr data, with cbcr_size bytes */
    calculated_sizes->total_size = calculated_sizes->y_size
                                 + (is_interleave ? calculated_sizes->cbcr_size : (calculated_sizes->cbcr_size * 2))
                                 + (alignment>1?alignment:0);
    calculated_sizes->image_format = image_format;

    return BM_JPU_DEC_RETURN_CODE_OK;
}


BmJpuDecReturnCodes bm_jpu_fill_framebuffer_params(BmJpuFramebuffer *framebuffer,
                                    BmJpuFramebufferSizes *calculated_sizes,
                                    bm_jpu_phys_addr_t dma_buffer_addr,
                                    size_t dma_buffer_size,
                                    void* context)
{
    if(framebuffer == NULL ||
       calculated_sizes == NULL) {
        BM_JPU_ERROR("bm_jpu_fill_framebuffer_params params err: framebuffer(0X%lx), calculated_sizes(0X%lx).", framebuffer, calculated_sizes);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    if (framebuffer->dma_buffer == NULL) {
        BM_JPU_ERROR("bm_jpu_fill_framebuffer_params params err: framebuffer->dma_buffer(0X%lx).", framebuffer->dma_buffer);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    framebuffer->dma_buffer->flags.u.mem_type = BM_MEM_TYPE_DEVICE;
    framebuffer->dma_buffer->u.device.device_addr = dma_buffer_addr;
    framebuffer->dma_buffer->u.device.dmabuf_fd = 1;
    framebuffer->dma_buffer->size = dma_buffer_size;

    framebuffer->context = context;
    framebuffer->y_stride = calculated_sizes->y_stride;
    framebuffer->cbcr_stride = calculated_sizes->cbcr_stride;
    framebuffer->y_offset = 0;
    framebuffer->cb_offset = calculated_sizes->y_size;
    framebuffer->cr_offset = calculated_sizes->y_size + calculated_sizes->cbcr_size;

    return BM_JPU_DEC_RETURN_CODE_OK;
}




/************************************************/
/******* DECODER STRUCTURES AND FUNCTIONS *******/
/************************************************/


/* Frames are not just occupied or free. They can be in one of three modes:
 * - FrameMode_Free: framebuffer is not being used for decoding, and does not hold
     a displayable frame
 * - FrameMode_ContainsDisplayableFrame: framebuffer contains frame that has
 *   been fully decoded; this can be displayed
 *
 * Only frames in FrameMode_ContainsDisplayableFrame mode, via the
 * bm_jpu_dec_get_decoded_frame() function.
 */
typedef enum
{
    FrameMode_Free,
    FrameMode_ContainsDisplayableFrame
}
FrameMode;


typedef struct
{
    void *context;
    uint64_t pts;
    uint64_t dts;
    FrameMode mode;
}
BmJpuDecFrameEntry;


struct _BmJpuDecoder
{
    unsigned int device_index;
    DecHandle handle;

    bm_device_mem_t *bs_dma_buffer;
    uint8_t *bs_virt_addr;
    bm_jpu_phys_addr_t bs_phys_addr;

    /* set frame_width and frame_height to limit the max resolution could be decoded
     * set to 0 means no limit */
    unsigned int min_frame_width;
    unsigned int min_frame_height;
    unsigned int max_frame_width;
    unsigned int max_frame_height;
    int chroma_interleave;
    int scale_ratio;

    unsigned int old_jpeg_width;
    unsigned int old_jpeg_height;
    BmJpuColorFormat old_jpeg_color_format;

    unsigned int num_framebuffers, num_used_framebuffers;
    /* internal_framebuffers and framebuffers are separate from
     * frame_entries: internal_framebuffers must be given directly
     * to the jpu_DecRegisterFrameBuffer() function, and framebuffers
     * is a user-supplied input value */
    FrameBuffer *internal_framebuffers;
    BmJpuFramebuffer *framebuffers;
    BmJpuDecFrameEntry *frame_entries;

    DecInitialInfo initial_info;
    BOOL initial_info_available;

    DecOutputInfo dec_output_info;
    int available_decoded_frame_idx;

    bm_jpu_dec_new_initial_info_callback initial_info_callback;
    void *callback_user_data;

    BOOL framebuffer_recycle;
    BOOL bitstream_from_user;
    BOOL framebuffer_from_user;

    int timeout;
    int timeout_count;
};


#define BM_JPU_DEC_HANDLE_ERROR(MSG_START, RET_CODE) \
    bm_jpu_dec_handle_error_full(__FILE__, __LINE__, __func__, (MSG_START), (RET_CODE))


static BmJpuDecReturnCodes bm_jpu_dec_handle_error_full(char const *fn, int linenr, char const *funcn, char const *msg_start, int ret_code);
static BmJpuDecReturnCodes bm_jpu_dec_get_initial_info(BmJpuDecoder *decoder);

static int bm_jpu_dec_find_free_framebuffer(BmJpuDecoder *decoder);

static void bm_jpu_dec_free_internal_arrays(BmJpuDecoder *decoder);


static BmJpuDecReturnCodes bm_jpu_dec_handle_error_full(char const *fn, int linenr, char const *funcn, char const *msg_start, int ret_code)
{
    switch (ret_code)
    {
        case JPG_RET_SUCCESS:
            return BM_JPU_DEC_RETURN_CODE_OK;

        case JPG_RET_FAILURE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: failure", msg_start);
            return BM_JPU_DEC_RETURN_CODE_ERROR;

        case JPG_RET_INVALID_HANDLE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid handle", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INVALID_HANDLE;

        case JPG_RET_INVALID_PARAM:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid parameters", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_INVALID_COMMAND:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid command", msg_start);
            return BM_JPU_DEC_RETURN_CODE_ERROR;

        case JPG_RET_ROTATOR_OUTPUT_NOT_SET:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: rotation enabled but rotator output buffer not set", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_ROTATOR_STRIDE_NOT_SET:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: rotation enabled but rotator stride not set", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_FRAME_NOT_COMPLETE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: frame decoding operation not complete", msg_start);
            return BM_JPU_DEC_RETURN_CODE_ERROR;

        case JPG_RET_INVALID_FRAME_BUFFER:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: frame buffers are invalid", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_INSUFFICIENT_FRAME_BUFFERS:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: not enough frame buffers specified (must be equal to or larger than the minimum number reported by bm_jpu_dec_get_initial_info)", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INSUFFICIENT_FRAMEBUFFERS;

        case JPG_RET_INVALID_STRIDE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid stride - check Y stride values of framebuffers (must be a multiple of 8 and equal to or larger than the frame width)", msg_start);
            return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_WRONG_CALL_SEQUENCE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: wrong call sequence", msg_start);
            return BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE;

        case JPG_RET_CALLED_BEFORE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: already called before (may not be called more than once in a JPU instance)", msg_start);
            return BM_JPU_DEC_RETURN_CODE_ALREADY_CALLED;

        case JPG_RET_NOT_INITIALIZED:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: JPU is not initialized", msg_start);
            return BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE;

        case JPG_RET_EOS:
            /* NOTE: this returns OK on purpose - reaching the MJPEG EOS is not an error */
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: MJPEG end-of-stream reached", msg_start);
            return BM_JPU_DEC_RETURN_CODE_OK;

        case JPG_RET_BIT_EMPTY:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: MJPEG bit buffer empty - cannot parse header", msg_start);
            return BM_JPU_DEC_RETURN_CODE_ERROR;

        default:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: unknown error 0x%x", msg_start, ret_code);
            return BM_JPU_DEC_RETURN_CODE_ERROR;
    }
}


char const * bm_jpu_dec_error_string(BmJpuDecReturnCodes code)
{
    switch (code)
    {
        case BM_JPU_DEC_RETURN_CODE_OK:                        return "ok";
        case BM_JPU_DEC_RETURN_CODE_ERROR:                     return "unspecified error";
        case BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS:            return "invalid params";
        case BM_JPU_DEC_RETURN_CODE_INVALID_HANDLE:            return "invalid handle";
        case BM_JPU_DEC_RETURN_CODE_INVALID_FRAMEBUFFER:       return "invalid framebuffer";
        case BM_JPU_DEC_RETURN_CODE_INSUFFICIENT_FRAMEBUFFERS: return "insufficient framebuffers";
        case BM_JPU_DEC_RETURN_CODE_INVALID_STRIDE:            return "invalid stride";
        case BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE:       return "wrong call sequence";
        case BM_JPU_DEC_RETURN_CODE_TIMEOUT:                   return "timeout";
        case BM_JPU_DEC_RETURN_CODE_ALREADY_CALLED:            return "already called";
        case BM_JPU_DEC_RETURN_ALLOC_MEM_ERROR:                return "error allocating memory";
        case BM_JPU_DEC_RETURN_OVER_LIMITED:                   return "over limited";
        default: return "<unknown>";
    }
}


BmJpuDecReturnCodes bm_jpu_dec_load(int device_index)
{
    return bm_jpu_load(device_index) ? BM_JPU_DEC_RETURN_CODE_OK : BM_JPU_DEC_RETURN_CODE_ERROR;
}


BmJpuDecReturnCodes bm_jpu_dec_unload(int device_index)
{
    return bm_jpu_unload(device_index) ? BM_JPU_DEC_RETURN_CODE_OK : BM_JPU_DEC_RETURN_CODE_ERROR;
}

bm_handle_t bm_jpu_dec_get_bm_handle(int device_index)
{
    return bm_jpu_get_handle(device_index);
}

void bm_jpu_dec_get_bitstream_buffer_info(size_t *size, unsigned int *alignment)
{
    *size = JPU_DEC_BITSTREAM_BUFFER_SIZE;
    *alignment = JPU_MEMORY_ALIGNMENT;
}

void bm_jpu_dec_set_interrupt_timeout(BmJpuDecoder *decoder, int timeout, int count)
{
    if (decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_dec_set_interrupt_timeout params: decoder(0X%lx)", decoder);
        return;
    }
    /* 
    decoder->timeout is 0  first time set timeout 
        timeout > 0   set timeout
        timeout <= 0  set deafult value
    decoder->timeout not 0 
        timeout > 0   set timeout
        timeout <= 0  use  lasttime value
    */
    if (decoder->timeout <= 0) {
        decoder->timeout = timeout > 0 ? timeout : JPU_WAIT_TIMEOUT;
        decoder->timeout_count = count > 0 ? count : JPU_MAX_TIMEOUT_COUNTS;
    } else {
        if (timeout > 0) {
            decoder->timeout = timeout;
        }
        if (count > 0) {
            decoder->timeout_count = count;
        }
    }
}

BmJpuDecReturnCodes bm_jpu_dec_open(BmJpuDecoder **decoder,
                                    BmJpuDecOpenParams *open_params,
                                    bm_device_mem_t *bs_dma_buffer,
                                    bm_jpu_dec_new_initial_info_callback new_initial_info_callback,
                                    void *callback_user_data)
{
    BmJpuDecReturnCodes ret;
    DecOpenParam dec_open_param;
    int dec_ret;

    if((decoder == NULL) || (open_params == NULL) || (bs_dma_buffer == NULL)) {
        BM_JPU_ERROR("bm_jpu_dec_open params err: decoder(0X%lx), open_params(0X%lx), bs_dma_buffer(0X%lx).", decoder, open_params, bs_dma_buffer);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    BM_JPU_DEBUG("opening decoder");

    /* Allocate decoder instance */
    *decoder = BM_JPU_ALLOC(sizeof(BmJpuDecoder));
    if ((*decoder) == NULL) {
        BM_JPU_ERROR("allocating memory for decoder object failed");
        return BM_JPU_DEC_RETURN_ALLOC_MEM_ERROR;
    }

    /* Set default decoder values */
    memset(*decoder, 0, sizeof(BmJpuDecoder));
    (*decoder)->device_index = open_params->device_index;
    (*decoder)->available_decoded_frame_idx = -1;

    /* Map the bitstream buffer. This mapping will persist until the decoder is closed. */
    unsigned long long vmem = 0;
#ifndef BM_PCIE_MODE
    bm_handle_t handle = bm_jpu_get_handle(open_params->device_index);
    bm_mem_mmap_device_mem(handle, bs_dma_buffer, &vmem);
#endif
    (*decoder)->bs_virt_addr = (uint8_t*)vmem;
    (*decoder)->bs_phys_addr = bs_dma_buffer->u.device.device_addr;
    // (*decoder)->bs_dma_buffer = bs_dma_buffer;
    (*decoder)->bs_dma_buffer = BM_JPU_ALLOC(sizeof(bm_device_mem_t));
    memcpy((*decoder)->bs_dma_buffer, bs_dma_buffer, sizeof(bm_device_mem_t));

    (*decoder)->initial_info_callback = new_initial_info_callback;
    (*decoder)->callback_user_data = callback_user_data;

    (*decoder)->framebuffer_recycle = open_params->framebuffer_recycle;
    (*decoder)->bitstream_from_user = open_params->bitstream_from_user;
    (*decoder)->framebuffer_from_user = open_params->framebuffer_from_user;

    BM_JPU_DEBUG("framebuffer_recycle = %d, framebuffer_from_user = %d", (*decoder)->framebuffer_recycle, (*decoder)->framebuffer_from_user);
    BM_JPU_DEBUG("bitstream_from_user = %d, bs_phys_addr = %#lx, bs_virt_addr = %p, bs_dma_buffer->size = %d", (*decoder)->bitstream_from_user, (*decoder)->bs_phys_addr, (*decoder)->bs_virt_addr, (*decoder)->bs_dma_buffer->size);


    /* Fill in values into the JPU's decoder open param structure */
    memset(&dec_open_param, 0, sizeof(dec_open_param));

    /**
     * With motion JPEG, the JPU is configured to operate in line buffer mode.
     * During decoding, pointers to the beginning of the JPEG data inside the
     * bitstream buffer have to be set, which is much simpler if the JPU operates
     * in line buffer mode.
     */

    dec_open_param.bitstreamBuffer = (*decoder)->bs_phys_addr;
    dec_open_param.bitstreamBufferSize = bs_dma_buffer->size;

    dec_open_param.chromaInterleave = open_params->chroma_interleave;
    dec_open_param.deviceIndex = open_params->device_index;

    if (open_params->roiEnable) {
        dec_open_param.roiEnable = open_params->roiEnable;
        dec_open_param.roiWidth = open_params->roiWidth;
        dec_open_param.roiHeight = open_params->roiHeight;
        dec_open_param.roiOffsetX = open_params->roiOffsetX;
        dec_open_param.roiOffsetY = open_params->roiOffsetY;
    }

    /* Now actually open the decoder instance */
    BM_JPU_DEBUG("opening decoder, min frame size: %u x %u pixel, max frame size: %u x %u pixel",
                 open_params->min_frame_width, open_params->min_frame_height,
                 open_params->max_frame_width, open_params->max_frame_height);
    BM_JPU_DEBUG("open_params->scale_ratio=%d", open_params->scale_ratio);

    if((*decoder)->handle == NULL) {
        (*decoder)->handle = BM_JPU_ALLOC(MAX_INST_HANDLE_SIZE);
    }
    memset((*decoder)->handle, 0x00, MAX_INST_HANDLE_SIZE);

    dec_open_param.rotationEnable  = open_params->rotationEnable;
    dec_open_param.rotationAngle   = open_params->rotationAngle;
    dec_open_param.mirrorEnable    = open_params->mirrorEnable;
    dec_open_param.mirrorDirection = open_params->mirrorDirection;

    dec_ret = jpu_DecOpen(&((*decoder)->handle), &dec_open_param);
    ret = BM_JPU_DEC_HANDLE_ERROR("could not open decoder", dec_ret);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        goto cleanup;

    (*decoder)->min_frame_width = open_params->min_frame_width;
    (*decoder)->min_frame_height = open_params->min_frame_height;
    (*decoder)->max_frame_width = open_params->max_frame_width;
    (*decoder)->max_frame_height = open_params->max_frame_height;
    (*decoder)->chroma_interleave = open_params->chroma_interleave;
    (*decoder)->scale_ratio = open_params->scale_ratio;

    bm_jpu_dec_set_interrupt_timeout(*decoder, open_params->timeout, open_params->timeout_count);

    /* Finish & cleanup (in case of error) */
finish:
    if (ret == BM_JPU_DEC_RETURN_CODE_OK)
        BM_JPU_DEBUG("successfully opened decoder");

    return ret;

cleanup:
#ifndef BM_PCIE_MODE
    bm_mem_unmap_device_mem(handle, (*decoder)->bs_virt_addr, bs_dma_buffer->size);
#endif
    BM_JPU_FREE((*decoder)->bs_dma_buffer, sizeof(bm_device_mem_t));
    BM_JPU_FREE(*decoder, sizeof(BmJpuDecoder));
    *decoder = NULL;

    goto finish;
}


BmJpuDecReturnCodes bm_jpu_dec_close(BmJpuDecoder *decoder)
{
    if(decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_dec_close params err: decoder(0X%lx).", decoder);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    BM_JPU_DEBUG("closing decoder");

    /* Remaining cleanup */
    if (decoder->bs_dma_buffer != NULL) {
        if (decoder->bs_virt_addr != NULL) {
        #ifndef BM_PCIE_MODE
            bm_handle_t handle = bm_jpu_get_handle(decoder->device_index);
            bm_mem_unmap_device_mem(handle, decoder->bs_virt_addr, decoder->bs_dma_buffer->size);
        #endif
        }
        BM_JPU_FREE(decoder->bs_dma_buffer, sizeof(bm_device_mem_t));
    }

    bm_jpu_dec_free_internal_arrays(decoder);

    if(decoder->handle != NULL)
         BM_JPU_FREE( (decoder->handle), MAX_INST_HANDLE_SIZE );

    BM_JPU_FREE(decoder, sizeof(BmJpuDecoder));

    return BM_JPU_DEC_RETURN_CODE_OK;
}


BmJpuDecReturnCodes bm_jpu_dec_flush(BmJpuDecoder *decoder)
{
    unsigned int i;

    if(decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_dec_flush params err: decoder(0X%lx).", decoder);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    BM_JPU_DEBUG("flushing decoder");

    /* Any context will be thrown away */
    for (i = 0; i < decoder->num_framebuffers; ++i) {
        decoder->frame_entries[i].context = NULL;
        decoder->frame_entries[i].mode = FrameMode_Free;
    }

    decoder->num_used_framebuffers = 0;

    BM_JPU_DEBUG("successfully flushed decoder");

    return BM_JPU_DEC_RETURN_CODE_OK;
}


BmJpuDecReturnCodes bm_jpu_dec_register_framebuffers(BmJpuDecoder *decoder,
                                                     BmJpuFramebuffer *framebuffers,
                                                     unsigned int num_framebuffers)
{
    unsigned long long i;
    BmJpuDecReturnCodes ret;
    int dec_ret;

    if((decoder == NULL) || (framebuffers == NULL) || (num_framebuffers <= 0)) {
        BM_JPU_ERROR("bm_jpu_dec_register_framebuffers params err: decoder(0X%lx), framebuffers(0X%lx), num_framebuffers(%d).", decoder, framebuffers, num_framebuffers);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    BM_JPU_DEBUG("attempting to register %u framebuffers", num_framebuffers);

    bm_jpu_dec_free_internal_arrays(decoder);


    /* Allocate memory for framebuffer structures and contexts */
    decoder->internal_framebuffers = BM_JPU_ALLOC(sizeof(FrameBuffer) * num_framebuffers);
    if (decoder->internal_framebuffers == NULL)
    {
        BM_JPU_ERROR("allocating memory for framebuffers failed");
        ret = BM_JPU_DEC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    decoder->frame_entries = BM_JPU_ALLOC(sizeof(BmJpuDecFrameEntry) * num_framebuffers);
    if (decoder->frame_entries == NULL)
    {
        BM_JPU_ERROR("allocating memory for frame entries failed");
        ret = BM_JPU_DEC_RETURN_CODE_ERROR;
        goto cleanup;
    }


    /* Copy the values from the framebuffers array to the internal_framebuffers
     * one, which in turn will be used by the JPU */
    memset(decoder->internal_framebuffers, 0, sizeof(FrameBuffer) * num_framebuffers);
    for (i = 0; i < num_framebuffers; ++i)
    {
        BmJpuFramebuffer *fb = &framebuffers[i];
        FrameBuffer *internal_fb = &(decoder->internal_framebuffers[i]);
        bm_jpu_phys_addr_t phys_addr;

        phys_addr = fb->dma_buffer->u.device.device_addr;
        if (phys_addr == 0)
        {
            BM_JPU_ERROR("could not map buffer %u/%u", i, num_framebuffers);
            ret = BM_JPU_DEC_RETURN_CODE_ERROR;
            goto cleanup;
        }

        BM_JPU_DEBUG("register framebuffer %d: phys_addr=%#lx, size=%u, y_stride=%d, cbcr_stride=%d",
                    i, phys_addr, fb->dma_buffer->size, fb->y_stride, fb->cbcr_stride);

        /* In-place modifications in the framebuffers array */
        fb->already_marked = TRUE;
        fb->internal = (void*)i; /* Use the internal value to contain the index */

        internal_fb->strideY = fb->y_stride;
        internal_fb->strideC = fb->cbcr_stride;
        internal_fb->myIndex = i;

        internal_fb->bufY  = (PhysicalAddress)(phys_addr + fb->y_offset);
        internal_fb->bufCb = (PhysicalAddress)(phys_addr + fb->cb_offset);
        internal_fb->bufCr = (PhysicalAddress)(phys_addr + fb->cr_offset);
    }
    /* The stride value is assumed to be the same for all framebuffers */
    dec_ret = jpu_DecRegisterFrameBuffer(decoder->handle,
                                         decoder->internal_framebuffers,
                                         num_framebuffers,
                                         framebuffers[0].y_stride,
                                         NULL);
    ret = BM_JPU_DEC_HANDLE_ERROR("could not register framebuffers", dec_ret);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        goto cleanup;

    /* Set default rotator settings for motion JPEG */
    {
        /* the datatypes are int, but this is undocumented; determined by looking
         * into the bm-jpu library's jpu_lib.c jpu_DecGiveCommand() definition */
        //int rotation_angle = 0;
        //int mirror = 0;
        int stride = framebuffers[0].y_stride;

        // jpu_DecGiveCommand(decoder->handle, SET_JPG_ROTATION_ANGLE,   (void *)(&rotation_angle));
        // jpu_DecGiveCommand(decoder->handle, SET_JPG_MIRROR_DIRECTION, (void *)(&mirror));
        jpu_DecGiveCommand(decoder->handle, SET_JPG_ROTATOR_STRIDE,   (void *)(&stride));
    }

    /* Store the pointer to the caller-supplied framebuffer array,
     * and set the context pointers to their initial value (NULL) */
    decoder->framebuffers = framebuffers;
    decoder->num_framebuffers = num_framebuffers;
    for (i = 0; i < num_framebuffers; ++i)
    {
        decoder->frame_entries[i].context = NULL;
        decoder->frame_entries[i].mode = FrameMode_Free;
    }
    decoder->num_used_framebuffers = 0;

    return BM_JPU_DEC_RETURN_CODE_OK;

cleanup:
    bm_jpu_dec_free_internal_arrays(decoder);

    return ret;
}


static BmJpuDecReturnCodes bm_jpu_dec_get_initial_info(BmJpuDecoder *decoder)
{
    BmJpuDecReturnCodes ret;
    int dec_ret;

    if(decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_dec_get_initial_info params err: decoder(0X%lx).", decoder);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    decoder->initial_info_available = FALSE;

    /* The actual retrieval */
    dec_ret = jpu_DecGetInitialInfo(decoder->handle, &(decoder->initial_info));

    ret = BM_JPU_DEC_HANDLE_ERROR("could not retrieve configuration information", dec_ret);
    if (ret == BM_JPU_DEC_RETURN_CODE_OK)
        decoder->initial_info_available = TRUE;

    return ret;
}

/* For motion JPEG, the user has to find a free framebuffer manually;
 * the JPU does not do that in this case */
static int bm_jpu_dec_find_free_framebuffer(BmJpuDecoder *decoder)
{
    unsigned int i;

    for (i = 0; i < decoder->num_framebuffers; ++i)
    {
        if (decoder->frame_entries[i].mode == FrameMode_Free)
            return (int)i;
    }

    return -1;
}


static void bm_jpu_dec_free_internal_arrays(BmJpuDecoder *decoder)
{
    if (decoder->internal_framebuffers != NULL)
    {
        BM_JPU_FREE(decoder->internal_framebuffers, sizeof(FrameBuffer) * decoder->num_framebuffers);
        decoder->internal_framebuffers = NULL;
    }

    if (decoder->frame_entries != NULL)
    {
        BM_JPU_FREE(decoder->frame_entries, sizeof(BmJpuDecFrameEntry) * decoder->num_framebuffers);
        decoder->frame_entries = NULL;
    }
}

BmJpuDecReturnCodes bm_jpu_dec_decode(BmJpuDecoder *decoder,
                                      bm_device_mem_t bs_mem,
                                      BmJpuEncodedFrame const *encoded_frame,
                                      unsigned int *output_code)
{
    BmJpuDecReturnCodes ret = BM_JPU_DEC_RETURN_CODE_OK;
    unsigned int jpeg_width, jpeg_height;
    unsigned int scale_width, scale_height;
    BmJpuColorFormat jpeg_color_format;
    int timeout;
    int pixels;

    if((decoder == NULL) || (encoded_frame == NULL) || (output_code == NULL)) {
        BM_JPU_ERROR("bm_jpu_dec_decode params err: decoder(0X%lx), encoded_frame(0X%lx), output_code(0X%lx).", decoder, encoded_frame, output_code);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    BM_JPU_LOG("input info: %d byte", encoded_frame->data_size);

    /* Handle main frame data */
    BM_JPU_LOG("pushing main frame data with %zu byte", encoded_frame->data_size);

    if(decoder->bs_dma_buffer->size < encoded_frame->data_size)
    {
        BM_JPU_ERROR("Error! bs size: %d < jpeg data length: %d\n", decoder->bs_dma_buffer->size, encoded_frame->data_size);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    bm_handle_t handle = bm_jpu_get_handle(decoder->device_index);

    if(bs_mem.size)
    {
#ifndef BM_PCIE_MODE
        bm_mem_unmap_device_mem(handle, decoder->bs_virt_addr, decoder->bs_dma_buffer->size);
#endif
        memcpy(decoder->bs_dma_buffer, &bs_mem, sizeof(bm_device_mem_t));
        decoder->bs_phys_addr = decoder->bs_dma_buffer->u.device.device_addr;
        jpu_DecSetRdPtrEx(decoder->handle, decoder->bs_phys_addr, TRUE);
#ifndef BM_PCIE_MODE
        unsigned long long vmem = 0;
        bm_mem_mmap_device_mem(handle, &bs_mem, &vmem);
        decoder->bs_virt_addr = vmem;
#endif
    }

    if(encoded_frame->data_size > decoder->bs_dma_buffer->size)
        return BM_JPU_DEC_RETURN_ILLEGAL_BS_SIZE;

#ifdef BM_PCIE_MODE
    ret = bm_memcpy_s2d_partial(handle, *(decoder->bs_dma_buffer), encoded_frame->data, encoded_frame->data_size);
#else
    unsigned long long p_virt_addr = 0;
    ret = bm_mem_mmap_device_mem(handle, decoder->bs_dma_buffer, &p_virt_addr);
    if (ret != 0)
    {
        BM_JPU_ERROR("Error! bm_mem_mmap_device_mem failed.\n");
        return ret;
    }
    memcpy((void*)p_virt_addr, encoded_frame->data, encoded_frame->data_size);
    ret = bm_mem_flush_partial_device_mem(handle, decoder->bs_dma_buffer, 0, encoded_frame->data_size);
    bm_mem_unmap_device_mem(handle, (void *)p_virt_addr, decoder->bs_dma_buffer->size);
#endif
    if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        return ret;

    *output_code = BM_JPU_DEC_OUTPUT_CODE_INPUT_USED;

    /**
     * The JPU does not report size or color format changes. Therefore,
     * JPEG header have to be parsed here manually to retrieve the
     * width, height, and color format and check if these changed.
     * If so, invoke the initial_info_callback again.
     */

    if (!bm_jpu_parse_jpeg_header(encoded_frame->data, encoded_frame->data_size, &jpeg_width, &jpeg_height, &jpeg_color_format))
    {
        BM_JPU_ERROR("encoded frame is not valid JPEG data");
        return BM_JPU_DEC_RETURN_CODE_ERROR;
    }

    bm_jpu_calc_framebuffer_sizes(jpeg_width, jpeg_height, 1, jpeg_color_format, &pixels);

    BM_JPU_LOG("JPEG frame information:  width: %u  height: %u  format: %s",
               jpeg_width, jpeg_height, bm_jpu_color_format_string(jpeg_color_format));

    scale_width  = jpeg_width  >> decoder->scale_ratio;
    scale_height = jpeg_height >> decoder->scale_ratio;

    BM_JPU_LOG("scale_width=%d, scale_height=%d", scale_width, scale_height);

    if ((decoder->max_frame_width != 0 && scale_width > decoder->max_frame_width) || (decoder->max_frame_height != 0 && scale_height > decoder->max_frame_height))
    {
        BM_JPU_ERROR("exceed the max resolution of decoder, limit: %u x %u, input: %u x %u",
                     decoder->max_frame_width, decoder->max_frame_height, scale_width, scale_height);
        return BM_JPU_DEC_RETURN_OVER_LIMITED;
    }

    if ((decoder->min_frame_width != 0 && scale_width < decoder->min_frame_width) || (decoder->min_frame_height != 0 && scale_height < decoder->min_frame_height))
    {
        BM_JPU_ERROR("deceed the min resolution of decoder, limit: %u x %u, input: %u x %u",
                     decoder->min_frame_width, decoder->min_frame_height, scale_width, scale_height);
        return BM_JPU_DEC_RETURN_OVER_LIMITED;
    }

    jpu_DecSetResolutionInfo(decoder->handle, scale_width, scale_height);
    jpu_DecSetBsPtr(decoder->handle, encoded_frame->data, encoded_frame->data_size);

    if (decoder->initial_info_available &&
        ((decoder->old_jpeg_width != scale_width) ||
         (decoder->old_jpeg_height != scale_height) ||
         (decoder->old_jpeg_color_format != jpeg_color_format)))
    {
        BM_JPU_DEBUG("input resolution is different with decoder, maybe bitstream changed");
        BmJpuDecInitialInfo initial_info;
        initial_info.frame_width = scale_width;
        initial_info.frame_height = scale_height;
        initial_info.min_num_required_framebuffers = MIN_NUM_FREE_FB_REQUIRED;
        initial_info.framebuffer_alignment = 1;
        initial_info.roiFrameWidth  = decoder->initial_info.roiFrameWidth;
        initial_info.roiFrameHeight = decoder->initial_info.roiFrameHeight;

        bm_jpu_convert_to_image_format(jpeg_color_format, decoder->chroma_interleave, BM_JPU_PACKED_FORMAT_NONE, &initial_info.image_format);

        /* Invoke the initial_info_callback. Framebuffers for decoding are allocated and registered there. */
        if ((ret = decoder->initial_info_callback(decoder, &initial_info, *output_code, decoder->callback_user_data)) != BM_JPU_DEC_RETURN_CODE_OK)
        {
            BM_JPU_ERROR("initial info callback reported failure - cannot continue");
            return ret;
        }
        BM_JPU_DEBUG("framebuffer configure changed");
    }

    decoder->old_jpeg_width = scale_width;
    decoder->old_jpeg_height = scale_height;
    decoder->old_jpeg_color_format = jpeg_color_format;


    /* Start decoding process */
    if (!(decoder->initial_info_available))
    {
        BmJpuDecInitialInfo initial_info;

        /* Initial info is not available yet. Fetch it, and store it
         * inside the decoder instance structure. */
        ret = bm_jpu_dec_get_initial_info(decoder);
        switch (ret)
        {
        case BM_JPU_DEC_RETURN_CODE_OK:
            break;

        case BM_JPU_DEC_RETURN_CODE_INVALID_HANDLE:
            return BM_JPU_DEC_RETURN_CODE_INVALID_HANDLE;
        case BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS:
            /* if this error occurs, something inside this code is wrong; this is no user error */
            BM_JPU_ERROR("Internal error: invalid info structure while retrieving initial info");
            return BM_JPU_DEC_RETURN_CODE_ERROR;
        case BM_JPU_DEC_RETURN_CODE_TIMEOUT:
            BM_JPU_ERROR("JPU reported timeout while retrieving initial info");
            return BM_JPU_DEC_RETURN_CODE_TIMEOUT;
        case BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE:
            return BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE;
        case BM_JPU_DEC_RETURN_CODE_ALREADY_CALLED:
            BM_JPU_ERROR("Initial info was already retrieved - duplicate call");
            return BM_JPU_DEC_RETURN_CODE_ALREADY_CALLED;
        case BM_JPU_DEC_RETURN_CODE_ERROR:
            BM_JPU_ERROR("Internal error: unspecified error");
            return BM_JPU_DEC_RETURN_CODE_ERROR;
        default:
            /* do not report error; instead, let the caller supply the
             * JPU with more data, until initial info can be retrieved */
            *output_code |= BM_JPU_DEC_OUTPUT_CODE_NOT_ENOUGH_INPUT_DATA;
        }

        if (decoder->initial_info.picWidth == 0)
            initial_info.frame_width = scale_width;
        else
            initial_info.frame_width = decoder->initial_info.picWidth>>decoder->scale_ratio;
        if (decoder->initial_info.picHeight == 0)
            initial_info.frame_height = scale_height;
        else
            initial_info.frame_height = decoder->initial_info.picHeight>>decoder->scale_ratio;

        int rotationEnable   = 0;
        int rotationAngle    = 0;
        int mirrorEnable     = 0;
        int mirrorDirection  = 0;
        jpu_DecGetRotateInfo(decoder->handle, &rotationEnable, &rotationAngle, &mirrorEnable, &mirrorDirection);
        if((rotationEnable == 1) && ((rotationAngle == 90) || (rotationAngle == 270))) {
            int temp = initial_info.frame_height;
            initial_info.frame_height = initial_info.frame_width;
            initial_info.frame_width  = temp;
        }

        initial_info.roiFrameWidth = decoder->initial_info.roiFrameWidth;
        initial_info.roiFrameHeight = decoder->initial_info.roiFrameHeight;
        BM_JPU_LOG("roiFrameWidth=%d, roiFrameHeight=%d", initial_info.roiFrameWidth, initial_info.roiFrameHeight);

        initial_info.min_num_required_framebuffers = decoder->initial_info.minFrameBufferCount;
        /* Make sure that at least one framebuffer is allocated and registered
         * (Also for motion JPEG, even though the JPU doesn't use framebuffers then) */
        if (initial_info.min_num_required_framebuffers < 1)
            initial_info.min_num_required_framebuffers = 1;

        initial_info.framebuffer_alignment = 1; /* for maptype 0 (linear, non-tiling) */

        switch (decoder->initial_info.sourceFormat)
        {
        case FORMAT_420:
            initial_info.image_format = ((decoder->chroma_interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE) ? BM_JPU_IMAGE_FORMAT_NV21 :
                                        ((decoder->chroma_interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE) ? BM_JPU_IMAGE_FORMAT_NV12 :
                                        BM_JPU_IMAGE_FORMAT_YUV420P));
            break;
        case FORMAT_422:
            initial_info.image_format = ((decoder->chroma_interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE) ? BM_JPU_IMAGE_FORMAT_NV61 :
                                        ((decoder->chroma_interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE) ? BM_JPU_IMAGE_FORMAT_NV16 :
                                        BM_JPU_IMAGE_FORMAT_YUV422P));
            break;
        // FIXME: which image format to use?
        // case FORMAT_224:
        //     initial_info.image_format = BM_JPU_COLOR_FORMAT_YUV422_VERTICAL;
        //     break;
        case FORMAT_444:
            initial_info.image_format = BM_JPU_IMAGE_FORMAT_YUV444P;
            break;
        case FORMAT_400:
            initial_info.image_format = BM_JPU_IMAGE_FORMAT_GRAY;
            break;
        default:
            BM_JPU_ERROR("unknown source color format value %d", decoder->initial_info.sourceFormat);
            return BM_JPU_DEC_RETURN_CODE_ERROR;
        }

        /* Invoke the initial_info_callback. Framebuffers for decoding are allocated
         * and registered there. */
        if ((ret = decoder->initial_info_callback(decoder, &initial_info, *output_code, decoder->callback_user_data)) != BM_JPU_DEC_RETURN_CODE_OK)
        {
            BM_JPU_ERROR("initial info callback reported failure - cannot continue");
            return ret;
        }
    }

    {
        int dec_ret;
        DecParam params;
        int jpeg_frame_idx = -1;

        memset(&params, 0, sizeof(params));

        params.scaleDownRatioWidth  = decoder->scale_ratio;
        params.scaleDownRatioHeight = decoder->scale_ratio;

        /* When decoding motion JPEG data,
         * the user has to manually specify a framebuffer for the
         * output by sending the SET_JPG_ROTATOR_OUTPUT command. */
        jpeg_frame_idx = bm_jpu_dec_find_free_framebuffer(decoder);
        if (jpeg_frame_idx < 0 )
        {
            BM_JPU_ERROR("could not find free framebuffer for MJPEG output");
            return BM_JPU_DEC_RETURN_CODE_ERROR;
        }

        jpu_DecGiveCommand(decoder->handle, SET_JPG_ROTATOR_OUTPUT, (void *)(&(decoder->internal_framebuffers[jpeg_frame_idx])));

        /* Start frame decoding
         * The error handling code below does dummy jpu_DecGetOutputInfo() calls
         * before exiting. This is done because according to the documentation,
         * jpu_DecStartOneFrame() "locks out" most JPU calls until
         * jpu_DecGetOutputInfo() is called, so this must be called *always*
         * after jpu_DecStartOneFrame(), even if an error occurred. */
        dec_ret = jpu_DecStartOneFrame(decoder->handle, &params);
        if (dec_ret == JPG_RET_BIT_EMPTY)
        {
            jpu_DecGetOutputInfo(decoder->handle, &(decoder->dec_output_info));
            *output_code |= BM_JPU_DEC_OUTPUT_CODE_NOT_ENOUGH_INPUT_DATA;
            ret = BM_JPU_DEC_RETURN_CODE_OK;
            goto release_jpu_res;
        }
        else if (dec_ret == JPG_RET_EOS)
        {
            *output_code |= BM_JPU_DEC_OUTPUT_CODE_EOS;
            dec_ret = JPG_RET_SUCCESS;
        }

        ret = BM_JPU_DEC_HANDLE_ERROR("could not decode frame", dec_ret);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        {
            jpu_DecGetOutputInfo(decoder->handle, &(decoder->dec_output_info));
            goto release_jpu_res;
        }


        /* Wait for frame completion */

        BM_JPU_LOG("waiting for decoding completion");

        timeout = jpu_DecWaitForInt(decoder->handle, decoder->timeout, decoder->timeout_count);
        /* If a timeout occurred earlier, this is the correct time to abort
         * decoding and return an error code, since jpu_DecGetOutputInfo()
         * has been called, unlocking the JPU decoder calls. */
        if (timeout) {
            BM_JPU_WARNING("timeout for jpeg decoding!");
            if(decoder->timeout < 15 * (pixels >> 20))
            {
                BM_JPU_WARNING("timeout %d ms may be too short, at least: %d ms\n", decoder->timeout, 15 * (pixels >> 20));
                jpu_DecGetOutputInfo(decoder->handle, &(decoder->dec_output_info));
            }
            ret = BM_JPU_DEC_RETURN_CODE_TIMEOUT;
            goto release_jpu_res;
        }
        /* Retrieve information about the result of the decode process.
         *
         * jpu_DecGetOutputInfo() is called even if a timeout occurred. This is
         * intentional, since according to the JPU docs, jpu_DecStartOneFrame() won't be
         * usable again until jpu_DecGetOutputInfo() is called. In other words, the
         * jpu_DecStartOneFrame() locks down some internals inside the JPU, and
         * jpu_DecGetOutputInfo() releases them. */

        dec_ret = jpu_DecGetOutputInfo(decoder->handle, &(decoder->dec_output_info));
        ret = BM_JPU_DEC_HANDLE_ERROR("could not get output information", dec_ret);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK)
            goto release_jpu_res;

#ifdef __linux__
        const char* value = getenv("FLUSH_JPEG_BUFFER");
        if (value != NULL) {
            int intValue = atoi(value);
            if(intValue > 0){
                usleep(intValue);
            }
        } else {
            BM_JPU_LOG("FLUSH_JPEG_BUFFER is not set. if you need work 200 us buffer time, please set FLUSH_JPEG_BUFFER = 200\n");
        }
#endif

        /* free the jpu resource */
        dec_ret = jpu_DecClose(decoder->handle);
        ret = BM_JPU_DEC_HANDLE_ERROR("could not close decoder", dec_ret);

        /* Log some information about the decoded frame */
        BM_JPU_LOG("output info:  indexFrameDisplay %d  numOfErrMBs %d  decodingSuccess %d  decPicWidth %d  decPicHeight %d",
                   decoder->dec_output_info.indexFrameDisplay,
                   decoder->dec_output_info.numOfErrMBs,
                   decoder->dec_output_info.decodingSuccess,
                   decoder->dec_output_info.decPicWidth,
                   decoder->dec_output_info.decPicHeight);

        /* Motion JPEG requires frame index adjustments */
        BM_JPU_DEBUG("MJPEG data -> adjust indexFrameDisplay value to %d", jpeg_frame_idx);
        decoder->dec_output_info.indexFrameDisplay = jpeg_frame_idx;

        {
            BmJpuDecFrameEntry *entry = &(decoder->frame_entries[jpeg_frame_idx]);

            entry->context = encoded_frame->context;
            entry->pts     = encoded_frame->pts;
            entry->dts     = encoded_frame->dts;
            entry->mode    = FrameMode_ContainsDisplayableFrame;

            decoder->num_used_framebuffers++;

            decoder->available_decoded_frame_idx = jpeg_frame_idx;

            *output_code |= BM_JPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE;

            BM_JPU_LOG("decoded and displayable frame available (framebuffer display index: %d context: %p pts: %" PRIu64 " dts: %" PRIu64 ")",
                       jpeg_frame_idx, entry->context, entry->pts, entry->dts);
        }
    }

    return ret;

release_jpu_res:
    /* free the jpu resource */
    jpu_DecClose(decoder->handle);
    return ret;
}


BmJpuDecReturnCodes bm_jpu_dec_get_decoded_frame(BmJpuDecoder *decoder, BmJpuRawFrame *decoded_frame)
{
    int idx;

    if((decoder == NULL) || (decoded_frame == NULL)) {
        BM_JPU_ERROR("bm_jpu_dec_get_decoded_frame params err: decoder(0X%lx), decoded_frame(0X%lx).", decoder, decoded_frame);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    /* available_decoded_frame_idx < 0 means there is no frame
     * to retrieve yet, or the frame was already retrieved */
    if (decoder->available_decoded_frame_idx < 0)
    {
        BM_JPU_ERROR("no decoded frame available, or function was already called earlier");
        return BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE;
    }

    idx = decoder->available_decoded_frame_idx;
    if (idx >= (int)(decoder->num_framebuffers)){
        BM_JPU_ERROR("the number of decoding reaches the upper limit[idx(%d), num_framebuffers(%d)].", idx, decoder->num_framebuffers);
        return BM_JPU_DEC_RETURN_CODE_ERROR;
    }

    /* retrieve the framebuffer at the given index, and set its already_marked flag
     * to FALSE, since it contains a fully decoded and still undisplayed framebuffer */
    decoded_frame->framebuffer = &(decoder->framebuffers[idx]);
    decoded_frame->framebuffer->already_marked = FALSE;
    decoded_frame->context = decoder->frame_entries[idx].context;
    decoded_frame->pts     = decoder->frame_entries[idx].pts;
    decoded_frame->dts     = decoder->frame_entries[idx].dts;


    /* erase the context from context_for_frames after retrieval, and set
     * available_decoded_frame_idx to -1 ; this ensures no erroneous
     * double-retrieval can occur */
    decoder->frame_entries[idx].context = NULL;
    decoder->available_decoded_frame_idx = -1;

    return BM_JPU_DEC_RETURN_CODE_OK;
}


int bm_jpu_dec_check_if_can_decode(BmJpuDecoder *decoder)
{
    if(decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_dec_check_if_can_decode params err: decoder(0X%lx).", decoder);
        return 0;
    }
    int num_free_framebuffers = decoder->num_framebuffers - decoder->num_used_framebuffers;
    return num_free_framebuffers >= MIN_NUM_FREE_FB_REQUIRED;
}


BmJpuDecReturnCodes bm_jpu_dec_mark_framebuffer_as_displayed(BmJpuDecoder *decoder, BmJpuFramebuffer *framebuffer)
{
    long long idx;

    if((decoder == NULL) || (framebuffer == NULL)) {
        BM_JPU_ERROR("bm_jpu_dec_mark_framebuffer_as_displayed params err: decoder(0X%lx), framebuffer(0X%lx).", decoder, framebuffer);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    /* don't do anything if the framebuffer has already been marked
     * this ensures the num_used_framebuffers counter remains valid
     * even if this function is called for the same framebuffer twice */
    if (framebuffer->already_marked)
    {
        BM_JPU_ERROR("framebuffer has already been marked as displayed");
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    /* the index into the framebuffer array is stored in the "internal" field */
    idx = (long long)(framebuffer->internal);
    if (idx >= (int)(decoder->num_framebuffers)){
        BM_JPU_ERROR("the number of decoding reaches the upper limit[idx(%d), num_framebuffers(%d)].", idx, decoder->num_framebuffers);
        return BM_JPU_DEC_RETURN_CODE_ERROR;
    }

    /* frame is no longer being used */
    decoder->frame_entries[idx].mode = FrameMode_Free;

    BM_JPU_LOG("marked framebuffer with index #%d as displayed", idx);


    /* set the already_marked flag to inform the rest of the bmjpuapi
     * decoder instance that the framebuffer isn't occupied anymore,
     * and count down num_used_framebuffers to reflect that fact */
    framebuffer->already_marked = TRUE;
    decoder->num_used_framebuffers--;


    return BM_JPU_DEC_RETURN_CODE_OK;
}




/************************************************/
/******* ENCODER STRUCTURES AND FUNCTIONS *******/
/************************************************/


struct _BmJpuEncoder
{
    unsigned int device_index;
    EncHandle handle;

    bm_device_mem_t *bs_dma_buffer;
    uint8_t *bs_virt_addr;

    BmJpuColorFormat color_format;
    unsigned int frame_width, frame_height;

    BmJpuFramebuffer *framebuffers;

    int timeout;
    int timeout_count;
};


typedef struct _BmJpuEncWriteContext BmJpuEncWriteContext;

struct _BmJpuEncWriteContext
{
    uint8_t *write_ptr;
    uint8_t *write_ptr_start;
    uint8_t *write_ptr_end;
    size_t mjpeg_header_size;
    bm_device_mem_t *bs_buffer;
};


#define BM_JPU_ENC_HANDLE_ERROR(MSG_START, RET_CODE) \
    bm_jpu_enc_handle_error_full(__FILE__, __LINE__, __func__, (MSG_START), (RET_CODE))

static BmJpuEncReturnCodes bm_jpu_enc_handle_error_full(char const *fn, int linenr, char const *funcn, char const *msg_start, int ret_code)
{
    switch (ret_code)
    {
        case JPG_RET_SUCCESS:
            return BM_JPU_ENC_RETURN_CODE_OK;

        case JPG_RET_FAILURE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: failure", msg_start);
            return BM_JPU_ENC_RETURN_CODE_ERROR;

        case JPG_RET_INVALID_HANDLE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid handle", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_HANDLE;

        case JPG_RET_INVALID_PARAM:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid parameters", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_INVALID_COMMAND:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid command", msg_start);
            return BM_JPU_ENC_RETURN_CODE_ERROR;

        case JPG_RET_ROTATOR_OUTPUT_NOT_SET:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: rotation enabled but rotator output buffer not set", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_ROTATOR_STRIDE_NOT_SET:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: rotation enabled but rotator stride not set", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_FRAME_NOT_COMPLETE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: frame encoding operation not complete", msg_start);
            return BM_JPU_ENC_RETURN_CODE_ERROR;

        case JPG_RET_INVALID_FRAME_BUFFER:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: frame buffers are invalid", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_INSUFFICIENT_FRAME_BUFFERS:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: not enough frame buffers specified (must be equal to or larger than the minimum number reported by bm_jpu_enc_get_initial_info)", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_INVALID_STRIDE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid stride - check Y stride values of framebuffers (must be a multiple of 8 and equal to or larger than the frame width)", msg_start);
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;

        case JPG_RET_WRONG_CALL_SEQUENCE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: wrong call sequence", msg_start);
            return BM_JPU_ENC_RETURN_CODE_WRONG_CALL_SEQUENCE;

        case JPG_RET_CALLED_BEFORE:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: already called before (may not be called more than once in a JPU instance)", msg_start);
            return BM_JPU_ENC_RETURN_CODE_ERROR;

        case JPG_RET_NOT_INITIALIZED:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: JPU is not initialized", msg_start);
            return BM_JPU_ENC_RETURN_CODE_WRONG_CALL_SEQUENCE;

        case JPG_RET_EOS:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: MJPEG end-of-stream reached", msg_start);
            return BM_JPU_ENC_RETURN_CODE_OK;

        case JPG_RET_BIT_EMPTY:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: MJPEG bit buffer empty - cannot parse header", msg_start);
            return BM_JPU_ENC_RETURN_CODE_ERROR;

        case JPG_RET_BS_BUFFER_FULL:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: bs buffer full", msg_start);
            return BM_JPU_ENC_RETURN_BS_BUFFER_FULL;

        default:
            BM_JPU_ERROR_FULL(fn, linenr, funcn, "%s: unknown error 0x%x", msg_start, ret_code);
            return BM_JPU_ENC_RETURN_CODE_ERROR;
    }
}

static void bm_jpu_enc_copy_quantization_table(uint8_t *dest_table, uint8_t const *src_table, size_t num_coefficients, unsigned int scale_factor)
{
    BM_JPU_LOG("quantization table:  num coefficients: %u  scale factor: %u ", num_coefficients, scale_factor);
    for (size_t i = 0; i < num_coefficients; ++i)
    {
        /* The +50 ensures rounding instead of truncation */
        long long val = (((long long)src_table[zigzag[i]]) * scale_factor + 50) / 100;

        /* The JPU's JPEG encoder supports baseline data only,
         * so no quantization matrix values above 255 are allowed */
        if (val <= 0)
            val = 1;
        else if (val >= 255)
            val = 255;

        dest_table[i] = val;
    }
}

static BmJpuEncReturnCodes bm_jpu_enc_set_mjpeg_tables(unsigned int quality_factor, EncOpenParam *enc_open_par)
{
    uint8_t const *component_info_table;
    unsigned int scale_factor;

    if (enc_open_par == NULL) {
        BM_JPU_ERROR("bm_jpu_enc_set_mjpeg_tables params err: enc_open_par(0X%lx).", enc_open_par);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    /* NOTE: The tables in structure referred to by enc_open_par must
     * have been filled with nullbytes, and the sourceFormat field
     * must be valid */


    /* Copy the Huffman tables */

    memcpy(enc_open_par->huffBits[DC_TABLE_INDEX0], mjpeg_enc_huffman_bits_luma_dc, sizeof(mjpeg_enc_huffman_bits_luma_dc));
    memcpy(enc_open_par->huffBits[AC_TABLE_INDEX0], mjpeg_enc_huffman_bits_luma_ac, sizeof(mjpeg_enc_huffman_bits_luma_ac));
    memcpy(enc_open_par->huffBits[DC_TABLE_INDEX1], mjpeg_enc_huffman_bits_chroma_dc, sizeof(mjpeg_enc_huffman_bits_chroma_dc));
    memcpy(enc_open_par->huffBits[AC_TABLE_INDEX1], mjpeg_enc_huffman_bits_chroma_ac, sizeof(mjpeg_enc_huffman_bits_chroma_ac));

    memcpy(enc_open_par->huffVal[DC_TABLE_INDEX0], mjpeg_enc_huffman_value_luma_dc, sizeof(mjpeg_enc_huffman_value_luma_dc));
    memcpy(enc_open_par->huffVal[AC_TABLE_INDEX0], mjpeg_enc_huffman_value_luma_ac, sizeof(mjpeg_enc_huffman_value_luma_ac));
    memcpy(enc_open_par->huffVal[DC_TABLE_INDEX1], mjpeg_enc_huffman_value_chroma_dc, sizeof(mjpeg_enc_huffman_value_chroma_dc));
    memcpy(enc_open_par->huffVal[AC_TABLE_INDEX1], mjpeg_enc_huffman_value_chroma_ac, sizeof(mjpeg_enc_huffman_value_chroma_ac));

    /* Ensure the quality factor is in the 1..100 range */
    if (quality_factor < 1)
        quality_factor = 1;
    if (quality_factor > 100)
        quality_factor = 100;

    /* Using the Independent JPEG Group's formula, used in libjpeg, for generating
     * a scale factor out of a quality factor in the 1..100 range */
    if (quality_factor < 50)
        scale_factor = 5000 / quality_factor;
    else
        scale_factor = 200 - quality_factor * 2;
    /* Copy the quantization tables */
    /* We use the same quant table for Cb and Cr */
    bm_jpu_enc_copy_quantization_table(enc_open_par->qMatTab[DC_TABLE_INDEX0], mjpeg_enc_quantization_luma,   sizeof(mjpeg_enc_quantization_luma),   scale_factor); /* Y */
    bm_jpu_enc_copy_quantization_table(enc_open_par->qMatTab[AC_TABLE_INDEX0], mjpeg_enc_quantization_chroma, sizeof(mjpeg_enc_quantization_chroma), scale_factor); /* Cb */
    memcpy(enc_open_par->qMatTab[DC_TABLE_INDEX1], enc_open_par->qMatTab[DC_TABLE_INDEX0],    64);
    memcpy(enc_open_par->qMatTab[AC_TABLE_INDEX1], enc_open_par->qMatTab[AC_TABLE_INDEX0],    64);

    /* Copy the component info table (depends on the format) */
    switch (enc_open_par->sourceFormat)
    {
        case FORMAT_420: component_info_table = mjpeg_enc_component_info_tables[0]; break;
        case FORMAT_224:
            if((enc_open_par->rotationEnable == 1) && ((enc_open_par->rotationAngle == 90) || (enc_open_par->rotationAngle == 270))) {
                component_info_table = mjpeg_enc_component_info_tables[1]; break;
            }else{
                component_info_table = mjpeg_enc_component_info_tables[2]; break;
            }
        case FORMAT_422:
            if((enc_open_par->rotationEnable == 1) && ((enc_open_par->rotationAngle == 90) || (enc_open_par->rotationAngle == 270))) {
                component_info_table = mjpeg_enc_component_info_tables[2]; break;
            }else{
                component_info_table = mjpeg_enc_component_info_tables[1]; break;
            }
        case FORMAT_444: component_info_table = mjpeg_enc_component_info_tables[3]; break;
        case FORMAT_400: component_info_table = mjpeg_enc_component_info_tables[4]; break;
        default:
            BM_JPU_ERROR("bm_jpu_enc_set_mjpeg_tables the format(%d) not support.", enc_open_par->sourceFormat);
            return BM_JPU_ENC_RETURN_CODE_ERROR;
    }

    memcpy(enc_open_par->cInfoTab, component_info_table, 4 * 6);

    return BM_JPU_ENC_RETURN_CODE_OK;
}


char const * bm_jpu_enc_error_string(BmJpuEncReturnCodes code)
{
    switch (code)
    {
        case BM_JPU_ENC_RETURN_CODE_OK:                        return "ok";
        case BM_JPU_ENC_RETURN_CODE_ERROR:                     return "unspecified error";
        case BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS:            return "invalid params";
        case BM_JPU_ENC_RETURN_CODE_INVALID_HANDLE:            return "invalid handle";
        case BM_JPU_ENC_RETURN_CODE_INVALID_FRAMEBUFFER:       return "invalid framebuffer";
        case BM_JPU_ENC_RETURN_CODE_INSUFFICIENT_FRAMEBUFFERS: return "insufficient framebuffers";
        case BM_JPU_ENC_RETURN_CODE_INVALID_STRIDE:            return "invalid stride";
        case BM_JPU_ENC_RETURN_CODE_WRONG_CALL_SEQUENCE:       return "wrong call sequence";
        case BM_JPU_ENC_RETURN_CODE_TIMEOUT:                   return "timeout";
        case BM_JPU_ENC_RETURN_CODE_WRITE_CALLBACK_FAILED:     return "write callback failed";
        case BM_JPU_ENC_RETURN_ALLOC_MEM_ERROR:                return "error allocating memory";
        default: return "<unknown>";
    }
}


BmJpuEncReturnCodes bm_jpu_enc_load(int device_index)
{
    return bm_jpu_load(device_index) ? BM_JPU_ENC_RETURN_CODE_OK : BM_JPU_ENC_RETURN_CODE_ERROR;
}


BmJpuEncReturnCodes bm_jpu_enc_unload(int device_index)
{
    return bm_jpu_unload(device_index) ? BM_JPU_ENC_RETURN_CODE_OK : BM_JPU_ENC_RETURN_CODE_ERROR;
}

bm_handle_t bm_jpu_enc_get_bm_handle(int device_index)
{
    return bm_jpu_get_handle(device_index);
}

void bm_jpu_enc_get_bitstream_buffer_info(size_t *size, unsigned int *alignment)
{
    *size = JPU_ENC_BITSTREAM_BUFFER_SIZE;
    *alignment = JPU_MEMORY_ALIGNMENT;
}

void bm_jpu_enc_set_interrupt_timeout(BmJpuEncoder *encoder, int timeout, int count)
{
    if (encoder == NULL) {
        BM_JPU_ERROR("bm_jpu_enc_set_interrupt_timeout params err: encoder(0X%lx)", encoder);
        return;
    }

    encoder->timeout = timeout > 0 ? timeout : JPU_WAIT_TIMEOUT;
    encoder->timeout_count = count > 0 ? count : JPU_MAX_TIMEOUT_COUNTS;

    /*
    encoder->timeout is 0  first time set timeout
        timeout > 0   set timeout
        timeout <= 0  set deafult value
    encoder->timeout not 0
        timeout > 0   set timeout
        timeout <= 0  use  lasttime value
    */
    if (encoder->timeout <= 0) {
        encoder->timeout = timeout > 0 ? timeout : JPU_WAIT_TIMEOUT;
        encoder->timeout_count = count > 0 ? count : JPU_MAX_TIMEOUT_COUNTS;
    } else {
        if (timeout > 0) {
            encoder->timeout = timeout;
        }
        if (count > 0) {
            encoder->timeout_count = count;
        }
    }


}

BmJpuEncReturnCodes bm_jpu_enc_set_default_open_params(BmJpuEncOpenParams *open_params)
{
    if(open_params == NULL) {
        BM_JPU_ERROR("bm_jpu_enc_set_default_open_params params err: open_params(0X%lx).", open_params);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    open_params->frame_width = 0;
    open_params->frame_height = 0;
    open_params->color_format = BM_JPU_COLOR_FORMAT_YUV420;
    open_params->chroma_interleave = 0;

    open_params->quality_factor = 85;

    return BM_JPU_ENC_RETURN_CODE_OK;
}


BmJpuEncReturnCodes bm_jpu_enc_open(BmJpuEncoder **encoder,
                                    BmJpuEncOpenParams *open_params,
                                    bm_device_mem_t *bs_dma_buffer)
{
    BmJpuEncReturnCodes ret;
    EncOpenParam enc_open_param;
    int enc_ret;

    if((encoder == NULL) || (open_params == NULL) || (bs_dma_buffer == NULL)) {
        BM_JPU_ERROR("bm_jpu_enc_open params err: encoder(0X%lx), open_params(0X%lx), bs_dma_buffer(0X%lx).", encoder, open_params, bs_dma_buffer);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    /* Allocate encoder instance */
    *encoder = BM_JPU_ALLOC(sizeof(BmJpuEncoder));
    if ((*encoder) == NULL)
    {
        BM_JPU_ERROR("allocating memory for encoder object failed");
        return BM_JPU_ENC_RETURN_CODE_ERROR;
    }


    /* Set default encoder values */
    memset(*encoder, 0, sizeof(BmJpuEncoder));
    (*encoder)->device_index = open_params->device_index;
    memset(&enc_open_param, 0, sizeof(enc_open_param));

    // TODO
    /* Map the bitstream buffer. This mapping will persist until the encoder is closed.
     * In pcie mode, bs_virt_addr is used for a temporary bs buffer*/
    unsigned long long vmem = 0;
#ifndef BM_PCIE_MODE
    bm_handle_t handle = bm_jpu_get_handle(open_params->device_index);
    bm_mem_mmap_device_mem(handle, bs_dma_buffer, &vmem);
#endif
    (*encoder)->bs_virt_addr = (uint8_t*)vmem;
    // (*encoder)->bs_dma_buffer = bs_dma_buffer;
    (*encoder)->bs_dma_buffer = BM_JPU_ALLOC(sizeof(bm_device_mem_t));
    memcpy((*encoder)->bs_dma_buffer, bs_dma_buffer, sizeof(bm_device_mem_t));

    /* Fill in the bitstream buffer address and size. */
    enc_open_param.bitstreamBuffer = bs_dma_buffer->u.device.device_addr;
    enc_open_param.bitstreamBufferSize = bs_dma_buffer->size;

    /* Miscellaneous codec format independent values */
    enc_open_param.picWidth = open_params->frame_width;
    enc_open_param.picHeight = open_params->frame_height;
    enc_open_param.chromaInterleave = open_params->chroma_interleave;
    enc_open_param.packedFormat  = open_params->packed_format;
    enc_open_param.deviceIndex = open_params->device_index;

    enc_open_param.rotationEnable  = open_params->rotationEnable;
    enc_open_param.mirrorEnable    = open_params->mirrorEnable;
    enc_open_param.mirrorDirection = open_params->mirrorDirection;
    enc_open_param.rotationAngle   = open_params->rotationAngle;

    switch (open_params->color_format)
    {
    case BM_JPU_COLOR_FORMAT_YUV420:
        enc_open_param.sourceFormat = FORMAT_420;
        break;
    case BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL:
        enc_open_param.sourceFormat = FORMAT_422;
        break;
    case BM_JPU_COLOR_FORMAT_YUV422_VERTICAL:
        enc_open_param.sourceFormat = FORMAT_224;
        break;
    case BM_JPU_COLOR_FORMAT_YUV444:
        enc_open_param.sourceFormat = FORMAT_444;
        break;
    case BM_JPU_COLOR_FORMAT_YUV400:
        enc_open_param.sourceFormat = FORMAT_400;
        break;

    default:
        BM_JPU_ERROR("unknown color format value %d", open_params->color_format);
        ret = BM_JPU_DEC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    ret = bm_jpu_enc_set_mjpeg_tables(open_params->quality_factor, &(enc_open_param));
    if (ret != BM_JPU_ENC_RETURN_CODE_OK) {
        BM_JPU_ERROR("bm_jpu_enc_set_mjpeg_tables failed.");
        goto cleanup;
    }

    enc_open_param.restartInterval = 0;

    /* Now actually open the encoder instance */
    BM_JPU_LOG("opening encoder, frame size: %u x %u pixel",
               open_params->frame_width, open_params->frame_height);

    BM_JPU_LOG("picWidth         = %u", enc_open_param.picWidth);
    BM_JPU_LOG("picHeight        = %u", enc_open_param.picHeight);
    BM_JPU_LOG("sourceFormat     = %u", enc_open_param.sourceFormat);
    BM_JPU_LOG("chromaInterleave = %u", enc_open_param.chromaInterleave);
    BM_JPU_LOG("packedFormat     = %u", enc_open_param.packedFormat);
    BM_JPU_LOG("rgbPacked        = %u", enc_open_param.rgbPacked);
    BM_JPU_LOG("frameEndian      = %u", enc_open_param.frameEndian);
    BM_JPU_LOG("streamEndian     = %u", enc_open_param.streamEndian);
    BM_JPU_LOG("restartInterval  = %u", enc_open_param.restartInterval);

    if((*encoder)->handle == NULL)
    {
        (*encoder)->handle = BM_JPU_ALLOC(MAX_INST_HANDLE_SIZE);
    }
    memset( (*encoder)->handle , 0x00, MAX_INST_HANDLE_SIZE);

    enc_ret = jpu_EncOpen(&((*encoder)->handle), &enc_open_param);
    ret = BM_JPU_ENC_HANDLE_ERROR("could not open encoder", enc_ret);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        goto cleanup;


    /* Store some parameters internally for later use */
    (*encoder)->color_format = open_params->color_format;
    (*encoder)->frame_width  = open_params->frame_width;
    (*encoder)->frame_height = open_params->frame_height;

    bm_jpu_enc_set_interrupt_timeout(*encoder, open_params->timeout, open_params->timeout_count);

finish:
    if (ret == BM_JPU_ENC_RETURN_CODE_OK)
        BM_JPU_DEBUG("successfully opened encoder");

    return ret;

cleanup:
#ifndef BM_PCIE_MODE
    bm_mem_unmap_device_mem(handle, (*encoder)->bs_virt_addr, bs_dma_buffer->size);
#endif
    if((*encoder)->handle != NULL)
    {
        BM_JPU_FREE((*encoder)->handle, MAX_INST_HANDLE_SIZE);
        (*encoder)->handle = NULL;
    }
    BM_JPU_FREE((*encoder)->bs_dma_buffer, sizeof(bm_device_mem_t));
    BM_JPU_FREE(*encoder, sizeof(BmJpuEncoder));
    *encoder = NULL;

    goto finish;
}


BmJpuEncReturnCodes bm_jpu_enc_close(BmJpuEncoder *encoder)
{
    if (encoder == NULL)
        return BM_JPU_ENC_RETURN_CODE_OK;

    BM_JPU_DEBUG("closing encoder");

#if 0
    /* Close the encoder handle */

    enc_ret = jpu_EncClose(encoder->handle);
    if (enc_ret == JPG_RET_FRAME_NOT_COMPLETE)
    {
        /* JPU refused to close, since a frame is partially encoded.
         * Force it to close by first resetting the handle and retry. */
        jpu_SWReset(); // TODO
        enc_ret = jpu_EncClose(encoder->handle);
    }
    ret = BM_JPU_ENC_HANDLE_ERROR("error while closing encoder", enc_ret);
#endif

    /* Remaining cleanup */

    if (encoder->bs_dma_buffer != NULL) {
        if (encoder->bs_virt_addr != NULL) {
        #ifndef BM_PCIE_MODE
            bm_handle_t handle = bm_jpu_get_handle(encoder->device_index);
            bm_mem_unmap_device_mem(handle, encoder->bs_virt_addr, encoder->bs_dma_buffer->size);
        #endif
        }
        BM_JPU_FREE(encoder->bs_dma_buffer, sizeof(bm_device_mem_t));
    }

    if(encoder->handle != NULL)
         BM_JPU_FREE( (encoder->handle), MAX_INST_HANDLE_SIZE);

    BM_JPU_FREE(encoder, sizeof(BmJpuEncoder));
    return BM_JPU_ENC_RETURN_CODE_OK;
}


BmJpuEncReturnCodes bm_jpu_enc_get_initial_info(BmJpuEncoder *encoder,
                                                BmJpuEncInitialInfo *info)
{
    int enc_ret;
    BmJpuEncReturnCodes ret;
    EncInitialInfo initial_info;

    if((encoder == NULL) || (info == NULL)) {
        BM_JPU_ERROR("bm_jpu_enc_get_initial_info params err: encoder(0X%lx), info(0X%lx).", encoder, info);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    enc_ret = jpu_EncGetInitialInfo(encoder->handle, &initial_info);
    ret = BM_JPU_ENC_HANDLE_ERROR("could not get initial info", enc_ret);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        return ret;

    info->framebuffer_alignment = 1;
    info->min_num_required_framebuffers = initial_info.minFrameBufferCount;
    if (info->min_num_required_framebuffers == 0)
        info->min_num_required_framebuffers = 1;

    return BM_JPU_ENC_RETURN_CODE_OK;
}


BmJpuEncReturnCodes bm_jpu_enc_encode(BmJpuEncoder *encoder,
                                      BmJpuRawFrame const *raw_frame,
                                      BmJpuEncodedFrame *encoded_frame,
                                      BmJpuEncParams *encoding_params,
                                      unsigned int *output_code)
{
    BmJpuEncReturnCodes ret = BM_JPU_ENC_RETURN_CODE_OK;
    int enc_ret;
    int close_ret = JPG_RET_FAILURE;
    EncParam enc_param;
    EncOutputInfo enc_output_info;
    FrameBuffer source_framebuffer;
    BmJpuFramebuffer *framebuffer = raw_frame->framebuffer;
    bm_jpu_phys_addr_t raw_frame_phys_addr;
    int timeout;
    int pixels;
    size_t encoded_data_size;
    BmJpuEncWriteContext write_context;

    if((encoder == NULL) || (encoded_frame == NULL) || (encoding_params == NULL) || (output_code == NULL)) {
        BM_JPU_ERROR("bm_jpu_enc_encode params err: encoder(0X%lx), encoded_frame(0X%lx), encoding_params(0X%lx), output_code(0X%lx).", encoder, encoded_frame, encoding_params, output_code);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if(!(encoding_params->write_output_data != NULL || encoding_params->acquire_output_buffer != NULL) ||
       !(encoding_params->write_output_data != NULL || encoding_params->finish_output_buffer != NULL)) {
        BM_JPU_ERROR("bm_jpu_enc_encode params err: write_output_data(0X%lx), acquire_output_buffer(0X%lx), finish_output_buffer(0X%lx).", encoding_params->write_output_data, encoding_params->acquire_output_buffer, encoding_params->finish_output_buffer);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    *output_code = 0;
    memset(&write_context, 0, sizeof(write_context));

    /* Set this here to ensure that the handle is NULL if an error occurs
     * before acquire_output_buffer() is called */
    encoded_frame->acquired_handle = NULL;

    /* Get the physical address for the raw_frame that shall be encoded
     * and the virtual pointer to the output buffer */
    raw_frame_phys_addr = framebuffer->dma_buffer->u.device.device_addr;
    pixels = framebuffer->dma_buffer->size;
    /* MJPEG frames always need JPEG headers, since each frame is an independent JPEG frame. */
    /* JPEG header is written in jpu_EncStartOneFrame() */
    EncParamSet mjpeg_param;
    memset(&mjpeg_param, 0, sizeof(mjpeg_param));
    // TODO setting mjpeg_param
    jpu_EncGiveCommand(encoder->handle, ENC_JPG_GET_HEADER, &mjpeg_param);

    write_context.mjpeg_header_size = 0;

    *output_code |= BM_JPU_ENC_OUTPUT_CODE_CONTAINS_HEADER;

    BM_JPU_LOG("encoding raw_frame with physical address %" BM_JPU_PHYS_ADDR_FORMAT, raw_frame_phys_addr);

    /* Copy over information from the raw_frame's framebuffer into the
     * source_framebuffer structure, which is what jpu_EncStartOneFrame()
     * expects as input */

    memset(&source_framebuffer, 0, sizeof(source_framebuffer));
    source_framebuffer.strideY = framebuffer->y_stride;
    source_framebuffer.strideC = framebuffer->cbcr_stride;
    source_framebuffer.myIndex = 1;
    source_framebuffer.bufY  = (PhysicalAddress)(raw_frame_phys_addr + framebuffer->y_offset);
    source_framebuffer.bufCb = (PhysicalAddress)(raw_frame_phys_addr + framebuffer->cb_offset);
    source_framebuffer.bufCr = (PhysicalAddress)(raw_frame_phys_addr + framebuffer->cr_offset);

    BM_JPU_LOG("source framebuffer:  Y stride: %u  CbCr stride: %u",
               framebuffer->y_stride, framebuffer->cbcr_stride);

    /* Fill encoding parameters structure */
    memset(&enc_param, 0, sizeof(enc_param));
    enc_param.sourceFrame = &source_framebuffer;

    /* Do the actual encoding */
    enc_ret = jpu_EncStartOneFrame(encoder->handle, &enc_param);
    ret = BM_JPU_ENC_HANDLE_ERROR("could not start frame encoding", enc_ret);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        goto finish;

    /* Wait for frame completion */
    BM_JPU_LOG("waiting for encoding completion");

    timeout = jpu_EncWaitForInt(encoder->handle, encoder->timeout, encoder->timeout_count);

    /* If a timeout occurred earlier, this is the correct time to abort
     * encoding and return an error code, since jpu_EncGetOutputInfo()
     * has been called, unlocking the JPU encoder calls. */
    if (timeout)
    {
        BM_JPU_WARNING("timeout for jpeg encoding!");
        if(encoder->timeout < 15 * (pixels >> 20))
        {
            BM_JPU_WARNING("timeout %d ms may be too short, at least: %d ms", encoder->timeout, 15 * (pixels >> 20));
            jpu_EncGetOutputInfo(encoder->handle, &enc_output_info);
        }
        ret = BM_JPU_ENC_RETURN_CODE_TIMEOUT;
        goto finish;
    }

    /* Retrieve information about the result of the encode process. Do so even if
     * a timeout occurred. This is intentional, since according to the JPU docs,
     * jpu_EncStartOneFrame() won't be usable again until jpu_EncGetOutputInfo()
     * is called. In other words, the jpu_EncStartOneFrame() locks down some
     * internals inside the JPU, and jpu_EncGetOutputInfo() releases them. */
    memset(&enc_output_info, 0, sizeof(enc_output_info));
    enc_ret = jpu_EncGetOutputInfo(encoder->handle, &enc_output_info);
    ret = BM_JPU_ENC_HANDLE_ERROR("could not get output information", enc_ret);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        goto finish;

#ifdef __linux__
    const char* value = getenv("FLUSH_JPEG_BUFFER");
    if (value != NULL) {
        int intValue = atoi(value);
        if(intValue > 0){
            usleep(intValue);
        }
    } else {
        BM_JPU_LOG("FLUSH_JPEG_BUFFER is not set. if you need work 200 us buffer time, please set FLUSH_JPEG_BUFFER = %d\n", 200);
    }
#endif

    close_ret = jpu_EncClose(encoder->handle);

    BM_JPU_DEBUG("output info: bitstreamBuffer %" BM_JPU_PHYS_ADDR_FORMAT " bitstreamSize %u",
               enc_output_info.bitstreamBuffer, enc_output_info.bitstreamSize);

    // check jpeg header error (should be dealt with by userself now)
//     bm_handle = bm_jpu_get_handle(encoder->device_index);
//     dev_mem = bm_mem_from_device(enc_output_info.bitstreamBuffer, enc_output_info.bitstreamSize);
// #ifdef BM_PCIE_MODE
//     jpeg_virt_addr = BM_JPU_ALLOC(sizeof(uint8_t) * 8);
//     bm_memcpy_d2s_partial_offset(bm_handle, jpeg_virt_addr, dev_mem, sizeof(uint8_t) * 8, 0);
// #else
//     bm_mem_mmap_device_mem(bm_handle, &dev_mem, &vaddr);
//     jpeg_virt_addr = (uint8_t *)vaddr;
// #endif

//     if (jpeg_virt_addr[0] != 0xff || jpeg_virt_addr[1] != 0xd8 || jpeg_virt_addr[2] != 0xff) {
//         BM_JPU_ERROR("error jpeg_virt_addr[0]=%x,jpeg_virt_addr[1]=%x,jpeg_virt_addr[2]=%x,size=%d\n",jpeg_virt_addr[0],jpeg_virt_addr[1],jpeg_virt_addr[2],enc_output_info.bitstreamSize);
//     #ifdef BM_PCIE_MODE
//         free(jpeg_virt_addr);
//     #else
//         bm_mem_unmap_device_mem(bm_handle, jpeg_virt_addr, enc_output_info.bitstreamSize);
//     #endif
//         bm_jpu_hwreset_all();
//         return BM_JPU_ENC_BYTE_ERROR;
//     } else {
//     #ifdef BM_PCIE_MODE
//         free(jpeg_virt_addr);
//     #else
//         bm_mem_unmap_device_mem(bm_handle, jpeg_virt_addr, enc_output_info.bitstreamSize);
//     #endif
//     }

    encoded_data_size = enc_output_info.bitstreamSize;
    encoded_data_size += write_context.mjpeg_header_size;

    /* Since the encoder does not perform any kind of delay
     * or reordering, this is appropriate, because in that
     * case, one input frame always immediately leads to
     * one output frame */
    encoded_frame->context = raw_frame->context;
    encoded_frame->pts = raw_frame->pts;
    encoded_frame->dts = raw_frame->dts;

    encoded_frame->data_size = encoded_data_size;

    if (encoding_params->write_output_data == NULL)
    {
        if(encoding_params->bs_in_device == 1){
            //buffer in device
            write_context.bs_buffer = encoding_params->acquire_output_buffer(encoding_params->output_buffer_context,
                                                                              encoded_data_size,
                                                                              &(encoded_frame->acquired_handle));
        }
        else{

            //buffer in host
            write_context.write_ptr_start = encoding_params->acquire_output_buffer(encoding_params->output_buffer_context,
                                                                                   encoded_data_size,
                                                                                   &(encoded_frame->acquired_handle));
            if (write_context.write_ptr_start == NULL)
            {
                BM_JPU_ERROR("could not acquire buffer with %zu byte for encoded frame data", encoded_data_size);
                ret = BM_JPU_ENC_RETURN_CODE_ERROR;
                goto finish;
            }

            write_context.write_ptr = write_context.write_ptr_start;
            write_context.write_ptr_end = write_context.write_ptr + encoded_data_size;
        }
    }

    /* Add header data if necessary */
    *output_code |= BM_JPU_ENC_OUTPUT_CODE_CONTAINS_HEADER; // TODO

    /* Add this flag since the raw frame has been successfully consumed */
    *output_code |= BM_JPU_ENC_OUTPUT_CODE_INPUT_USED;

    /* Get the encoded data out of the bitstream buffer into the output buffer */
    if (enc_output_info.bitstreamBuffer != 0)
    {
        bm_handle_t handle = bm_jpu_get_handle(encoder->device_index);

        if(encoding_params->bs_in_device == 1)
        {
            //bm_jpu_phys_addr_t  jpu_bs_phy_address = bm_jpu_dma_buffer_get_physical_address(encoder->bs_dma_buffer);
            //bm_device_mem_t devcie_mem = bm_mem_from_device(jpu_bs_phy_address, encoded_data_size);
            bm_memcpy_d2d_byte(handle,*((bm_device_mem_t *)write_context.bs_buffer), 0, *encoder->bs_dma_buffer, 0, encoded_data_size);
            // bm_dev_free(handle);
            *output_code |= BM_JPU_ENC_OUTPUT_CODE_ENCODED_FRAME_AVAILABLE;
            goto finish;
        }
        if (encoding_params->write_output_data == NULL)
        {
            ptrdiff_t available_space = write_context.write_ptr_end - write_context.write_ptr;

            if (available_space < (ptrdiff_t)(enc_output_info.bitstreamSize))
            {
                BM_JPU_ERROR("insufficient space in output buffer for encoded data: need %u byte, got %td",
                             enc_output_info.bitstreamSize,
                             available_space);
                ret = BM_JPU_ENC_RETURN_BS_BUFFER_FULL;

                goto finish;
            }
#ifdef BM_PCIE_MODE
            bm_memcpy_d2s_partial_offset(handle, write_context.write_ptr, *(encoder->bs_dma_buffer), enc_output_info.bitstreamSize, 0);
#else
            bm_mem_invalidate_device_mem(handle, encoder->bs_dma_buffer);
            memcpy(write_context.write_ptr, encoder->bs_virt_addr, enc_output_info.bitstreamSize);
#endif
            write_context.write_ptr += enc_output_info.bitstreamSize;
        }
        else
        {
#ifdef BM_PCIE_MODE
            encoder->bs_virt_addr = BM_JPU_ALLOC(enc_output_info.bitstreamSize);
            if(encoder->bs_virt_addr != NULL)
            {
                bm_memcpy_d2s_partial(handle, encoder->bs_virt_addr, *(encoder->bs_dma_buffer), enc_output_info.bitstreamSize);
            }
            else
            {
                BM_JPU_ERROR("could not alloc mem  %u bytes", enc_output_info.bitstreamSize);
                ret = BM_JPU_ENC_RETURN_ALLOC_MEM_ERROR;
                goto finish;
            }
#else
            bm_mem_invalidate_device_mem(handle, encoder->bs_dma_buffer);
#endif
            if (encoding_params->write_output_data(encoding_params->output_buffer_context,
                                                   encoder->bs_virt_addr,
                                                   enc_output_info.bitstreamSize,
                                                   encoded_frame) < 0)
            {
                BM_JPU_ERROR("could not output encoded data with %u bytes: write callback reported failure",
                             enc_output_info.bitstreamSize);
                ret = BM_JPU_ENC_RETURN_CODE_WRITE_CALLBACK_FAILED;
                goto finish;
            }
        }

        BM_JPU_LOG("added main encoded frame data with %u byte", enc_output_info.bitstreamSize);
        *output_code |= BM_JPU_ENC_OUTPUT_CODE_ENCODED_FRAME_AVAILABLE;
    }

finish:

#ifdef BM_PCIE_MODE
    if(encoder->bs_virt_addr != NULL)
    {
        free(encoder->bs_virt_addr);
        encoder->bs_virt_addr = NULL;
    }
#endif

    if (write_context.write_ptr_start != NULL)
    {
        encoding_params->finish_output_buffer(encoding_params->output_buffer_context, encoded_frame->acquired_handle);
    }

    if(close_ret != JPG_RET_SUCCESS)
    {
        BM_JPU_DEBUG("close_ret=%d",close_ret);
        enc_ret = jpu_EncClose(encoder->handle);
    }
    return ret;
}

int bm_jpu_hwreset()
{
    return jpu_HWReset();
}

#ifndef _WIN32
int bm_jpu_get_dump()
{
    int ret =  jpu_GetDump();
    return ret;
}
#endif
