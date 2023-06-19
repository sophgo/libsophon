#ifndef __VPP_LIB_H__
#define __VPP_LIB_H__
#include "vppion.h"
#include <assert.h>

#define VPP_OK (0)
#define VPP_ERR (-1)

/*color space BM1682 supported*/
#define FMT_I420        (0)    /*YUV420 Planar(I420)*/
#define FMT_Y           (1)    /*Y only*/
#define FMT_RGBP        (2)    /*rgb 24 planar,rrrgggbbb,r g b three channels of data in three different physically contiguous memory spaces*/
#define FMT_BGRA        (3)    /*BGRA Packed 32, (low)B-G-R-A(high)*/
#define FMT_RGBA        (4)    /*RGBA Packed 32, (low) R-G-B-A(high)*/
#define FMT_BGR         (5)    /*BGR Packed 24, (low) B-G-R (high)*/
#define FMT_RGB         (6)    /*RGB Packed 24, (low) R-G-B (high)*/
#define FMT_NV12        (7)    /*YUV420 SemiPlanar(NV12)*/
#define FMT_RGBP_C      (8)    /*rgb 24 planar, r g b three channels of data in one physically contiguous memory spaces*/
#define FMT_BGRP        (9)    /*rgb 24 planar,rrrgggbbb,r g b three channels of data in three different physically contiguous memory spaces*/

/*maximum and minimum image resolution*/
#define MIN_RESOLUTION_W    (16)
#define MIN_RESOLUTION_H    (16)
#define MIN_RESOLUTION_W_I420    (32)
#define MIN_RESOLUTION_H_I420    (32)
#define MAX_RESOLUTION_W    (4096)
#define MAX_RESOLUTION_H    (4096)

struct vpp_cmd_n {
    int src_format;
    int src_stride;
    int src_fd0;
    int src_fd1;
    int src_fd2;
    unsigned long src_addr0;
    unsigned long src_addr1;
    unsigned long src_addr2;
    unsigned short src_axisX;
    unsigned short src_axisY;
    unsigned short src_cropW;
    unsigned short src_cropH;

    int dst_format;
    int dst_stride;
    int dst_fd0;
    int dst_fd1;
    int dst_fd2;
    unsigned long dst_addr0;
    unsigned long dst_addr1;
    unsigned long dst_addr2;
    unsigned short dst_axisX;
    unsigned short dst_axisY;
    unsigned short dst_cropW;
    unsigned short dst_cropH;
    int csc_type;
};

struct vpp_batch_n {
    int num;
    struct vpp_cmd_n cmd[16];
};

typedef struct _vpp_fd_
{
    int dev_fd;/*vpp dev fd*/
    char *name;/*if u fill in the value of vpp_dev_fd,u must fill in vpp_dev_name as bm-vpp*/
}vpp_fd_s;

typedef struct vpp_mat_s
{
    int num_comp;        /*channel number of data blocks. packet data:1, yuv420sp: 2, yuv420p:3*/
    int format;          /*image data format*/
    int is_pa;           /*Judging whether to use ion memory handles or physical addresses directly */
    vpp_fd_s vpp_fd;     /*vpp handle*/
    int stride;           /*stride of image*/
    int fd[4];            /*Handles pointing to ion memory*/
    void* va[4];          /*Virtual address of ion memory*/
    unsigned long pa[4];  /*Physical address of ion memory*/
    int ion_len[4];       /* Memory Length of Three Channels*/
    int axisX;            /*Image offset in x quadrant*/
    int axisY;            /*Image offset in y quadrant*/
    int cols;             /*Image width*/
    int rows;             /*Image height*/
} vpp_mat;

typedef struct vpp_rect_s {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} vpp_rect;

#define VPP_UPDATE_BATCH _IOWR('v', 0x01, unsigned long)
#define VPP_UPDATE_BATCH_VIDEO _IOWR('v', 0x02, unsigned long)
#define VPP_UPDATE_BATCH_SPLIT _IOWR('v', 0x03, unsigned long)
#define VPP_UPDATE_BATCH_NON_CACHE _IOWR('v', 0x04, unsigned long)
#define VPP_UPDATE_BATCH_CROP_TEST _IOWR('v', 0x05, unsigned long)
#define VPP_GET_STATUS _IOWR('v', 0x06, unsigned long)
#define VPP_TOP_RST _IOWR('v', 0x07, unsigned long)
#define VPP_UPDATE_BATCH_VIDEO_FD_PA _IOWR('v', 0x08, unsigned long)
#define VPP_UPDATE_BATCH_FD_PA _IOWR('v', 0x09, unsigned long)
#define BMDEV_TRIGGER_VPP               _IOWR('p', 0x60, unsigned long)

typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef int                 LONG;
typedef unsigned char       BYTE;

typedef struct tagBITMAPFILEHEADER {
    WORD          bfType;
    DWORD         bfSize;
    WORD          bfReserved1;
    WORD          bfReserved2;
    DWORD         bfOffBits;
}__attribute__((packed, aligned(1))) BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{
    DWORD         biSize;
    LONG          biWidth;
    LONG          biHeight;
    WORD          biPlanes;
    WORD          biBitCount;
    DWORD         biCompression;
    DWORD         biSizeImage;
    LONG          biXPelsPerMeter;
    LONG          biYPelsPerMeter;
    DWORD         biClrUsed;
    DWORD         biClrImportant;
}__attribute__((packed, aligned(1))) BITMAPINFOHEADER;

typedef struct tagRGBQUAD{
    BYTE rgbRed;
    BYTE rgbGreen;
    BYTE rgbBlue;
    BYTE rgbReserved;
}__attribute__((packed, aligned(1))) RGBQUAD;


#define STRIDE_ALIGN    (64)
#define ALIGN(x, mask)  (((x) + ((mask)-1)) & ~((mask)-1))

#define VPP_MSG

#define VPP_MASK_ERR     0x1
#define VPP_MASK_WARN    0x2
#define VPP_MASK_INFO    0x4
#define VPP_MASK_DBG     0x8
#define VPP_MASK_TRACE   0x100

extern int vpp_level;

#ifdef VPP_MSG
#define VppErr(msg, ... )   if (vpp_level & VPP_MASK_ERR)   { printf("[ERR] %s = %d, "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define VppWarn(msg, ... )  if (vpp_level & VPP_MASK_WARN)  { printf("[WARN] %s = %d, "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define VppInfo(msg, ...)   if (vpp_level & VPP_MASK_INFO)  { printf("[INFO] %s = %d, "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define VppDbg(msg, ...)    if (vpp_level & VPP_MASK_DBG)   { printf("[DBG] %s = %d, "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define VppTrace(msg, ...)  if (vpp_level & VPP_MASK_TRACE) { printf("[TRACE] %s = %d, "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#else
#define VppErr(msg, ... )   if (vpp_level & VPP_MASK_ERR)   { printf("[ERR] %s = %d, "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); abort();}
#define VppWarn(msg, ...)
#define VppInfo(msg, ...)
#define VppDbg(msg, ...)
#define VppTrace(msg, ...)
#endif

#define VppPrint            printf
#define VppAssert(cond)     do { if (!(cond)) {printf("[vppassert]:%s<%d> : %s\n", __FILE__, __LINE__, #cond); abort();}} while (0)
void vpp_init_lib(void);

void vpp_resize(vpp_mat* src, vpp_mat* dst);
void vpp_border(struct vpp_mat_s* src, struct vpp_mat_s* dst, int top, int bottom, int left, int right);
void vpp_split_to_bgr(struct vpp_mat_s* src, struct vpp_mat_s* dst);
void vpp_split_to_rgb(vpp_mat *src, vpp_mat *dst);
void vpp_crop_single(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num);
void vpp_crop_multi(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num);
void vpp_yuv420_to_rgb(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type);
void vpp_misc(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, char csc_type);
void vpp_nv12_to_yuv420p(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type);

void i420tonv12(void * const in ,unsigned int w, unsigned int h, void * const out,unsigned int stride);
int vpp_read_file(vpp_mat *mat, const ion_dev_fd_s *ion_dev_fd, char *file_name);
int vpp_write_file(char *file_name, vpp_mat *mat);
void* vpp_ion_malloc(int rows, int stride, ion_para* para);
void* vpp_ion_malloc_len(int len, ion_para* para);
void vpp_ion_free(ion_para * para);
void vpp_ion_free_close_devfd(ion_para * para);
int vpp_bmp_header(int height, int width, int bitcount, FILE *fout);
int vpp_bmp_bgr888(char *img_name, unsigned char *bgr_data, int cols, int rows, int stride);
int vpp_output_mat_to_yuv420(char *file_name, vpp_mat *mat);
int vpp_bmp_gray(char *img_name, unsigned char *bgr_data, int cols, int rows, int stride);
void vpp_get_status(void);
int vpp_creat_ion_mem(vpp_mat *mat, int format, int in_w, int in_h);
int vpp_creat_ion_mem_fd(vpp_mat *mat, int format, int in_w, int in_h, const ion_dev_fd_s *ion_dev_fd);
void vpp_free_ion_mem(vpp_mat *mat);
#endif
