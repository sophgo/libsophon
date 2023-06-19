#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include "stdio.h"
#include "bmlib_runtime.h"
#include "bmcv_api_ext.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define BM1684X 0x1686
#define UNUSED(x) (void)(x)
#define bm_min(x, y) (((x)) < ((y)) ? (x) : (y))
#define bm_max(x, y) (((x)) > ((y)) ? (x) : (y))
#define IS_NAN(x) ((((x >> 23) & 0xff) == 0xff) && ((x & 0x7fffff) != 0))

#ifdef __linux__
#define EU_SHIFT                (CONFIG_EU_SHIFT)
#else
#define EU_SHIFT                5
#endif

#ifndef USING_CMODEL
#define EU_NUM                  (1<<EU_SHIFT)
#else
#define EU_NUM 64
#endif

#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

#ifdef __linux__
#define ALIGN(x, a) __ALIGN_MASK(x, (__typeof__(x))(a)-1)
#else
#define ALIGN(x, a) __ALIGN_MASK(x, (int)(a)-1)
#endif

#define filename(x) (strrchr(x, '/') ? strrchr(x, '/') + 1 : x)

typedef enum {
  STORAGE_MODE_1N_FP32    = 0,
  STORAGE_MODE_1N_INT8    = 1,
  STORAGE_MODE_1N_INT16   = 2,
  STORAGE_MODE_2N_INT16   = 3,
  STORAGE_MODE_4N_INT8    = 4,
  STORAGE_MODE_2IC_FP32   = 5,  // special for 2IC weight
  STORAGE_MODE_4N_4IC_4OC = 6,
  STORAGE_MODE_4N_INT16   = 7,
  STORAGE_MODE_UNINITILIZED,
  STORAGE_MODE_END
} TENSOR_STORAGE_MODE;

typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long u64;

typedef struct border {
  bool   rect_border_enable;
  u16 st_x;
  u16 st_y;
  u16 width;
  u16 height;
  u8  value_r;
  u8  value_g;
  u8  value_b;
  u8  thickness;
}border_t;

typedef struct font {
  bool   font_enable;
  bool   font_type;
  bool   font_nf_range;
  bool   font_dot_en;
  u16 font_pitch;
  u16 font_st_x;
  u16 font_st_y;
  u16 font_width;
  u16 font_height;
  u64 font_addr;
  u8  font_value_r;
  u8  font_value_g;
  u8  font_value_b;
}font_t;

typedef struct {
    int   index;
    float val;
#ifndef WIN32
} __attribute__((packed)) sort_t;
#else
} sort_t;
#endif

typedef union {
  int ival;
  float fval;
} IF_VAL;

typedef enum {
    CONVERT_1N_TO_1N = 0,
    CONVERT_4N_TO_4N,
    CONVERT_4N_TO_1N
} convert_storage_mode_e;

typedef struct {
    int width;
    int height;
    bm_image_format_ext format;
    void** data;
    int* step;
} bmMat;


int array_cmp_(float *p_exp, float *p_got, int len, const char *info_label, float delta);
int array_cmp_fix8b_(
    void *p_exp,
    void *p_got,
    int sign,
    int len,
    const char *info_label,
    int delta) ;
int array_cmp_fix16b_(
    void *p_exp,
    void *p_got,
    int sign,
    int len,
    const char *info_label,
    int delta);
#ifdef _WIN32
int clock_gettime_win(int dummy, struct timespec* ct);
int gettimeofday_win(struct timeval* tp, void* tzp);
#endif

int gettimeofday_(struct timeval* tp);
int clock_gettime_(int dummy, struct timespec* ct);

#if defined (__cplusplus)
}
#endif
