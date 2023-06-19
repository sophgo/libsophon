#ifndef BMCV_CPU_FUNC_H
#define BMCV_CPU_FUNC_H

#if defined(__cplusplus)
extern "C" {
#endif

extern void bmcpu_dev_log(unsigned int level, const char *format, ...);
extern void bmcpu_dev_flush_dcache(void* start, void* end);
extern void bmcpu_dev_flush_and_invalidate_dcache(void* start, void* end);

//////////////////// API PARAMETER ///////////////////
#include "bmcv_api_struct.h"

/////////////////// API FUNCTIONS ///////////////////
int bmcv_cpu_draw_line(void* addr, int size);
int bmcv_cpu_morph(void* addr, int size);
int bmcv_cpu_lkpyramid(void* addr, int size);

#if defined(__cplusplus)
}
#endif
#endif

