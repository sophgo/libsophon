#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "vppion.h"
#include "vpplib.h"

int main(int argc, char *argv[])
{
    ion_dev_fd_s ion_dev_fd;
    int vpp_dev_fd;
    int src_fmt, dst_fmt;
    vpp_mat src, dst;
    struct vpp_rect_s loca;
    char *input_name;
    int in_w, in_h = 0, out_w, out_h = 0;
    char *img_name;
    ion_para src_para;
    ion_para dst_para;
    void *src_va = 0;
    void *dst_va = 0;
    int src_stride = 0;    /*src image stride*/
    int dst_stride = 0;    /*dst image stride*/

    if (argc != 13) {
        printf("usage: %d\n", argc);
        printf("%s src_width src_height img2.yuv x y width height  src-format dst-format dst_width dst_height dst_img_name\n", argv[0]);
        printf("example:\n");
        printf("yuv420p-> RGBP:%s 1920 1080 img2.yuv  960 540 960 540  0 2 2048 1024 out.rgbp\n", argv[0]);
        printf("yuv420p-> yuv420p:%s 1920 1080 img2.yuv  960 540 960 540  0 0 2048 1024 out.i420\n", argv[0]);
        printf("nv12-> yuv420p:%s 1920 1080 img1.nv12  128 364 1080 586  7 0 2012 768 out.i420\n", argv[0]);
        printf("nv12-> RGBP:%s 1920 1080 img1.nv12  128 364 1080 586  7 2 2012 768 out.rgbp\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);    /*src image w*/
    in_h = atoi(argv[2]);    /*src image h*/
    input_name = argv[3];    /*src image name*/
    loca.x = atoi(argv[4]);    /*offset of crop image on the x axis*/
    loca.y = atoi(argv[5]);    /*offset of crop image on the y axis*/
    loca.width= atoi(argv[6]);    /*width of crop image*/
    loca.height= atoi(argv[7]);    /*height of crop image*/
    src_fmt = atoi(argv[8]);    /*src image format w*/
    dst_fmt = atoi(argv[9]);    /*dst image format*/
    out_w = atoi(argv[10]);    /*dst image w*/
    out_h = atoi(argv[11]);    /*dst image h*/
    img_name = argv[12];/*dst image name*/

    /*open /dev/ion*/
    ion_dev_fd.dev_fd = open("/dev/ion", O_RDWR | O_DSYNC);
    if (ion_dev_fd.dev_fd < 0) {
        VppErr("open /dev/ion failed, errno = %d, msg: %s\n", errno, strerror(errno));
        return VPP_ERR;
    }
    memcpy(ion_dev_fd.name,"ion",sizeof("ion"));

    /*open /dev/bm-vpp*/
    vpp_dev_fd = open("/dev/bm-vpp", O_RDWR | O_DSYNC);
    if (vpp_dev_fd < 0) {
        VppErr("open /dev/bm-vpp failed, errno = %d, msg: %s\n", errno, strerror(errno));
        return VPP_ERR;
    }

    /*check vpp debug level, not necessary*/
    vpp_init_lib();

    memset(&src_para, 0, sizeof(ion_para));
    memset(&dst_para, 0, sizeof(ion_para));

    /*set /dev/ion device fd. If you set the device fd correctly,vpp_ion_malloc_len() 
    or vpp_ion_malloc() does not need to open the /dev/ion every time to get the device fd*/
    src_para.ionDevFd = ion_dev_fd;
    dst_para.ionDevFd = ion_dev_fd;

    memset(&src, 0, sizeof(vpp_mat));    /*This operation must be done*/
    memset(&dst, 0, sizeof(vpp_mat));    /*This operation must be done*/

    switch (src_fmt)
    {
        case FMT_I420:
            src.num_comp = 3;    /*If you don't call the vpp_read_file(), you don't care about this parameter.*/
            src_stride = ALIGN(in_w, STRIDE_ALIGN);    /*stride must 64B align for vpp hw*/
            src.ion_len[0] = src_stride * in_h;     /*data length*/
            src.ion_len[2] = src.ion_len[1] = (src_stride >> 1) * (in_h >> 1);
            break;
        case FMT_RGBP:
            src.num_comp = 3;    /*If you don't call the vpp_read_file(), you don't care about this parameter.*/
            src_stride = ALIGN(in_w, STRIDE_ALIGN);    /*stride must 64B align for vpp hw*/
            src.ion_len[2] = src.ion_len[1] = src.ion_len[0] = src_stride * in_h;
            break;
        case FMT_NV12:
            src.num_comp = 2;
            src_stride = ALIGN(in_w, STRIDE_ALIGN);
            src.ion_len[0] = src_stride * in_h;
            src.ion_len[1] = src_stride * (in_h >> 1);
            src.ion_len[2] = 0;
            break;
        default:
            VppErr("not supported: src_fmt = %d,dst_fmt = %d\n", src_fmt, dst_fmt);
            break;
    }

    switch (dst_fmt)
    {
        case FMT_I420:
            dst.num_comp = 3;    /*If you don't call the vpp_read_file(), you don't care about this parameter.*/
            dst_stride = ALIGN(out_w, STRIDE_ALIGN);    /*stride must 64B align for vpp hw*/
            dst.ion_len[0] = dst_stride * out_h;
            dst.ion_len[2] = dst.ion_len[1] = (dst_stride >> 1) * (out_h >> 1);
            break;
        case FMT_RGBP:
            dst.num_comp = 3;    /*If you don't call the vpp_read_file(), you don't care about this parameter.*/
            dst_stride = ALIGN(out_w, STRIDE_ALIGN);    /*stride must 64B align for vpp hw*/
            dst.ion_len[2] = dst.ion_len[1] = dst.ion_len[0] = dst_stride * out_h;
            break;
        case FMT_BGR:
            dst.num_comp = 1;    /*If you don't call the vpp_read_file(), you don't care about this parameter.*/
            dst_stride = ALIGN(out_w * 3, STRIDE_ALIGN);    /*stride must 64B align for vpp hw*/
            dst.ion_len[0] = dst_stride * out_h ;
            dst.ion_len[2] = dst.ion_len[1] = 0;
            break;
        default:
            VppErr("not supported: src_fmt = %d,dst_fmt = %d\n", src_fmt, dst_fmt);
            break;
    }

    /*alloc ion mem.u can use vpp_ion_malloc() or vpp_ion_malloc to get ion mem*/
    //src_va = vpp_ion_malloc(in_h, src_stride, &src_para);
    src_va = vpp_ion_malloc_len((src.ion_len[0] + src.ion_len[1] + src.ion_len[2]), &src_para);
    if (src_va == NULL) 
    {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err0;
    }

    /*Allocate the ion memory for the destination image and configure the destination image parameters*/
    dst_va = vpp_ion_malloc_len((dst.ion_len[0] + dst.ion_len[1] + dst.ion_len[2]), &dst_para);
    if (dst_va == NULL) 
    {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err1;
    }

    src.fd[0] = 0;    /*ion mem fd,if not use ,please set 0*/
    src.fd[1] = 0;    /*ion mem fd,if not use ,do not care*/
    src.fd[2] = 0;    /*ion mem fd,if not use ,do not care*/
    src.pa[0] = src_para.pa;    /*data Physical address*/
    src.pa[1] = src_para.pa + src.ion_len[0];    /*data Physical address,if not use ,do not care*/
    src.pa[2] = src_para.pa + src.ion_len[0] + src.ion_len[1];    /*data Physical address,if not use ,do not care*/
    src.va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[1] = src_para.va + src.ion_len[0];    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[2] = src_para.va + src.ion_len[0] + src.ion_len[1];    /*data Virtual address,if not use ,do not care, vpp hw not use va*/

    src.format = src_fmt;/*data format*/
    src.cols = in_w;    /*image width*/
    src.rows = in_h;    /*image height*/
    src.stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/

    /*set /dev/bm-vpp device fd. If you set the device fd correctly,
    vpp driver does not need to open the /dev/bm-vpp every time to get the device fd*/
    src.vpp_fd.dev_fd = vpp_dev_fd;
    src.vpp_fd.name = "bm-vpp";
    src.is_pa = 1;

    dst.fd[0] = 0;
    dst.fd[1] = 0;
    dst.fd[2] = 0;
    dst.pa[0] = dst_para.pa;
    dst.pa[1] = dst_para.pa + dst.ion_len[0];
    dst.pa[2] = dst_para.pa + dst.ion_len[0] + dst.ion_len[1];
    dst.va[0] = dst_para.va;
    dst.va[1] = dst_para.va + dst.ion_len[0];
    dst.va[2] = dst_para.va + dst.ion_len[0] + dst.ion_len[1];

    dst.format = dst_fmt;
    dst.cols = out_w;
    dst.rows = out_h;
    dst.stride = dst_stride;
    dst.is_pa = 1;

    /*Write source image data into ion mem*/
    if (vpp_read_file(&src, &ion_dev_fd, input_name) < 0)
    {
        goto err2;
    }

    /*ion cache flush*/
    ion_flush(&ion_dev_fd, src_va, src_para.length);//vpp_read_file called ion_flush
    /*ion cache invalid*/
    ion_invalidate(&ion_dev_fd, dst_va, dst_para.length);

    /*Call vpp hardware driver api*/
    vpp_misc(&src, &loca, &dst, 0);

    /*Generate a yuv420 data from the dst data*/
    if (vpp_write_file(img_name, &dst) < 0)
    {
        VppErr("vpp_write_file failed, errno = %d, msg: %s\n", errno, strerror(errno));
    }

err2:
    vpp_ion_free(&dst_para);    /*Release dst image memory*/
err1:
    vpp_ion_free(&src_para);    /*Release src image memory*/
err0:
    close(ion_dev_fd.dev_fd);
    close(vpp_dev_fd);
    return 0;
}

