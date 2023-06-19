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


int main(int argc, char *argv[])
{
    int src_fmt, dst_fmt;
    vpp_mat src, dst;
    char *offset_base_y, *comp_base_y, *offset_base_c, *comp_base_c;
    int in_w, in_h = 0, out_w, out_h = 0;
    char *img_name;
    mem_layout src_para;
    mem_layout dst_para;
    void *src_va = 0;
    void *dst_va = 0;
    int src_stride = 0;    /*src image stride*/
    int dst_stride = 0;    /*dst image stride*/
    int ret, src_len, idx;

    if (argc != 12) {
        printf("usage: %d\n", argc);
        printf("%s src_width src_height offset_base_y comp_base_y offset_base_c comp_base_c src-stride dst-format dst_width dst_height dst_img_name\n", argv[0]);
        printf("example:\n");
        printf("nv12-> BGR:%s 1920 1080 fbc_offset_table_y.bin fbc_comp_data_y.bin fbc_offset_table_c.bin fbc_comp_data_c.bin 1920 3 984 638 out.bmp\n", argv[0]);
        return 0;
    }

    in_w = atoi(argv[1]);    /*src image w*/
    in_h = atoi(argv[2]);    /*src image h*/
    offset_base_y = argv[3];    /*src image name*/
    comp_base_y = argv[4];    /*src image name*/
    offset_base_c = argv[5];    /*src image name*/
    comp_base_c = argv[6];    /*src image name*/

    src_stride = atoi(argv[7]);    /*src image format w*/
    dst_fmt = atoi(argv[8]);    /*dst image format*/
    out_w = atoi(argv[9]);    /*dst image w*/
    out_h = atoi(argv[10]);    /*dst image h*/
    img_name = argv[11];/*dst image name*/

    FILE *fp0 = fopen(offset_base_y, "rb");
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
//    printf("len0 : %d\n", len0);
    fseek(fp0, 0, SEEK_SET);

    FILE *fp1 = fopen(comp_base_y, "rb");
    fseek(fp1, 0, SEEK_END);
    int len1 = ftell(fp1);
//    printf("len1 : %d\n", len1);
    fseek(fp1, 0, SEEK_SET);

    FILE *fp2 = fopen(offset_base_c, "rb");
    fseek(fp2, 0, SEEK_END);
    int len2 = ftell(fp2);
//    printf("len2 : %d\n", len2);
    fseek(fp2, 0, SEEK_SET);

    FILE *fp3 = fopen(comp_base_c, "rb");
    fseek(fp3, 0, SEEK_END);
    int len3 = ftell(fp3);
//    printf("len3 : %d\n", len3);
    fseek(fp3, 0, SEEK_SET);

    src.num_comp = 4;
    src.ion_len[0] = len0;
    src.ion_len[1] = len1;
    src.ion_len[2] = len2;
    src.ion_len[3] = len3;
//    printf("len0 %d, len1 %d, len2 %d, len3 %d\n", len0, len1, len2, len3);

    bm_handle_t handle = NULL;

    bm_device_mem_t dev_buffer_src;
    bm_device_mem_t dev_buffer_dst[3];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        VppErr("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    src_len = src.ion_len[0] + src.ion_len[1] + src.ion_len[2] + src.ion_len[3];
    printf("src_len %d\n", src_len);

    ret = bm_malloc_device_byte_heap(handle, &dev_buffer_src, DDR_CH, src_len);
        if (ret != BM_SUCCESS) {
            VppErr("malloc device memory size = %d failed, ret = %d\n", src_len, ret);
    }

    src_para.pa = dev_buffer_src.u.device.device_addr;
    src_para.va = malloc(src_len);

    src.pa[0] = src_para.pa;    /*data Physical address*/
    src.pa[1] = src_para.pa + src.ion_len[0];
    src.pa[2] = src_para.pa + src.ion_len[0] + src.ion_len[1];
    src.pa[3] = src_para.pa + src.ion_len[0] + src.ion_len[1] + src.ion_len[2];
    src.va[0] = src_para.va;    /*data Virtual address,if not use ,do not care, vpp hw not use va*/
    src.va[1] = (char *)src_para.va + src.ion_len[0];
    src.va[2] = (char *)src_para.va + src.ion_len[0] + src.ion_len[1];
    src.va[3] = (char *)src_para.va + src.ion_len[0] + src.ion_len[1] + src.ion_len[2];
    src.format = FMT_NV12;/*data format*/
    src.cols = in_w;    /*image width*/
    src.rows = in_h;    /*image height*/
    src.stride = src_stride;    /*The number of bytes of memory occupied by one line of image data*/
    src.is_pa = 1;
    src.vpp_fd.handle = handle;

    dst.vpp_fd.handle = handle;
    vpp_creat_host_and_device_mem(dev_buffer_dst, &dst, dst_fmt, out_w, out_h);


    fread(src.va[0], 1, len0, fp0);
    fread(src.va[1], 1, len1, fp1);
    fread(src.va[2], 1, len2, fp2);
    fread(src.va[3], 1, len3, fp3);

    ret = bm_memcpy_s2d(handle, dev_buffer_src, src_para.va);
    if (ret != BM_SUCCESS) {
        VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
    }

    fbd_csc_resize(&src, &dst);

    for (idx = 0; idx < dst.num_comp; idx++) {
        ret = bm_memcpy_d2s(handle, (void *)dst.va[idx], dev_buffer_dst[idx]);
        if (ret != BM_SUCCESS) {
            VppErr("CDMA transfer from system to device failed, ret = %d\n", ret);
        }
    }

    ret = output_file(img_name, &dst);

    bm_free_device(handle, dev_buffer_src);
    free(src.va[0]);

    for (idx = 0; idx < dst.num_comp; idx++) {
        bm_free_device(handle, dev_buffer_dst[idx]);
        free(dst.va[idx]);
    }

    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
    return 0;
}

