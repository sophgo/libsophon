#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
extern "C" {
#include "vppion.h"
#include "vpplib.h"
}

using namespace cv;

// BGR or RGB format
int mat_to_vppmat(Mat& img, vpp_mat *src,int format)
{
    if (src == NULL){
        printf("src is NULL\n");
        return -2;
    }
    switch (format)
    {
        case FMT_BGR: case FMT_RGB:
            src->stride = img.step.p[0];
            src->pa[0]  = img.u->paddr;
            src->pa[1]  = 0;
            src->pa[2]  = 0;
            src->rows   = img.rows;
            src->cols   = img.cols;
            src->is_pa  = 1;
            src->num_comp = 1;
            src->format = format;
            src->va[0]   = img.u->data; 
            src->ion_len[0] = src->stride * src->rows;
            break;
        default:
            printf("format = %d", format);
            return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int format;
    char *input_name;
    char* dst_img_name;
    int out_w;
    int out_h;

    vpp_mat src;
    vpp_mat dst;
    memset(&src, 0, sizeof(vpp_mat));    /*This operation must be done*/
    format = FMT_BGR;

    if (argc != 5)
    {
        printf("usage: %d\n", argc);
        printf("%s input_name out_w out_h dst_img_name\n", argv[0]);
        return 0;
    }

    input_name   = argv[1];
    out_w        = atoi(argv[2]);
    out_h        = atoi(argv[3]);
    dst_img_name = argv[4];

    //get input pic
    Mat input_img;
    input_img = imread(input_name);
    mat_to_vppmat(input_img , &src , format);

    // malloc dst ion mempry
    if (vpp_creat_ion_mem(&dst, FMT_BGR, out_w , out_h) == VPP_ERR) {
        return VPP_ERR;
    }
    dst.is_pa = 1;

    ion_flush(NULL, src.va[0], src.ion_len[0]);
    ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);

    // resize
    vpp_resize(&src, &dst);

    vpp_bmp_bgr888(dst_img_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

    vpp_free_ion_mem(&dst);
    return 0;
}

