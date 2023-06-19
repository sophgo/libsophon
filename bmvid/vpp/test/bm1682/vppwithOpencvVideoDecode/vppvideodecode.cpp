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

extern "C"
{
#include "vppion.h"
#include "vpplib.h"
}

using namespace cv;
int mat_to_vppmat(Mat& img, vpp_mat *src,int format);
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
            src->is_pa = 1;
            src->num_comp = 1;
            src->format = format; 
            break;
        default:
            printf("format = %d", format);
            return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    VideoCapture cap;
    vpp_mat src, dst;
    char *input_name , *output_name;
    Mat frame;

    if (argc != 3) {
        printf("usage: %d\n", argc);
        printf("%s input.avi out.bmp\n", argv[0]);
        return 0;
    }

    input_name = argv[1];    /*src file name*/
    output_name = argv[2];    /*dst image name*/

    memset(&src, 0, sizeof(vpp_mat));    /*This operation must be done*/
    cap.open(input_name); 

    if(!cap.isOpened())
        return -1;

    cap.set(cv::CAP_PROP_OUTPUT_YUV, 1.0);
    cap.read(frame);

    int height = (int) cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    int width  = (int) cap.get(CV_CAP_PROP_FRAME_WIDTH);

    Mat image(height, width, CV_8UC3);
    vpp::toMat(frame, image);

    mat_to_vppmat(image, &src,FMT_BGR);

    if (vpp_creat_ion_mem(&dst, FMT_BGR, width, height) == VPP_ERR) {
        goto err0;
    }
    dst.is_pa = 1;

    /*ion cache invalid*/
    ion_invalidate(NULL, dst.va[0], dst.ion_len[0]);

  /*Call vpp hardware driver api*/
    vpp_resize(&src, &dst);

    vpp_bmp_bgr888(output_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

    vpp_free_ion_mem(&dst);
err0:
    cap.release();
    return 0;
}

