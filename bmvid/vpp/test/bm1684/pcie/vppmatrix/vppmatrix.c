#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "vpplib.h"
#if !defined _WIN32 
#include <unistd.h>
#include <sys/ioctl.h>
#include "vppion.h"
#endif

struct csc_matrix matrix[12] = {
//YCbCr2RGB,BT601
{0x000004a8, 0x00000000, 0x00000662, 0xfffc8450,
	0x000004a8, 0xfffffe6f, 0xfffffcc0, 0x00021e4d,
	0x000004a8, 0x00000812, 0x00000000, 0xfffbaca8 },
//YPbPr2RGB,BT601
{0x00000400, 0x00000000, 0x0000059b, 0xfffd322d,
	0x00000400, 0xfffffea0, 0xfffffd25, 0x00021dd6,
	0x00000400, 0x00000716, 0x00000000, 0xfffc74bc },
//RGB2YCbCr,BT601
{ 0x107, 0x204, 0x64, 0x4000,
	0xffffff68, 0xfffffed6, 0x1c2, 0x20000,
	0x1c2, 0xfffffe87, 0xffffffb7, 0x20000 },
//YCbCr2RGB,BT709
{ 0x000004a8, 0x00000000, 0x0000072c, 0xfffc1fa4,
	0x000004a8, 0xffffff26, 0xfffffdde, 0x0001338e,
	0x000004a8, 0x00000873, 0x00000000, 0xfffb7bec },
//RGB2YCbCr,BT709
{ 0x000000bb, 0x00000275, 0x0000003f, 0x00004000,
	0xffffff99, 0xfffffea5, 0x000001c2, 0x00020000,
	0x000001c2, 0xfffffe67, 0xffffffd7, 0x00020000 },
//RGB2YPbPr,BT601
{ 0x132, 0x259, 0x74, 0,
	0xffffff54, 0xfffffead, 0x200, 0x20000,
	0x200, 0xfffffe54, 0xffffffad, 0x20000 },
//YPbPr2RGB,BT709
{ 0x00000400, 0x00000000, 0x0000064d, 0xfffcd9be,
	0x00000400, 0xffffff40, 0xfffffe21, 0x00014fa1,
	0x00000400, 0x0000076c, 0x00000000, 0xfffc49ed },
//RGB2YPbPr,BT709
{ 0x000000da, 0x000002dc, 0x0000004a, 0,
	0xffffff8b, 0xfffffe75, 0x00000200, 0x00020000,
	0x00000200, 0xfffffe2f, 0xffffffd1, 0x00020000 },

{	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0 },

{	1, 1, 1, 1,
	1, 1, 1, 1,
	1, 1, 1, 1 },

{	0x5a, 0x5a, 0x5a, 0x5a,
	0x5a, 0x5a, 0x5a, 0x5a,
	0x5a, 0x5a, 0x5a, 0x5a },

{	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff }
};
int main(int argc, char *argv[])
{
    int src_fmt, dst_fmt;
    int convert_fmt,ret = VPP_OK;
    int width , height, mode = 0;
    vpp_mat src;
    vpp_mat dst;
    char *img_name;
    char *input_name;
    struct vpp_rect_s loca;

    if (argc != 8)
    {
        printf("usage: %d\n", argc);
        printf("%s width height src_img_name  src-format dst-format dst_img_name csc_matrix\n", argv[0]);
        printf("format: 0 -- FMT_Y, 1 -- FMT_I420, 2 -- FMT_NV12, 3 -- FMT_BGR, 4 -- FMT_RGB, 5 -- FMT_RGBP, 6 -- FMT_BGRP\n");
        printf("7 -- FMT_YUV444P, 8 -- FMT_YUV422P,9 -- FMT_YUV444\n");
        printf("example:\n");
        printf("FMT_BGRP     -> FMT_YUV444P:  %s 1920 1080 bgrp.bin  6 7 yuv444p  2\n", argv[0]);
        printf("FMT_YUV444P  -> FMT_BGRP   :  %s 1920 1080 yuv444p.bin  7 6 bgrpmatrix.bin  0\n", argv[0]);
        return 0;
    }

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    input_name = argv[3];
    src_fmt = atoi(argv[4]);
    dst_fmt = atoi(argv[5]);
    img_name = argv[6];
    mode = atoi(argv[7]);

    int idx;
    bm_handle_t handle = NULL;

    bm_device_mem_t dev_buffer_src[3];
    bm_device_mem_t dev_buffer_dst[3];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }
    src.vpp_fd.handle = handle;
    dst.vpp_fd.handle = handle;

    vpp_creat_host_and_device_mem(dev_buffer_src, &src, src_fmt, width, height);
    vpp_creat_host_and_device_mem(dev_buffer_dst, &dst, dst_fmt, width, height);

    vpp_read_file_pcie(&src, dev_buffer_src, input_name);

    loca.height = src.rows;
    loca.width = src.cols;
    loca.x = 0;
    loca.y = 0;

    vpp_misc_matrix(&src, &loca, &dst, 1, CSC_USER_DEFINED, VPP_SCALE_BILINEAR, &(matrix[mode]));

    for (idx = 0; idx < dst.num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst.va[idx], dev_buffer_dst[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    ret = output_file(img_name, &dst);
    if(VPP_OK != ret)
        printf("output file wrong: ret = %d\n", ret);

    for (idx = 0; idx < src.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_src[idx]);
        free(src.va[idx]);
    }
    for (idx = 0; idx < dst.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_dst[idx]);
        free(dst.va[idx]);
    }

    return 0;
}
