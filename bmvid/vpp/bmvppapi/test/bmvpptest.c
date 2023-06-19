
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmvppapi.h"

#if defined _WIN32
#if !defined(bool)
#define	bool	int
#endif
#if !defined(true)
#define true	1
#endif
#if !defined(false)
#define	false	0
#endif
#endif

#include "bmlib_runtime.h"


#define ALIGN(x, mask)  (((x) + ((mask)-1)) & ~((mask)-1))

int main(int argc, char *argv[])
{
    int src_fmt, dst_fmt;
    int convert_fmt,ret = 0;
    int input_width , input_height, output_width, output_height;
    char *output_name;
    char *input_name;
    int src_len, idx;
    bmvpp_mat_t s = {0};
    bmvpp_mat_t d = {0};
    void* s_va;
    void* d_va;
	FILE* fp0;
	FILE* fp1;


    if (argc != 9)
    {
        printf("usage: %d\n", argc);
        printf("%s input_width input_height src_img_name  src-format  output_width output_height dst_img_name dst-format\n", argv[0]);
        printf("example:\n");
        printf("    FMT_NV12  -> FMT_BGR:  %s 1920 1080 img1.nv12  2 1540 864 out.bmp 3\n", argv[0]);
        return 0;
    }

    input_width   = atoi(argv[1]);
    input_height  = atoi(argv[2]);
    input_name    = argv[3];
    src_fmt      = atoi(argv[4]);
    output_width  = atoi(argv[5]);
    output_height = atoi(argv[6]);
    output_name  = argv[7];
    dst_fmt      = atoi(argv[8]);

    bm_handle_t handle = NULL;
    bm_device_mem_t dev_buffer_src;
    bm_device_mem_t dev_buffer_dst;
    bmvpp_rect_t crop;

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    ret = bmvpp_open(0);
    if (ret < 0) {
        fprintf(stderr, "bmvpp_open failed\n");
        return -1;
    }
	
	s.soc_idx  = 0;
    s.format  = BMVPP_FMT_NV12;
    s.width   = input_width;
    s.height  = input_height;
    s.stride  = ALIGN(s.width, 64);
    s.c_stride= s.stride;
	
    s.size[0] = input_width * input_height;
    s.size[1] = s.size[0]>>1;
    s.size[2] = 0;
    ret = bm_malloc_device_byte_heap(handle, &dev_buffer_src, 1, s.size[0]+s.size[1]);
    if (ret != BM_SUCCESS) {
        printf("malloc device memory size = %d failed, ret = %d\n", s.size[0] + s.size[1], ret);
    }	
	s.pa[0]   = (uint64_t)dev_buffer_src.u.device.device_addr;
    s.pa[1]   = s.pa[0] + s.size[0];
    s.pa[2]   = 0;
	
    d.format  = BMVPP_FMT_BGR; // TODO
    d.width   = output_width;
    d.height  = output_height;
    d.stride  = ALIGN(d.width * 3, 64);
    d.c_stride= 0;
	
    d.size[0] = d.stride * d.height;
    d.size[1] = 0;
    d.size[2] = 0;

    ret = bm_malloc_device_byte_heap(handle, &dev_buffer_dst, 1, d.size[0]);
    if (ret != BM_SUCCESS) {
        printf("malloc device memory size = %d failed, ret = %d\n", d.size[0], ret);
    }		
    d.pa[0]   = (uint64_t)dev_buffer_dst.u.device.device_addr;
    d.pa[1]   = 0;
    d.pa[2]   = 0;
	
	s_va = malloc(s.size[0]+s.size[1]);
	d_va = malloc(d.size[0]);

    fp0 = fopen(input_name, "rb");
    fseek(fp0, 0, SEEK_END);
    int len0 = ftell(fp0);
    printf("len0 : %d, src size  %d\n", len0, s.size[0]+s.size[1]);
    fseek(fp0, 0, SEEK_SET);
    fread(s_va, 1, len0, fp0);

    ret = bm_memcpy_s2d(handle, dev_buffer_src, s_va);
    if (ret != BM_SUCCESS) {
        printf("CDMA transfer from system to device failed, ret = %d\n", ret);
    }

    fprintf(stderr, "s data 0: 0x%llx\n", s.pa[0]);
    fprintf(stderr, "s data 1: 0x%llx\n", s.pa[1]);
    fprintf(stderr, "s.stride: %d\n", s.stride );
    fprintf(stderr, "s.c_stride: %d\n", s.c_stride );
    fprintf(stderr, "s.size[0]: %d\n", s.size[0] );
    fprintf(stderr, "s.size[1] : %d\n", s.size[1] );

    fprintf(stderr, "d data 0: 0x%llx\n", d.pa[0]);
    fprintf(stderr, "d.stride: %d\n", d.stride );
    fprintf(stderr, "d.c_stride: %d\n", d.c_stride );
    fprintf(stderr, "d.size[0]: %d\n", d.size[0] );
    fprintf(stderr, "d.size[1] : %d\n", d.size[1] );

 
	crop.x = 0;
    crop.y = 0;
    crop.width = s.width;
    crop.height = s.height;

    ret = bmvpp_scale(0, &s, &crop, &d, 0, 0, BMVPP_RESIZE_BICUBIC);
    if (ret == 0) {
        fprintf(stderr, "[%s,%d] bmvpp_scale failed\n", __func__, __LINE__);
        return -1;
    }

    ret = bm_memcpy_d2s(handle, (void *)d_va, dev_buffer_dst);
    if (ret != BM_SUCCESS) {
        printf("CDMA transfer from system to device failed, ret = %d\n", ret);
    }

    fp1 = fopen(output_name, "wb");
    if(!fp1) {
        printf("ERROR: Unable to open the output file!\n");
        exit(-1);
    }
	fwrite(d_va, 1, d.size[0], fp1);

    bm_free_device(handle, dev_buffer_src);
    bm_free_device(handle, dev_buffer_dst);
	free(s_va);
    free(d_va);

	fclose(fp0);
	fclose(fp1);

    return 0;
}

