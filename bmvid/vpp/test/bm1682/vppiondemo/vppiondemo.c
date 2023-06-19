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
    int is_src_pa, is_dst_pa;
    vpp_mat src;
    vpp_mat dst;
    char *input_name;
    int in_w, in_h;
    int top;
    int bottom;
    int left;
    int right;
    char *img_name;
    ion_para src_para;
    ion_para dst_para;
    void *src_va;
    void *dst_va;

    if (argc != 11) {
        printf("usage: %d\n", argc);
        printf("%s width height bgr-org-opencv-bmp3.bin top bottom left right is_src_pa is_dst_pa dst_img_name\n", argv[0]);
        printf("example:\n");
        printf("        %s 1920 1080 bgr-org-opencv-bmp3.bin 32 32 32 32 0 0 out.bmp\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);    /*src image w*/
    in_h = atoi(argv[2]);    /*src image h*/
    int src_stride;    /*src image stride*/
    int dst_stride;    /*dst image stride*/

    input_name = argv[3];    /*src image name*/
    top = atoi(argv[4]);    /*The number of pixels that need to be filled on the top boundary*/
    bottom = atoi(argv[5]);    /*The number of pixels that need to be filled on the bottom boundary*/
    left = atoi(argv[6]);    /*The number of pixels that need to be filled on the left boundary*/
    right = atoi(argv[7]);    /*The number of pixels that need to be filled on the right boundary*/
    is_src_pa    = atoi(argv[8]);    /*Tell vpp driver, src data address is ion fd or physical address*/
    is_dst_pa    = atoi(argv[9]);    /*Tell vpp driver, dst data address is ion fd or physical address*/
    img_name = argv[10];/*dst image name*/

    VppAssert((is_src_pa == 0) || (is_src_pa == 1));
    VppAssert((is_dst_pa == 0) || (is_dst_pa == 1));

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
    /*Allocate the ion memory for the destination image and configure the destination image parameters*/

    /*set /dev/ion device fd. If you set the device fd correctly,vpp_ion_malloc_len() 
    or vpp_ion_malloc() does not need to open the /dev/ion every time to get the device fd*/
    src_para.ionDevFd = ion_dev_fd;

    /*stride must 64B align for vpp hw*/
    src_stride = ALIGN(in_w * 3, STRIDE_ALIGN);

    /*alloc ion mem.u can use vpp_ion_malloc() or vpp_ion_malloc to get ion mem*/
    //src_va = vpp_ion_malloc(in_h, src_stride, &src_para);
    src_va = vpp_ion_malloc_len(in_h * src_stride, &src_para);
    if (src_va == NULL) {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err0;
    }

    memset(&src, 0, sizeof(vpp_mat));    /*This operation must be done*/
    memset(&dst, 0, sizeof(vpp_mat));    /*This operation must be done*/

    src.ion_len[0] = src_para.length;    /*data length*/
    src.fd[0] = src_para.memFd;    /*ion mem fd,if not use ,please set 0*/
    src.fd[1] = 0;    /*ion mem fd,if not use ,do not care*/
    src.fd[2] = 0;    /*ion mem fd,if not use ,do not care*/
    src.pa[0] = src_para.pa;    /*data Physical address*/
    src.pa[1] = 0;    /*data Physical address,if not use ,do not care*/
    src.pa[2] = 0;    /*data Physical address,if not use ,do not care*/
    src.va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[1] = 0;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[2] = 0;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/

    src.format = FMT_BGR;/*data format*/
    src.cols = in_w;    /*image width*/
    src.rows = in_h;    /*image height*/
    src.stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/
    src.num_comp = 1;    /*If you don't call the vpp_read_file(), you don't care about this parameter.*/

    /*set /dev/bm-vpp device fd. If you set the device fd correctly,
    vpp driver does not need to open the /dev/bm-vpp every time to get the device fd*/
    src.vpp_fd.dev_fd = vpp_dev_fd;
    src.vpp_fd.name = "bm-vpp";

    src.is_pa = is_src_pa;
    src.axisX = 0;/*X-axis of cropping source starting point , if not crop ,set 0*/
    src.axisY = 0;/*Y-axis of cropping source starting point, if not crop ,set 0*/


    /*Allocate the ion memory for the destination image and configure the destination image parameters*/

    dst_stride = ALIGN((in_w + left + right) * 3, STRIDE_ALIGN);

    //dst_va = vpp_ion_malloc(in_h + top + bottom, dst_stride, &dst_para);
    dst_va = vpp_ion_malloc_len((in_h + top + bottom) * dst_stride, &dst_para);
    if (dst_va == NULL) {
        VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
        goto err1;
    }

    dst.ion_len[0] = dst_para.length;
    dst.fd[0] = dst_para.memFd;
    dst.fd[1] = 0;
    dst.fd[2] = 0;
    dst.pa[0] = dst_para.pa;
    dst.va[0] = dst_para.va;

    dst.format = FMT_BGR;
    dst.cols = in_w + left + right;
    dst.rows = in_h + top + bottom;
    dst.stride = dst_stride;

    dst.is_pa = is_dst_pa;

    /*Write source image data into ion mem*/
    if (vpp_read_file(&src, &ion_dev_fd, input_name) < 0)
    {
        goto err2;
    }
    memset(dst.va[0],0,dst.ion_len[0]);
    /*ion cache flush*/
    ion_flush(&ion_dev_fd, src_va, src_para.length);
    /*ion cache invalid*/
    ion_invalidate(&ion_dev_fd, dst_para.va, dst_para.length);

    /*Call vpp hardware driver api*/
    vpp_border(&src, &dst, top, bottom, left, right);

    /*Generate a bmp image from the dst image data*/
    vpp_bmp_bgr888(img_name, (unsigned  char *)dst.va[0], dst.cols, dst.rows, dst.stride);

err2:
    vpp_ion_free(&dst_para);    /*Release dst image memory*/
err1:
    vpp_ion_free(&src_para);    /*Release src image memory*/
err0:
    close(ion_dev_fd.dev_fd);
    close(vpp_dev_fd);
    return 0;
}
