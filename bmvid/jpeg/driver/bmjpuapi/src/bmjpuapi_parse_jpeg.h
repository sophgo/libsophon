#ifndef BMJPUAPI_PARSE_JPEG_H
#define BMJPUAPI_PARSE_JPEG_H

#include "bm_jpeg_interface.h"

#ifdef __cplusplus
extern "C" {
#endif


int bm_jpu_parse_jpeg_header(void *jpeg_data, size_t jpeg_data_size, unsigned int *width, unsigned int *height, BmJpuColorFormat *color_format);


#ifdef __cplusplus
}
#endif


#endif
