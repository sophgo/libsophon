#include <stdarg.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <time.h>
#include <windows.h>
#include <io.h>
#include <shlwapi.h>
#endif

#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#ifndef USING_CMODEL
#include "vpplib.h"
#else
#define STRIDE_ALIGN  (64)
#endif

#ifndef __linux__
#define STRIDE_ALIGN (64)
#endif

#if __linux__
#include <dlfcn.h>
#endif
#ifdef _WIN32
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

#include <map>
#define FIRMWARE_NAME "libbm1684x_kernel_module.so"

static std::map<std::pair<bm_image_format_ext, bm_image_format_ext>, vpp_limitation> vpp_format_allowed_map = {
    /* RGBP_SEPERATE i.e RGB_PALANAR with seperate address for each plane */
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 0, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 0, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 0, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 0, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    ////////////////////////////////////////////////////////
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_GRAY, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PACKED, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PACKED, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGB_PLANAR, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGR_PLANAR, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_RGBP_SEPARATE, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_BGRP_SEPARATE, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ARGB_PACKED, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_ABGR_PACKED, FORMAT_GRAY), {32, 32, {STRIDE_ALIGN, 1, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV420P, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1},  {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1},  {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_NV12, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, 1, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1}},

    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 2, 2, 2, 2, 1, 0}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_YUV444P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_YUV444P, FORMAT_YUV422P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0}},

    //////////
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_ARGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_ABGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_RGB_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_BGR_PACKED), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_RGB_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_BGR_PLANAR), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, 1, 1, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_RGBP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_BGRP_SEPARATE), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}},
    {std::pair<bm_image_format_ext, bm_image_format_ext>(FORMAT_COMPRESSED, FORMAT_YUV420P), {32, 32, {STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN, STRIDE_ALIGN}, {STRIDE_ALIGN, STRIDE_ALIGN/2, STRIDE_ALIGN/2, 1}, 4096, 16, 4096, 16, 32, 32, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1}},

};


static bm_status_t bm_image_get_heap_id(bm_image image, int *heap_location) {
    bm_device_mem_t dev_mem[3];
    bm_image_get_device_mem(image, dev_mem);
    for (int i = 0; i < bm_image_get_plane_num(image); i++) {
        u64 dev_addr = bm_mem_get_device_addr(dev_mem[i]);
        if ((dev_addr >= 0x100000000) && (dev_addr < 0x300000000)) {
            heap_location[i] = BMCV_HEAP0_ID;
        } else {
            heap_location[i] = BMCV_HEAP1_ID;
        }
    }

    return BM_SUCCESS;
}

const char *bm_get_bmcv_version() {
    return BMCV_VERSION;
}

bm_status_t bm_vpp_query_limitation(bm_image_format_ext input_format,
                                    bm_image_format_ext output_format,
                                    vpp_limitation &limit)
{
    bm_status_t ret = BM_SUCCESS;
    auto key = std::pair<bm_image_format_ext, bm_image_format_ext>
                (input_format, output_format);

    if(vpp_format_allowed_map.find(key) == vpp_format_allowed_map.end())
        ret = BM_NOT_SUPPORTED;
    else
        limit = vpp_format_allowed_map[key];

    return ret;
}

void bmcv_err_log_internal(char *      log_buffer,
                           size_t      string_sz,
                           const char *frmt,
                           ...) {
    va_list args;
    va_start(args, frmt);
    vsnprintf(log_buffer + string_sz, MAX_BMCV_LOG_BUFFER_SIZE, frmt, args);
    va_end(args);
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "%s", log_buffer);
}

bm_status_t concat_images_to_tensor(bm_handle_t      handle,
                                    int              image_num,
                                    bm_image *       images,
                                    bm_image_tensor *image_tensor) {

    if (bm_image_get_plane_num(images[0]) != 1) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "NOT supported image format\n");
        return BM_NOT_SUPPORTED;
    }
    bm_device_mem_t first_dev_mem;
    bm_image_get_device_mem(images[0], &first_dev_mem);
    u64 last_dev_addr = bm_mem_get_device_addr(first_dev_mem);
    for (int i = 0; i < image_num - 1; i++) {
        int last_image_size = 0;
        bm_image_get_byte_size(images[i], &last_image_size);
        bm_device_mem_t dev_mem;
        bm_image_get_device_mem(images[i + 1], &dev_mem);
        u64 dev_addr = bm_mem_get_device_addr(dev_mem);
        if (dev_addr != last_dev_addr + last_image_size) {
            return BM_ERR_FAILURE;
        }
        if ((images[i].width != images[i + 1].width) ||
            (images[i].height != images[i + 1].height)) {
            return BM_ERR_FAILURE;
        }
        last_dev_addr = dev_addr;
    }
    int image_channel = (images[0].image_format == FORMAT_GRAY) ? (1) : (3);
    bm_image_tensor_create( \
            handle,
            (images[0].data_type == DATA_TYPE_EXT_4N_BYTE) ? (image_num * 4) : (image_num),
            image_channel,
            images[0].height,
            images[0].width,
            images[0].image_format,
            images[0].data_type,
            image_tensor);
    first_dev_mem.size = first_dev_mem.size * image_num;
    bm_image_tensor_attach(*image_tensor, &first_dev_mem);

    return BM_SUCCESS;
}

bm_status_t bm_image_mem_layout_adjust(bm_handle_t handle,
                                       bm_image    input_image,
                                       bm_image    output_image) {
    // keep same heap location with before
    if (!bm_image_is_attached(input_image)) {
        BMCV_ERR_LOG("image should be attached memory firstly\n");

        return BM_ERR_FAILURE;
    }
    // check and alloc output mem
    int input_heap_id[3], output_heap_id[3];
    bm_image_get_heap_id(input_image, input_heap_id);
    int plane_num = bm_image_get_plane_num(input_image);
    // check if all planes are in same heap
    for (int i = 1; i < plane_num; i++) {
        if (input_heap_id[i] != input_heap_id[i - 1]) {
            BMCV_ERR_LOG("all planes should be in same heap\n");

            return BM_ERR_FAILURE;
        }
    }
    if (bm_image_is_attached(output_image)) {
        bm_image_get_heap_id(output_image, output_heap_id);
        // check if all planes are in same heap
        for (int i = 1; i < plane_num; i++) {
            if (output_heap_id[i] != output_heap_id[i - 1]) {
                BMCV_ERR_LOG("all planes should be in same heap\n");

                return BM_ERR_FAILURE;
            }
        }
        if (input_heap_id[0] != output_heap_id[0]) {
            BMCV_ERR_LOG("output and input should be allocated in same heap\n");

            return BM_ERR_FAILURE;
        }
    } else {
        // keep same heap location with input
        if (BM_SUCCESS != bm_image_alloc_dev_mem(output_image, input_heap_id[0])) {
            BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");

            return BM_ERR_FAILURE;
        }
    }
    // do copyTo
    bmcv_copy_to_atrr_t copy_to_attr;
    copy_to_attr.start_x    = 0;
    copy_to_attr.start_y    = 0;
    copy_to_attr.padding_b  = 0;
    copy_to_attr.padding_g  = 0;
    copy_to_attr.padding_r  = 0;
    copy_to_attr.if_padding = 1;
    if (BM_SUCCESS !=
        bmcv_image_copy_to(handle, copy_to_attr, input_image, output_image)) {
        BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
        bm_device_mem_t dev_mem[3];
        bm_image_get_device_mem(output_image, dev_mem);
        for (int i = 0; i < plane_num; i++) {
            bm_free_device(handle, dev_mem[i]);
        }

        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

int find_tpufirmaware_path(char fw_path[512], const char* path){
    char* ptr;
    int dirname_len;
    int ret = 0;
#ifdef __linux__
    Dl_info dl_info;
    const char* path1 = "/opt/sophon/libsophon-current/lib/tpu_module/";

    /* 1.test ./libbm1684x_kernel_module.so */
    ret = access(path, F_OK);
    if (ret == 0){
        memset(fw_path, 0, 512);
        strcpy(fw_path, path);
        return ret;
    }

    /* 2. test /system/data/lib/vpu_firmware/chagall.bin */
    memset(fw_path, 0, 512);
    strcpy(fw_path, path1);
    strcat(fw_path, path);
    ret = access(fw_path, F_OK);
    if (ret == 0)
    {
        return ret;
    }

    /* 3.test libbmcv_so_path/libbm1684x_kernel_module.so */
    ret = dladdr((void*)find_tpufirmaware_path, &dl_info);
    if (ret == 0){
        printf("dladdr() failed: %s\n", dlerror());
        return -1;
    }
    if (dl_info.dli_fname == NULL){
        printf("%s is NOT a symbol\n", __FUNCTION__);
        return -1;
    }

    ptr = (char*)strrchr(dl_info.dli_fname, '/');
    if (!ptr){
        printf("Invalid absolute path name of libbmvpu.so\n");
        return -1;
    }

    dirname_len = ptr - dl_info.dli_fname + 1;
    if (dirname_len <= 0){
        printf("Invalid length of folder name\n");
        return -1;
    }

    memset(fw_path, 0, 512);
    strncpy(fw_path, dl_info.dli_fname, dirname_len);
    strcat(fw_path, "tpu_module/");
    strcat(fw_path, path);

    ret = access(fw_path, F_OK);
    return ret;
#endif

#ifdef _WIN32
    strcpy(fw_path, ".\\libbm1684x_kernel_module.so");

    /* 1. test ./libbm1684x_kernel_module.so */
    ret = _access(fw_path, 0);
    if (ret == 0){
        return ret;
    }

    /* 2. test libbmcv_so_path/libbm1684x_kernel_module.so */
    LPTSTR  strDLLPath1 = (char*)malloc(512);
    GetModuleFileName((HINSTANCE)&__ImageBase, strDLLPath1, _MAX_PATH);

    ptr = strrchr(strDLLPath1, '\\');

    if (!ptr){
        printf("Invalid absolute path name of libbmvpu.so\n");
        return -1;
    }

    dirname_len = strlen(strDLLPath1) - strlen(ptr) + 1;
    if (dirname_len <= 0)
    {
        printf("Invalid length of folder name\n");
        return -1;
    }
    memset(fw_path, 0, 512);
    strncpy(fw_path, strDLLPath1, dirname_len);
    strcat(fw_path, "tpu_module\\");
    strcat(fw_path, path);

    free(strDLLPath1);
    return ret;
#endif
}

bm_status_t bm_load_tpu_module(bm_handle_t handle, tpu_kernel_module_t *tpu_module){
    const  char* fw_fname = FIRMWARE_NAME;
    static char fw_path[512] = {0};
    static bool first = true;
    if(first){
        if(0 != find_tpufirmaware_path(fw_path, fw_fname)){
            printf("libbm1684x_kernel_module.so does not exist\n");
            return BM_ERR_FAILURE;
        }
        first = false;
    }
    int key_size = strlen(FIRMWARE_NAME);
    *tpu_module = tpu_kernel_load_module_file_key(handle,
                                                 fw_path,
                                                 FIRMWARE_NAME,
                                                 key_size);
    return BM_SUCCESS;
}

bm_status_t bm_tpu_kernel_launch(bm_handle_t handle,
                         const char *func_name,
                         void       *param,
                         size_t      size){
    tpu_kernel_module_t tpu_module = NULL;
    tpu_kernel_function_t func_id = 0;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_load_tpu_module(handle, &tpu_module);
    if(ret != BM_SUCCESS){
        printf("module load error! \n");
        return BM_ERR_FAILURE;
    }
    func_id = tpu_kernel_get_function(handle, tpu_module, (char*)func_name);
    ret = tpu_kernel_launch(handle, func_id, param, size);
    return ret;
}

bm_status_t bm_kernel_main_launch(bm_handle_t handle, int api_id, void *param, size_t size)
{
    bm_status_t ret = BM_SUCCESS;
    size_t full_size = sizeof(dynamic_load_param) + size;
    dynamic_load_param *full_param = reinterpret_cast<dynamic_load_param*>(new char[full_size]);
    full_param->api_id = api_id;
    full_param->size = size;
    memcpy(full_param->param, param, size);

    tpu_kernel_module_t tpu_module = NULL;
    tpu_kernel_function_t func_id = 0;
    ret = bm_load_tpu_module(handle, &tpu_module);
    if(ret != BM_SUCCESS){
        printf("module load error! \n");
        delete [] full_param;
        return BM_ERR_FAILURE;
    }
    func_id = tpu_kernel_get_function(handle, tpu_module, "tpu_kernel_main");
    ret = tpu_kernel_launch(handle, func_id, full_param, full_size);
    delete [] full_param;
    return ret;
}

bm_status_t bm_shape_align(bm_image  image,
                           bm_image *out_image,
                           int       align_option,
                           int       align_num) {
    if (bm_image_is_attached(*out_image)) {
        BMCV_ERR_LOG("out_image should not be attached firstly\n");

        return BM_ERR_FAILURE;
    }
    int      plane_num = bm_image_get_plane_num(image);
    #ifdef __linux__
    bm_image tmp_in_image[plane_num], tmp_out_image[plane_num];
    #else
    std::shared_ptr<bm_image> tmp_in_image_(new bm_image[plane_num], std::default_delete<bm_image[]>());
    bm_image*                 tmp_in_image = tmp_in_image_.get();
    std::shared_ptr<bm_image> tmp_out_image_(new bm_image[plane_num], std::default_delete<bm_image[]>());
    bm_image*                 tmp_out_image = tmp_out_image_.get();
    #endif
    int      width[3]          = {0};
    int      height[3]         = {0};
    int      aligned_width[3]  = {0};
    int      aligned_height[3] = {0};
    // get width,height
    switch (image.image_format) {
        case FORMAT_YUV420P: {
            width[0]  = image.width;
            width[1]  = ALIGN(image.width, 2) / 2;
            width[2]  = width[1];
            height[0] = image.height;
            height[1] = ALIGN(image.height, 2) / 2;
            height[2] = height[1];
            break;
        }
        case FORMAT_YUV422P: {
            width[0]  = image.width;
            width[1]  = ALIGN(image.width, 2) / 2;
            width[2]  = width[1];
            height[0] = image.height;
            height[1] = height[0];
            height[2] = height[1];
            break;
        }
        case FORMAT_YUV444P: {
            width[0]  = image.width;
            width[1]  = width[0];
            width[2]  = width[1];
            height[0] = image.height;
            height[1] = height[0];
            height[2] = height[1];
            break;
        }
        case FORMAT_NV24: {
            width[0]  = image.width;
            width[1]  = width[0];
            height[0] = image.height;
            height[1] = height[0];
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            width[0]  = image.width;
            width[1]  = ALIGN(image.width, 2);
            height[0] = image.height;
            height[1] = ALIGN(image.height, 2) / 2;
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            width[0]  = image.width;
            width[1]  = ALIGN(image.width, 2);
            height[0] = image.height;
            height[1] = image.height;
            break;
        }
        case FORMAT_GRAY: {
            width[0]  = image.width;
            height[0] = image.height;
            break;
        }
        case FORMAT_COMPRESSED: {
            width[0]  = image.width;
            height[0] = image.height;
            break;
        }
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            width[0]  = image.width * 3;
            height[0] = image.height;
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            width[0]  = image.width;
            height[0] = image.height * 3;
            break;
        }
        default: {
            BMCV_ERR_LOG("image format not support\n");

            return BM_ERR_FAILURE;
        }
    }
    int width_align = (align_option == DO_HEIGHT_ALIGN) ? (width[0]) : (ALIGN(width[0], align_num));
    int height_align = (align_option == DO_WITH_ALIGN) ? (height[0]) : (ALIGN(height[0], align_num));
    switch (image.image_format) {
        case FORMAT_YUV420P: {
            aligned_width[0]  = width_align;
            aligned_width[1]  = ALIGN(width_align, 2) / 2;
            aligned_width[2]  = aligned_width[1];
            aligned_height[0] = height_align;
            aligned_height[1] = ALIGN(height_align, 2) / 2;
            aligned_height[2] = aligned_height[1];
            break;
        }
        case FORMAT_YUV422P: {
            aligned_width[0]  = width_align;
            aligned_width[1]  = ALIGN(width_align, 2) / 2;
            aligned_width[2]  = aligned_width[1];
            aligned_height[0] = height_align;
            aligned_height[1] = aligned_height[0];
            aligned_height[2] = aligned_height[1];
            break;
        }
        case FORMAT_YUV444P: {
            aligned_width[0]  = width_align;
            aligned_width[1]  = aligned_width[0];
            aligned_width[2]  = aligned_width[1];
            aligned_height[0] = height_align;
            aligned_height[1] = aligned_height[0];
            aligned_height[2] = aligned_height[1];
            break;
        }
        case FORMAT_NV24: {
            aligned_width[0]  = width_align;
            aligned_width[1]  = aligned_width[0];
            aligned_height[0] = height_align;
            aligned_height[1] = aligned_height[0];
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            aligned_width[0]  = width_align;
            aligned_width[1]  = ALIGN(width_align, 2);
            aligned_height[0] = height_align;
            aligned_height[1] = ALIGN(height_align, 2) / 2;
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            aligned_width[0]  = width_align;
            aligned_width[1]  = ALIGN(width_align, 2);
            aligned_height[0] = height_align;
            aligned_height[1] = height_align;
            break;
        }
        case FORMAT_GRAY: {
            aligned_width[0]  = width_align;
            aligned_height[0] = height_align;
            break;
        }
        case FORMAT_COMPRESSED: {
            aligned_width[0]  = width_align;
            aligned_height[0] = height_align;
            break;
        }
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            aligned_width[0]  = width_align * 3;
            aligned_height[0] = height_align;
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            aligned_width[0]  = width_align;
            aligned_height[0] = height_align * 3;
            break;
        }
        default: {
            BMCV_ERR_LOG("image format not support\n");

            return BM_ERR_FAILURE;
        }
    }
    #ifdef __linux__
    bm_device_mem_t dev_mem[plane_num];
    bm_device_mem_t out_dev_mem[plane_num];
    #else
    std::shared_ptr<bm_device_mem_t> dev_mem_(new bm_device_mem_t[plane_num], std::default_delete<bm_device_mem_t[]>());
    bm_device_mem_t*                 dev_mem = dev_mem_.get();
    std::shared_ptr<bm_device_mem_t> out_dev_mem_(new bm_device_mem_t[plane_num], std::default_delete<bm_device_mem_t[]>());
    bm_device_mem_t*                 out_dev_mem = out_dev_mem_.get();
    #endif
    bm_image_get_device_mem(image, dev_mem);
    bm_handle_t handle = bm_image_get_handle(&image);
    bm_image_destroy(*out_image);
    bm_image_create(handle,
                    height_align,
                    width_align,
                    image.image_format,
                    image.data_type,
                    out_image);
    int input_heap_id[3] = {0};
    bm_image_get_heap_id(image, input_heap_id);
    if (BM_SUCCESS != bm_image_alloc_dev_mem(*out_image, input_heap_id[0])) {
        BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");

        return BM_ERR_FAILURE;
    }
    bm_image_get_device_mem(*out_image, out_dev_mem);
    for (int i = 0; i < plane_num; i++) {
        // create input image of gray
        if (BM_SUCCESS != bm_image_create(handle,
                                          height[i],
                                          width[i],
                                          FORMAT_GRAY,
                                          DATA_TYPE_EXT_1N_BYTE,
                                          &tmp_in_image[i])) {
            BMCV_ERR_LOG("bm_image_create error\n");
            for (int free_idx = 0; free_idx < i; free_idx++) {
                bm_image_destroy(tmp_in_image[free_idx]);
                bm_image_destroy(tmp_out_image[free_idx]);
            }
            bm_image_destroy(*out_image);

            return BM_ERR_FAILURE;
        }
        bm_image_attach(tmp_in_image[i], &dev_mem[i]);
        // create output image of gray
        if (BM_SUCCESS != bm_image_create(handle,
                                          aligned_height[i],
                                          aligned_width[i],
                                          FORMAT_GRAY,
                                          DATA_TYPE_EXT_1N_BYTE,
                                          &tmp_out_image[i])) {
            BMCV_ERR_LOG("bm_image_create error\n");
            for (int free_idx = 0; free_idx <= i; free_idx++) {
                bm_image_destroy(tmp_in_image[free_idx]);
            }
            for (int free_idx = 0; free_idx <= i; free_idx++) {
                bm_image_destroy(tmp_out_image[free_idx]);
            }
            bm_image_destroy(*out_image);

            return BM_ERR_FAILURE;
        }
        bm_image_attach(tmp_out_image[i], &out_dev_mem[i]);
        if (1 != bm_image_get_plane_num(tmp_out_image[i])) {
            BMCV_ERR_LOG("image plane error\n");
            for (int free_idx = 0; free_idx <= i; free_idx++) {
                bm_image_destroy(tmp_in_image[free_idx]);
                bm_image_destroy(tmp_out_image[free_idx]);
            }
            bm_image_destroy(*out_image);

            return BM_ERR_FAILURE;
        }
        // change layout
        if (BM_SUCCESS != bm_image_mem_layout_adjust(
                              handle, tmp_in_image[i], tmp_out_image[i])) {
            BMCV_ERR_LOG("image format not support\n");
            for (int free_idx = 0; free_idx <= i; free_idx++) {
                bm_image_destroy(tmp_in_image[free_idx]);
                bm_image_destroy(tmp_out_image[free_idx]);
            }
            bm_image_destroy(*out_image);

            return BM_ERR_FAILURE;
        }
    }
    for (int free_idx = 0; free_idx < plane_num; free_idx++) {
        bm_image_destroy(tmp_in_image[free_idx]);
        bm_image_destroy(tmp_out_image[free_idx]);
    }

    return BM_SUCCESS;
}

bm_status_t bm_shape_dealign(bm_image in_image,
                             bm_image out_image,
                             int      align_option) {
    if (align_option == DO_HEIGHT_ALIGN) {
        int             plane_num = bm_image_get_plane_num(in_image);
        #ifdef __linux__
        bm_device_mem_t in_dev_mem[plane_num];
        bm_device_mem_t out_dev_mem[plane_num];
        #else
        std::shared_ptr<bm_device_mem_t> in_dev_mem_(new bm_device_mem_t[plane_num], std::default_delete<bm_device_mem_t[]>());
        bm_device_mem_t*                 in_dev_mem = in_dev_mem_.get();
        std::shared_ptr<bm_device_mem_t> out_dev_mem_(new bm_device_mem_t[plane_num], std::default_delete<bm_device_mem_t[]>());
        bm_device_mem_t*                 out_dev_mem = out_dev_mem_.get();
        #endif
        bm_image_get_device_mem(in_image, in_dev_mem);
        bm_image_get_device_mem(out_image, out_dev_mem);
        #ifdef __linux__
        int out_plane_size[plane_num];
        #else
        std::shared_ptr<int> out_plane_size_(new int[plane_num], std::default_delete<int[]>());
        int*                 out_plane_size = out_plane_size_.get();
        #endif
        bm_image_get_byte_size(out_image, out_plane_size);
        bm_handle_t handle = bm_image_get_handle(&in_image);
        for (int i = 0; i < plane_num; i++) {
            if (BM_SUCCESS != bm_memcpy_d2d_byte(handle,
                                                 out_dev_mem[i],
                                                 0,
                                                 in_dev_mem[i],
                                                 0,
                                                 out_plane_size[i])) {
                BMCV_ERR_LOG("bm_memcpy_d2d_byte error\n");

                return BM_ERR_FAILURE;
            }
        }
    } else {
        BMCV_ERR_LOG("align_option  not support\n");

        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bm_yuv_height_align(bm_image  input_image,
                                bm_image *output_image,
                                int &     if_yuv_input) {
    if (input_image.image_format == FORMAT_NV12 ||
        input_image.image_format == FORMAT_NV16 ||
        input_image.image_format == FORMAT_NV21 ||
        input_image.image_format == FORMAT_YUV420P ||
        input_image.image_format == FORMAT_NV61) {
        bm_shape_align(input_image, output_image);
        if_yuv_input = 1;
    } else {
        *output_image = input_image;
        if_yuv_input  = 0;
    }

    return BM_SUCCESS;
}

bm_status_t bm_separate_to_planar(bm_handle_t handle,
                                  bm_image input_image,
                                  bm_image output_image) {
    if (input_image.image_format != FORMAT_BGRP_SEPARATE &&
        input_image.image_format != FORMAT_RGBP_SEPARATE) {
        BMCV_ERR_LOG("bm_separate_to_planar input should be FORMAT_BGRP_SEPARATE or FORMAT_RGBP_SEPARATE\n");
        return BM_ERR_FAILURE;
    }
    if (output_image.image_format != FORMAT_BGR_PLANAR &&
        output_image.image_format != FORMAT_RGB_PLANAR) {
        BMCV_ERR_LOG("bm_separate_to_planar output should be FORMAT_BGR_PLANAR or FORMAT_RGB_PLANAR\n");
        return BM_ERR_FAILURE;
    }
    if ((input_image.data_type != output_image.data_type) ||
        (input_image.width != output_image.width) ||
        (input_image.height != output_image.height)) {
        BMCV_ERR_LOG("bm_separate_to_planar input and output should be same data_type, width and height\n");
        return BM_ERR_FAILURE;
    }
    int stride_i[3];
    int stride_o[3];
    bm_image_get_stride(input_image, stride_i);
    bm_image_get_stride(output_image, stride_o);
    for (int k = 0; k < bm_image_get_plane_num(output_image); k++) {
        if(stride_i[k] != stride_o[k]) {
            BMCV_ERR_LOG("bm_separate_to_planar input and output should be same stride!\n");
            return BM_ERR_FAILURE;
        }
    }
    int height = input_image.height;
    if(!bm_image_is_attached(output_image)) {
        if(BM_SUCCESS != bm_image_alloc_dev_mem(output_image, BMCV_HEAP_ANY)) {
            BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t mem_i[3], mem_o;
    bm_image_get_device_mem(input_image, mem_i);
    bm_image_get_device_mem(output_image, &mem_o);
    for (int i = 0; i < 3; i++) {
        bm_memcpy_d2d_byte(handle, mem_o, stride_i[0] * height * i, mem_i[i], 0, stride_i[0] * height);
    }
    return BM_SUCCESS;
}

bm_status_t bm_planar_to_separate(bm_handle_t handle,
                                  bm_image input_image,
                                  bm_image output_image) {
    if (input_image.image_format != FORMAT_BGR_PLANAR &&
        input_image.image_format != FORMAT_RGB_PLANAR) {
        BMCV_ERR_LOG("bm_separate_to_planar input should be FORMAT_BGR_PLANAR or FORMAT_RGB_PLANAR\n");
        return BM_ERR_FAILURE;
    }
    if (output_image.image_format != FORMAT_BGRP_SEPARATE &&
        output_image.image_format != FORMAT_RGBP_SEPARATE) {
        BMCV_ERR_LOG("bm_separate_to_planar output should be FORMAT_BGRP_SEPARATE or FORMAT_RGBP_SEPARATE\n");
        return BM_ERR_FAILURE;
    }
    if ((input_image.data_type != output_image.data_type) ||
        (input_image.width != output_image.width) ||
        (input_image.height != output_image.height)) {
        BMCV_ERR_LOG("bm_separate_to_planar input and output should be same data_type, width and height\n");
        return BM_ERR_FAILURE;
    }
    int stride_i[3];
    int stride_o[3];
    bm_image_get_stride(input_image, stride_i);
    bm_image_get_stride(output_image, stride_o);
    for (int k = 0; k < bm_image_get_plane_num(input_image); k++) {
        if(stride_i[k] != stride_o[k]) {
            BMCV_ERR_LOG("bm_separate_to_planar input and output should be same stride!\n");
            return BM_ERR_FAILURE;
        }
    }
    int height = input_image.height;
    if(!bm_image_is_attached(output_image)) {
        if(BM_SUCCESS != bm_image_alloc_dev_mem(output_image, BMCV_HEAP_ANY)) {
            BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t mem_i, mem_o[3];
    bm_image_get_device_mem(input_image, &mem_i);
    bm_image_get_device_mem(output_image, mem_o);
    for (int i = 0; i < 3; i++) {
        bm_memcpy_d2d_byte(handle, mem_o[i], 0, mem_i, stride_i[0] * height * i, stride_i[0] * height);
    }
    return BM_SUCCESS;
}

layout::plane_layout* bm_image_get_layout(bm_image input_image, int plane_idx)
{
    if (plane_idx < input_image.image_private->plane_num)
        return input_image.image_private->memory_layout + plane_idx;
    else
        return NULL;
}

layout::plane_layout layout::align_width(plane_layout src, int align)
{
    ASSERT(align > 0 && ((align & (align - 1)) == 0));
    plane_layout res = src;
    res.pitch_stride     = ALIGN(res.W * res.data_size, align);
    res.channel_stride  = res.pitch_stride * res.H;
    res.batch_stride     = res.channel_stride * res.C;
    res.size             = res.batch_stride * res.N;
    return res;
};

layout::plane_layout layout::align_height(plane_layout src, int align)
{
    ASSERT(align > 0 && ((align & (align - 1)) == 0));
    plane_layout res = src;
    res.channel_stride  = res.pitch_stride * ALIGN(res.H, align);
    res.batch_stride     = res.channel_stride * res.C;
    res.size             = res.batch_stride * res.N;
    return res;
};

layout::plane_layout layout::align_channel(plane_layout src, int pitch_align, int height_align) {
    ASSERT(pitch_align > 0 && ((pitch_align & (pitch_align - 1)) == 0));
    ASSERT(height_align > 0 && ((height_align & (height_align - 1)) == 0));
    plane_layout res = src;

    res.pitch_stride = ALIGN(res.W * res.data_size, pitch_align);
    res.channel_stride  = res.pitch_stride * ALIGN(res.H, height_align);
    res.batch_stride     = res.channel_stride * res.C;
    res.size             = res.batch_stride * res.N;
    return res;
};
layout::plane_layout layout::stride_width(plane_layout src, int stride)
{
    plane_layout res = src;
    ASSERT_INFO(stride >= res.data_size * res.W && stride % res.data_size == 0,
        "invalidate stride: W_stride=%d, W=%d, data_size=%d\n", stride, res.W, res.data_size);
    res.pitch_stride    = stride;
    res.channel_stride  = res.pitch_stride * res.H;
    res.batch_stride     = res.channel_stride * res.C;
    res.size             = res.batch_stride * res.N;
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_DEBUG,"stride_width stride=%d \n",stride);
    return res;
};

bm_status_t layout::update_memory_layout(bm_handle_t     handle,
                        bm_device_mem_t src,
                        plane_layout    src_layout,
                        bm_device_mem_t dst,
                        plane_layout    dst_layout) {
    ASSERT(src_layout.data_size == dst_layout.data_size &&
            src_layout.C == dst_layout.C && src_layout.N == dst_layout.N);

  int N = bm_min(src_layout.N, dst_layout.N);
  int C = bm_min(src_layout.C, dst_layout.C);
  int H = bm_min(src_layout.H, dst_layout.H);
  int W = bm_min(src_layout.W, dst_layout.W);

  bm_api_cv_width_align_t api = {bm_mem_get_device_addr(src),
                                 bm_mem_get_device_addr(dst),
                                 N,
                                 C,
                                 H,
                                 W,
                                 src_layout.batch_stride / src_layout.data_size,
                                 src_layout.channel_stride / src_layout.data_size,
                                 src_layout.pitch_stride / src_layout.data_size,
                                 dst_layout.batch_stride / dst_layout.data_size,
                                 dst_layout.channel_stride / dst_layout.data_size,
                                 dst_layout.pitch_stride / dst_layout.data_size,
                                 src_layout.data_size
                                 };

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    switch(chipid){
        case 0x1684:
            if(bm_send_api(handle,  BM_API_ID_CV_CORRECT_LAYOUT, (u8 *)&api, sizeof(api)) != BM_SUCCESS || bm_sync_api(handle) != BM_SUCCESS)
                return BM_ERR_FAILURE;
            break;
        case BM1684X:
            bm_tpu_kernel_launch(handle, "cv_width_align", (u8 *)&api, sizeof(api));
            break;
        default:
            printf("BM_NOT_SUPPORT !\n");
            return BM_NOT_SUPPORTED;
            break;
    }
    return BM_SUCCESS;

}

image_warpper::image_warpper(bm_handle_t handle, int num, int height, int width, bm_image_format_ext image_format, bm_image_data_format_ext data_type, int* stride)
{
    image_num = num;
    inner = new bm_image[image_num];
    for (int i = 0; i < num; i++)
    {
        if(bm_image_create(handle, height, width, image_format, data_type, inner + i, stride) != BM_SUCCESS)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_INFO, "create intermediate bm image failed, the call may incorrect %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
        }
    }
}

image_warpper::~image_warpper()
{
    for (int i = 0; i < image_num; i++)
    {
        bm_image_destroy(inner[i]);
    }
    delete[] inner;
}

bm_status_t bmcv_warp_affine_bilinear_bm1684(bm_handle_t       handle,
                                      bm_image          src,
                                      int               output_num,
                                      bm_image *        dst,
                                      bmcv_warp_matrix *matrix)
{
    if (src.data_type != dst[0].data_type) {
        bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "src and dst data type is not same!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (src.image_format != dst[0].image_format) {
        bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "src and dst data format is not same!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (src.data_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "data type only support DATA_TYPE_EXT_1N_BYTE!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (src.image_format != FORMAT_RGB_PLANAR && src.image_format != FORMAT_BGR_PLANAR) {
        bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "format only support RGB_PLANAR and BGR_PLANAR!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (!bm_image_is_attached(src)) {
        bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "src image not attached data !\r\n");
        return BM_ERR_PARAM;
    }
    for (int i = 0; i < output_num; i++) {
        if (dst[0].data_type != dst[i].data_type) {
            bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "dst images' data type not same !\r\n");
            return BM_ERR_PARAM;
        }
        if (dst[0].image_format != dst[i].image_format) {
            bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "dst images' format not same !\r\n");
            return BM_ERR_PARAM;
        }
        if (!bm_image_is_attached(dst[i])) {
            if (bm_image_alloc_dev_mem(dst[i]) != BM_SUCCESS) {
                bmlib_log("AFFINE BILINEAR", BMLIB_LOG_ERROR, "alloc dst dev_mem failed !\r\n");
                return BM_ERR_FAILURE;
            }
        }
    }

    bm_api_cv_warp_bilinear_t api;
    api.image_c = 3;
    api.image_sh = src.height;
    api.image_sw = src.width;

    for (int i = 0; i < output_num; i++){
        bm_device_mem_t src_mem, dst_mem;
        bm_image_get_device_mem(src, &src_mem);
        bm_image_get_device_mem(dst[i], &dst_mem);
        int stride = 0;
        bm_image_get_stride(src, &stride);

        api.S_global_offset = bm_mem_get_device_addr(src_mem);
        api.P_global_offset = bm_mem_get_device_addr(dst_mem);
        for (int k = 0; k < 6; k++)
            api.matrix[k] = matrix[i].m[k];

        int h_loop   = ALIGN(dst[i].height, WARP_MAX_H) / WARP_MAX_H;
        int w_loop   = ALIGN(dst[i].width, WARP_MAX_W) / WARP_MAX_W;

        api.image_dh_real = dst[i].height;
        api.image_dw_real = dst[i].width;
        api.src_stride    = stride;
        for (int h = 0; h < h_loop; h++) {
            for (int w = 0; w < w_loop; w++) {
                int image_dh = (h == h_loop - 1) ? (dst[i].height - (h_loop - 1) * WARP_MAX_H) : (WARP_MAX_H);
                int image_dw = (w == w_loop - 1) ? (dst[i].width - (w_loop - 1) * WARP_MAX_W) : (WARP_MAX_W);
                api.image_dh = image_dh;
                api.image_dw = image_dw;
                api.blockIdx_x = WARP_MAX_W * w;
                api.blockIdx_y = WARP_MAX_H * h;

                if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_WARP_BILINEAR, (u8 *)&api, sizeof(api))) {
                    BMCV_ERR_LOG("warp_bilinear send api error\r\n");
                    return BM_ERR_FAILURE;
                }
                if (BM_SUCCESS != bm_sync_api(handle)) {
                    BMCV_ERR_LOG("warp_bilinear sync api error\r\n");
                    return BM_ERR_FAILURE;
                }
            }
        }
    }

    return BM_SUCCESS;
 }

typedef float bm_gen_proposal_data_type_t;
#define PARSE_SYS_MEM_STRUCT(intput_addr, type, output_str)                    \
    do {                                                                       \
        if (bm_mem_get_type(intput_addr) == BM_MEM_TYPE_SYSTEM) {              \
            output_str = (type)bm_mem_get_system_addr(intput_addr);            \
        } else {                                                               \
            bmlib_log(BMCV_LOG_TAG,                                            \
                      BMLIB_LOG_ERROR,                                         \
                      "addr must be in sys memory:%s:%d\n",                    \
                      __FILE__,                                                \
                      __LINE__);                                               \
            return BM_ERR_PARAM;                                               \
        }                                                                      \
    } while (0)

bm_status_t bmcv_gen_prop_nms_bm1684(bm_handle_t     handle,
                              bm_device_mem_t scores_addr_0,
                              bm_device_mem_t bbox_deltas_addr_0,
                              bm_device_mem_t anchor_scales_addr_0,
                              bm_device_mem_t gen_proposal_attr_addr_0,

                              bm_device_mem_t scores_addr_1,
                              bm_device_mem_t bbox_deltas_addr_1,
                              bm_device_mem_t anchor_scales_addr_1,
                              bm_device_mem_t gen_proposal_attr_addr_1,

                              bm_device_mem_t scores_addr_2,
                              bm_device_mem_t bbox_deltas_addr_2,
                              bm_device_mem_t anchor_scales_addr_2,
                              bm_device_mem_t gen_proposal_attr_addr_2,

                              int             anchor_num,
                              float           nms_threshold,
                              float           filter_threshold,
                              bm_device_mem_t filter_output,
                              bm_device_mem_t filter_shape_output) {
    if (handle == NULL) {
        bmlib_log("GEN_PROP_NMS", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_api_cv_gen_proposal_and_nms_t arg;
    bm_device_mem_t                  scores_buf_device_0, scores_buf_device_1,
        scores_buf_device_2;
    bm_device_mem_t bbox_deltas_buf_device_0, bbox_deltas_buf_device_1,
        bbox_deltas_buf_device_2;
    bm_device_mem_t anchor_scales_buf_device_0, anchor_scales_buf_device_1,
        anchor_scales_buf_device_2;
    bm_device_mem_t prop_output_buf_device;
    // bm_device_mem_t nms_output_buf_device;
    bm_device_mem_t           filter_output_buf_device;
    bm_device_mem_t           filter_output_shape_buf_device;
    bmcv_gen_proposal_attr_t *bmcv_gen_proposal_attr_0;
    bmcv_gen_proposal_attr_t *bmcv_gen_proposal_attr_1;
    bmcv_gen_proposal_attr_t *bmcv_gen_proposal_attr_2;
    PARSE_SYS_MEM_STRUCT(gen_proposal_attr_addr_0,
                         bmcv_gen_proposal_attr_t *,
                         bmcv_gen_proposal_attr_0);
    PARSE_SYS_MEM_STRUCT(gen_proposal_attr_addr_1,
                         bmcv_gen_proposal_attr_t *,
                         bmcv_gen_proposal_attr_1);
    PARSE_SYS_MEM_STRUCT(gen_proposal_attr_addr_2,
                         bmcv_gen_proposal_attr_t *,
                         bmcv_gen_proposal_attr_2);
    bmcv_shape_t *score_shape_0 = &bmcv_gen_proposal_attr_0->score_shape;
    bmcv_shape_t *bbox_shape_0  = &bmcv_gen_proposal_attr_0->bbox_shape;
    bmcv_shape_t *score_shape_1 = &bmcv_gen_proposal_attr_1->score_shape;
    bmcv_shape_t *bbox_shape_1  = &bmcv_gen_proposal_attr_1->bbox_shape;
    bmcv_shape_t *score_shape_2 = &bmcv_gen_proposal_attr_2->score_shape;
    bmcv_shape_t *bbox_shape_2  = &bmcv_gen_proposal_attr_2->bbox_shape;
    int           elem_size     = sizeof(float);
    int32_t       data_size     = sizeof(bm_gen_proposal_data_type_t);

    int32_t bbox_deltas_size_0 = 12 * bmcv_gen_proposal_attr_0->feat_w *
                                 bmcv_gen_proposal_attr_0->feat_h * data_size *
                                 bbox_shape_0->n;
    int32_t bbox_deltas_size_1 = 12 * bmcv_gen_proposal_attr_1->feat_w *
                                 bmcv_gen_proposal_attr_1->feat_h * data_size *
                                 bbox_shape_0->n;
    int32_t bbox_deltas_size_2 = 12 * bmcv_gen_proposal_attr_2->feat_w *
                                 bmcv_gen_proposal_attr_2->feat_h * data_size *
                                 bbox_shape_2->n;
    if (bm_mem_get_type(scores_addr_0) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &scores_buf_device_0,
                                  score_shape_0->n * score_shape_0->c *
                                      bmcv_gen_proposal_attr_0->feat_w *
                                      bmcv_gen_proposal_attr_0->feat_h *
                                      data_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          scores_buf_device_0,
                          bm_mem_get_system_addr(scores_addr_0))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        scores_buf_device_0 = scores_addr_0;
    }
    if (bm_mem_get_type(bbox_deltas_addr_0) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &bbox_deltas_buf_device_0,
                                                bbox_deltas_size_0)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          bbox_deltas_buf_device_0,
                          bm_mem_get_system_addr(bbox_deltas_addr_0))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err2;
        }
    } else {
        bbox_deltas_buf_device_0 = bbox_deltas_addr_0;
    }
    if (bm_mem_get_type(anchor_scales_addr_0) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(
                handle,
                &anchor_scales_buf_device_0,
                data_size * bmcv_gen_proposal_attr_0->anchor_scale_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err2;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          anchor_scales_buf_device_0,
                          bm_mem_get_system_addr(anchor_scales_addr_0))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err3;
        }
    } else {
        anchor_scales_buf_device_0 = anchor_scales_addr_0;
    }

    if (bm_mem_get_type(scores_addr_1) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &scores_buf_device_1,
                                  score_shape_1->n * score_shape_1->c *
                                      bmcv_gen_proposal_attr_1->feat_w *
                                      bmcv_gen_proposal_attr_1->feat_h *
                                      data_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err3;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          scores_buf_device_1,
                          bm_mem_get_system_addr(scores_addr_1))) {
            BMCV_ERR_LOG("bm_memcpy_s2d  error\r\n");

            goto err4;
        }
    } else {
        scores_buf_device_1 = scores_addr_1;
    }
    if (bm_mem_get_type(bbox_deltas_addr_1) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &bbox_deltas_buf_device_1,
                                                bbox_deltas_size_1)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err4;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          bbox_deltas_buf_device_1,
                          bm_mem_get_system_addr(bbox_deltas_addr_1))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err5;
        }
    } else {
        bbox_deltas_buf_device_1 = bbox_deltas_addr_1;
    }
    if (bm_mem_get_type(anchor_scales_addr_1) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(
                handle,
                &anchor_scales_buf_device_1,
                data_size * bmcv_gen_proposal_attr_1->anchor_scale_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err5;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          anchor_scales_buf_device_1,
                          bm_mem_get_system_addr(anchor_scales_addr_1))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err6;
        }
    } else {
        anchor_scales_buf_device_1 = anchor_scales_addr_1;
    }

    if (bm_mem_get_type(scores_addr_2) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &scores_buf_device_2,
                                  score_shape_2->n * score_shape_2->c *
                                      bmcv_gen_proposal_attr_2->feat_w *
                                      bmcv_gen_proposal_attr_2->feat_h *
                                      data_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err6;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          scores_buf_device_2,
                          bm_mem_get_system_addr(scores_addr_2))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err7;
        }
    } else {
        scores_buf_device_2 = scores_addr_2;
    }
    if (bm_mem_get_type(bbox_deltas_addr_2) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &bbox_deltas_buf_device_2,
                                                bbox_deltas_size_2)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err7;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          bbox_deltas_buf_device_2,
                          bm_mem_get_system_addr(bbox_deltas_addr_2))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err8;
        }
    } else {
        bbox_deltas_buf_device_2 = bbox_deltas_addr_2;
    }
    if (bm_mem_get_type(anchor_scales_addr_2) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(
                handle,
                &anchor_scales_buf_device_2,
                data_size * bmcv_gen_proposal_attr_2->anchor_scale_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err8;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          anchor_scales_buf_device_2,
                          bm_mem_get_system_addr(anchor_scales_addr_2))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err9;
        }
    } else {
        anchor_scales_buf_device_2 = anchor_scales_addr_2;
    }

    if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                            &prop_output_buf_device,
                                            sizeof(m_proposal_t))) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

        goto err9;
    }
    // if( BM_SUCCESS !=bm_malloc_device_byte(
    //    handle, &nms_output_buf_device, sizeof(nms_proposal_t)));
    if (bm_mem_get_type(filter_output) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &filter_output_buf_device,
                                                sizeof(nms_proposal_t))) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err10;
        }
    } else {
        filter_output_buf_device = filter_output;
    }
    if (bm_mem_get_type(filter_shape_output) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &filter_output_shape_buf_device,
                                  score_shape_0->n * sizeof(int))) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err11;
        }
    } else {
        filter_output_shape_buf_device = filter_shape_output;
    }

    arg.scores_addr_0      = bm_mem_get_device_addr(scores_buf_device_0);
    arg.bbox_deltas_addr_0 = bm_mem_get_device_addr(bbox_deltas_buf_device_0);
    arg.anchor_scales_addr_0 =
        bm_mem_get_device_addr(anchor_scales_buf_device_0);
    arg.scale_factor_0      = bmcv_gen_proposal_attr_0->scale_factor;
    arg.feat_factor_0       = bmcv_gen_proposal_attr_0->feat_factor;
    arg.channel_0           = score_shape_0->c;
    arg.feat_w_0            = bmcv_gen_proposal_attr_0->feat_w;
    arg.feat_h_0            = bmcv_gen_proposal_attr_0->feat_h;
    arg.width_0             = bmcv_gen_proposal_attr_0->width;
    arg.height_0            = bmcv_gen_proposal_attr_0->height;
    arg.feat_stride_0       = bmcv_gen_proposal_attr_0->feat_stride;
    arg.anchor_scale_size_0 = bmcv_gen_proposal_attr_0->anchor_scale_size;
    arg.base_size_0         = bmcv_gen_proposal_attr_0->base_size;
    arg.im_scale_0          = bmcv_gen_proposal_attr_0->im_scale;
    arg.min_size_0          = bmcv_gen_proposal_attr_0->min_size;
    arg.base_threshold_0    = bmcv_gen_proposal_attr_0->base_threshold;
    arg.per_nms_topn_0      = bmcv_gen_proposal_attr_0->per_nms_topn;
    arg.bbox_n_offset_0 =
        elem_size * bbox_shape_0->w * bbox_shape_0->h * bbox_shape_0->c;

    arg.scores_addr_1      = bm_mem_get_device_addr(scores_buf_device_1);
    arg.bbox_deltas_addr_1 = bm_mem_get_device_addr(bbox_deltas_buf_device_1);
    arg.anchor_scales_addr_1 =
        bm_mem_get_device_addr(anchor_scales_buf_device_1);
    arg.scale_factor_1      = bmcv_gen_proposal_attr_1->scale_factor;
    arg.channel_1           = score_shape_1->c;
    arg.feat_factor_1       = bmcv_gen_proposal_attr_1->feat_factor;
    arg.feat_w_1            = bmcv_gen_proposal_attr_1->feat_w;
    arg.feat_h_1            = bmcv_gen_proposal_attr_1->feat_h;
    arg.width_1             = bmcv_gen_proposal_attr_1->width;
    arg.height_1            = bmcv_gen_proposal_attr_1->height;
    arg.feat_stride_1       = bmcv_gen_proposal_attr_1->feat_stride;
    arg.anchor_scale_size_1 = bmcv_gen_proposal_attr_1->anchor_scale_size;
    arg.base_size_1         = bmcv_gen_proposal_attr_1->base_size;
    arg.im_scale_1          = bmcv_gen_proposal_attr_1->im_scale;
    arg.min_size_1          = bmcv_gen_proposal_attr_1->min_size;
    arg.base_threshold_1    = bmcv_gen_proposal_attr_1->base_threshold;
    arg.per_nms_topn_1      = bmcv_gen_proposal_attr_1->per_nms_topn;
    arg.bbox_n_offset_1 =
        elem_size * bbox_shape_1->w * bbox_shape_1->h * bbox_shape_1->c;

    arg.scores_addr_2      = bm_mem_get_device_addr(scores_buf_device_2);
    arg.bbox_deltas_addr_2 = bm_mem_get_device_addr(bbox_deltas_buf_device_2);
    arg.anchor_scales_addr_2 =
        bm_mem_get_device_addr(anchor_scales_buf_device_2);
    arg.scale_factor_2      = bmcv_gen_proposal_attr_2->scale_factor;
    arg.feat_factor_2       = bmcv_gen_proposal_attr_2->feat_factor;
    arg.channel_2           = score_shape_2->c;
    arg.feat_w_2            = bmcv_gen_proposal_attr_2->feat_w;
    arg.feat_h_2            = bmcv_gen_proposal_attr_2->feat_h;
    arg.width_2             = bmcv_gen_proposal_attr_2->width;
    arg.height_2            = bmcv_gen_proposal_attr_2->height;
    arg.feat_stride_2       = bmcv_gen_proposal_attr_2->feat_stride;
    arg.anchor_scale_size_2 = bmcv_gen_proposal_attr_2->anchor_scale_size;
    arg.base_size_2         = bmcv_gen_proposal_attr_2->base_size;
    arg.im_scale_2          = bmcv_gen_proposal_attr_2->im_scale;
    arg.min_size_2          = bmcv_gen_proposal_attr_2->min_size;
    arg.base_threshold_2    = bmcv_gen_proposal_attr_2->base_threshold;
    arg.per_nms_topn_2      = bmcv_gen_proposal_attr_2->per_nms_topn;
    arg.bbox_n_offset_2 =
        elem_size * bbox_shape_2->w * bbox_shape_2->h * bbox_shape_2->c;

    arg.nms_threshold        = nms_threshold;
    arg.filer_threshold      = filter_threshold;
    arg.batch_num            = score_shape_0->n;
    arg.anchor_num           = anchor_num;
    arg.output_proposal_addr = bm_mem_get_device_addr(prop_output_buf_device);
    // arg.nms_output = bm_mem_get_device_addr(nms_output_buf_device);
    arg.filter_output = bm_mem_get_device_addr(filter_output_buf_device);
    arg.filter_output_shape_addr =
        bm_mem_get_device_addr(filter_output_shape_buf_device);

    bm_send_api(
        handle,  BM_API_ID_CV_GEN_PROP_AND_NMS, (uint8_t *)&arg, sizeof(arg));
    bm_sync_api(handle);
    if (bm_mem_get_type(filter_shape_output) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(handle,
                          bm_mem_get_system_addr(filter_shape_output),
                          filter_output_shape_buf_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err12;
        }
        bm_free_device(handle, filter_output_shape_buf_device);
    }
    if (bm_mem_get_type(filter_output) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(filter_output),
                                        filter_output_buf_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err11;
        }
        bm_free_device(handle, filter_output_buf_device);
    }

    if (bm_mem_get_type(scores_addr_0) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, scores_buf_device_0);
    }
    if (bm_mem_get_type(bbox_deltas_addr_0) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, bbox_deltas_buf_device_0);
    }
    if (bm_mem_get_type(anchor_scales_addr_0) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, anchor_scales_buf_device_0);
    }
    if (bm_mem_get_type(scores_addr_1) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, scores_buf_device_1);
    }
    if (bm_mem_get_type(bbox_deltas_addr_1) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, bbox_deltas_buf_device_1);
    }
    if (bm_mem_get_type(anchor_scales_addr_1) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, anchor_scales_buf_device_1);
    }
    if (bm_mem_get_type(scores_addr_2) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, scores_buf_device_2);
    }
    if (bm_mem_get_type(bbox_deltas_addr_2) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, bbox_deltas_buf_device_2);
    }
    if (bm_mem_get_type(anchor_scales_addr_2) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, anchor_scales_buf_device_2);
    }
    bm_free_device(handle, prop_output_buf_device);
    // bm_free_device(handle, nms_output_buf_device);
    return BM_SUCCESS;

err12:
    if (bm_mem_get_type(filter_shape_output) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, filter_output_shape_buf_device);
    }
err11:
    if (bm_mem_get_type(filter_output) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, filter_output_buf_device);
    }
err10:
    bm_free_device(handle, prop_output_buf_device);
err9:
    if (bm_mem_get_type(anchor_scales_addr_2) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, anchor_scales_buf_device_2);
    }
err8:
    if (bm_mem_get_type(bbox_deltas_addr_2) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, bbox_deltas_buf_device_2);
    }
err7:
    if (bm_mem_get_type(scores_addr_2) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, scores_buf_device_2);
    }
err6:
    if (bm_mem_get_type(anchor_scales_addr_1) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, anchor_scales_buf_device_1);
    }
err5:
    if (bm_mem_get_type(bbox_deltas_addr_1) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, bbox_deltas_buf_device_1);
    }
err4:
    if (bm_mem_get_type(scores_addr_1) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, scores_buf_device_1);
    }
err3:
    if (bm_mem_get_type(anchor_scales_addr_0) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, anchor_scales_buf_device_0);
    }
err2:
    if (bm_mem_get_type(bbox_deltas_addr_0) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, bbox_deltas_buf_device_0);
    }
err1:
    if (bm_mem_get_type(scores_addr_0) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, scores_buf_device_0);
    }
err0:
    return BM_ERR_FAILURE;
}

void format_to_str(bm_image_format_ext format, char* res)
{
    switch(format)
    {
        case FORMAT_YUV420P:
            strcpy(res, "FORMAT_YUV420P");
            break;
        case FORMAT_YUV422P:
            strcpy(res, "FORMAT_YUV422P");
            break;
        case FORMAT_YUV444P:
            strcpy(res, "FORMAT_YUV444P");
            break;
        case FORMAT_NV12:
            strcpy(res, "FORMAT_NV12");
            break;
        case FORMAT_NV21:
            strcpy(res, "FORMAT_NV21");
            break;
        case FORMAT_NV16:
            strcpy(res, "FORMAT_NV16");
            break;
        case FORMAT_NV61:
            strcpy(res, "FORMAT_NV61");
            break;
        case FORMAT_NV24:
            strcpy(res, "FORMAT_NV24");
            break;
        case FORMAT_RGB_PLANAR:
            strcpy(res, "FORMAT_RGB_PLANAR");
            break;
        case FORMAT_BGR_PLANAR:
            strcpy(res, "FORMAT_BGR_PLANAR");
            break;
        case FORMAT_RGB_PACKED:
            strcpy(res, "FORMAT_RGB_PACKED");
            break;
        case FORMAT_BGR_PACKED:
            strcpy(res, "FORMAT_BGR_PACKED");
            break;
        case FORMAT_ARGB_PACKED:
            strcpy(res, "FORMAT_ARGB_PACKED");
            break;
        case FORMAT_ABGR_PACKED:
            strcpy(res, "FORMAT_ABGR_PACKED");
            break;
        case FORMAT_RGBP_SEPARATE:
            strcpy(res, "FORMAT_RGBP_SEPARATE");
            break;
        case FORMAT_BGRP_SEPARATE:
            strcpy(res, "FORMAT_BGRP_SEPARATE");
            break;
        case FORMAT_GRAY:
            strcpy(res, "FORMAT_GRAY");
            break;
        case FORMAT_COMPRESSED:
            strcpy(res, "FORMAT_COMPRESSED");
            break;
        case FORMAT_HSV_PLANAR:
            strcpy(res, "FORMAT_HSV_PLANAR");
            break;
        case FORMAT_YUV444_PACKED:
            strcpy(res, "FORMAT_YUV444_PACKED");
            break;
        case FORMAT_YVU444_PACKED:
            strcpy(res, "FORMAT_YVU444_PACKED");
            break;
        case FORMAT_YUV422_YUYV:
            strcpy(res, "FORMAT_YUV422_YUYV");
            break;
        case FORMAT_YUV422_YVYU:
            strcpy(res, "FORMAT_YUV422_YVYU");
            break;
        case FORMAT_YUV422_UYVY:
            strcpy(res, "FORMAT_YUV422_UYVY");
            break;
       case FORMAT_YUV422_VYUY:
            strcpy(res, "FORMAT_YUV422_VYUY");
            break;
       case FORMAT_RGBYP_PLANAR:
            strcpy(res, "FORMAT_RGBYP_PLANAR");
            break;
       case FORMAT_HSV180_PACKED:
            strcpy(res, "FORMAT_HSV180_PACKED");
            break;
       case FORMAT_HSV256_PACKED:
            strcpy(res, "FORMAT_HSV256_PACKED");
            break;

        default:
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "Not found such format%s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
            break;
    }
}

bm_status_t bmcv_warp_affine_bilinear(bm_handle_t       handle,
                                      bm_image          src,
                                      int               output_num,
                                      bm_image *        dst,
                                      bmcv_warp_matrix *matrix) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_warp_affine_bilinear_bm1684(handle, src, output_num, dst, matrix);
        break;

      case BM1684X:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}


bm_status_t bmcv_gen_prop_nms(bm_handle_t     handle,
                              bm_device_mem_t scores_addr_0,
                              bm_device_mem_t bbox_deltas_addr_0,
                              bm_device_mem_t anchor_scales_addr_0,
                              bm_device_mem_t gen_proposal_attr_addr_0,

                              bm_device_mem_t scores_addr_1,
                              bm_device_mem_t bbox_deltas_addr_1,
                              bm_device_mem_t anchor_scales_addr_1,
                              bm_device_mem_t gen_proposal_attr_addr_1,

                              bm_device_mem_t scores_addr_2,
                              bm_device_mem_t bbox_deltas_addr_2,
                              bm_device_mem_t anchor_scales_addr_2,
                              bm_device_mem_t gen_proposal_attr_addr_2,

                              int             anchor_num,
                              float           nms_threshold,
                              float           filter_threshold,
                              bm_device_mem_t filter_output,
                              bm_device_mem_t filter_shape_output) {


    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_gen_prop_nms_bm1684(handle,
                                       scores_addr_0,
                                       bbox_deltas_addr_0,
                                       anchor_scales_addr_0,
                                       gen_proposal_attr_addr_0,
                                       scores_addr_1,
                                       bbox_deltas_addr_1,
                                       anchor_scales_addr_1,
                                       gen_proposal_attr_addr_1,
                                       scores_addr_2,
                                       bbox_deltas_addr_2,
                                       anchor_scales_addr_2,
                                       gen_proposal_attr_addr_2,
                                       anchor_num,
                                       nms_threshold,
                                       filter_threshold,
                                       filter_output,
                                       filter_shape_output);
        break;

      case BM1684X:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

int is_yuv_or_rgb(bm_image_format_ext fmt)
{
  int ret = COLOR_SPACE_YUV;
  switch(fmt)
  {
    case FORMAT_YUV420P:
    case FORMAT_YUV422P:
    case FORMAT_YUV444P:
    case FORMAT_NV12:
    case FORMAT_NV21:
    case FORMAT_NV16:
    case FORMAT_NV61:
    case FORMAT_NV24:
    case FORMAT_GRAY:
    case FORMAT_COMPRESSED:
    case FORMAT_YUV444_PACKED:
    case FORMAT_YVU444_PACKED:
    case FORMAT_YUV422_YUYV:
    case FORMAT_YUV422_YVYU:
    case FORMAT_YUV422_UYVY:
    case FORMAT_YUV422_VYUY:
      ret = COLOR_SPACE_YUV;
      break;
    case FORMAT_RGB_PLANAR:
    case FORMAT_BGR_PLANAR:
    case FORMAT_RGB_PACKED:
    case FORMAT_BGR_PACKED:
    case FORMAT_RGBP_SEPARATE:
    case FORMAT_BGRP_SEPARATE:
    case FORMAT_ARGB_PACKED:
    case FORMAT_ABGR_PACKED:
      ret = COLOR_SPACE_RGB;
      break;
    case FORMAT_HSV_PLANAR:
    case FORMAT_HSV180_PACKED:
    case FORMAT_HSV256_PACKED:
      ret = COLOR_SPACE_HSV;
      break;
    case FORMAT_RGBYP_PLANAR:
      ret = COLOR_SPACE_RGBY;
      break;
    default:
      ret = COLOR_NOT_DEF;
      break;
  }
  return ret;
}

void calculate_yuv(u8 r, u8 g, u8 b, u8* y_, u8* u_, u8* v_)
{
    double y, u, v;
    y = (0.257 * r + 0.504 * g + 0.098 * b + 16);
    u = (-0.148 * r - 0.291 * g + 0.439 * b + 128);
    v = (0.439 * r - 0.368 * g - 0.071 * b + 128);
    y = (y <= 0) ? 0 : y;
    y = (y >= 255) ? 255 : y;
    u = (u <= 0) ? 0 : u;
    u = (u >= 255) ? 255 : u;
    v = (v <= 0) ? 0 : v;
    v = (v >= 255) ? 255 : v;

    *y_ = (u8)y;
    *u_ = (u8)u;
    *v_ = (u8)v;
}
