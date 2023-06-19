#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "vpplib.h"
#if !defined _WIN32 
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "vppion.h"
#include "1684vppcmodel.h"
#else
#pragma comment(lib, "libbmlib.lib")
#endif
#define VPP_ERR_COPY_FROM_USER             (-2)
#define VPP_ERR_WRONG_CROPNUM              (-3)
#define VPP_ERR_INVALID_FD                 (-4)
#define VPP_ERR_INT_TIMEOUT                (-5)
#define VPP_ERR_INVALID_PA                 (-6)
#define VPP_ERR_INVALID_CMD                (-7)

#define VPP_ENOMEM                         (-12)
#define VPP_ERR_IDLE_BIT_MAP               (-256)
#define VPP_ERESTARTSYS                    (-512)

enum {
    VPP_FILTER_SEL_BICUBIC_UP   = 1,
    VPP_FILTER_SEL_BICUBIC_DOWN = 4,
    VPP_FILTER_SEL_BILINEAR     = 5,
    VPP_FILTER_SEL_NEAREST      = 7,
};

int vpp_level = VPP_MASK_ERR | VPP_MASK_WARN | VPP_MASK_INFO;

static int vpp_batch_format(struct vpp_cmd_n *cmd, int src_format,int dst_format);
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
    if((bitcount != 24) && (bitcount != 8)) {
        printf("vpp_bmp_header parms err. bitcount=%d.\n", bitcount);
        return VPP_ERR;
    }

    int outextrabytes = 0;
    RGBQUAD RGBQUAD[256];
    int gray;
    BITMAPFILEHEADER fileh;
    BITMAPINFOHEADER info;

    if (bitcount == 24) {
        outextrabytes = VPP_ALIGN((width * 3), 4) - (width * 3);
        info.biSizeImage = VPP_ALIGN((width * 3), 4) * height;// includes padding for 4 byte alignment
        fileh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    } else if (bitcount == 8) {
        outextrabytes = VPP_ALIGN(width, 4) - width;
        info.biSizeImage = VPP_ALIGN(width, 4) * height;// includes padding for 4 byte alignment
        fileh.bfOffBits = 1078;//sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD[256]);// sizeof(FileHeader)+sizeof(InfoHeader)+ sizeof(RGBQUAD[256])
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
            RGBQUAD[gray].rgbRed = RGBQUAD[gray].rgbGreen = RGBQUAD[gray].rgbBlue = gray;
            RGBQUAD[gray].rgbReserved = 0;
        }

        if (fwrite((void *)RGBQUAD, 1, 1024, fout) != 1024) {
            fclose(fout);
            VppErr("fwrite err3\n");
            return VPP_ERR;
       }
    }

    VppDbg("w %d, ALIGN((width * 3), 4) %d, outextrabytes %d\n", width, VPP_ALIGN((width * 3), 4), outextrabytes);
    return outextrabytes;
}

int vpp_bmp_bgr888(char *img_name, unsigned char *bgr_data, int cols, int rows, int stride)
{
    if(img_name == NULL) {
        printf("vpp_bmp_gbr888 parms err. img_name=0X%p.\n", img_name);
        return VPP_ERR;
    }

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

    if(file_name == NULL) {
        printf("write_yuv420_planar parms err. file_name=0X%p.\n", file_name);
        return VPP_ERR;
    }

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

    if(file_name == NULL) {
        printf("write_yuv420_planar parms err. file_name=0X%p.\n", file_name);
        return VPP_ERR;
    }

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
    if(img_name == NULL) {
        printf("write_yuv420_planar parms err. img_name=0X%p.\n", img_name);
        return VPP_ERR;
    }

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

void vpp_dump(struct vpp_batch_n *batch)
{
    struct vpp_cmd_n *cmd ;
    for (int i = 0; i < batch->num; i++) {
        cmd = (batch->cmd + i);
        VppInfo("batch->num    is  %d      \n",batch->num);
        VppInfo("cmd id %d, cmd->src_format  0x%x\n",i,cmd->src_format);
        VppInfo("cmd id %d, cmd->src_stride  0x%x\n",i,cmd->src_stride);
        VppInfo("cmd id %d, cmd->src_uv_stride  0x%x\n",i,cmd->src_uv_stride);
        VppInfo("cmd id %d, cmd->src_endian  0x%x\n",i,cmd->src_endian);
        VppInfo("cmd id %d, cmd->src_endian_a  0x%x\n",i,cmd->src_endian_a);
        VppInfo("cmd id %d, cmd->src_plannar  0x%x\n",i,cmd->src_plannar);
        VppInfo("cmd id %d, cmd->src_fd0  0x%x\n",i,cmd->src_fd0);
        VppInfo("cmd id %d, cmd->src_fd1  0x%x\n",i,cmd->src_fd1);
        VppInfo("cmd id %d, cmd->src_fd2  0x%x\n",i,cmd->src_fd2);
        VppInfo("cmd id %d, cmd->src_addr0  0x%lx\n",i,cmd->src_addr0);
        VppInfo("cmd id %d, cmd->src_addr1  0x%lx\n",i,cmd->src_addr1);
        VppInfo("cmd id %d, cmd->src_addr2  0x%lx\n",i,cmd->src_addr2);
        VppInfo("cmd id %d, cmd->src_axisX  0x%x\n",i,cmd->src_axisX);
        VppInfo("cmd id %d, cmd->src_axisY  0x%x\n",i,cmd->src_axisY);
        VppInfo("cmd id %d, cmd->src_cropW  0x%x\n",i,cmd->src_cropW);
        VppInfo("cmd id %d, cmd->src_cropH  0x%x\n",i,cmd->src_cropH);
        VppInfo("cmd id %d, cmd->dst_format  0x%x\n",i,cmd->dst_format);
        VppInfo("cmd id %d, cmd->dst_stride  0x%x\n",i,cmd->dst_stride);
        VppInfo("cmd id %d, cmd->dst_uv_stride  0x%x\n",i,cmd->dst_uv_stride);
        VppInfo("cmd id %d, cmd->dst_endian  0x%x\n",i,cmd->dst_endian);
        VppInfo("cmd id %d, cmd->dst_endian_a  0x%x\n",i,cmd->dst_endian_a);
        VppInfo("cmd id %d, cmd->dst_plannar  0x%x\n",i,cmd->dst_plannar);
        VppInfo("cmd id %d, cmd->dst_fd0  0x%x\n",i,cmd->dst_fd0);
        VppInfo("cmd id %d, cmd->dst_fd1  0x%x\n",i,cmd->dst_fd1);
        VppInfo("cmd id %d, cmd->dst_fd2  0x%x\n",i,cmd->dst_fd2);
        VppInfo("cmd id %d, cmd->dst_addr0  0x%lx\n",i,cmd->dst_addr0);
        VppInfo("cmd id %d, cmd->dst_addr1  0x%lx\n",i,cmd->dst_addr1);
        VppInfo("cmd id %d, cmd->dst_addr2  0x%lx\n",i,cmd->dst_addr2);
        VppInfo("cmd id %d, cmd->dst_axisX  0x%x\n",i,cmd->dst_axisX);
        VppInfo("cmd id %d, cmd->dst_axisY  0x%x\n",i,cmd->dst_axisY);
        VppInfo("cmd id %d, cmd->dst_cropW  0x%x\n",i,cmd->dst_cropW);
        VppInfo("cmd id %d, cmd->dst_cropH  0x%x\n",i,cmd->dst_cropH);
        VppInfo("cmd id %d, cmd->src_csc_en  0x%x\n",i,cmd->src_csc_en);
        VppInfo("cmd id %d, cmd->hor_filter_sel  0x%x\n",i,cmd->hor_filter_sel);
        VppInfo("cmd id %d, cmd->ver_filter_sel  0x%x\n",i,cmd->ver_filter_sel);
        VppInfo("cmd id %d, cmd->scale_x_init  0x%x\n",i,cmd->scale_x_init);
        VppInfo("cmd id %d, cmd->scale_y_init  0x%x\n",i,cmd->scale_y_init);
        VppInfo("cmd id %d, cmd->csc_type  0x%x\n",i,cmd->csc_type);
        VppInfo("cmd id %d, cmd->mapcon_enable  0x%x\n",i,cmd->mapcon_enable);
        VppInfo("cmd id %d, cmd->src_fd3  0x%x\n",i,cmd->src_fd3);
        VppInfo("cmd id %d, cmd->src_addr3  0x%lx\n",i,cmd->src_addr3);
        VppInfo("cmd id %d, cmd->cols  0x%x\n",i,cmd->cols);
        VppInfo("cmd id %d, cmd->rows  0x%x\n",i,cmd->rows);
    }


    return;
}

static void  vpp_return_value_analysis(int ret)
{
    VppErr("vpp_ioctl failed, ret %d,errno = %d\n",ret, errno);
    switch (-errno)
    {
        case VPP_ERR_COPY_FROM_USER:
            VppErr("copy_from_user failed\n");
            break;
        case VPP_ERR_WRONG_CROPNUM:
            VppErr("crop num is error\n");
            break;
        case VPP_ERR_INVALID_FD:
            VppErr("fd map pa failed\n");
            break;
        case VPP_ERR_INT_TIMEOUT:
            VppErr("interrupt timeout,vpp maybe hang\n");
            break;
        case VPP_ERR_INVALID_PA:
            VppErr("pa is invalid after check ion region\n");
            break;
        case VPP_ENOMEM:
            VppErr("kzalloc failed in driver, memory exhaustion\n");
            break;
        case VPP_ERR_IDLE_BIT_MAP:
            VppErr("vpp bit_map wrong. fatal logical error\n");
            break;
        case VPP_ERESTARTSYS:
            VppErr("driver is interrupted by signal or anyting other\n");
            break;
        case VPP_ERR_INVALID_CMD:
            VppErr("Ioctl command word mismatch between user and driver\n");
            break;
        case VPP_ERR:
//            VppErr("general error\n");
            break;
        default:
            VppErr("can not analysis\n");
            break;
    }

    return;
}

#ifdef PCIE_MODE
int vpp_creat_host_and_device_mem(bm_device_mem_t *dev_buffer, vpp_mat *mat, int format, int in_w, int in_h)
{
    if(!(mat != NULL) ||
       !((in_w <= MAX_RESOLUTION_W) && (in_h <= MAX_RESOLUTION_H)) ||
       !((in_w >= MIN_RESOLUTION_W) && (in_h >= MIN_RESOLUTION_H))) {
        VppErr("vpp_creat_host_and_device_mem parms err. mat(0X%lx), in_w(%d), in_h(%d).\n", mat, in_w, in_h);
        return VPP_ERR;
    }

    int idx;
    int ret = VPP_OK;

    mat->format = format;
    mat->axisX = 0;
    mat->axisY = 0;
    mat->cols = in_w;
    mat->rows = in_h;
    mat->is_pa = 1;
    mat->uv_stride = 0;

    switch (mat->format)
    {
        case FMT_BGR:
        case FMT_RGB:
        case FMT_YUV444:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w * 3, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_ABGR:
        case FMT_ARGB:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w * 4, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_RGBP:
        case FMT_BGRP:
        case FMT_YUV444P:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] =
            mat->ion_len[1] =
            mat->ion_len[2] = mat->stride * in_h;
            break;
        case FMT_YUV422P:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = in_h * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_I420:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_NV12:
            mat->num_comp = 2;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * mat->stride;
            break;
        case FMT_Y:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        default:
            VppErr("format = %d\n", mat->format);
            ret = VPP_ERR;
            break;
    }

    VppTrace("num_comp = 0x%x\n", mat->num_comp);
    for (idx = 0; idx < mat->num_comp; idx++)
    {
        void *p_va;

        p_va = malloc(mat->ion_len[idx]);
        if (NULL == p_va) {
            VppErr("allocate host mem err\n", idx);
            ret = VPP_ERR;
            break;
        }

        ret = bm_malloc_device_byte_heap((bm_handle_t )mat->vpp_fd.handle, &dev_buffer[idx], DDR_CH, mat->ion_len[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("malloc device memory size = %d failed, ret = %d\n", mat->ion_len[idx], ret);
        }

        mat->fd[idx] = 0;
        mat->pa[idx] = dev_buffer[idx].u.device.device_addr;
        mat->va[idx] = p_va;
    }

    return ret;
}

int vpp_read_file_pcie(vpp_mat *mat, bm_device_mem_t *dev_buffer_src, char *file_name)
{
    FILE *fp;
    int ret = 0;
    int size = 0, idx = 0, i = 0, rows = 0, cols = 0, stride = 0;

    fp = fopen(file_name, "rb");
    if (fp == NULL)
    {
        VppErr("%s open failed\n", file_name);
        return -1;
    }
    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
        case FMT_RGBP:case FMT_BGRP:
        case FMT_YUV444P: case FMT_YUV444:
            size = mat->rows * mat->cols *3;
            break;
        case FMT_ABGR: case FMT_ARGB:
            size = mat->rows * mat->cols *4;
            break;
        case FMT_YUV422P:
            size = mat->rows * mat->cols *2;
            break;
        case FMT_I420:
            size = mat->rows * mat->cols *3/2;
            break;
        case FMT_NV12:
            size = mat->rows * mat->cols *3/2;
            break;
        case FMT_Y:
            size = mat->rows * mat->cols;
            break;
        default:
            VppErr("format = %d", mat->format);
            return -1;
    }

    if (check_file_size(file_name, &fp, size) < 0)
    {
        fclose(fp);
        return -1;
    }

    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
        case FMT_YUV444:
            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[0] + i * stride, 1, cols, fp);
            }
            break;
        case FMT_ABGR: case FMT_ARGB:
            rows = mat->rows;
            cols = mat->cols * 4;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[0] + i * stride, 1, cols, fp);
            }
            break;
        case FMT_RGBP: case FMT_BGRP:
        case FMT_YUV444P:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (idx = 0; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    fread((char *)mat->va[idx] + i * stride, 1, cols, fp);
                }
            }
            break;
        case FMT_YUV422P:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[0] + i * stride, 1, cols, fp);
            }

            rows = mat->rows;
            cols = mat->cols / 2;
            stride = (mat->uv_stride == 0) ? (mat->stride >> 1) : mat->uv_stride;
            for (idx = 1; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    fread((char *)mat->va[idx] + i * stride, 1, cols, fp);
                }
            }
            break;
        case FMT_I420:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[0] + i * stride, 1, cols, fp);
            }

            rows = mat->rows / 2;
            cols = mat->cols / 2;
            stride = (mat->uv_stride == 0) ? (mat->stride >> 1) : mat->uv_stride;
            for (idx = 1; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    fread((char *)mat->va[idx] + i * stride, 1, cols, fp);
                }
            }
            break;

        case FMT_NV12:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[0] + i * stride, 1, cols, fp);
            }

            rows = mat->rows >> 1;
            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[1] + i * stride, 1, cols, fp);
            }
            break;

        case FMT_Y:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread((char *)mat->va[0] + i * stride, 1, cols, fp);
            }
            break;

        default:
            VppErr("format = %d", mat->format);
            return -1;
    }

    for (idx = 0; idx < mat->num_comp; idx++)
    {
        ret = bm_memcpy_s2d((bm_handle_t )mat->vpp_fd.handle, dev_buffer_src[idx], mat->va[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    fclose(fp);
    return ret;
}

int vpp_disguise_y400_as_nv12_pcie(bm_handle_t handle, struct vpp_mat_s* src,
  struct vpp_mat_s* dst, int crop_num, bm_device_mem_t *dev_buf_uv, int *need_restore)
{
    int ret = VPP_OK;
    int crop_idx;
    int src_fmt;
    int dst_fmt;
    int src_h, dst_h;
    int src_w, dst_w;
    int src_stride, dst_stride;
    int uv_data_len;

    for (crop_idx = 0; crop_idx < crop_num; crop_idx++) {
        src_fmt = src[crop_idx].format;
        dst_fmt = dst[crop_idx].format;

        if ((src_fmt == FMT_Y) && ((dst_fmt == FMT_BGR) || (dst_fmt == FMT_RGB) || (dst_fmt == FMT_RGBP) ||
           (dst_fmt == FMT_BGRP) || (dst_fmt == FMT_ARGB) || (dst_fmt == FMT_ABGR))) {
            src_w = src[crop_idx].cols;
            src_h = src[crop_idx].rows;
            src_stride = src[crop_idx].stride;
            if (src_stride < src_w) {
                VppErr("vpp_disguise_y400_as_nv12_pcie src_stride(%d) < src_w(%d).\n", src_stride, src_w);
                return VPP_ERR;
            }
            if (src_stride % STRIDE_ALIGN != 0) {
                VppErr("vpp_disguise_y400_as_nv12_pcie stride not align. src_stride(%d), STRIDE_ALIGN(%d).\n", src_stride, STRIDE_ALIGN);
                return VPP_ERR;
            }

            uv_data_len = VPP_ALIGN(src_h, 2) / 2 * src_stride;

            /*alloc device mem for uv data*/
            if (!((handle != NULL) && (dev_buf_uv != NULL))) {
                VppErr("vpp_disguise_y400_as_nv12_pcie params err. handle(0X%lx), dev_buf_uv(0X%lx).\n", handle, dev_buf_uv);
                return VPP_ERR;
            }
            ret = bm_malloc_device_byte_heap(handle, &dev_buf_uv[crop_idx], DDR_CH, uv_data_len);
            if (ret != BM_SUCCESS) {
                VppErr("malloc device memory size = %d failed, ret = %d\n", uv_data_len, ret);
            }

            /*memset device mem*/
            bm_memset_device(handle, 0x80808080, dev_buf_uv[crop_idx]);

            /*modify Y400 to NV12: padding uv data to src mat*/
            src[crop_idx].format = FMT_NV12;
            src[crop_idx].pa[1] = dev_buf_uv[crop_idx].u.device.device_addr;
            src[crop_idx].va[1] = dev_buf_uv[crop_idx].u.system.system_addr;  // Maybe there is no mapping done yet
            src[crop_idx].ion_len[1] = uv_data_len;
            src[crop_idx].uv_stride = src_stride;
            need_restore[crop_idx] = 1;
        }
        else if ((dst_fmt == FMT_Y) && ((src_fmt == FMT_BGR) || (src_fmt == FMT_RGB) || (src_fmt == FMT_RGBP) ||
          (src_fmt == FMT_BGRP) || (src_fmt == FMT_ARGB) || (src_fmt == FMT_ABGR)))
        {
          dst_w = dst[crop_idx].cols;
          dst_h = dst[crop_idx].rows;
          dst_stride = dst[crop_idx].stride;
          if (dst_stride < dst_w) {
              VppErr("vpp_disguise_y400_as_nv12_pcie dst_stride(%d) < src_w(%d).\n", dst_stride, dst_w);
              return VPP_ERR;
          }
          if (dst_stride % STRIDE_ALIGN != 0) {
              VppErr("vpp_disguise_y400_as_nv12_pcie stride not align. dst_stride(%d), STRIDE_ALIGN(%d).\n", dst_stride, STRIDE_ALIGN);
              return VPP_ERR;
          }

          uv_data_len = dst_h * dst_stride;

          /*alloc device mem for uv data*/
          if (!((handle != NULL) && (dev_buf_uv != NULL))) {
              VppErr("vpp_disguise_y400_as_nv12_pcie params err. handle(0X%lx), dev_buf_uv(0X%lx).\n", handle, dev_buf_uv);
              return VPP_ERR;
          }
          ret = bm_malloc_device_byte_heap(handle, &dev_buf_uv[crop_idx], DDR_CH, uv_data_len * 2);
          if (ret != BM_SUCCESS) {
              VppErr("malloc device memory size = %d failed, ret = %d\n", uv_data_len * 2, ret);
          }


          /*modify Y400 to NV12: padding uv data to src mat*/
          dst[crop_idx].format = FMT_YUV444P;
          dst[crop_idx].pa[1] = dev_buf_uv[crop_idx].u.device.device_addr;
          dst[crop_idx].va[1] = dev_buf_uv[crop_idx].u.system.system_addr;
          dst[crop_idx].ion_len[1] = uv_data_len;
          dst[crop_idx].uv_stride = dst_stride;
          dst[crop_idx].pa[2] = dev_buf_uv[crop_idx].u.device.device_addr + uv_data_len;
          dst[crop_idx].va[2] = (void *)((unsigned long long)dev_buf_uv[crop_idx].u.system.system_addr + uv_data_len);
          dst[crop_idx].ion_len[2] = uv_data_len;

          need_restore[crop_idx] = 1;

        }

        else {
            need_restore[crop_idx] = 0;
            continue;
        }
    }
    return VPP_OK;
}

void vpp_restore_nv12_to_y400_pcie(bm_handle_t handle, struct vpp_mat_s* src,
  struct vpp_mat_s* dst, int crop_num, bm_device_mem_t *dev_buf_uv, int *need_restore)
{
    int crop_idx;
    int src_fmt;
    int dst_fmt;

    for (crop_idx = 0; crop_idx < crop_num; crop_idx++) {
        src_fmt = src[crop_idx].format;
        dst_fmt = dst[crop_idx].format;

        if ((src_fmt == FMT_NV12) && ((dst_fmt == FMT_BGR) || (dst_fmt == FMT_RGB) || (dst_fmt == FMT_RGBP) ||
             (dst_fmt == FMT_BGRP) || (dst_fmt == FMT_ARGB) || (dst_fmt == FMT_ABGR)) &&
             (need_restore[crop_idx] == 1)) {
            /*free device mem of uv data*/
            bm_free_device(handle, dev_buf_uv[crop_idx]);

            /*modify  NV12 to Yonly*/
            src[crop_idx].format = FMT_Y;
            src[crop_idx].pa[1] = 0;
            src[crop_idx].va[1] = NULL;
            src[crop_idx].ion_len[1] = 0;
            src[crop_idx].uv_stride = 0;
        }
        else if((dst_fmt == FMT_YUV444P) && ((src_fmt == FMT_BGR) || (src_fmt == FMT_RGB) || (src_fmt == FMT_RGBP) ||
             (src_fmt == FMT_BGRP) || (src_fmt == FMT_ARGB) || (src_fmt == FMT_ABGR)) &&
             (need_restore[crop_idx] == 1))
        {
          /*free device mem of uv data*/
          bm_free_device(handle, dev_buf_uv[crop_idx]);

          dst[crop_idx].format = FMT_Y;
          dst[crop_idx].pa[1] = 0;
          dst[crop_idx].va[1] = NULL;
          dst[crop_idx].ion_len[1] = 0;
          dst[crop_idx].uv_stride = 0;
          dst[crop_idx].pa[2] = 0;
          dst[crop_idx].va[2] = NULL;
          dst[crop_idx].ion_len[2] = 0;

        }
        else {
            continue;
        }
    }
}
#endif
#if !defined _WIN32
int vpp_creat_ion_mem(vpp_mat *mat, int format, int in_w, int in_h)
{
    if(!(mat != NULL) ||
       !((in_w <= MAX_RESOLUTION_W) && (in_h <= MAX_RESOLUTION_H)) ||
       !((in_w >= MIN_RESOLUTION_W) && (in_h >= MIN_RESOLUTION_H))) {
        VppErr("vpp_creat_ion_mem parms err. mat(0X%lx), in_w(%d), in_h(%d).\n", mat, in_w, in_h);
        return VPP_ERR;
    }

    ion_para para;
    int idx;
    int ret = VPP_OK;

    memset(mat, 0, sizeof(vpp_mat));

    mat->format = format;
    mat->cols = in_w;
    mat->rows = in_h;
    mat->is_pa = 1;
    mat->uv_stride = 0;

    switch (mat->format)
    {
        case FMT_BGR:
        case FMT_RGB:
        case FMT_YUV444:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w * 3, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_ABGR:
        case FMT_ARGB:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w * 4, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_RGBP:
        case FMT_BGRP:
        case FMT_YUV444P:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] =
            mat->ion_len[1] =
            mat->ion_len[2] = mat->stride * in_h;
            break;
        case FMT_YUV422P:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = in_h * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_I420:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_NV12:
            mat->num_comp = 2;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * mat->stride;
            break;
        case FMT_Y:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
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
#if defined USING_CMODEL || defined PCIE_MODE
        mat->va[idx]= malloc(mat->ion_len[idx]);
        mat->fd[idx] = 0;
        mat->pa[idx] = 0;
#else
        if (vpp_ion_malloc_len(mat->ion_len[idx], &para) == NULL)
        {
            VppErr("allocate ion err\n", idx);
            ret = VPP_ERR;
            break;
        }

        mat->fd[idx] = para.memFd;
        mat->pa[idx] = para.pa;
        mat->va[idx] = para.va;
#endif
    }
    return ret;
}

int vpp_creat_ion_mem_fd(vpp_mat *mat, int format, int in_w, int in_h, const ion_dev_fd_s *ion_dev_fd)
{
    if(!(mat != NULL) ||
       !((in_w <= MAX_RESOLUTION_W) && (in_h <= MAX_RESOLUTION_H)) ||
       !((in_w >= MIN_RESOLUTION_W) && (in_h > MIN_RESOLUTION_H))) {
        VppErr("vpp_creat_ion_mem_fd parms err. mat(0X%lx), in_w(%d), in_h(%d).\n", mat, in_w, in_h);
        return VPP_ERR;
    }

    ion_para para;
    int idx;
    int ret = VPP_OK;

    memset(mat, 0, sizeof(vpp_mat));

    mat->format = format;
    mat->cols = in_w;
    mat->rows = in_h;
    mat->is_pa = 1;
    mat->uv_stride = 0;

    switch (mat->format)
    {
        case FMT_BGR:
        case FMT_RGB:
        case FMT_YUV444:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w * 3, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_ABGR:
        case FMT_ARGB:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w * 4, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            break;
        case FMT_RGBP:
        case FMT_BGRP:
        case FMT_YUV444P:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] =
            mat->ion_len[1] =
            mat->ion_len[2] = mat->stride * in_h;
            break;
        case FMT_YUV422P:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = in_h * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_I420:
            mat->num_comp = 3;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_NV12:
            mat->num_comp = 2;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
            mat->ion_len[1] = (in_h >> 1) * mat->stride;
            break;
        case FMT_Y:
            mat->num_comp = 1;
            mat->stride = VPP_ALIGN(in_w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * in_h;
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
            if ((ion_dev_fd->dev_fd > 0) && (strncmp(ion_dev_fd->name, "ion", 4) == 0))
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
void* vpp_ion_malloc(int rows, int stride, ion_para* para)
{
    if ((rows <= 0) || (para == NULL) || (stride <= 0)) {
        VppErr("vpp_ion_malloc parms err. rows(%d), stride(%d), para(0X%lx).\n", rows, stride, para);
        return NULL;
    }

    int devFd, ext_ion_devfd = 0;
    if (stride & (STRIDE_ALIGN - 1)) {
        VppErr("stride not 64B alige\n");
    }
    para->length = rows * stride;

    if ((para->ionDevFd.dev_fd > 0) && (strncmp(para->ionDevFd.name, "ion", 4) == 0)) {
        devFd = para->ionDevFd.dev_fd;
        ext_ion_devfd = 1;
    }
    else {
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
    if ((len <= 0) || (para == NULL)) {
        VppErr("vpp_ion_malloc_len parms err. rows(%d), para(0X%lx).\n", len, para);
        return NULL;
    }

    int devFd, ext_ion_devfd = 0;

    if (len & (STRIDE_ALIGN - 1)) {
        VppWarn("len  %d,not 64B alige\n", len);
    }
    para->length = len;

    if ((para->ionDevFd.dev_fd > 0) && (strncmp(para->ionDevFd.name, "ion", 4) == 0)) {
        /*do nothing*/
        devFd = para->ionDevFd.dev_fd;
        ext_ion_devfd = 1;
    }
    else {
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

void vpp_ion_free(ion_para* para)
{
    ionFree(para);
}

void vpp_ion_free_close_devfd(ion_para* para)
{
    ionFree(para);
    close(para->ionDevFd.dev_fd);
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
        case FMT_BGR:
        case FMT_RGB:
        case FMT_RGBP:
        case FMT_BGRP:
        case FMT_YUV444P:
        case FMT_YUV444:
            size = mat->rows * mat->cols *3;
            break;
        case FMT_ABGR:
        case FMT_ARGB:
            size = mat->rows * mat->cols *4;
            break;
        case FMT_YUV422P:
            size = mat->rows * mat->cols *2;
            break;
        case FMT_I420:
            size = mat->rows * mat->cols *3/2;
            break;
        case FMT_NV12:
            size = mat->rows * mat->cols *3/2;
            break;
        case FMT_Y:
            size = mat->rows * mat->cols;
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
        case FMT_BGR:
        case FMT_RGB:
        case FMT_YUV444:
            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }
            break;
        case FMT_ABGR:
        case FMT_ARGB:
            rows = mat->rows;
            cols = mat->cols * 4;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }
            break;
        case FMT_RGBP:
        case FMT_BGRP:
        case FMT_YUV444P:
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
        case FMT_YUV422P:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }

            rows = mat->rows;
            cols = mat->cols / 2;
            stride = (mat->uv_stride == 0) ? (mat->stride >> 1) : mat->uv_stride;
            for (idx = 1; idx < 3; idx++)
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
            stride = (mat->uv_stride == 0) ? (mat->stride >> 1) : mat->uv_stride;
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

        case FMT_Y:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                fread(mat->va[0]+i*stride, 1, cols, fp);
            }
            break;

        default:
            VppErr("format = %d", mat->format);
            return -1;
    }

    fclose(fp);
    return status;
}

int vpp_disguise_y400_as_nv12_soc(struct vpp_mat_s* src, struct vpp_mat_s* dst,
  int crop_num, ion_para *uv_data_para, int *need_restore)
{
    int crop_idx;
    int src_fmt;
    int dst_fmt;
    int src_h, dst_h;
    int src_w, dst_w;
    int src_stride, dst_stride;
    int uv_data_len;
    void *uv_data_va;

    if(!((uv_data_para != NULL) && (need_restore != NULL))) {
        VppErr("vpp_disguise_y400_as_nv12_soc params is err. uv_data_para=0X%lx, need_restore=0X%lx\n", uv_data_para, need_restore);
        return VPP_ERR;
    }
    for (crop_idx = 0; crop_idx < crop_num; crop_idx++) {
        src_fmt = src[crop_idx].format;
        dst_fmt = dst[crop_idx].format;

        if ((src_fmt == FMT_Y) && ((dst_fmt == FMT_BGR) || (dst_fmt == FMT_RGB) || (dst_fmt == FMT_RGBP) ||
            (dst_fmt == FMT_BGRP) || (dst_fmt == FMT_ARGB)|| (dst_fmt == FMT_ABGR))) {
            src_w = src[crop_idx].cols;
            src_h = src[crop_idx].rows;
            src_stride = src[crop_idx].stride;
            if(!(src_stride >= src_w) ||
               !(src_stride % STRIDE_ALIGN == 0)) {
                VppErr("vpp_disguise_y400_as_nv12_soc src_stride is err. src_stride=%d, src_w=%d, STRIDE_ALIGN=%d\n", src_stride, src_w, STRIDE_ALIGN);
                return VPP_ERR;
            }

            uv_data_len = VPP_ALIGN(src_h, 2) / 2 * src_stride;

            /*alloc ion mem for uv data*/
            uv_data_va = vpp_ion_malloc_len(uv_data_len, &uv_data_para[crop_idx]);
            if (uv_data_va == NULL) {
                VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
            }

            /*memset device mem*/
            memset(uv_data_va, 128, uv_data_len);

            /*flush data to ion pa*/
            ion_flush(NULL, uv_data_va, uv_data_len);

            /*modify Y400 to NV12: padding uv data to src mat*/
            src[crop_idx].format = FMT_NV12;
            src[crop_idx].pa[1] = uv_data_para[crop_idx].pa;
            src[crop_idx].va[1] = uv_data_va;
            src[crop_idx].ion_len[1] = uv_data_len;
            src[crop_idx].uv_stride = src_stride;
            need_restore[crop_idx] = 1;
        }
        else if((dst_fmt == FMT_Y) && ((src_fmt == FMT_BGR) || (src_fmt == FMT_RGB) || (src_fmt == FMT_RGBP) ||
            (src_fmt == FMT_BGRP) || (src_fmt == FMT_ARGB)|| (src_fmt == FMT_ABGR)))
        {
          dst_w = dst[crop_idx].cols;
          dst_h = dst[crop_idx].rows;
          dst_stride = dst[crop_idx].stride;
          if(!(dst_stride >= dst_w) ||
             !(dst_stride % STRIDE_ALIGN == 0)) {
              VppErr("vpp_disguise_y400_as_nv12_soc dst_stride is err. dst_stride=%d, dst_w=%d, STRIDE_ALIGN=%d\n", dst_stride, dst_w, STRIDE_ALIGN);
              return VPP_ERR;
          }

          uv_data_len = dst_h * dst_stride;

          /*alloc ion mem for uv data*/
          uv_data_va = vpp_ion_malloc_len(uv_data_len * 2, &uv_data_para[crop_idx]);
          if (uv_data_va == NULL) {
              VppErr("alloc ion mem failed, errno = %d, msg: %s\n", errno, strerror(errno));
          }

          /*modify Y400 to YUV444P: padding uv data to src mat*/
          dst[crop_idx].format = FMT_YUV444P;
          dst[crop_idx].pa[1] = uv_data_para[crop_idx].pa;
          dst[crop_idx].va[1] = uv_data_va;
          dst[crop_idx].ion_len[1] = uv_data_len;
          dst[crop_idx].uv_stride = dst_stride;

          dst[crop_idx].pa[2] = uv_data_para[crop_idx].pa + uv_data_len;
          dst[crop_idx].va[2] = (void *)((unsigned long long)uv_data_va + uv_data_len);
          dst[crop_idx].ion_len[2] = uv_data_len;
          need_restore[crop_idx] = 1;
        }
        else {
            need_restore[crop_idx] = 0;
            continue;
        }
    }
    return VPP_OK;
}

void vpp_restore_nv12_to_y400_soc(struct vpp_mat_s* src, struct vpp_mat_s* dst,
  int crop_num, ion_para *uv_data_para, int *need_restore)
{
    int crop_idx;
    int src_fmt;
    int dst_fmt;

    for (crop_idx = 0; crop_idx < crop_num; crop_idx++) {
        src_fmt = src[crop_idx].format;
        dst_fmt = dst[crop_idx].format;

        if ((src_fmt == FMT_NV12) && ((dst_fmt == FMT_BGR) || (dst_fmt == FMT_RGB) || (dst_fmt == FMT_RGBP) ||
            (dst_fmt == FMT_BGRP) || (dst_fmt == FMT_ARGB)|| (dst_fmt == FMT_ABGR)) && (need_restore[crop_idx] == 1)) {
            /*free ion mem of uv data*/
            vpp_ion_free(&uv_data_para[crop_idx]);

            /*modify  NV12 to Yonly*/
            src[crop_idx].format = FMT_Y;
            src[crop_idx].pa[1] = 0;
            src[crop_idx].va[1] = NULL;
            src[crop_idx].ion_len[1] = 0;
            src[crop_idx].uv_stride = 0;
        }
        else if((dst_fmt == FMT_YUV444P) && ((src_fmt == FMT_BGR) || (src_fmt == FMT_RGB) || (src_fmt == FMT_RGBP) ||
            (src_fmt == FMT_BGRP) || (src_fmt == FMT_ARGB)|| (src_fmt == FMT_ABGR)) && (need_restore[crop_idx] == 1))
        {
          /*free ion mem of uv data*/
          vpp_ion_free(&uv_data_para[crop_idx]);

          /*modify  NV12 to Yonly*/
          dst[crop_idx].format = FMT_Y;
          dst[crop_idx].pa[1] = 0;
          dst[crop_idx].va[1] = NULL;
          dst[crop_idx].ion_len[1] = 0;
          dst[crop_idx].uv_stride = 0;
          dst[crop_idx].pa[2] = 0;
          dst[crop_idx].va[2] = NULL;
          dst[crop_idx].ion_len[2] = 0;
        }
        else {
            continue;
        }
    }
}

// csc simplified version for jpu pre/post process, do not support scale and batchs
int vpp_csc_single(vpp_csc_info * vpp_info)
{
    int ret = 0;
    bool is_reopen = false;
    if(vpp_info == NULL)
        return -1;
    if(vpp_info->vpp_fd <= 0)
    {
        vpp_info->vpp_fd = open("/dev/bm-vpp", O_RDWR);
        if (vpp_info->vpp_fd <= 0) {
            VppErr("open /dev/bm-vpp failed, errno = %d, msg: %s\n", errno, strerror(errno));
            return -1;
        }
        is_reopen = true;
    }

    struct vpp_batch_n batch;
    batch.num = 1;
    batch.cmd = malloc(batch.num * (sizeof(struct vpp_cmd_n)));
    memset(batch.cmd,0,(batch.num * (sizeof(struct vpp_cmd_n))));

    struct vpp_cmd_n *cmd = batch.cmd;

    vpp_batch_format(cmd, vpp_info->src_format, vpp_info->dst_format);

    cmd->csc_type = vpp_info->csc_type;
    cmd->src_stride = vpp_info->src_stride;
    cmd->dst_stride = vpp_info->dst_stride;
    cmd->cols = vpp_info->cols;
    cmd->rows = vpp_info->rows;

    cmd->src_addr0 = vpp_info->src_addr0;
    cmd->src_addr1 = vpp_info->src_addr1;
    cmd->src_addr2 = vpp_info->src_addr2;
    cmd->dst_addr0 = vpp_info->dst_addr0;
    cmd->dst_addr1 = vpp_info->dst_addr1;
    cmd->dst_addr2 = vpp_info->dst_addr2;

    cmd->src_cropH = vpp_info->rows;
    cmd->src_cropW = vpp_info->cols;
    cmd->dst_cropH = vpp_info->rows;
    cmd->dst_cropW = vpp_info->cols;
    if (vpp_info->algorithm == VPP_SCALE_BICUBIC) {
        cmd->hor_filter_sel = VPP_FILTER_SEL_BICUBIC_UP;
        cmd->ver_filter_sel = VPP_FILTER_SEL_BICUBIC_UP;
    } else if (vpp_info->algorithm == VPP_SCALE_NEAREST) {
        cmd->hor_filter_sel = VPP_FILTER_SEL_NEAREST;
        cmd->ver_filter_sel = VPP_FILTER_SEL_NEAREST;
    } else {
        cmd->hor_filter_sel = VPP_FILTER_SEL_BILINEAR;
        cmd->ver_filter_sel = VPP_FILTER_SEL_BILINEAR;
    }

    ret = ioctl(vpp_info->vpp_fd , VPP_UPDATE_BATCH_FD_PA, &batch);
    if (ret != 0)
    {
        vpp_return_value_analysis(ret);
        vpp_dump(&batch);
    }
    free(batch.cmd);
    if(is_reopen)
    {
        close(vpp_info->vpp_fd);
    }
    return ret;
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

int i420tonv12(void * const in ,unsigned int w, unsigned int h, void * const out,unsigned int stride)
{
    int i = 0, j = 0;
    char *in_tmp, *out_tmp;
    if(!((in != NULL) && (out != NULL)) ||
       !((h > 0) && (w > 0) && (stride > 0)) ||
       !((h < 0x10000) && (w < 0x10000) && (stride < 0x10000)) ||
       !((h % 2 ==0) && (w %2 == 0))) {
        VppErr("i420tonv12 parms err. in(0X%lx), out(%d), h(%d), w(%d), stride(%d).\n", in, out, h, w, stride);
        return VPP_ERR;
    }

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
    return VPP_OK;
}

#endif

int vpp_misc_cmodel(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type, int scale_type)
{
    int ret = 0;
    struct vpp_cmd_n batch_cmd;
    unsigned long long addr_temp;
    struct vpp_batch_n batch;

    batch.num = crop_num;
    batch.cmd = malloc(batch.num * (sizeof(batch_cmd)));
    if (batch.cmd == NULL) {
        ret = -VPP_ERR;
        VppErr("batch memory malloc failed\n");
        return ret;
    }
    memset(batch.cmd,0,(batch.num * (sizeof(batch_cmd))));
    for (int i = 0; i < crop_num; i++) {
        struct vpp_cmd_n *cmd = (batch.cmd + i);

        ret = vpp_batch_pa(cmd, &src[i], &dst[i]);

        cmd->src_fd0 = 0;
        cmd->src_fd1 = 0;
        cmd->src_fd2 = 0;
        cmd->src_fd3 = 0;
        cmd->src_addr0 = (unsigned long long)src[i].va[0];
        cmd->src_addr1 = (unsigned long long)src[i].va[1];
        cmd->src_addr2 = (unsigned long long)src[i].va[2];
        cmd->src_addr3 = (unsigned long long)src[i].va[3];
        if(src->format == FMT_BGRP) {
            addr_temp = cmd->src_addr0;
            cmd->src_addr0 = cmd->src_addr2;
            cmd->src_addr2 = addr_temp;
        }
        cmd->dst_fd0 = 0;
        cmd->dst_fd1 = 0;
        cmd->dst_fd2 = 0;
        cmd->dst_addr0 = (unsigned long long)dst[i].va[0];
        cmd->dst_addr1 = (unsigned long long)dst[i].va[1];
        cmd->dst_addr2 = (unsigned long long)dst[i].va[2];
        if(dst->format == FMT_BGRP) {
            addr_temp = cmd->dst_addr0;
            cmd->dst_addr0 = cmd->dst_addr2;
            cmd->dst_addr2 = addr_temp;
        }
        cmd->mapcon_enable = 0;
        cmd->src_csc_en = 0;
        ret = vpp_batch_format(cmd, src[i].format, dst[i].format);
        if (ret < 0) {
            VppErr("format config wrong\n");
        }

        ret = vpp_addr_config(cmd, src[i].format, dst[i].format);
        if (ret == VPP_ERR) {
            return ret;
        }
        if (scale_type == VPP_SCALE_BICUBIC) {
          VppErr("vpp cmodel soupport NEAREST and BILINEAR\n");
          return VPP_ERR;

        } else if (scale_type == VPP_SCALE_NEAREST) {
            cmd->hor_filter_sel = VPP_FILTER_SEL_NEAREST;
            cmd->ver_filter_sel = VPP_FILTER_SEL_NEAREST;
        } else {
            cmd->hor_filter_sel = VPP_FILTER_SEL_BILINEAR;
            cmd->ver_filter_sel = VPP_FILTER_SEL_BILINEAR;
        }

        cmd->src_stride = src[i].stride;
        cmd->dst_stride = dst[i].stride;
        cmd->src_uv_stride = src[i].uv_stride;
        cmd->dst_uv_stride = dst[i].uv_stride;
        cmd->cols  = src[i].cols;
        cmd->rows  = src[i].rows ;
        cmd->src_cropH = loca[i].height;
        cmd->src_cropW = loca[i].width;
        cmd->src_axisX = loca[i].x;
        cmd->src_axisY = loca[i].y;
        cmd->dst_cropH = dst[i].rows;
        cmd->dst_cropW = dst[i].cols;
        cmd->dst_axisX = dst[i].axisX;//border use
        cmd->dst_axisY = dst[i].axisY;//border use
        if (csc_type !=CSC_MAX) {
            cmd->csc_type = csc_type;
        }
    }

    vpp1684_cmodel_test(&batch);
    free(batch.cmd);
    return ret;
}


void vpp_free_ion_mem(vpp_mat *mat)
{
    int idx;

    for (idx = 0; idx < mat->num_comp; idx++) {
#if defined USING_CMODEL || defined PCIE_MODE
    free(mat->va[idx]);
#else
        munmap(mat->va[idx], mat->ion_len[idx]);
        close(mat->fd[idx]);
#endif
    }
}

int vpp_write_file(char *file_name, vpp_mat *mat)
{
    FILE *fp;
    int status = 0;
    int idx = 0, i = 0, rows = 0, cols = 0, stride = 0;

    if(file_name == NULL) {
        VppErr("vpp_write_file parms err. file_name(0X%lx).\n", file_name);
        return VPP_ERR;
    }
    fp = fopen(file_name, "wb");
    if(!fp) {
        VppErr("ERROR: Unable to open the output file!\n");
        exit(-1);
    }

    switch (mat->format)
    {
        case FMT_BGR: case FMT_RGB:
        case FMT_YUV444:
            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                if (fwrite((char *)mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }
            break;
       case FMT_ABGR: case FMT_ARGB:
            rows = mat->rows;
            cols = mat->cols * 4;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                if (fwrite((char *)mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }
            break;
        case FMT_RGBP:case FMT_BGRP:
        case FMT_YUV444P:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (idx = 0; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    if (fwrite((char *)mat->va[idx]+i*stride, 1, cols, fp) != cols)
                    {
                        fclose(fp);
                        return VPP_ERR;
                    }
                }
            }
            break;
        case FMT_YUV422P:
            rows = mat->rows;
            cols = mat->cols;
            stride = mat->stride;
            for (i = 0; i < rows; i++)
            {
                if (fwrite((char *)mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }

            rows = mat->rows;
            cols = mat->cols / 2;
            stride = (mat->uv_stride == 0) ? (mat->stride >> 1) : mat->uv_stride;
            for (idx = 1; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    if (fwrite((char *)mat->va[idx]+i*stride, 1, cols, fp) != cols)
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
                if (fwrite((char *)mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }

            rows = mat->rows / 2;
            cols = mat->cols / 2;
            stride = (mat->uv_stride == 0) ? (mat->stride >> 1) : mat->uv_stride;
            for (idx = 1; idx < 3; idx++)
            {
                for (i = 0; i < rows; i++)
                {
                    if (fwrite((char *)mat->va[idx]+i*stride, 1, cols, fp) != cols)
                    {
                        fclose(fp);
                        return VPP_ERR;
                    }
                }
            }
            break;
        case FMT_Y:
            rows = mat->rows;
            cols = mat->cols * 3;
            stride = mat->stride;

            for (i = 0; i < rows; i++)
            {
                if (fwrite((char *)mat->va[0]+i*stride, 1, cols, fp) != cols)
                {
                    fclose(fp);
                    return VPP_ERR;
                }
            }
            break;
        default:
            VppErr("format err!, format = %d", mat->format);
            return -1;
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


int output_file(char *file_name, vpp_mat *mat)
{
    int ret = 0;
    int format = mat->format;

    switch (format)
    {
    case FMT_BGR: case FMT_RGB:
        ret = vpp_bmp_bgr888(file_name, mat->va[0], mat->cols, mat->rows, mat->stride);
        break;
    case FMT_Y:
        ret |= vpp_bmp_gray("yonly.bmp", (unsigned char *) mat->va[0], mat->cols, mat->rows, mat->stride);
        break;
    case FMT_RGBP:
    case FMT_BGRP:
        ret |= vpp_bmp_gray("b.bmp", (unsigned char *) mat->va[0], mat->cols, mat->rows, mat->stride);
        ret |= vpp_bmp_gray("g.bmp", (unsigned char *) mat->va[1], mat->cols, mat->rows, mat->stride);
        ret |= vpp_bmp_gray("r.bmp", (unsigned char *) mat->va[2], mat->cols, mat->rows, mat->stride);
    default:
        ret = vpp_write_file(file_name, mat);
        break;
    }
    return ret;
}

int vpp_batch_format(struct vpp_cmd_n *cmd, int src_format,int dst_format)
{
    if(cmd == NULL) {
        VppErr("vpp_batch_format parms err. cmd(0X%lx).\n", cmd);
        return VPP_ERR;
    }
    int ret = VPP_OK, src_colorformat = 0, dst_colorformat = 0;

    switch (src_format) {
    case FMT_BGR:
        cmd->src_format = RGB24;
        cmd->src_endian_a = 0;
        cmd->src_endian = 1;
        cmd->src_plannar = 0;
        src_colorformat = 0;
        break;
    case FMT_RGB:
        cmd->src_format = RGB24;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 0;
        src_colorformat = 0;
        break;
    case FMT_ABGR:
        cmd->src_format = ARGB32;
        cmd->src_endian_a = 1;
        cmd->src_endian = 1;
        cmd->src_plannar = 0;
        src_colorformat = 0;
        break;
    case FMT_ARGB:
        cmd->src_format = ARGB32;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 0;
        src_colorformat = 0;
        break;
    case FMT_RGBP:
    case FMT_BGRP:
        cmd->src_format = RGB24;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 1;
        src_colorformat = 0;
        break;
    case FMT_YUV444:
        cmd->src_format = RGB24;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 0;
        src_colorformat = 1;
        break;
    case FMT_YUV444P:
        cmd->src_format = RGB24;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 1;
        src_colorformat = 1;
        break;
    case FMT_YUV422P:
        cmd->src_format = YUV422;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 1;
        src_colorformat = 1;
        break;
    case FMT_I420:
        cmd->src_format = YUV420;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 1;
        src_colorformat = 1;
        break;
    case FMT_NV12:
        cmd->src_format = YUV420;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 0;
        src_colorformat = 1;
        break;
    case FMT_Y:
        cmd->src_format = YOnly;
        cmd->src_endian_a = 0;
        cmd->src_endian = 0;
        cmd->src_plannar = 1;
        src_colorformat = 1;
        break;
    default:
        ret = VPP_ERR;
        VppErr("unrecognized format\n");
        break;
    }

    switch (dst_format) {
    case FMT_BGR:
        cmd->dst_format = RGB24;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 1;
        cmd->dst_plannar = 0;
        dst_colorformat = 0;
        break;
    case FMT_RGB:
        cmd->dst_format = RGB24;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 0;
        dst_colorformat = 0;
        break;
    case FMT_ABGR:
        cmd->dst_format = ARGB32;
        cmd->dst_endian_a = 1;
        cmd->dst_endian = 1;
        cmd->dst_plannar = 0;
        dst_colorformat = 0;
        break;
    case FMT_ARGB:
        cmd->dst_format = ARGB32;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 0;
        dst_colorformat = 0;
        break;
    case FMT_RGBP:
    case FMT_BGRP:
        cmd->dst_format = RGB24;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 1;
        dst_colorformat = 0;
        break;
    case FMT_YUV444:
        cmd->dst_format = RGB24;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 0;
        dst_colorformat = 1;
        break;
    case FMT_YUV444P:
        cmd->dst_format = RGB24;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 1;
        dst_colorformat = 1;
        break;
    case FMT_YUV422P:
        cmd->dst_format = YUV422;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 1;
        dst_colorformat = 1;
        break;
    case FMT_I420:
        cmd->dst_format = YUV420;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 1;
        dst_colorformat = 1;
        break;
    case FMT_Y:
        cmd->dst_format = YOnly;
        cmd->dst_endian_a = 0;
        cmd->dst_endian = 0;
        cmd->dst_plannar = 1;
        dst_colorformat = 1;
        break;
    default:
        ret = VPP_ERR;
        VppErr("unrecognized format\n");
        break;
    }

    if((src_colorformat == 0) && (dst_colorformat == 1))
    {
        cmd->csc_type = RGB2YCbCr_BT601;
        cmd->src_csc_en = 1;
    }
    if((src_colorformat == 1) && (dst_colorformat == 0))
    {
        cmd->csc_type = YCbCr2RGB_BT601;
        cmd->src_csc_en = 1;
    }
    return ret;
}

int vpp_batch_pa(struct vpp_cmd_n *cmd, struct vpp_mat_s* src,struct vpp_mat_s* dst)
{
    if((cmd == NULL) || (src == NULL) || (dst == NULL)) {
        VppErr("vpp_batch_pa parms err. cmd(0X%lx), src(0X%lx), dst(0X%lx).\n", cmd, src, dst);
        return VPP_ERR;
    }
    int fd_tmp, ret = VPP_OK;
    unsigned long long addr_temp;

    switch (src->is_pa) {
    case 1:
        cmd->src_fd0 = 0;
        cmd->src_fd1 = 0;
        cmd->src_fd2 = 0;
        cmd->src_fd3 = 0;

#if defined USING_CMODEL
        cmd->src_addr0 = (unsigned long long)src->va[0];
        cmd->src_addr1 = (unsigned long long)src->va[1];
        cmd->src_addr2 = (unsigned long long)src->va[2];
        cmd->src_addr3 = (unsigned long long)src->va[3];
#else
        cmd->src_addr0 = src->pa[0];
        cmd->src_addr1 = src->pa[1];
        cmd->src_addr2 = src->pa[2];
        cmd->src_addr3 = src->pa[3];
#endif
        if(src->format == FMT_BGRP) {
            addr_temp = cmd->src_addr0;
            cmd->src_addr0 = cmd->src_addr2;
            cmd->src_addr2 = addr_temp;
        }
        break;
    case 0:
        cmd->src_fd0 = src->fd[0];
        cmd->src_fd1 = src->fd[1];
        cmd->src_fd2 = src->fd[2];
        cmd->src_fd3 = src->fd[3];
        cmd->src_addr0 = 0;
        cmd->src_addr1 = 0;
        cmd->src_addr2 = 0;
        cmd->src_addr3 = 0;
        if(src->format == FMT_BGRP) {
            fd_tmp = cmd->src_fd0;
            cmd->src_fd0 = cmd->src_fd2;
            cmd->src_fd2 = fd_tmp;
        }
        break;
    default:
        ret = VPP_ERR;
        VppErr("False parameters \n");
        break;
    }

    switch (dst->is_pa) {
    case 1:
        cmd->dst_fd0 = 0;
        cmd->dst_fd1 = 0;
        cmd->dst_fd2 = 0;

#if defined USING_CMODEL
        cmd->dst_addr0 = (unsigned long long)dst->va[0];
        cmd->dst_addr1 = (unsigned long long)dst->va[1];
        cmd->dst_addr2 = (unsigned long long)dst->va[2];
#else
        cmd->dst_addr0 = dst->pa[0];
        cmd->dst_addr1 = dst->pa[1];
        cmd->dst_addr2 = dst->pa[2];
#endif

        if(dst->format == FMT_BGRP) {
            addr_temp = cmd->dst_addr0;
            cmd->dst_addr0 = cmd->dst_addr2;
            cmd->dst_addr2 = addr_temp;
        }
        break;
    case 0:
        cmd->dst_fd0 = dst->fd[0];
        cmd->dst_fd1 = dst->fd[1];
        cmd->dst_fd2 = dst->fd[2];
        cmd->dst_addr0 = 0;
        cmd->dst_addr1 = 0;
        cmd->dst_addr2 = 0;

        if(dst->format == FMT_BGRP) {
            fd_tmp = cmd->dst_fd0;
            cmd->dst_fd0 = cmd->dst_fd2;
            cmd->dst_fd2 = fd_tmp;
        }
        break;
    default:
        ret = VPP_ERR;
        VppErr("False parameters \n");
        break;
    }

    VppDbg("src_fd0 = 0x%lx, src_fd1 = 0x%lx, src_fd2 = 0x%lx\n", cmd->src_fd0, cmd->src_fd1, cmd->src_fd2);
    VppDbg("src_addr0 = 0x%lx, src_addr1 = 0x%lx, src_addr2 = 0x%lx\n", cmd->src_addr0, cmd->src_addr1, cmd->src_addr2);
    VppDbg("dst_fd0 = 0x%lx, dst_fd1 = 0x%lx, dst_fd2 = 0x%lx\n", cmd->dst_fd0, cmd->dst_fd1, cmd->dst_fd2);
    VppDbg("dst_addr0 = 0x%lx, dst_addr1 = 0x%lx, dst_addr2 = 0x%lx\n", cmd->dst_addr0, cmd->dst_addr1, cmd->dst_addr2);

    return ret;
}

int vpp_addr_config(struct vpp_cmd_n *cmd, int src_format,int dst_format)
{
    if(cmd == NULL) {
        VppErr("vpp_addr_config parms err. cmd(0X%lx).\n", cmd);
        return VPP_ERR;
    }

    switch (src_format) {
    case FMT_BGR:
    case FMT_RGB:
    case FMT_ABGR:
    case FMT_ARGB:
    case FMT_Y:
        cmd->src_fd1 = 0;
        cmd->src_fd2 = 0;
        cmd->src_addr1 = 0;
        cmd->src_addr2 = 0;
        break;
    case FMT_NV12:
        cmd->src_fd2 = 0;
        cmd->src_addr2 = 0;
        break;
    default:
        break;
    }

    cmd->src_addr3 =0;
    cmd->src_fd3 = 0;

    switch (dst_format) {
    case FMT_BGR:
    case FMT_RGB:
    case FMT_ABGR:
    case FMT_ARGB:
    case FMT_Y:
        cmd->dst_fd1 = 0;
        cmd->dst_fd2 = 0;
        cmd->dst_addr1 = 0;
        cmd->dst_addr2 = 0;
        break;
    default:
        break;
    }
    return VPP_OK;
}

#if !defined PCIE_MODE
static int vpp_open_fd(struct vpp_mat_s* src,int *vpp_dev_fd);
int vpp_open_fd(struct vpp_mat_s* src,int *vpp_dev_fd)
{
    if((src == NULL) || (vpp_dev_fd == NULL)) {
        VppErr("vpp_open_fd parms err. src(0X%lx), vpp_dev_fd(0X%lx).\n", src, vpp_dev_fd);
        return VPP_ERR;
    }
    int fd_external = 0;
    if ((src->vpp_fd.dev_fd > 0) && (strncmp(src->vpp_fd.name, "bm-vpp", 7) == 0)) {
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
#endif

static int vpp_check(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type, int scale_type, struct csc_matrix *matrix)
{
    if((src == NULL) || (dst == NULL) || (loca == NULL)) {
        VppErr("vpp_check parms err. src(0X%lx), dst(0X%lx), loca(0X%lx).\n", src, dst ,loca);
        return VPP_ERR;
    }
    if((crop_num <= 0) || (crop_num > VPP_CROP_NUM_MAX)) {
        VppErr("vpp_check parms err. crop_num(%d).\n", crop_num);
        return VPP_ERR;
    }
    if((scale_type != VPP_SCALE_BILINEAR) && (scale_type != VPP_SCALE_NEAREST) &&
            (scale_type != VPP_SCALE_BICUBIC) ) {
        VppErr("vpp_check parms err. scale_type(%d).\n", scale_type);
        return VPP_ERR;
    }
    if(!((csc_type >= 0 && csc_type <= CSC_MAX) || ((matrix != NULL) && (csc_type == CSC_USER_DEFINED)))) {
        VppErr("vpp_check parms err. csc_type(%c), matrix(0X%lx).\n", csc_type, matrix);
        return VPP_ERR;
    }

    for (int i = 0; i < crop_num; i++) {
        if (!((src[i].format == FMT_I420) || (src[i].format == FMT_NV12) || (src[i].format == FMT_RGBP) || (src[i].format == FMT_BGR) ||
              (src[i].format == FMT_RGB) || (src[i].format == FMT_Y) || (src[i].format == FMT_BGRP) || (src[i].format == FMT_YUV444P) ||
              (src[i].format == FMT_YUV444) || (src[i].format == FMT_ABGR) || (src[i].format == FMT_ARGB))) {
            VppErr("vpp_check src[%d].format=%d err.\n", i, src[i].format);
            return VPP_ERR;
        }

        switch (src[i].format)
        {
            case FMT_Y:
                 if ((dst[i].format != FMT_Y) && (dst[i].format != FMT_RGB) && (dst[i].format != FMT_BGR) && (dst[i].format != FMT_RGBP) &&
                     (dst[i].format != FMT_BGRP) && (dst[i].format != FMT_ARGB)&& (dst[i].format != FMT_ABGR)) {
                     printf("vpp_check dst[%d].format(%d) not support, when src[%d].format is %d.\n", i, dst[i].format, i, src[i].format);
                     return VPP_ERR;
                 }
                 break;
            case FMT_NV12:
            case FMT_I420:
                 if ((dst[i].format != FMT_BGR) && (dst[i].format != FMT_RGB) && (dst[i].format != FMT_I420) && (dst[i].format != FMT_RGBP) &&
                     (dst[i].format != FMT_BGRP) && (dst[i].format != FMT_ARGB)&& (dst[i].format != FMT_ABGR)) {
                     printf("vpp_check dst[%d].format(%d) not support, when src[%d].format is %d.\n", i, dst[i].format, i, src[i].format);
                     return VPP_ERR;
                 }
                 break;
            case FMT_BGR:
            case FMT_RGB:
            case FMT_ABGR:
            case FMT_ARGB:
            case FMT_BGRP:
            case FMT_RGBP:
            case FMT_YUV444P:
                 if ((dst[i].format != FMT_BGR) && (dst[i].format != FMT_RGB) && (dst[i].format != FMT_I420) && (dst[i].format != FMT_RGBP) &&
                    (dst[i].format != FMT_BGRP) && (dst[i].format != FMT_YUV444P) && (dst[i].format != FMT_YUV422P) &&
                    (dst[i].format != FMT_ARGB) && (dst[i].format != FMT_ABGR)) {
                     printf("vpp_check dst[%d].format(%d) not support, when src[%d].format is %d.\n", i, dst[i].format, i, src[i].format);
                     return VPP_ERR;
                 }
                 break;
            case FMT_YUV444:
                 if ((dst[i].format != FMT_BGR) && (dst[i].format != FMT_RGB) && (dst[i].format != FMT_RGBP) &&
                    (dst[i].format != FMT_BGRP) && (dst[i].format != FMT_ARGB) && (dst[i].format != FMT_ABGR)) {
                     printf("vpp_check dst[%d].format(%d) not support, when src[%d].format is %d.\n", i, dst[i].format, i, src[i].format);
                     return VPP_ERR;
                 }
                 break;
            default:
                VppErr("wrong src format\n");
                return VPP_ERR;
        }

        if (((src[i].cols > MAX_RESOLUTION_W) || (src[i].rows > MAX_RESOLUTION_H)) ||
            ((src[i].cols < MIN_RESOLUTION_W) || (src[i].rows < MIN_RESOLUTION_H)) ||
            ((dst[i].cols > MAX_RESOLUTION_W) || (dst[i].rows > MAX_RESOLUTION_H)) ||
            ((dst[i].cols < MIN_RESOLUTION_W) || (dst[i].rows < MIN_RESOLUTION_H))) {
            printf("vpp_check cols or rows not support, src[%d].cols=%d, src[%d].rows=%d. dst[%d].cols=%d, dst[%d].rows=%d.\n", i, src[i].cols, i, src[i].rows, i, dst[i].cols, i, dst[i].rows);
            return VPP_ERR;
        }


        if(!((src[i].stride >= VPP_ALIGN(src[i].cols, STRIDE_ALIGN)) && ((src[i].stride % STRIDE_ALIGN) == 0)) ||
           !((dst[i].stride >= VPP_ALIGN(dst[i].cols, STRIDE_ALIGN)) && ((dst[i].stride % STRIDE_ALIGN) == 0))) {
            printf("vpp_check src stride(%d) not support or dst stride(%d) not support.\n", src[i].stride, dst[i].stride);
            return VPP_ERR;
        }

        if(!(((src[i].uv_stride >= (src[i].cols / 2)) || (src[i].uv_stride == 0)) && ((src[i].uv_stride % (STRIDE_ALIGN / 2)) == 0))) {
            printf("vpp_check src uv_stride(%d) not support.\n", src[i].uv_stride);
            return VPP_ERR;
        }
        if (!(((dst[i].uv_stride >= (dst[i].cols / 2)) || (dst[i].uv_stride == 0)) && ((dst[i].uv_stride % (STRIDE_ALIGN / 2)) == 0))) {
            printf("vpp_check dst uv_stride(%d) not support.\n", dst[i].uv_stride);
            return VPP_ERR;
        }

        if(!((loca[i].x >= 0) && (loca[i].x <= src[i].cols)) ||
           !((loca[i].y >= 0) && (loca[i].y <= src[i].rows)) ||
           !((loca[i].width >= MIN_RESOLUTION_W) && (loca[i].width <= src[i].cols)) ||
           !((loca[i].height >= MIN_RESOLUTION_W) && (loca[i].height <= src[i].rows))) {
            printf("vpp_check loca info maybe err. loca[x=%d, y=%d], loca[width=%d, height=%d], src[cols=%d, rows=%d]\n", \
                    loca[i].x, loca[i].y, loca[i].width, loca[i].height, src[i].cols, src[i].rows);
            return VPP_ERR;
        }

        if(!(((loca[i].x + loca[i].width) <= src[i].cols) && ((loca[i].y + loca[i].height) <= src[i].rows)) ||
           !((loca[i].width * 32 >= dst[i].cols) && (loca[i].height * 32 >= dst[i].rows)) ||
           !((loca[i].width <= dst[i].cols * 32) && (loca[i].height <= dst[i].rows * 32))) {
            printf("vpp_check loca info maybe err. loca[x=%d, y=%d], loca[width=%d, height=%d], src[cols=%d, rows=%d] dst[cols=%d, rows=%d].\n", \
                    loca[i].x, loca[i].y, loca[i].width, loca[i].height, src[i].cols, src[i].rows, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }

        switch (src[i].format)
        {
            case FMT_Y:
            case FMT_BGR:
            case FMT_RGB:
            case FMT_ABGR:
            case FMT_ARGB:
            case FMT_YUV444:
                 if (src[i].is_pa) {
                     if(src[i].pa[0] % 32 != 0) {
                         VppErr("vpp_check src.pa[0](%d) not 32bit alige.\n", src[i].pa[0]);
                         return VPP_ERR;
                     }
                 }
                 break;
            case FMT_NV12:
                 if (src[i].is_pa) {
                     if ((src[i].pa[0] % 32 != 0) ||
                         (src[i].pa[1] % 32 != 0)) {
                         VppErr("vpp_check src.pa[0](%d) or src.pa[1](%d) not 32bit alige.\n", src[i].pa[0], src[i].pa[1]);
                         return VPP_ERR;
                     }
                 }
                 break;
            case FMT_I420:
            case FMT_BGRP:
            case FMT_RGBP:
            case FMT_YUV444P:
                 if (src[i].is_pa) {
                     if ((src[i].pa[0] % 32 != 0) ||
                         (src[i].pa[1] % 32 != 0) ||
                         (src[i].pa[2] % 32 != 0)) {
                         VppErr("vpp_check src.pa[0](%d) or src.pa[1](%d) or src.pa[2](%d) not 32bit alige.\n", src[i].pa[0], src[i].pa[1], src[i].pa[2]);
                         return VPP_ERR;
                     }
                 }
                 break;
            default:
                VppErr("wrong src format\n");
                return VPP_ERR;
        }
        switch (dst[i].format)
        {
            case FMT_Y:
            case FMT_BGR:
            case FMT_RGB:
            case FMT_ABGR:
            case FMT_ARGB:
                 if (dst[i].is_pa) {
                     if(dst[i].pa[0] % 32 != 0) {
                         VppErr("vpp_check dst.pa[0](%d) not 32bit alige.\n", dst[i].pa[0]);
                         return VPP_ERR;
                     }
                 }
                 break;
            case FMT_I420:
            case FMT_BGRP:
            case FMT_RGBP:
            case FMT_YUV444P:
            case FMT_YUV422P:
                 if (dst[i].is_pa) {
                     if ((dst[i].pa[0] % 32 != 0) ||
                         (dst[i].pa[1] % 32 != 0) ||
                         (dst[i].pa[2] % 32 != 0)) {
                         VppErr("vpp_check dst.pa[0](%d) or dst.pa[1](%d) or dst.pa[2](%d) not 32bit alige.\n", dst[i].pa[0], dst[i].pa[1], dst[i].pa[2]);
                         return VPP_ERR;
                     }
                 }
                 break;
            default:
                VppErr("wrong dst format\n");
                return VPP_ERR;
        }

        if (((src[i].format == FMT_I420) || (src[i].format == FMT_NV12)) && (dst[i].format == FMT_I420)) {
            if (!((loca[i].x % 2== 0) && (loca[i].y % 2== 0) && (loca[i].width % 2== 0) && (loca[i].height % 2== 0)) ||
                 !((dst[i].cols % 2== 0) && (dst[i].rows % 2== 0) && (dst[i].axisX % 2== 0) && (dst[i].axisY % 2== 0))) {
                VppErr("vpp_check loca or dst info maybe err when src format=%d, dst format=%d.  \
                        loca(x=%d, y=%d, width=%d, height=%d), dst(cols=%d, rows=%d, axisX=%d, axisY=%d)\n", \
                        src[i].format, dst[i].format, loca[i].x, loca[i].y, loca[i].width, loca[i].height,  \
                        dst[i].cols, dst[i].rows, dst[i].axisX, dst[i].axisY);
                return VPP_ERR;
            }
        }
        if (dst[i].format == FMT_I420) {
            if(!((dst[i].cols % 2== 0) && (dst[i].rows % 2== 0) && (dst[i].axisX % 2== 0) && (dst[i].axisY % 2== 0))){
                VppErr("vpp_check dst info maybe err when dst format=%d. dst(cols=%d, rows=%d, axisX=%d, axisY=%d)\n", \
                       dst[i].format, dst[i].cols, dst[i].rows, dst[i].axisX, dst[i].axisY);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGBP) || (src[i].format == FMT_BGRP) || (src[i].format == FMT_YUV444P) ||
           (src[i].format == FMT_ABGR) || (src[i].format == FMT_ARGB)) && (dst[i].format == FMT_I420)) {
            if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))){
                VppErr("vpp_check loca or dst info maybe err when src format=%d, dst format=%d. \
                       loca(width=%d, height=%d), dst(cols=%d, rows=%d)\n", \
                       src[i].format, dst[i].format, loca[i].width, loca[i].height, dst[i].cols, dst[i].rows);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGBP) || (src[i].format == FMT_BGRP) || (src[i].format == FMT_YUV444P) ||
           (src[i].format == FMT_ABGR) || (src[i].format == FMT_ARGB)) && (dst[i].format == FMT_YUV422P)) {
            if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows)) ||
                !((dst[i].cols % 2== 0) && (dst[i].axisX % 2== 0))){
                VppErr("vpp_check loca or dst info maybe err when src format=%d, dst format=%d.  \
                       loca(width=%d, height=%d), dst(cols=%d, rows=%d, axisX=%d)\n", \
                       src[i].format, dst[i].format, loca[i].width, loca[i].height, dst[i].cols, dst[i].rows, dst[i].axisX);
                return VPP_ERR;
            }
        }

        if ((src[i].format == FMT_YUV444) &&
           ((dst[i].format == FMT_RGBP) || (dst[i].format == FMT_BGRP) || (dst[i].format == FMT_RGB) ||
           (dst[i].format == FMT_BGR) || (dst[i].format == FMT_ARGB) || (dst[i].format == FMT_ABGR))) {
            if(!((loca[i].x == 0) && (loca[i].y == 0))){
                VppErr("vpp_check loca info maybe err when src format=%d, dst format=%d. \
                       loca(x=%d, y=%d), dst(cols=%d, rows=%d, axisX=%d)\n",  \
                       src[i].format, dst[i].format, loca[i].x, loca[i].y, dst[i].cols, dst[i].rows, dst[i].axisX);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGB) || (src[i].format == FMT_BGR)) && (dst[i].format == FMT_YUV444P)) {
            if(!((loca[i].x == 0) && (loca[i].y == 0))) {
                VppErr("vpp_check loca info maybe err when src format=%d, dst format=%d. \
                       loca(x=%d, y=%d), dst(cols=%d, rows=%d, axisX=%d)\n",  \
                       src[i].format, dst[i].format, loca[i].x, loca[i].y, dst[i].cols, dst[i].rows, dst[i].axisX);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGB) || (src[i].format == FMT_BGR)) && (dst[i].format == FMT_I420)) {
            if(!((loca[i].x == 0) && (loca[i].y == 0)) ||
               !((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
                VppErr("vpp_check loca info maybe err when src format=%d, dst format=%d.  \
                       loca(x=%d, y=%d, width=%d), dst(cols=%d, rows=%d)\n",  \
                       src[i].format, dst[i].format, loca[i].x, loca[i].y, loca[i].width,dst[i].cols, dst[i].rows);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGB) || (src[i].format == FMT_BGR)) && (dst[i].format == FMT_YUV422P)) {
            if(!((loca[i].x == 0) && (loca[i].y == 0)) ||
               !((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows)) ||
               !((dst[i].cols % 2== 0) && (dst[i].axisX % 2== 0))) {
                VppErr("vpp_check loca info maybe err when src format=%d, dst format=%d. \
                       loca(x=%d, y=%d, width=%d, height=%d), dst(cols=%d, rows=%d, axisX=%d).\n", \
                       src[i].format, dst[i].format, loca[i].x, loca[i].y, loca[i].width, loca[i].height,  \
                       dst[i].cols, dst[i].rows, dst[i].axisX);
                return VPP_ERR;
            }
        }

        switch (src[i].format)
        {
            case FMT_Y:
            case FMT_NV12:
            case FMT_I420:
            case FMT_YUV444P:
            case FMT_YUV444:
            case FMT_YUV422P:
                 if ((dst[i].format == FMT_RGB)  ||
                     (dst[i].format == FMT_BGR)  ||
                     (dst[i].format == FMT_ABGR) ||
                     (dst[i].format == FMT_ARGB) ||
                     (dst[i].format == FMT_BGRP) ||
                     (dst[i].format == FMT_RGBP)) {
                     if(!((csc_type == CSC_MAX) || (csc_type == YCbCr2RGB_BT601) || (csc_type == YPbPr2RGB_BT601) ||
                               (csc_type == CSC_USER_DEFINED) || (csc_type == YCbCr2RGB_BT709) || (csc_type == YPbPr2RGB_BT709))) {
                         VppErr("csc_type(%d) err when src format is %d, dst format is %d.", csc_type, src[i].format, dst[i].format);
                         return VPP_ERR;
                     }
                 }
                 break;
            case FMT_BGR:
            case FMT_RGB:
            case FMT_ABGR:
            case FMT_ARGB:
            case FMT_BGRP:
            case FMT_RGBP:
                 if ((dst[i].format == FMT_I420)     ||
                     (dst[i].format == FMT_YUV444P)  ||
                     (dst[i].format == FMT_YUV422P) ) {
                     if(!((csc_type == CSC_MAX) || (csc_type == RGB2YCbCr_BT601) || (csc_type == RGB2YCbCr_BT709) ||
                               (csc_type == CSC_USER_DEFINED) || (csc_type == RGB2YPbPr_BT601) || (csc_type == RGB2YPbPr_BT709))) {
                         VppErr("csc_type(%d) err when src format is %d, dst format is %d.", csc_type, src[i].format, dst[i].format);
                         return VPP_ERR;
                     }
                 }

                 break;
            default:
                break;
        }

    }

    return VPP_OK;
}

int vpp_misc(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type, int scale_type)
{
    int ret = VPP_OK;

    ret = vpp_misc_matrix(src, loca, dst, crop_num, csc_type, scale_type, NULL);

    return ret;
}

int vppmat_from_plane(vpp_mat *d, vpp_mat *s, int idx)
{
    d->format = FMT_Y;
    d->num_comp = 1;
    d->pa[0] = s->pa[idx];
    d->ion_len[0] = s->ion_len[idx];
    if (s->format == FMT_YUV422P) {
        switch (idx) {
            case 0:
                d->stride = s->stride;
                d->cols = s->cols;
                d->rows = (s->rows+1)&(~1);
                break;
            case 1:
            case 2:
                d->stride = s->uv_stride;
                d->cols = s->uv_stride;
                d->rows = (s->rows+1)&(~1);
                break;
        }
    }else if (s->format == FMT_I420) {
        switch (idx) {
            case 0:
                d->stride = s->stride;
                d->cols = s->cols;
                d->rows = (s->rows+1)&(~1);
                break;
            case 1:
            case 2:
                d->stride = s->uv_stride;
                d->cols = s->uv_stride;
                d->rows = (s->rows+1)/2;
                break;
        }
    }else{
        VppErr("vppmat_from_plane error, format not support\n");
        return -1;
    }
    d->uv_stride = s->uv_stride;
    d->is_pa = 1;
    d->axisX = 0;
    d->axisY = 0;
    d->vpp_fd = s->vpp_fd;
    return 0;
}

int vpp_yuv422p_to_yuv420p(vpp_mat* src, vpp_rect* loca, vpp_mat* dst, int crop_num, vpp_csc_type csc_type, vpp_scale_type scale_type, vpp_csc_matrix *matrix)
{
    int ret = 0;
    for(int crop_idx = 0; crop_idx < crop_num; crop_idx++){
        for(int i = 0;i < 3; i++) {
            vpp_mat ps={0};
            vpp_mat pd={0};
            if(vppmat_from_plane(&ps, &src[crop_idx], i) < 0)
                return -1;
            if(vppmat_from_plane(&pd, &dst[crop_idx], i) < 0)
                return -1;
            vpp_rect crop1;
            crop1.x = 0;
            crop1.y = 0;
            crop1.width = ps.cols;
            crop1.height = ps.rows;
            ret = vpp_misc(&ps, &crop1, &pd, 1, csc_type, scale_type);
            if (ret < 0) {
                VppErr("vpp_misc errr");
                return ret;
            }
        }
    }
    return 0;
}

int vpp_basic(vpp_mat* src, vpp_rect* loca, vpp_mat* dst, int in_img_num, int* crop_num_vec, vpp_csc_type csc_type, vpp_scale_type scale_type, vpp_csc_matrix *matrix)
{
    int crop_num = 0;
    for (int i = 0; i < in_img_num; i++) {
        crop_num += crop_num_vec[i];
    }
    vpp_mat buf[VPP_CROP_NUM_MAX];
    int idx = 0;
    for (int i = 0; i < in_img_num; i++) {
        for (int j = 0; j < crop_num_vec[i]; j++) {
            buf[idx + j] = src[i];
        }
        idx += crop_num_vec[i];
    }
    int ret = VPP_OK;
    if(src->format == FMT_YUV422P && dst->format == FMT_I420){
        ret = vpp_yuv422p_to_yuv420p(buf, loca, dst, crop_num, csc_type, scale_type, matrix);
    }
    else
        ret = vpp_misc_matrix(buf, loca, dst, crop_num, csc_type, scale_type, matrix);
    return ret;
}

int vpp_misc_matrix(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst,
  int crop_num, char csc_type, int scale_type, struct csc_matrix *matrix)
{
    int ret = 0;
    struct vpp_cmd_n batch_cmd;

#ifdef PCIE_MODE
    if(!(src[0].vpp_fd.handle != NULL)) {
        VppErr("vpp_misc_matrix params is err. src[0].vpp_fd.handle=0X%lx.\n", src[0].vpp_fd.handle);
        return VPP_ERR;
    }
    bm_handle_t handle = NULL;
    handle = src[0].vpp_fd.handle;
    int * need_restore;
	need_restore = (int *)malloc(crop_num * sizeof(int));

    bm_device_mem_t *dev_buf_uv;
	dev_buf_uv = (bm_device_mem_t *)malloc(crop_num * sizeof(bm_device_mem_t));
    ret = vpp_disguise_y400_as_nv12_pcie(handle, src, dst, crop_num, dev_buf_uv, need_restore);
    if (ret == VPP_ERR) {
        return ret;
    }
#elif defined USING_CMODEL

#else
    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(&src[0],&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        ret = VPP_ERR;
        return ret;
    }

    int need_restore[crop_num];
    ion_para uv_data_para[crop_num];
    vpp_disguise_y400_as_nv12_soc(src, dst, crop_num, uv_data_para, need_restore);
#endif

    ret = vpp_check(src, loca, dst, crop_num, csc_type, scale_type, matrix);
    if (ret == VPP_ERR) {
        return ret;
    }

    struct vpp_batch_n batch;

    batch.num = crop_num;
    batch.cmd = malloc(batch.num * (sizeof(batch_cmd)));
    if (batch.cmd == NULL) {
        ret = -VPP_ERR;
        VppErr("batch memory malloc failed\n");
        return ret;
    }
    memset(batch.cmd,0,(batch.num * (sizeof(batch_cmd))));
    for (int i = 0; i < crop_num; i++) {
        struct vpp_cmd_n *cmd = (batch.cmd + i);

        ret = vpp_batch_pa(cmd, &src[i], &dst[i]);
        cmd->mapcon_enable = 0;
        cmd->src_csc_en = 0;
        ret = vpp_batch_format(cmd, src[i].format, dst[i].format);
        if (ret < 0) {
            VppErr("format config wrong\n");
        }

        ret = vpp_addr_config(cmd, src[i].format, dst[i].format);
        if (ret == VPP_ERR) {
            return ret;
        }

        if (scale_type == VPP_SCALE_BICUBIC) {
            if (loca[i].width > dst[i].cols)
                cmd->hor_filter_sel = VPP_FILTER_SEL_BICUBIC_DOWN;
            else
                cmd->hor_filter_sel = VPP_FILTER_SEL_BICUBIC_UP;

            if (loca[i].height > dst[i].rows)
                cmd->ver_filter_sel = VPP_FILTER_SEL_BICUBIC_DOWN;
            else
                cmd->ver_filter_sel = VPP_FILTER_SEL_BICUBIC_UP;
        } else if (scale_type == VPP_SCALE_NEAREST) {
            cmd->hor_filter_sel = VPP_FILTER_SEL_NEAREST;
            cmd->ver_filter_sel = VPP_FILTER_SEL_NEAREST;
        } else {
            cmd->hor_filter_sel = VPP_FILTER_SEL_BILINEAR;
            cmd->ver_filter_sel = VPP_FILTER_SEL_BILINEAR;
        }

        /*scl ctr  SA5SW-558*/
        if (((src[i].format == FMT_RGB) || (src[i].format == FMT_BGR)|| (src[i].format == FMT_ABGR)|| (src[i].format == FMT_ARGB) ||
           (src[i].format == FMT_RGBP) || (src[i].format == FMT_BGRP) || (src[i].format == FMT_YUV444P) || (src[i].format == FMT_ARGB) ||
           (src[i].format == FMT_ABGR)) && ((dst[i].format == FMT_I420) || (dst[i].format == FMT_YUV422P)) &&
           ((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
            cmd->hor_filter_sel = VPP_SCALE_NEAREST;
            cmd->ver_filter_sel = cmd->hor_filter_sel;
        }

        cmd->src_stride    = src[i].stride;
        cmd->dst_stride    = dst[i].stride;
        cmd->src_uv_stride = src[i].uv_stride;
        cmd->dst_uv_stride = dst[i].uv_stride;
        cmd->cols         = src[i].cols;
        cmd->rows         = src[i].rows ;
        cmd->src_cropH    = loca[i].height;
        cmd->src_cropW    = loca[i].width;
        cmd->src_axisX    = loca[i].x;
        cmd->src_axisY    = loca[i].y;
        cmd->dst_cropH    = dst[i].rows;
        cmd->dst_cropW    = dst[i].cols;
        cmd->dst_axisX    = dst[i].axisX;//border use
        cmd->dst_axisY    = dst[i].axisY;//border use
       if((matrix != NULL) && (csc_type == CSC_USER_DEFINED)) {
            cmd->matrix.csc_coe00 = matrix->csc_coe00;
            cmd->matrix.csc_coe01 = matrix->csc_coe01;
            cmd->matrix.csc_coe02 = matrix->csc_coe02;
            cmd->matrix.csc_add0  = matrix->csc_add0;
            cmd->matrix.csc_coe10 = matrix->csc_coe10;
            cmd->matrix.csc_coe11 = matrix->csc_coe11;
            cmd->matrix.csc_coe12 = matrix->csc_coe12;
            cmd->matrix.csc_add1  = matrix->csc_add1;
            cmd->matrix.csc_coe20 = matrix->csc_coe20;
            cmd->matrix.csc_coe21 = matrix->csc_coe21;
            cmd->matrix.csc_coe22 = matrix->csc_coe22;
            cmd->matrix.csc_add2  = matrix->csc_add2;
            cmd->src_csc_en = 1;
        }
        if (csc_type != CSC_MAX) {
            cmd->csc_type = csc_type;
        }
    }

#ifdef PCIE_MODE
    ret = bm_trigger_vpp((bm_handle_t )src[0].vpp_fd.handle, &batch);

    vpp_restore_nv12_to_y400_pcie(handle, src, dst, crop_num, dev_buf_uv, need_restore);
	free(dev_buf_uv);
	free(need_restore);
#elif defined USING_CMODEL
    vpp1684_cmodel_test(&batch);
#else
    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (!fd_external) {
        close(vpp_dev_fd);
    }

    vpp_restore_nv12_to_y400_soc(src, dst, crop_num, uv_data_para, need_restore);
#endif
    if (ret != 0) {
        vpp_return_value_analysis(ret);
        vpp_dump(&batch);
    }

    free(batch.cmd);
    return ret;
}

int vpp_crop_csc_multi_ctype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_crop_csc_multi_ctype params is err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))){
            VppErr("vpp_crop_csc_multi_ctype loca info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d).\n", \
                   i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = vpp_misc(src, loca, dst, crop_num, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_crop_csc_single_ctype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_crop_csc_single_ctype params is err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))){
            VppErr("vpp_crop_csc_multi_ctype loca info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d).\n", \
                   i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }
        if(!(dst[i].format != FMT_NV12)) {
            VppErr("vpp_crop_csc_single_ctype dst format[%d]=%d not FMT_NV12.\n", i, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = vpp_misc(src1, loca, dst, crop_num, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_crop_csc_multi(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_crop_csc_multi params is err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
            VppErr("vpp_crop_csc_multi loca info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d).\n", \
                   i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = vpp_misc(src, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_crop_csc_single(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_crop_csc_single params is err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
            VppErr("vpp_crop_csc_single loca info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d).\n", \
                   i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }
        if(!(dst[i].format != FMT_NV12)) {
            VppErr("vpp_crop_csc_single dst format[%d]=%d not FMT_NV12.\n", i, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = vpp_misc(src1, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}
int vpp_resize_crop_single(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_resize_crop_single params is err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!(src[0].format == dst[i].format) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("vpp_resize_crop_single src format[0](%d)  or dst[%d](%d) is error.\n", src[0].format, i, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = vpp_misc(src1, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_resize_crop_multi(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_resize_crop_multi params is err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!(src[i].format == dst[i].format) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("vpp_resize_crop_multi src format[0](%d)  or dst[%d](%d) is error.\n", src[0].format, i, dst[i].format);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = vpp_misc(src, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_resize_crop_single_stype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, int scale_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        printf("vpp_resize_crop_single_stype params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!(src[0].format == dst[i].format) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("vpp_resize_crop_multi src format[0](%d)  or dst[%d](%d) is error.\n", src[0].format, i, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = vpp_misc(src1, loca, dst, crop_num, CSC_MAX, scale_type);
    return ret;
}

int vpp_resize_crop_multi_stype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, int scale_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_resize_crop_multi_stype params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!(src[i].format == dst[i].format) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("vpp_resize_crop_multi_stype src format[0](%d)  or dst[%d](%d) is error.\n", src[0].format, i, dst[i].format);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = vpp_misc(src, loca, dst, crop_num, CSC_MAX, scale_type);
    return ret;
}

int vpp_resize_csc(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    if(!((src != NULL) && (dst != NULL))) {
        VppErr("vpp_resize_csc params err. src=0X%lx, dst=0X%lx\n", src, dst);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_resize_csc_ctype(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX)) {
        VppErr("vpp_resize_csc_ctype params err. src=0X%lx, dst=0X%lx, csc_type=%d\n", src, dst, csc_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_resize_csc_stype(struct vpp_mat_s* src, struct vpp_mat_s* dst, int scale_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(scale_type == VPP_SCALE_BILINEAR|| scale_type == VPP_SCALE_NEAREST || scale_type == VPP_SCALE_BICUBIC)) {
        VppErr("vpp_resize_csc_stype params err. src=0X%lx, dst=0X%lx, scale_type=%d\n", src, dst, scale_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, scale_type);
    return ret;
}
int vpp_resize_csc_ctype_stype(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type, int scale_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX) ||
       !(scale_type == VPP_SCALE_BILINEAR || scale_type == VPP_SCALE_NEAREST || scale_type == VPP_SCALE_BICUBIC)) {
        VppErr("vpp_resize_csc_ctype_stype params err. src=0X%lx, dst=0X%lx, csc_type=%d, scale_type=%d\n", src, dst, csc_type, scale_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, csc_type, scale_type);
    return ret;
}

int vpp_resize(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    if (!((src != NULL) && (dst != NULL))) {
        VppErr("vpp_resize params err. src=0X%lx, dst=0X%lx\n", src, dst);
        return VPP_ERR;
    }
    if (!(src->format == dst->format) ||
        !(dst->format != FMT_NV12)) {
        VppErr("vpp_resize params err. src format=%d, dst format=%d\n", src->format, dst->format);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_resize_stype(struct vpp_mat_s* src, struct vpp_mat_s* dst, int scale_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(scale_type == VPP_SCALE_BILINEAR || scale_type == VPP_SCALE_NEAREST || scale_type == VPP_SCALE_BICUBIC)) {
        VppErr("vpp_resize_stype params err. src=0X%lx, dst=0X%lx, scale_type=%d\n", src, dst, scale_type);
        return VPP_ERR;
    }
    if(!(src->format == dst->format) ||
       !(dst->format != FMT_NV12)) {
        VppErr("vpp_resize_stype src format(%d) or dst format(%d) is error.\n", src->format, dst->format);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, scale_type);
    return ret;
}

int vpp_csc_ctype(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX)) {
        VppErr("vpp_csc_ctype params err. src=0X%lx, dst=0X%lx, csc_type=%d\n", src, dst, csc_type);
        return VPP_ERR;
    }
    if(!(src->rows == dst->rows) ||
       !(src->cols == dst->cols) ||
       !(dst->format != FMT_NV12)) {
        VppErr("vpp_csc_ctype src or dst info is err. src(rows=%d,cols=%d), dst(rows=%d, cols=%d, format=%d).\n", \
                src->rows, src->cols, dst->rows, dst->cols, dst->format);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}
int vpp_csc(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    if(!((src != NULL) && (dst != NULL))) {
        VppErr("vpp_csc params err. src=0X%lx, dst=0X%lx.\n", src, dst);
        return VPP_ERR;
    }
    if(!(src->rows == dst->rows) ||
       !(src->cols == dst->cols) ||
       !(dst->format != FMT_NV12)) {
        VppErr("vpp_csc_ctype src or dst info is err. src(rows=%d,cols=%d), dst(rows=%d, cols=%d, format=%d).\n", \
                src->rows, src->cols, dst->rows, dst->cols, dst->format);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_split(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    if(!((src != NULL) && (dst != NULL))) {
        VppErr("vpp_split params err. src=0X%lx, dst=0X%lx.\n", src, dst);
        return VPP_ERR;
    }
    if(!(src->rows == dst->rows) ||
       !(src->cols == dst->cols) ||
       !((dst->format == FMT_RGBP) || (dst->format == FMT_BGRP)) ||
       !((src->format == FMT_BGR) || (src->format == FMT_RGB))) {
        VppErr("vpp_split src or dst info is err. src(rows=%d,cols=%d,format=%d), dst(rows=%d, cols=%d, format=%d).\n", \
                src->rows, src->cols, src->format, dst->rows, dst->cols, dst->format);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}


int vpp_border(struct vpp_mat_s* src, struct vpp_mat_s* dst, int top, int bottom, int left, int right)
{
    if(!((src != NULL) && (dst != NULL))) {
        VppErr("vpp_border params err. src=0X%lx, dst=0X%lx.\n", src, dst);
        return VPP_ERR;
    }
    if(!(src->format == dst->format) ||
       !(dst->format != FMT_NV12) ||
       !(top >= 0 && bottom >= 0 && left >= 0 && right >= 0) ||
       !(((src->cols + left + right) <= MAX_RESOLUTION_W) && ((src->rows + top + bottom) <= MAX_RESOLUTION_H)) ||
       !(((src->cols + left + right) >= MIN_RESOLUTION_W) && ((src->rows + top + bottom) >= MIN_RESOLUTION_H))) {
        VppErr("vpp_border params err. src(rows=%d,cols=%d,format=%d), dst(format=%d), top=%d, bottom=%d, left=%d, right=%d.\n", \
                src->rows, src->cols, src->format, dst->format, top, bottom, left, right);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    int axisX = dst->axisX, axisY = dst->axisY, rows = dst->rows, cols = dst->cols;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    dst->axisX = left;
    dst->axisY = top;
    dst->rows = src->rows;
    dst->cols = src->cols;

    ret = vpp_misc(src, &loca, dst, 1, CSC_MAX, VPP_SCALE_BILINEAR);

    dst->axisX = axisX;
    dst->axisY = axisY;
    dst->rows = rows;
    dst->cols = cols;
    return ret;
}

int vpp_crop_multi(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_crop_multi params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows)) ||
           !(src[i].format == dst[i].format) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("vpp_crop_multi loca info or src info or dst info ie err. \
                    loca[%d](width=%d, height=%d), src[%d](format=%d), dst[%d](cols=%d, rows=%d,format=%d).\n", \
                    i, loca[i].width, loca[i].height, i, src[i].format, i, dst[i].cols, dst[i].rows, dst[i].format);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = vpp_misc(src, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int vpp_crop_single(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("vpp_crop_single params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows)) ||
           !(src[0].format == dst[i].format) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("vpp_crop_single loca info or src info or dst info ie err. \
                    loca[%d](width=%d, height=%d), src[%d](format=%d), dst[%d](cols=%d, rows=%d,format=%d).\n", \
                    i, loca[i].width, loca[i].height, i, src[i].format, i, dst[i].cols, dst[i].rows, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = vpp_misc(src1, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int fbd_csc_resize(struct vpp_mat_s* src, struct vpp_mat_s* dst)
{
    if(!((src != NULL) && (dst != NULL))) {
        VppErr("fbd_csc_resize params err. src=0X%lx, dst=0X%lx.\n", src, dst);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = fbd_csc_crop_multi_resize_ctype_stype(src, &loca, dst, 1, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}
int fbd_csc_resize_ctype(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX)) {
        VppErr("fbd_csc_resize_ctype params err. src=0X%lx, dst=0X%lx, csc_type=%d.\n", src, dst, csc_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = fbd_csc_crop_multi_resize_ctype_stype(src, &loca, dst, 1, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}
int fbd_csc_resize_stype(struct vpp_mat_s* src, struct vpp_mat_s* dst, int scale_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(scale_type == VPP_SCALE_BILINEAR || scale_type == VPP_SCALE_NEAREST || scale_type == VPP_SCALE_BICUBIC)) {
        VppErr("fbd_csc_resize_stype params err. src=0X%lx, dst=0X%lx, scale_type=%d.\n", src, dst, scale_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = fbd_csc_crop_multi_resize_ctype_stype(src, &loca, dst, 1, CSC_MAX, scale_type);
    return ret;

}
int fbd_csc_resize_ctype_stype(struct vpp_mat_s* src, struct vpp_mat_s* dst, char csc_type, int scale_type)
{
    if(!((src != NULL) && (dst != NULL)) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX) ||
       !(scale_type == VPP_SCALE_BILINEAR || scale_type == VPP_SCALE_NEAREST || scale_type == VPP_SCALE_BICUBIC)) {
        VppErr("fbd_csc_resize_ctype_stype params err. src=0X%lx, dst=0X%lx, csc_type=%d, scale_type=%d.\n", src, dst, csc_type, scale_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    struct vpp_rect_s loca;
    loca.height = src->rows;
    loca.width = src->cols;
    loca.x = 0;
    loca.y = 0;

    ret = fbd_csc_crop_multi_resize_ctype_stype(src, &loca, dst, 1, csc_type, scale_type);
    return ret;
}
int fbd_csc_crop_multi(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("fbd_csc_crop_multi params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }

    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
            VppErr("fbd_csc_crop_multi loca info or dst info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d).\n", \
                    i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = fbd_csc_crop_multi_resize_ctype_stype(src, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}
int fbd_csc_crop_single(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX)) {
        VppErr("fbd_csc_crop_single params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d.\n", src, dst, loca, crop_num);
        return VPP_ERR;
    }

    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows)) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("fbd_csc_crop_single loca info or dst info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d, format=%d).\n", \
                    i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = fbd_csc_crop_multi_resize_ctype_stype(src1, loca, dst, crop_num, CSC_MAX, VPP_SCALE_BILINEAR);
    return ret;
}

int fbd_csc_crop_multi_ctype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX)) {
        VppErr("fbd_csc_crop_multi_ctype params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d, csc_type=%d.\n", src, dst, loca, crop_num, csc_type);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if (!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
            VppErr("fbd_csc_crop_multi_ctype loca info or dst info err. loca[%d](width=%d, height=%d), dst[%d](cols=%d, rows=%d).\n", \
                    i, loca[i].width, loca[i].height, i, dst[i].cols, dst[i].rows);
            return VPP_ERR;
        }
    }

    int ret = VPP_OK;
    ret = fbd_csc_crop_multi_resize_ctype_stype(src, loca, dst, crop_num, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}

int fbd_csc_crop_single_ctype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX)) {
        VppErr("fbd_csc_crop_single_ctype params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d, csc_type=%d.\n", \
                 src, dst, loca, crop_num, csc_type);
        return VPP_ERR;
    }
    for (int i = 0; i < crop_num; i++) {
        if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows)) ||
           !(dst[i].format != FMT_NV12)) {
            VppErr("fbd_csc_crop_single_ctype loca info or dst info err. loca[%d](width=%d, height=%d), dst[%d](format=%d).\n", \
                    i, loca[i].width, loca[i].height, i, dst[i].format);
            return VPP_ERR;
        }
    }

    struct vpp_mat_s src1[VPP_CROP_NUM_MAX];
    for (int i = 0; i < crop_num; i++) {
        src1[i] = src[0];
    }

    int ret = VPP_OK;
    ret = fbd_csc_crop_multi_resize_ctype_stype(src1, loca, dst, crop_num, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}
int fbd_csc_crop_multi_resize_ctype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX) ||
       !(csc_type >= 0 && csc_type <= CSC_MAX)) {
        VppErr("fbd_csc_crop_single_ctype params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d, csc_type=%d.\n", \
                src, dst, loca, crop_num, csc_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    ret = fbd_csc_crop_multi_resize_ctype_stype(src, loca, dst, crop_num, csc_type, VPP_SCALE_BILINEAR);
    return ret;
}
int fbd_csc_crop_multi_resize_stype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, int scale_type)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX) ||
       !(scale_type == VPP_SCALE_BILINEAR || scale_type == VPP_SCALE_NEAREST || VPP_SCALE_BICUBIC == scale_type)) {
        VppErr("fbd_csc_crop_multi_resize_stype params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d, scale_type=%d.\n", \
                src, dst, loca, crop_num, scale_type);
        return VPP_ERR;
    }

    int ret = VPP_OK;
    ret = fbd_csc_crop_multi_resize_ctype_stype(src, loca, dst, crop_num, CSC_MAX, scale_type);
    return ret;

}
static int fbd_vpp_check(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst,
	int crop_num, char csc_type, int scale_type, struct csc_matrix *matrix)
{
    if(!((src != NULL) && (dst != NULL) && (loca != NULL)) ||
       !(crop_num > 0 && crop_num <= VPP_CROP_NUM_MAX) ||
       !(scale_type == VPP_SCALE_BILINEAR || scale_type == VPP_SCALE_NEAREST || scale_type== VPP_SCALE_BICUBIC) ||
       !((csc_type >= 0 && csc_type <= CSC_MAX) || ((matrix != NULL) && (csc_type == CSC_USER_DEFINED)))) {
        VppErr("fbd_vpp_check params err. src=0X%lx, dst=0X%lx, loca=0X%lx, crop_num=%d, csc_type=%d, scale_type=%d, matrix=0X%lx.\n", \
                src, dst, loca, crop_num, csc_type, scale_type, matrix);
        return VPP_ERR;
    }

    for (int i = 0; i < crop_num; i++) {
        if(!((src[i].format == FMT_I420) || (src[i].format == FMT_NV12))) {
            VppErr("fbd_vpp_check src[%d].fortmat=%d not support.\n", i, src[i].format);
            return VPP_ERR;
        }
        if(!((dst[i].format == FMT_BGR) || (dst[i].format == FMT_RGB) || (dst[i].format == FMT_ABGR) ||
          (dst[i].format == FMT_ARGB) || (dst[i].format == FMT_I420) || (dst[i].format == FMT_RGBP) ||
          (dst[i].format == FMT_BGRP))) {
            VppErr("fbd_vpp_check dst[%d].fortmat=%d not support.\n", i, dst[i].format);
            return VPP_ERR;
        }

        if(!((src[i].cols <= MAX_RESOLUTION_W) && (src[i].rows <= MAX_RESOLUTION_H)) ||
           !((src[i].cols >= MIN_RESOLUTION_W) && (src[i].rows >= MIN_RESOLUTION_H)) ||
           !((dst[i].cols <= MAX_RESOLUTION_W) && (dst[i].rows <= MAX_RESOLUTION_H)) ||
           !((dst[i].cols >= MIN_RESOLUTION_W) && (dst[i].rows >= MIN_RESOLUTION_H))) {
            VppErr("fbd_vpp_check src[%d].cols=%d or src[%d].rows=%d or dst[%d].cols=%d or dst[%d].rows=%d not support.\n", \
                  i, src[i].cols, i, src[i].rows, i, dst[i].cols, i, dst[i].rows);
            return VPP_ERR;
        }

//      VppAssert((src[i].stride >= VPP_ALIGN(src[i].cols, STRIDE_ALIGN)) && ((src[i].stride % STRIDE_ALIGN) == 0));
//      VppAssert(((src[i].uv_stride >= (src[i].cols / 2)) || (src[i].uv_stride == 0)) && ((src[i].uv_stride % (STRIDE_ALIGN / 2)) == 0));

        if(!((dst[i].stride >= VPP_ALIGN(dst[i].cols, STRIDE_ALIGN)) && ((dst[i].stride % STRIDE_ALIGN) == 0)) ||
           !(((dst[i].uv_stride >= (dst[i].cols / 2)) || (dst[i].uv_stride == 0)) && ((dst[i].uv_stride % (STRIDE_ALIGN / 2)) == 0))) {
            VppErr("fbd_vpp_check dst[%d].stride is not support.\n", i);
            return VPP_ERR;
        }

        if(!((loca[i].x >= 0) && (loca[i].x <= src[i].cols)) ||
           !((loca[i].y >= 0) && (loca[i].y <= src[i].rows)) ||
           !((loca[i].width >= MIN_RESOLUTION_W) && (loca[i].width <= src[i].cols)) ||
           !((loca[i].height >= MIN_RESOLUTION_W) && (loca[i].height <= src[i].rows))) {
            VppErr("fbd_vpp_check loca[%d] rect not support.\n", i);
            return VPP_ERR;
        }

        if(!(((loca[i].x + loca[i].width) <= src[i].cols) && ((loca[i].y + loca[i].height) <= src[i].rows)) ||
           !((loca[i].width * 32 >= dst[i].cols) && (loca[i].height * 32 >= dst[i].rows)) ||
           !((loca[i].width <= dst[i].cols * 32) && (loca[i].height <= dst[i].rows * 32))) {
            VppErr("fbd_vpp_check loca[%d] rect or src[%d] rect or dst[%d] rect not support.\n", i, i, i);
            return VPP_ERR;
        }

        if (src[i].is_pa) {
            if(!(src[i].pa[0] % 32 == 0) ||
               !(src[i].pa[1] % 32 == 0) ||
               !(src[i].pa[2] % 32 == 0) ||
               !(src[i].pa[3] % 32 == 0)) {
                VppErr("fbd_vpp_check src[%d].pa not align.\n", i);
                return VPP_ERR;
            }
        }

        switch (dst[i].format)
        {
            case FMT_BGR:
            case FMT_RGB:
            case FMT_ABGR:
            case FMT_ARGB:
                 if (dst[i].is_pa) {
                     if(!(dst[i].pa[0] % 32 == 0)) {
                         VppErr("fbd_vpp_check dst[%d].pa[0]=0x%p not align when dst[%d].format=%d.\n", i, (void*)dst[i].pa[0], i, dst[i].format);
                         return VPP_ERR;
                     }
                 }
                 break;
            case FMT_I420:
            case FMT_BGRP:
            case FMT_RGBP:
                 if (dst[i].is_pa) {
                     if(!(dst[i].pa[0] % 32 == 0) ||
                        !(dst[i].pa[1] % 32 == 0) ||
                        !(dst[i].pa[2] % 32 == 0)) {
                         VppErr("fbd_vpp_check dst[%d].pa not align when dst[%d].format=%d.\n", i, i, dst[i].format);
                         return VPP_ERR;
                     }
                 }
                 break;
            default:
                VppErr("wrong dst format\n");
                return VPP_ERR;
        }

        if (dst[i].format == FMT_I420) {
            if(!((loca[i].x % 2== 0) && (loca[i].y % 2== 0) && (loca[i].width % 2== 0) && (loca[i].height % 2== 0)) ||
               !((dst[i].cols % 2== 0) && (dst[i].rows % 2== 0) && (dst[i].axisX % 2== 0) && (dst[i].axisY % 2== 0))) {
                VppErr("fbd_vpp_check loca[%d] rect or src[%d] rect or dst[%d] rect not support.\n", i, i, i);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGBP) || (src[i].format == FMT_BGRP) || (src[i].format == FMT_YUV444P)) && (dst[i].format == FMT_I420)) {
            if(!((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))){
                VppErr("fbd_vpp_check loca[%d] rect or src[%d] rect or dst[%d] rect not support.\n", i, i, i);
                return VPP_ERR;
            }
        }

        if (((src[i].format == FMT_RGB) || (src[i].format == FMT_BGR)) && (dst[i].format == FMT_I420)) {
            if(!((loca[i].x == 0) && (loca[i].y == 0)) ||
               !((loca[i].width == dst[i].cols) && (loca[i].height == dst[i].rows))) {
                VppErr("fbd_vpp_check loca[%d] rect or src[%d] rect or dst[%d] rect not support.\n", i, i, i);
                return VPP_ERR;
            }
        }
        switch (src[i].format)
        {
            case FMT_NV12:
            case FMT_I420:
                 if ((dst[i].format == FMT_RGB)  ||
                     (dst[i].format == FMT_BGR)  ||
                     (dst[i].format == FMT_ABGR) ||
                     (dst[i].format == FMT_ARGB) ||
                     (dst[i].format == FMT_BGRP) ||
                     (dst[i].format == FMT_RGBP)) {
                     if(!((csc_type == CSC_MAX) || (csc_type == YCbCr2RGB_BT601) || (csc_type == YPbPr2RGB_BT601) ||
                               (csc_type == CSC_USER_DEFINED) || (csc_type == YCbCr2RGB_BT709) || (csc_type == YPbPr2RGB_BT709))) {
                         VppErr("fbd_vpp_check csc_type=%d not support when dst[%d].format=%d.\n", csc_type, i, dst[i].format);
                         return VPP_ERR;
                     }
                 }
                 break;
            default:
                break;
        }
    }

    return VPP_OK;
}

int fbd_csc_crop_multi_resize_ctype_stype(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type, int scale_type)
{
    int ret = VPP_OK;

    ret = fbd_matrix(src, loca, dst, crop_num, csc_type, scale_type, NULL);

    return ret;
}

int fbd_matrix(struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst,
	int crop_num, char csc_type, int scale_type, struct csc_matrix *matrix)
{
    int ret = VPP_OK;
    ret = fbd_vpp_check(src, loca, dst, crop_num, csc_type, scale_type, matrix);
    if (ret != VPP_OK) {
        return VPP_ERR;
    }
    struct vpp_cmd_n batch_cmd;
#ifdef PCIE_MODE
    if(!(src[0].vpp_fd.handle != NULL)) {
        VppErr("src[0].vpp_fd.handle is null.\n");
        return VPP_ERR;
    }
#else
    int vpp_dev_fd = -1, fd_external = 0;
    fd_external = vpp_open_fd(&src[0],&vpp_dev_fd);
    if (vpp_dev_fd < 0) {
        ret = VPP_ERR;
        return ret;
    }
#endif

    struct vpp_batch_n batch;

    batch.num = crop_num;
    batch.cmd = malloc(batch.num * (sizeof(batch_cmd)));
    if (batch.cmd == NULL) {
        ret = -VPP_ERR;
        VppErr("batch memory malloc failed\n");
        return ret;
    }
    memset(batch.cmd,0,(batch.num * (sizeof(batch_cmd))));
    for (int i = 0; i < crop_num; i++) {
        struct vpp_cmd_n *cmd = (batch.cmd + i);

        ret = vpp_batch_pa(cmd, &src[i], &dst[i]);
        cmd->src_csc_en = 0;
        ret = vpp_batch_format(cmd, src[i].format, dst[i].format);
        if (ret < 0) {
            VppErr("format config wrong\n");
        }

        /*scl ctr*/
        if (scale_type == VPP_SCALE_BICUBIC) {
            if (loca[i].width > dst[i].cols)
                cmd->hor_filter_sel = VPP_FILTER_SEL_BICUBIC_DOWN;
            else
                cmd->hor_filter_sel = VPP_FILTER_SEL_BICUBIC_UP;

            if (loca[i].height > dst[i].rows)
                cmd->ver_filter_sel = VPP_FILTER_SEL_BICUBIC_DOWN;
            else
                cmd->ver_filter_sel = VPP_FILTER_SEL_BICUBIC_UP;
        } else if (scale_type == VPP_SCALE_NEAREST) {
            cmd->hor_filter_sel = VPP_FILTER_SEL_NEAREST;
            cmd->ver_filter_sel = VPP_FILTER_SEL_NEAREST;
        } else {
            cmd->hor_filter_sel = VPP_FILTER_SEL_BILINEAR;
            cmd->ver_filter_sel = VPP_FILTER_SEL_BILINEAR;
        }

        cmd->src_stride = src[i].stride;
        cmd->dst_stride = dst[i].stride;
        cmd->src_uv_stride = src[i].uv_stride;
        cmd->dst_uv_stride = dst[i].uv_stride;
        cmd->dst_cropH = dst[i].rows;
        cmd->dst_cropW = dst[i].cols;
        cmd->src_cropH = loca[i].height;
        cmd->src_cropW = loca[i].width;
        cmd->src_axisX = loca[i].x;
        cmd->src_axisY = loca[i].y;

        cmd->dst_axisX = dst[i].axisX;//border use
        cmd->dst_axisY = dst[i].axisY;//border use

       if((matrix != NULL) && (csc_type == CSC_USER_DEFINED)) {
            cmd->matrix.csc_coe00 = matrix->csc_coe00;
            cmd->matrix.csc_coe01 = matrix->csc_coe01;
            cmd->matrix.csc_coe02 = matrix->csc_coe02;
            cmd->matrix.csc_add0  = matrix->csc_add0;
            cmd->matrix.csc_coe10 = matrix->csc_coe10;
            cmd->matrix.csc_coe11 = matrix->csc_coe11;
            cmd->matrix.csc_coe12 = matrix->csc_coe12;
            cmd->matrix.csc_add1  = matrix->csc_add1;
            cmd->matrix.csc_coe20 = matrix->csc_coe20;
            cmd->matrix.csc_coe21 = matrix->csc_coe21;
            cmd->matrix.csc_coe22 = matrix->csc_coe22;
            cmd->matrix.csc_add2  = matrix->csc_add2;
            cmd->src_csc_en = 1;
        }

        if (csc_type !=CSC_MAX) {
            cmd->csc_type = csc_type;
        }

        cmd->cols  = src[i].cols;
        cmd->rows  = src[i].rows ;
        cmd->mapcon_enable = 1;

        switch (dst[i].format) {
        case FMT_BGR:
        case FMT_RGB:
        case FMT_ABGR:
        case FMT_ARGB:
        case FMT_Y:
            cmd->dst_fd1 = 0;
            cmd->dst_fd2 = 0;
            cmd->dst_addr1 = 0;
            cmd->dst_addr2 = 0;
            break;
        default:
            break;
        }
    }

#ifdef PCIE_MODE
    ret = bm_trigger_vpp((bm_handle_t )src[0].vpp_fd.handle, &batch);
#else
    ret = ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch);
    if (!fd_external) {
        close(vpp_dev_fd);
    }
#endif
    if (ret != 0) {
        vpp_return_value_analysis(ret);
        vpp_dump(&batch);
    }

    free(batch.cmd);
    return ret;
}

