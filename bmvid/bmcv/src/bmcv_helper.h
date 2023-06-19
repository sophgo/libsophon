#ifndef BMCV_HELPER_H
#define BMCV_HELPER_H
#ifdef __linux__
  #include "common.h"
#else
  #include "..\..\..\common\bm1684\include_win\common_win.h"
#endif
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"

#if defined (__cplusplus)
extern "C" {
#endif

INLINE static bm_image_data_format_ext bmcv_get_data_format_from_sc3(bmcv_data_format data_format)
{
    switch(data_format) {
    case DATA_TYPE_FLOAT:
        return DATA_TYPE_EXT_FLOAT32;
    break;
    case DATA_TYPE_BYTE:
        return DATA_TYPE_EXT_1N_BYTE;
    break;
    default:
        return DATA_TYPE_EXT_1N_BYTE;
    break;
    }
}

INLINE static bm_image_format_ext bmcv_get_image_format_from_sc3(bm_image_format img_format)
{
    switch(img_format) {
    case YUV420P:
        return FORMAT_YUV420P;
    break;
    case NV12:
        return FORMAT_NV12;
    break;
    case NV21:
        return FORMAT_NV21;
    break;
    case RGB:
        return FORMAT_RGB_PLANAR;
    break;
    case BGR: 
        return FORMAT_BGR_PLANAR;
    break;
    case RGB_PACKED:
        return FORMAT_RGB_PACKED;
    break;
    case BGR_PACKED:
        return FORMAT_BGR_PACKED;
    break;
    default:
        return FORMAT_BGR_PACKED;
    break;
    }
}

#if defined (__cplusplus)
}
#endif

#endif /* BMCV_HELPER_H */
