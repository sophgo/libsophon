#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "vppion.h"
#include "vpplib.h"

int vpp_level = VPP_MASK_ERR | VPP_MASK_WARN;
static int vpp_open_fd(struct vpp_mat_s* src,int *vpp_dev_fd);
static int vpp_batch_pa(struct vpp_cmd_n *cmd, struct vpp_mat_s* src,struct vpp_mat_s* dst);
static void check_env(void);
static int write_yuv420_planar(char *file_name, vpp_mat *mat);
static int write_yuv420_stride(char *file_name, vpp_mat *mat);
static int check_file_size(char *file_name, FILE **fp, int length);

void vpp_init_lib(void)
{
    check_env();
}

static void check_env(void)
{
    char *val = getenv("vpp_level");

    if (val)
    {
        sscanf(val, "0x%x", &vpp_level);
    }
}

int vpp_bmp_header(int height, int width, int bitcount, FILE *fout)
{
    VppAssert((bitcount == 24) || (bitcount == 8));
    int outextrabytes = 0;
    RGBQUAD rgbQuad[256];
    int gray;
    BITMAPFILEHEADER fileh;
    BITMAPINFOHEADER info;

    if (bitcount == 24) {
        outextrabytes = ALIGN((width * 3), 4) - (width * 3);
        info.biSizeImage = ALIGN((width * 3), 4) * height;// includes padding for 4 byte alignment
        fileh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    } else if (bitcount == 8) {
        outextrabytes = ALIGN(width, 4) - width;
        info.biSizeImage = ALIGN(width, 4) * height;// includes padding for 4 byte alignment
        fileh.bfOffBits = 1078;//sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(rgbQuad[256]);// sizeof(FileHeader)+sizeof(InfoHeader)+ sizeof(rgbQuad[256])
    } else {
        VppErr("bitcount only support 24 or 8");
    }

    // Fill the bitmap info structure
    info.biBitCount = bitcount;//24 bits per pixel {1,4,8,24}
    info.biClrImportant = 0;//important colors , 0:no used
    info.biClrUsed = 0;//colors in the colormap, 0:no used
    info.biCompression = 0;//no compression
    info.biPlanes = 1;//must 1
    info.biSize = sizeof(BITMAPINFOHEADER);
    info.biXPelsPerMeter = 4096;//width pixels per meter
    info.biYPelsPerMeter = 4096;//height pixels per meter

    info.biWidth = width;
    info.biHeight = height;

    // Fill the bitmap file header structure
    fileh.bfType = ((WORD)('M' << 8) | 'B');//"BM"
    fileh.bfReserved1 = 0;
    fileh.bfReserved2 = 0;

    fileh.bfSize = info.biSizeImage + fileh.bfOffBits;// This can be 0 for BI_RGB bitmaps

    if (fwrite(&fileh, 1, sizeof(BITMAPFILEHEADER), fout) != sizeof(BITMAPFILEHEADER)) {     // write bmp file header
        fclose(fout);
        VppErr("fwrite err1\n");
        return VPP_ERR;
    }
    if (fwrite(&info, 1, sizeof(BITMAPINFOHEADER), fout) != sizeof(BITMAPINFOHEADER)) {    // write bmp info header
        fclose(fout);
        VppErr("fwrite err2\n");
        return VPP_ERR;
    }

    if (bitcount == 8) {

        for (gray = 0; gray < 256; gray++)
        {
            rgbQuad[gray].rgbRed = rgbQuad[gray].rgbGreen = rgbQuad[gray].rgbBlue = gray;
            rgbQuad[gray].rgbReserved = 0;
        }

        if (fwrite((void *)rgbQuad, 1, 1024, fout) != 1024) {
            fclose(fout);
            VppErr("fwrite err3\n");
            return VPP_ERR;
       }
    }

    VppInfo("w %d, ALIGN((width * 3), 4) %d, outextrabytes %d\n", width, ALIGN((width * 3), 4), outextrabytes);
    return outextrabytes;
}

int vpp_bmp_bgr888(char *img_name, unsigned char *bgr_data, int cols, int rows, int stride)
{
    VppAssert(img_name != NULL);

    int extrabytes, idx;
    int valuable_data;
    unsigned char * prow;

    valuable_data = cols * 3;

    FILE *fp_bmp = fopen(img_name, "wb");
    if(!fp_bmp) {
        VppErr("ERROR: Unable to open the output file!\n");
         exit(-1);
    }

    extrabytes = vpp_bmp_header(rows, cols, 24, fp_bmp);

    for (prow = bgr_data + (rows - 1) * stride; prow >= bgr_data; prow-= stride) {
        if (fwrite((void *)prow, 1, valuable_data, fp_bmp) != valuable_data) {
            fclose(fp_bmp);
            return VPP_ERR;
        }

        for (idx = 0; idx < extrabytes; idx++) {
            if (fwrite((void *)(prow + valuable_data - 3), 1, 1, fp_bmp) != 1) {
                fclose(fp_bmp);
                return VPP_ERR;
            }
        }
    }

    fclose(fp_bmp);

    return VPP_OK;
}

int vpp_output_mat_to_yuv420(char *file_name, vpp_mat *mat)
{
    if (mat->stride == mat->cols)
    {
        if (write_yuv420_planar(file_name, mat) < 0)
        {
            VppErr("write_yuv420_planar\n");
            return -1;
        }
    }
    else
    {
        if (write_yuv420_stride(file_name, mat) < 0)
        {
            VppErr("write_yuv420_stride\n");
            return -1;
        }
    }

    return VPP_OK;
}

static int write_yuv420_planar(char *file_name, vpp_mat *mat)
{
    int y_size, uv_size;
    FILE *fp;

    VppAssert(file_name!= NULL);

    fp = fopen(file_name, "wb");
    if(!fp) {
        VppErr("ERROR: Unable to open the output file!\n");
         exit(-1);
    }

    y_size = mat->cols * mat->rows;
    uv_size = y_size >> 2;

    if (fwrite(mat->va[0], 1, y_size, fp) != y_size) {
        fclose(fp);
        return VPP_ERR;
    }

    if (fwrite(mat->va[1], 1, uv_size, fp) != uv_size) {
        fclose(fp);
        return VPP_ERR;
    }

    if (fwrite(mat->va[2], 1, uv_size, fp) != uv_size) {
        fclose(fp);
        return VPP_ERR;
    }

    fclose(fp);

    return 0;
}

static int write_yuv420_stride(char *file_name, vpp_mat *mat)
{
    int idx, h, w, s;
    unsigned char *ptr;
    FILE *fp;

    VppAssert(file_name!= NULL);

    fp = fopen(file_name, "wb");
    if(!fp) {
        VppErr("ERROR: Unable to open the output file!\n");
        exit(-1);
    }

    w = mat->cols;
    h = mat->rows;
    s = mat->stride;

    ptr = mat->va[0];
    for (idx = 0; idx < h; idx++)
    {
        if (fwrite(ptr, 1, w, fp) != w)
        {
            fclose(fp);
            return VPP_ERR;
        }
        ptr += s;
    }

    w = mat->cols >> 1;
    h = mat->rows >> 1;
    s = mat->stride >> 1;

    ptr = mat->va[1];
    for (idx = 0; idx < h; idx++)
    {
        if (fwrite(ptr, 1, w, fp) != w)
        {
            fclose(fp);
            return VPP_ERR;
        }
        ptr += s;
    }

    ptr = mat->va[2];
    for (idx = 0; idx < h; idx++)
    {
        if (fwrite(ptr, 1, w, fp) != w)
        {
            fclose(fp);
            return VPP_ERR;
        }
        ptr += s;
    }

    fclose(fp);

    return 0;
}

int vpp_bmp_gray(char *img_name, unsigned char *bgr_data, int cols, int rows, int stride)
{
    VppAssert(img_name != NULL);

    int extrabytes, idx;
    int valuable_data;
    unsigned char * prow;
    valuable_data = cols;

    FILE *fp_bmp = fopen(img_name, "wb");
    if(!fp_bmp) {
        VppErr("ERROR: Unable to open the output file!\n");
         exit(-1);
    }

    extrabytes = vpp_bmp_header(rows, cols, 8, fp_bmp);

    for (prow = bgr_data + (rows - 1) * stride; prow >= bgr_data; prow-= stride) {
        if (fwrite((void *)prow, 1, valuable_data, fp_bmp) != valuable_data) {
            fclose(fp_bmp);
            return VPP_ERR;
        }

        for (idx = 0; idx < extrabytes; idx++) {
            if (fwrite((void *)(prow + valuable_data - 3), 1, 1, fp_bmp) != 1) {
                fclose(fp_bmp);
                return VPP_ERR;
            }
        }
    }

    fclose(fp_bmp);

    return VPP_OK;
}

int vpp_creat_ion_mem(vpp_mat *mat, int format, int in_w, int in_h)
{
    VppAssert(mat != NULL);
    VppAssert((in_w <= MAX_RESOLUTION_W) && (in_h <= MAX_RESOLUTION_H));
    VppAssert((in_w >= MIN_RESOLUTION_W) && (in_h >= MIN_RESOLUTION_H));

    ion_para para;
    int idx;
    int ret = VPP_OK;

    memset(mat, 0, sizeof(vpp_mat));

    mat->format = format;
    mat->cols = in_w;
    mat->rows = in_h;

    switch (mat->format)
    {
        case FMT_BGR:
        case FMT_RGB:
            mat->num_comp = 1;
            mat->stride = ALIGN(in_w * 3, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_RGBP_C:
            mat->num_comp = 1;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h * 3;
            break;
        case FMT_RGBP:case FMT_BGRP:
            mat->num_comp = 3;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] =
            mat->ion_len[1] =
            mat->ion_len[2] = mat->stride * in_h;
            break;
        case FMT_I420:
            mat->num_comp = 3;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_NV12:
            mat->num_comp = 2;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * mat->stride;
            break;
        default:
            VppErr("format = %d\n", mat->format);
            ret = VPP_ERR;
            break;
    }

    VppTrace("num_comp = 0x%x\n", mat->num_comp);
    for (idx = 0; idx < mat->num_comp; idx++)
    {
        memset(&para, 0, sizeof(ion_para));

        if (vpp_ion_malloc_len(mat->ion_len[idx], &para) == NULL)
        {
            VppErr("allocate ion err\n", idx);
            ret = VPP_ERR;
            break;
        }

        mat->fd[idx] = para.memFd;
        mat->pa[idx] = para.pa;
        mat->va[idx] = para.va;
    }
    return ret;
}

int vpp_creat_ion_mem_fd(vpp_mat *mat, int format, int in_w, int in_h, const ion_dev_fd_s *ion_dev_fd)
{
    VppAssert(mat != NULL);
    VppAssert((in_w <= MAX_RESOLUTION_W) && (in_h <= MAX_RESOLUTION_H));
    VppAssert((in_w >= MIN_RESOLUTION_W) && (in_h > MIN_RESOLUTION_H));

    ion_para para;
    int idx;
    int ret = VPP_OK;

    memset(mat, 0, sizeof(vpp_mat));

    mat->format = format;
    mat->cols = in_w;
    mat->rows = in_h;

    switch (mat->format)
    {
        case FMT_BGR:
        case FMT_RGB:
            mat->num_comp = 1;
            mat->stride = ALIGN(in_w * 3, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_RGBP_C:
            mat->num_comp = 1;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h * 3;
            break;
        case FMT_RGBP:
            mat->num_comp = 3;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] =
            mat->ion_len[1] =
            mat->ion_len[2] = mat->stride * in_h;
            break;
        case FMT_I420:
            mat->num_comp = 3;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_NV12:
            mat->num_comp = 2;
            mat->stride = ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * mat->stride;
            break;
        default:
            VppErr("format = %d", mat->format);
            ret = VPP_ERR;
            break;
    }

    VppTrace("num_comp = 0x%x\n", mat->num_comp);
    for (idx = 0; idx < mat->num_comp; idx++)
    {
        memset(&para, 0, sizeof(ion_para));

        if (NULL != ion_dev_fd)
        {
            if ((ion_dev_fd->dev_fd > 0) && (strcmp(ion_dev_fd->name, "ion") == 0))
            {
                para.ionDevFd.dev_fd = ion_dev_fd->dev_fd;
                memcpy(para.ionDevFd.name,ion_dev_fd->name,sizeof(para.ionDevFd.name));
            }
        }
        if (vpp_ion_malloc_len(mat->ion_len[idx], &para) == NULL)
        {
            VppErr("allocate ion err\n", idx);
            ret = VPP_ERR;
            break;
        }

        mat->fd[idx] = para.memFd;
        mat->pa[idx] = para.pa;
        mat->va[idx] = para.va;
    }
    return ret;
}
void vpp_free_ion_mem(vpp_mat *mat)
{
    int idx;

    for (idx = 0; idx < mat->num_comp; idx++) {
        munmap(mat->va[idx], mat->ion_len[idx]);
        close(mat->fd[idx]);
    }
}

void i420tonv12(void * const in ,unsigned int w, unsigned int h, void * const out,unsigned int stride)
{
    int i = 0, j = 0;
    char *in_tmp, *out_tmp;

    VppAssert((in != NULL) && (out != NULL));
    VppAssert((h > 0) && (w > 0) && (stride > 0));
    VppAssert((h < 0x10000) && (w < 0x10000) && (stride < 0x10000));
    VppAssert((h % 2 ==0) && (w %2 == 0));

    memcpy(out, in, (h * stride));

    in_tmp = in + h * stride;
    out_tmp = out + h * stride;
    for(i = 0, j = 0; i < h * stride / 2;i = i + 2, j++)
    {
        out_tmp[i] = in_tmp[j];
    }

    for(i = 1, j = h * stride / 4; i < h * stride / 2; i = i + 2, j++)
    {
        out_tmp[i] = in_tmp[j];
    }
    return;
}


int vpp_write_file(char *file_name, vpp_mat *mat)
{
    FILE *fp;
    int status = 0;
    int idx = 0, i = 0, rows = 0, cols = 0, stride = 0;

    VppAssert(file_name!= NULL);
    fp = fopen(file_name, "wb");
    if(!fp) {
        VppErr("ERROR: Unable to open the output file!\n");
        exit(-1);
    }

    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                if (fwrite(mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }
            break;

        case FMT_RGBP:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (idx = 0; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    if (fwrite(mat->va[idx]+i*stride, 1, cols, fp) != cols)
                    {
                        fclose(fp);
                        return VPP_ERR;
                    }
                }
            }
            break;

        case FMT_I420:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                if (fwrite(mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }

            rows = mat->rows / 2;
            cols = mat->cols / 2;
            stride = mat->stride >> 1;
            for (idx = 1; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    if (fwrite(mat->va[idx]+i*stride, 1, cols, fp) != cols)
                    {
                        fclose(fp);
                        return VPP_ERR;
                    }
                }
            }
            break;
        default:
            VppErr("format = %d", mat->format);
            return -1;
    }

    fclose(fp);
    return status;
}

int vpp_read_file(vpp_mat *mat, const ion_dev_fd_s *ion_dev_fd, char *file_name)
{
    FILE *fp;
    int status = 0;
    int size = 0, idx = 0, i = 0, rows = 0, cols = 0, stride = 0;

    fp = fopen(file_name, "rb");
    if (fp == NULL)
    {
        printf("%s open failed\n", file_name);
        return -1;
    }
    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
        case FMT_RGBP: case FMT_RGBP_C:
            size = mat->rows * mat->cols *3;
            break;
        case FMT_I420:
            size = mat->rows * mat->cols *3/2;
            break;
        case FMT_NV12:
            size = mat->rows * mat->cols *3/2;
            break;
        default:
            VppErr("format = %d", mat->format);
            return -1;
    }
    //printf("num_comp %d, length %d, size %d, para[0].va %p\n",mat->num_comp, para[0].length, size, para[0].va);

    if (check_file_size(file_name, &fp, size) < 0)
    {
        fclose(fp);
        return -1;
    }

    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }
            break;

        case FMT_RGBP_C:
            rows = mat->rows *3;
            cols = mat->cols;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }
            break;

        case FMT_RGBP:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (idx = 0; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    fread(mat->va[idx]+i*stride, 1, cols, fp);
                }
            }
            break;
        case FMT_I420:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }

            rows = mat->rows / 2;
            cols = mat->cols / 2;
            stride = mat->stride >> 1;
            for (idx = 1; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    fread(mat->va[idx]+i*stride, 1, cols, fp);
                }
            }
            break;

        case FMT_NV12:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }

            rows = mat->rows >> 1;
            for (i = 0; i < rows; i++)
            {
                fread(mat->va[1]+i*stride, 1, cols, fp);
            }
            break;

        default:
            VppErr("format = %d", mat->format);
            return -1;
    }
    for (idx = 0; idx < mat->num_comp; idx++)
    {
        ion_flush(ion_dev_fd, mat->va[idx], mat->ion_len[idx]);
    }

    fclose(fp);
    return status;
}
static int check_file_size(char *file_name, FILE **fp, int length)
{
    int len;

    fseek(*fp, 0, SEEK_END);
    len = ftell(*fp);

    if (len != length)
    {
        VppErr("%s len not matched input %d but expect %d\n",
                file_name,
                len,
                length);
        fclose(*fp);
        return -1;
    }

    fseek(*fp, 0, SEEK_SET);

    return 0;
}

void* vpp_ion_malloc(int rows, int stride, ion_para* para)
{
    VppAssert((rows > 0) && (para != NULL) && (stride > 0));

    int devFd, ext_ion_devfd = 0;
    if (stride & (STRIDE_ALIGN -1)) {
        VppErr("stride not 64B alige\n");
    }
    para->length = rows * stride;

    if ((para->ionDevFd.dev_fd > 0) && (strcmp(para->ionDevFd.name, "ion") == 0)) {
        devFd = para->ionDevFd.dev_fd;
        ext_ion_devfd = 1;
    } else {
        devFd = open("/dev/ion", O_RDWR | O_DSYNC);
        if (devFd < 0) {
            VppErr("open /dev/ion failed, errno = %d, msg: %s\n", errno, strerror(errno));
            goto err0;
        }
        ext_ion_devfd = 0;
    }

    if (ionMalloc(devFd, para) == NULL) {
        VppErr("ionMalloc failed");
    }

    if (ext_ion_devfd == 0) {
        close(devFd);
    }
err0:
    return para->va;
}

void* vpp_ion_malloc_len(int len, ion_para* para)
{
    VppAssert((len > 0) && (para != NULL));

    int devFd, ext_ion_devfd = 0;

    if (len & (STRIDE_ALIGN -1)) {
        VppErr("len not 64B alige\n");
    }
    para->length = len;

    if ((para->ionDevFd.dev_fd > 0) && (strcmp(para->ionDevFd.name, "ion") == 0)) {
        /*do nothing*/
        devFd = para->ionDevFd.dev_fd;
        ext_ion_devfd = 1;
    } else {
        devFd = open("/dev/ion", O_RDWR | O_DSYNC);
        if (devFd < 0) {
            VppErr("open /dev/ion failed, errno = %d, msg: %s\n", errno, strerror(errno));
            goto err0;
        }
        ext_ion_devfd = 0;
    }

    if (ionMalloc(devFd, para) == NULL) {
        VppErr("ionMalloc failed");
    }

    if (ext_ion_devfd == 0) {
        close(devFd);
    }
err0:
    return para->va;
}

void vpp_ion_free(ion_para * para)
{
    ionFree(para);
}

void vpp_ion_free_close_devfd(ion_para * para)
{
    ionFree(para);
    close(para->ionDevFd.dev_fd);
}

int vpp_batch_pa(struct vpp_cmd_n *cmd, struct vpp_mat_s* src,struct vpp_mat_s* dst)
{
    VppAssert((cmd != NULL) && (src != NULL) && (dst != NULL));
    int ret = 0;

    switch (src->is_pa) {
    case 1:
        cmd->src_fd0 = 0;
        cmd->src_fd1 = 0;
        cmd->src_fd2 = 0;
        cmd->src_addr0 = src->pa[0];
        cmd->src_addr1 = src->pa[1];
        cmd->src_addr2 = src->pa[2];
        break;
    case 0:
        cmd->src_fd0 = src->fd[0];
        cmd->src_fd1 = src->fd[1];
        cmd->src_fd2 = src->fd[2];
        cmd->src_addr0 = 0;
        cmd->src_addr1 = 0;
        cmd->src_addr2 = 0;
        break;
    default:
        ret = -1;
        VppErr("False parameters \n");
        break;
    }

    switch (dst->is_pa) {
    case 1:
        cmd->dst_fd0 = 0;
        cmd->dst_fd1 = 0;
        cmd->dst_fd2 = 0;
        cmd->dst_addr0 = dst->pa[0];
        cmd->dst_addr1 = dst->pa[1];
        cmd->dst_addr2 = dst->pa[2];
        break;
    case 0:
        cmd->dst_fd0 = dst->fd[0];
        cmd->dst_fd1 = dst->fd[1];
        cmd->dst_fd2 = dst->fd[2];
        cmd->dst_addr0 = 0;
        cmd->dst_addr1 = 0;
        cmd->dst_addr2 = 0;
        break;
    default:
        ret = -1;
        VppErr("False parameters \n");
        break;
    }
    VppDbg("src_fd0 = 0x%lx, src_fd1 = 0x%lx, src_fd2 = 0x%lx\n", cmd->src_fd0, cmd->src_fd1, cmd->src_fd2);
    VppDbg("src_addr0 = 0x%lx, src_addr1 = 0x%lx, src_addr2 = 0x%lx\n", cmd->src_addr0, cmd->src_addr1, cmd->src_addr2);
    VppDbg("dst_fd0 = 0x%lx, dst_fd1 = 0x%lx, dst_fd2 = 0x%lx\n", cmd->dst_fd0, cmd->dst_fd1, cmd->dst_fd2);
    VppDbg("dst_addr0 = 0x%lx, dst_addr1 = 0x%lx, dst_addr2 = 0x%lx\n", cmd->dst_addr0, cmd->dst_addr1, cmd->dst_addr2);

    return ret;
}

int vpp_open_fd(struct vpp_mat_s* src,int *vpp_dev_fd)
{
    VppAssert((src != NULL) && (vpp_dev_fd != NULL));
    int fd_external = 0;
    if ((src->vpp_fd.dev_fd > 0) && (strcmp(src->vpp_fd.name, "bm-vpp") == 0)) {
        *vpp_dev_fd = src->vpp_fd.dev_fd;
        fd_external = 1;
        VppDbg("fd %d, name %s\n", src->vpp_fd.dev_fd, src->vpp_fd.name);
    } else {
        *vpp_dev_fd = open("/dev/bm-vpp", O_RDWR);
        if (*vpp_dev_fd < 0) {
            VppErr("open /dev/bm-vpp failed, errno = %d, msg: %s\n", errno, strerror(errno));
        }
        fd_external = 0;
    }
    return fd_external;
}

void vpp_resize(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    VppAssert((src != NULL) && (dst != NULL));
    VppAssert((src->format == FMT_BGR) || (src->format == FMT_I420) || (src->format == FMT_NV12) ||
             (src->format == FMT_RGB) || (src->format == FMT_RGBP));
    VppAssert((dst->format == FMT_BGR) || (dst->format == FMT_I420) || (dst->format == FMT_RGB) ||
             (dst->format == FMT_RGBP));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((dst->cols <= MAX_RESOLUTION_W) && (dst->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert((dst->cols >= MIN_RESOLUTION_W) && (dst->rows >= MIN_RESOLUTION_H));
    VppAssert((src->stride >= ALIGN(src->cols, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));
    VppAssert((src->cols * 32 >= dst->cols) && (src->rows * 32 >= dst->rows));

    if (dst->format == FMT_I420) {
        VppAssert((src->format == FMT_I420) || (src->format == FMT_NV12));
        VppAssert((src->cols % 2== 0) && (src->rows % 2== 0) && (src->axisX % 2== 0) && (src->axisY % 2== 0));
        VppAssert((dst->cols % 2== 0) && (dst->rows % 2== 0) && (dst->axisX % 2== 0) && (dst->axisY % 2== 0));
    }

    int ret = 0;
    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(src,&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;

    struct vpp_cmd_n *cmd = &batch.cmd[0];

    if (src->format == FMT_NV12) {
        src->format = 1;//NV12 for bm1682
    }
    cmd->src_format = src->format;
    cmd->dst_format = dst->format;
    cmd->src_stride = src->stride;
    cmd->dst_stride = dst->stride;
    cmd->src_cropH = src->rows;
    cmd->src_cropW = src->cols;
    cmd->dst_cropH = dst->rows;
    cmd->dst_cropW = dst->cols;

    ret = vpp_batch_pa(cmd,src,dst);
    if (ret < 0) {
        VppErr("pa config wrong\n");
    }

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);

    if (ret < 0) {
        VppErr("ioctl resize failed\n");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }
    return;
}

void vpp_border(struct vpp_mat_s* src, struct vpp_mat_s* dst, int top, int bottom, int left, int right)
{
    VppAssert((src != NULL) && (dst != NULL));
    VppAssert( top >= 0 && bottom >= 0 && left >= 0 && right >= 0 );
    VppAssert((src->format == FMT_BGR) || (dst->format == FMT_BGR));
    VppAssert((src->stride >= ALIGN(src->cols*3, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols*3, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert(((src->cols + left + right) <= MAX_RESOLUTION_W) && ((src->rows + top + bottom) <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert(((src->cols + left + right) >= MIN_RESOLUTION_W) && ((src->rows + top + bottom) >= MIN_RESOLUTION_H));
    VppDbg("cols = %d, cols = %d\n", src->cols, src->rows);

    int ret = 0;

    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(src,&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;
    struct vpp_cmd_n *cmd = &batch.cmd[0];

    cmd->src_format = FMT_BGR;
    cmd->dst_format = FMT_BGR;
    cmd->src_cropH = src->rows;
    cmd->src_cropW = src->cols;
    cmd->src_stride = src->stride;
    cmd->dst_stride = dst->stride;
    cmd->dst_cropH = src->rows;
    cmd->dst_cropW = src->cols;
    cmd->dst_axisX = left;
    cmd->dst_axisY = top;

    ret = vpp_batch_pa(cmd,src,dst);
    if (ret < 0) {
        VppErr("pa config wrong\n");
    }
    

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl border failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }

    return;
}


void vpp_split_to_bgr(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    VppAssert((src != NULL) && (dst != NULL));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert((src->format == FMT_BGR) && (dst->format == FMT_RGBP_C));
    VppAssert((src->stride >= ALIGN(src->cols*3, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));

    int stride = ALIGN(src->cols, STRIDE_ALIGN);
    int ret = 0;
    int vpp_dev_fd = -1, fd_external = 0;

    fd_external = vpp_open_fd(src,&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;

    struct vpp_cmd_n *cmd = &batch.cmd[0];

    cmd->src_format = FMT_BGR;
    cmd->dst_format = FMT_RGBP;
    cmd->src_fd0 = src->fd[0];
    cmd->src_fd1 = 0;
    cmd->src_fd2 = 0;
    cmd->dst_fd0 = dst->fd[0];
    cmd->dst_fd1 = 0;
    cmd->dst_fd2 = 0;
    cmd->src_stride = src->stride;
    cmd->dst_stride = stride;
    cmd->src_cropH = src->rows;
    cmd->src_cropW = src->cols;
    cmd->dst_cropH = src->rows;
    cmd->dst_cropW = src->cols;


    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_SPLIT, &batch);
    if (ret < 0) {
        VppErr("ioctl VPP_UPDATE_BATCH_SPLIT failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }

    return;
}

void vpp_split_to_rgb(vpp_mat *src, vpp_mat *dst)
{
    VppAssert((src != NULL) && (dst != NULL));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert((src->format == FMT_BGR) && (dst->format == FMT_RGBP));
    VppAssert((src->stride >= ALIGN(src->cols*3, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));

    int stride = ALIGN(src->cols, STRIDE_ALIGN);
    int ret = 0;
    int vpp_dev_fd = -1, fd_external = 0;

    fd_external = vpp_open_fd(src,&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }


    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;
    struct vpp_cmd_n *cmd = &batch.cmd[0];

    cmd->src_format = FMT_BGR;
    cmd->dst_format = FMT_RGBP;
    cmd->src_cropH = src->rows;
    cmd->src_cropW = src->cols;
    cmd->dst_cropH = src->rows;
    cmd->dst_cropW = src->cols;
    cmd->src_stride = src->stride;
    cmd->dst_stride = stride;

    ret = vpp_batch_pa(cmd,src,dst);
    if (ret < 0) {
        VppErr("pa config wrong\n");
    }

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl vpp_split_component failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }


    return;
}

void vpp_crop_single(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    VppAssert((src != NULL) && (dst != NULL) && (loca != NULL));
    VppAssert(crop_num > 0 && crop_num <= 16);
    VppAssert((src->format == FMT_BGR) || (src->format == FMT_I420) || (src->format == FMT_NV12) ||
             (src->format == FMT_RGB) || (src->format == FMT_RGBP));
    VppAssert((src->stride >= ALIGN(src->cols, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    for (int i = 0; i < crop_num; i++) {
        VppAssert((loca[i].x >= 0) && (loca[i].y >= 0));
        VppAssert((loca[i].x <= src->cols) && (loca[i].y <= src->rows));
        VppAssert(((loca[i].x + loca[i].width) <= src->cols) && ((loca[i].y + loca[i].height) <= src->rows));
        VppAssert((loca[i].width >= MIN_RESOLUTION_W) && (loca[i].height >= MIN_RESOLUTION_H));
        VppAssert((dst[i].format == FMT_BGR) || (dst[i].format == FMT_I420) || (dst[i].format == FMT_RGB) 
            || (dst[i].format == FMT_RGBP) || (dst[i].format == FMT_BGRP));
        VppAssert((dst[i].stride >= ALIGN(dst[i].cols, STRIDE_ALIGN)) && ((dst[i].stride % STRIDE_ALIGN) == 0));
        VppAssert((dst[i].cols <= MAX_RESOLUTION_W) && (dst[i].rows <= MAX_RESOLUTION_H));
        VppAssert((dst[i].cols >= MIN_RESOLUTION_W) && (dst[i].rows >= MIN_RESOLUTION_H));
        VppAssert((loca[i].width * 32 >= dst[i].cols) && (loca[i].height * 32 >= dst[i].rows));
        VppAssert((loca[i].width <= dst[i].cols * 32) && (loca[i].height <= dst[i].rows * 32));

        if (((src->format == FMT_I420) || (src->format == FMT_NV12)) && (dst[i].format == FMT_I420)){
            VppAssert((loca[i].x % 2== 0) && (loca[i].y % 2== 0) && (loca[i].width % 2== 0) && (loca[i].height % 2== 0));
            VppAssert((dst[i].cols % 2== 0) && (dst[i].rows % 2== 0) && (dst[i].axisX % 2== 0) && (dst[i].axisY % 2== 0));
        }
        if (dst[i].format == FMT_I420) {
            VppAssert((dst[i].cols % 2== 0) && (dst[i].rows % 2== 0) && (dst[i].axisX % 2== 0) && (dst[i].axisY % 2== 0));
        }
    }

    int ret = 0;

    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(src,&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = crop_num;

    for (int i = 0; i < crop_num; i++) {
        struct vpp_cmd_n *cmd = &batch.cmd[i];

        if (src->format == FMT_NV12) {
            src->format = 1;//NV12 for bm1682
        }
        cmd->src_format = src->format;
        cmd->dst_format = dst[i].format;
        if ( cmd->dst_format == FMT_BGRP) {
            cmd->dst_format = FMT_RGBP;
        }

        if (src->is_pa) {
            cmd->src_addr0 = src->pa[0];
            cmd->src_addr1 = src->pa[1];
            cmd->src_addr2 = src->pa[2];
        } else {
            cmd->src_fd0 = src->fd[0];
            cmd->src_fd1 = src->fd[1];
            cmd->src_fd2 = src->fd[2];
        }
        if (dst[i].is_pa) {
            if (dst[i].format == FMT_BGRP) {
                cmd->dst_addr0 = dst[i].pa[2];
                cmd->dst_addr1 = dst[i].pa[1];
                cmd->dst_addr2 = dst[i].pa[0];
            }else{
                cmd->dst_addr0 = dst[i].pa[0];
                cmd->dst_addr1 = dst[i].pa[1];
                cmd->dst_addr2 = dst[i].pa[2];
            }
        } else {
            if (dst[i].format == FMT_BGRP) {
                cmd->dst_fd0 = dst[i].fd[2];
                cmd->dst_fd1 = dst[i].fd[1];
                cmd->dst_fd2 = dst[i].fd[0];
            } else {
                cmd->dst_fd0 = dst[i].fd[0];
                cmd->dst_fd1 = dst[i].fd[1];
                cmd->dst_fd2 = dst[i].fd[2];
            }
        }
        cmd->src_cropH = loca[i].height;
        cmd->src_cropW = loca[i].width;
        cmd->src_axisX = loca[i].x;
        cmd->src_axisY = loca[i].y;
        cmd->src_stride = src->stride;
        cmd->dst_stride = dst[i].stride;
        cmd->dst_cropH = dst[i].rows;
        cmd->dst_cropW = dst[i].cols;
    }

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl crop single failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }

    return;
}

void vpp_crop_multi(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    VppAssert((src != NULL) && (dst != NULL) && (loca != NULL));
    VppAssert(crop_num > 0 && crop_num <= 16);
    for (int i = 0; i < crop_num; i++) {
        VppAssert((src[i].cols <= MAX_RESOLUTION_W) && (src[i].rows <= MAX_RESOLUTION_H));
        VppAssert((loca[i].x >= 0) && (loca[i].y >= 0));
        VppAssert((loca[i].x <= src[i].cols) && (loca[i].y <= src[i].rows));
        VppAssert(((loca[i].x + loca[i].width) <= src[i].cols) && ((loca[i].y + loca[i].height) <= src[i].rows));
        VppAssert((src[i].cols >= MIN_RESOLUTION_W) && (src[i].rows >= MIN_RESOLUTION_H));
        VppAssert((loca[i].width >= MIN_RESOLUTION_W) && (loca[i].height >= MIN_RESOLUTION_H));
        VppAssert(src[i].format == FMT_BGR);
        VppAssert(dst[i].format == FMT_BGR);
        VppAssert((src[i].stride >= ALIGN(src[i].cols*3, STRIDE_ALIGN)) && ((src[i].stride % STRIDE_ALIGN) == 0));
        VppAssert((dst[i].stride >= ALIGN(dst[i].cols*3, STRIDE_ALIGN)) && ((dst[i].stride % STRIDE_ALIGN) == 0));
        VppAssert((dst[i].cols <= MAX_RESOLUTION_W) && (dst[i].rows <= MAX_RESOLUTION_H));
        VppAssert((dst[i].cols >= MIN_RESOLUTION_W) && (dst[i].rows >= MIN_RESOLUTION_H));
        VppAssert((loca[i].width * 32 >= dst[i].cols) && (loca[i].height * 32 >= dst[i].rows));
    }

    int ret = 0;
    int vpp_dev_fd = -1, fd_external = 0;

    fd_external = vpp_open_fd(&src[0],&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = crop_num;

    for (int i = 0; i < crop_num; i++) {
        struct vpp_cmd_n *cmd = &batch.cmd[i];

        if (src->format == FMT_NV12) {
            src->format = 1;//NV12 for bm1682
        }
        cmd->src_format = src->format;
        cmd->dst_format = dst->format;
        cmd->src_stride = src[i].stride;
        cmd->dst_stride = dst[i].stride;
        cmd->dst_cropH = dst[i].rows;
        cmd->dst_cropW = dst[i].cols;
        cmd->src_cropH = loca[i].height;
        cmd->src_cropW = loca[i].width;
        cmd->src_axisX = loca[i].x;
        cmd->src_axisY = loca[i].y;
        if (src[i].is_pa) {
            cmd->src_addr0 = src[i].pa[0];
        } else {
            cmd->src_fd0 = src[i].fd[0];
        }
        if (dst[i].is_pa) {
            cmd->dst_addr0 = dst[i].pa[0];
        } else {
            cmd->dst_fd0 = dst[i].fd[0];
        }
    }

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl crop multi failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }

    return;
}

void vpp_yuv420_to_rgb(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type)
{
    VppAssert((src != NULL) && (dst != NULL) && ((csc_type == 0) || (csc_type == 1)));
    VppAssert((src->format == FMT_I420) || (src->format == FMT_NV12));
    VppAssert((dst->format == FMT_BGR) || (dst->format == FMT_RGB));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((dst->cols <= MAX_RESOLUTION_W) && (dst->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert((dst->cols >= MIN_RESOLUTION_W) && (dst->rows >= MIN_RESOLUTION_H));
    VppAssert((src->stride >= ALIGN(src->cols, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols * 3, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));
    VppAssert((src->cols * 32 >= dst->cols) && (src->rows * 32 >= dst->rows));
    if (src->format == FMT_I420) {
        VppAssert((src->cols % 2== 0) && (src->rows % 2== 0));
    }

    int ret = 0;
    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(&src[0],&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;
    struct vpp_cmd_n *cmd = &batch.cmd[0];


    if (src->format == FMT_NV12) {
        src->format = 1;//NV12 for bm1682
    }
    cmd->src_format = src->format;
    cmd->dst_format = dst->format;
    cmd->src_stride = src->stride;
    cmd->dst_stride = dst->stride;
    cmd->src_cropH = src->rows;
    cmd->src_cropW = src->cols;
    cmd->dst_cropH = dst->rows;
    cmd->dst_cropW = dst->cols;
    cmd->csc_type = csc_type;
    ret = vpp_batch_pa(cmd,src,dst);
    if (ret < 0) {
        VppErr("pa config wrong\n");
    }

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl vpp_yuv420_to_rgb failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }

    return;
}

void vpp_misc(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, char csc_type)
{
    VppAssert((src != NULL) && (dst != NULL) && (loca != NULL));
    VppAssert((src->format == FMT_I420) || (src->format == FMT_NV12) || (src->format == FMT_RGBP));
    VppAssert((dst->format == FMT_BGR) || (dst->format == FMT_RGB)  || (dst->format == FMT_I420) || (dst->format == FMT_RGBP));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert((src->stride >= ALIGN(src->cols, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));
    VppAssert((loca->x >= 0) && (loca->x <= src->cols));
    VppAssert((loca->y >= 0) && (loca->y <= src->rows));
    VppAssert((loca->width >= MIN_RESOLUTION_W) && (loca->width <= src->cols));
    VppAssert((loca->height >= MIN_RESOLUTION_W) && (loca->height <= src->rows));
    VppAssert(((loca->x + loca->width) <= src->cols) && ((loca->y + loca->height) <= src->rows));
    VppAssert((loca->width * 32 >= dst->cols) && (loca->height * 32 >= dst->rows));
    VppAssert((dst->cols <= MAX_RESOLUTION_W) && (dst->rows <= MAX_RESOLUTION_H));
    VppAssert((dst->cols >= MIN_RESOLUTION_W) && (dst->rows >= MIN_RESOLUTION_H));
    if (((src->format == FMT_I420) || (src->format == FMT_NV12)) && (dst->format == FMT_I420)){
        VppAssert((loca->x % 2== 0) && (loca->y % 2== 0) && (loca->width % 2== 0) && (loca->height % 2== 0));
        VppAssert((dst->cols % 2== 0) && (dst->rows % 2== 0) && (dst->axisX % 2== 0) && (dst->axisY % 2== 0));
    }
    if (dst->format == FMT_I420) {
        VppAssert((dst->cols % 2== 0) && (dst->rows % 2== 0) && (dst->axisX % 2== 0) && (dst->axisY % 2== 0));
    }

    int ret = 0;

    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(&src[0],&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;

    struct vpp_cmd_n *cmd = &batch.cmd[0];

    if (src->format == FMT_NV12) {
        src->format = 1;//NV12 for bm1682
    }
    cmd->src_format = src->format;
    cmd->dst_format = dst->format;
    cmd->src_stride = src->stride;
    cmd->dst_stride = dst->stride;
    cmd->src_cropH = loca->height;
    cmd->src_cropW = loca->width;
    cmd->src_axisX = loca->x;
    cmd->src_axisY = loca->y;
    cmd->dst_cropH = dst->rows;
    cmd->dst_cropW = dst->cols;
    cmd->csc_type = csc_type;

    ret = vpp_batch_pa(cmd,src,dst);
    if (ret < 0) {
        VppErr("pa config wrong\n");
    }

    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl vpp_yuv420_to_rgb failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }


    return;
}

void vpp_nv12_to_yuv420p(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type)
{
    VppAssert((src != NULL) && (dst != NULL) && ((csc_type == 0) || (csc_type == 1)));
    VppAssert((dst->format == FMT_I420) && (src->format == FMT_NV12));
    VppAssert((src->cols <= MAX_RESOLUTION_W) && (src->rows <= MAX_RESOLUTION_H));
    VppAssert((dst->cols <= MAX_RESOLUTION_W) && (dst->rows <= MAX_RESOLUTION_H));
    VppAssert((src->cols >= MIN_RESOLUTION_W) && (src->rows >= MIN_RESOLUTION_H));
    VppAssert((dst->cols >= MIN_RESOLUTION_W) && (dst->rows >= MIN_RESOLUTION_H));
    VppAssert((src->stride >= ALIGN(src->cols, STRIDE_ALIGN)) && ((src->stride % STRIDE_ALIGN) == 0));
    VppAssert((dst->stride >= ALIGN(dst->cols, STRIDE_ALIGN)) && ((dst->stride % STRIDE_ALIGN) == 0));
    VppAssert((src->cols * 32 >= dst->cols) && (src->rows * 32 >= dst->rows));
    if (dst->format == FMT_I420) {
        VppAssert((dst->cols % 2== 0) && (dst->rows % 2== 0));
    }

    int ret = 0;
    int vpp_dev_fd = -1, fd_external = 0;

    fd_external = vpp_open_fd(&src[0],&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        return;
    }

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;

    struct vpp_cmd_n *cmd = &batch.cmd[0];

    if (src->format == FMT_NV12) {
        src->format = 1;//NV12 for bm1682
    }
    cmd->src_format = src->format;
    cmd->dst_format = dst->format;
    cmd->src_stride = src->stride;
    cmd->dst_stride = dst->stride;
    cmd->src_cropH = src->rows;
    cmd->src_cropW = src->cols;
    cmd->dst_cropH = dst->rows;
    cmd->dst_cropW = dst->cols;
    cmd->csc_type = csc_type;
    ret = vpp_batch_pa(cmd,src,dst);
    if (ret < 0) {
        VppErr("pa config wrong\n");
    }

   ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret < 0) {
        VppErr("ioctl vpp_nv12_to_yuv420p failed");
    }

    if (!fd_external) {
        close(vpp_dev_fd);
    }


    return;
}

void vpp_get_status(void)
{

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;

    int fd = open("/dev/bm-vpp", O_RDWR);
    if (fd < 0) {
        VppErr("open /dev/bm-vpp failed");
    }

    int ret = ioctl(fd, VPP_GET_STATUS, &batch);

    if (ret < 0) {
        VppErr("ioctl vpp_get_status failed");
    }

    close(fd);
}

void vpp_top_rst(void)
{

    struct vpp_batch_n batch;
    memset(&batch,0,sizeof(batch));
    batch.num = 1;

    int fd = open("/dev/bm-vpp", O_RDWR);
    if (fd < 0) {
        VppErr("open /dev/bm-vpp failed");
    }

    int ret = ioctl(fd, VPP_TOP_RST, &batch);

    if (ret < 0) {
        VppErr("ioctl vpp_top_rst failed");
    }

    close(fd);
}

