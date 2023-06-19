/*
 *  This file is uesd both HOST and A53
*/
#ifndef BMCV_API_STRUCT_H
#define BMCV_API_STRUCT_H

#ifdef WIN32
#pragma pack(push, 1)
#endif
typedef struct {
    unsigned long long input_addr[3];
    int format;
    int width;
    int height;
    int line_num;
    int thick;
    int rval;
    int gval;
    int bval;
#ifdef WIN32
} bmcv_draw_line_param_t;
#pragma pack(pop)
#else
} __attribute__((packed)) bmcv_draw_line_param_t;
#endif

#ifdef WIN32
#pragma pack(push, 1)
#endif
typedef struct {
    unsigned long long src_addr;
    unsigned long long dst_addr;
    unsigned long long kernel_addr;
    int kh;
    int kw;
    int width;
    int height;
    int srcstep;
    int dststep;
    int format;
    int op;
#ifdef WIN32
} bmcv_morph_param_t;
#pragma pack(pop)
#else
} __attribute__((packed)) bmcv_morph_param_t;
#endif

#ifdef WIN32
#pragma pack(push, 1)
#endif
typedef struct {
    unsigned long long prevPyrAddr[6];
    unsigned long long nextPyrAddr[6];
    unsigned long long gradAddr[6];
    int width;
    int height;
    int winW;
    int winH;
    int ptsNum;
    int maxLevel;
    int maxCount;
    double eps;
#ifdef WIN32
} bmcv_lkpyramid_param_t;
#pragma pack(pop)
#else
} __attribute__((packed)) bmcv_lkpyramid_param_t;
#endif

#endif

