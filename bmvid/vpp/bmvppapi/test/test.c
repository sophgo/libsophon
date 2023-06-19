
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmvppapi.h"
#include "bm_ion.h"

enum {
    MY_OP_SCALING  = 0,
    MY_OP_CROPPING = 1,
    MY_OP_PADDING  = 2,
    MY_OP_PADDING2 = 3,
    MY_OP_MAX
};

typedef struct frame_s {
    bm_ion_buffer_t* buffer;
    int num_comp;
    uint8_t* data[4];
    int      linesize[4];

    int      width;
    int      height;
    int      size[4];
    int      total_size;
} frame_t;

static int upload_data(frame_t* frame, char* in_name);
static int scaling(frame_t* dst, frame_t* src);
static int cropping(frame_t* dst, frame_t* src);
static int padding(frame_t* dst, frame_t* src);
static int padding2(frame_t* dst, frame_t* src);
static int download_data(frame_t* frame, char* out_name);

int main(int argc, char* argv[])
{
    frame_t src = {0};
    frame_t dst = {0};
    int op = 0;
    int ret;
    int flags = 0;

    if (argc != 4) {
        fprintf(stderr, "Usage:\n\t %s in op out\n", argv[0]);
        return -1;
    }

    op = atoi(argv[2]);
    if (op < 0 || op > MY_OP_MAX) {
        fprintf(stderr, "[%s,%d] invalid op %d.\n", __func__, __LINE__, op);
        return -1;
    }

    src.width  = 1920;
    src.height = 1080;
    src.linesize[0] = 1920;
    src.linesize[1] =  960;
    src.linesize[2] =  960;
    src.num_comp = 3;
    src.size[0] = src.linesize[0] * src.height;
    src.size[1] = src.linesize[1] * src.height/2;
    src.size[2] = src.linesize[2] * src.height/2;
    src.total_size = src.size[0] + src.size[1] + src.size[2];

    if (op != MY_OP_PADDING2) {
        dst.width  = 352;
        dst.height = 288;
        dst.linesize[0] = 384;
        dst.linesize[1] = 192;
        dst.linesize[2] = 192;
    } else {
        dst.width  = src.width;
        dst.height = src.height;
        dst.linesize[0] = 2048;
        dst.linesize[1] = 1024;
        dst.linesize[2] = 1024;
    }
    dst.size[0] = dst.linesize[0] * dst.height;
    dst.size[1] = dst.linesize[1] * dst.height/2;
    dst.size[2] = dst.linesize[2] * dst.height/2;
    dst.total_size = dst.size[0] + dst.size[1] + dst.size[2];

    ret = bmvpp_open(0);
    if (ret < 0) {
        fprintf(stderr, "bmvpp_open failed\n");
        return -1;
    }

    ret = bm_ion_allocator_open(0);
    if (ret != 0) {
        fprintf(stderr, "bm_ion_allocator_open failed\n");
        return -1;
    }

    src.buffer = bm_ion_allocate_buffer(0, src.total_size, (BM_ION_FLAG_VPP<<4) | flags);
    if (src.buffer == NULL) {
        fprintf(stderr, "[%s,%d] bm_ion_allocate_buffer failed.\n", __func__, __LINE__);
        goto fail;
    }
    src.data[0] = (uint8_t*)(src.buffer->paddr);
    src.data[1] = src.data[0] + src.size[0];
    src.data[2] = src.data[1] + src.size[1];

    dst.buffer = bm_ion_allocate_buffer(0, dst.total_size, (BM_ION_FLAG_VPP<<4) | flags);
    if (dst.buffer == NULL) {
        fprintf(stderr, "[%s,%d] bm_ion_allocate_buffer failed.\n", __func__, __LINE__);
        goto fail;
    }
    dst.data[0] = (uint8_t*)(dst.buffer->paddr);
    dst.data[1] = dst.data[0] + dst.size[0];
    dst.data[2] = dst.data[1] + dst.size[1];

    ret = upload_data(&src, argv[1]);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] upload_data failed.\n", __func__, __LINE__);
        goto fail;
    }

    if (op == MY_OP_SCALING) {
        ret = scaling(&dst, &src);
        if (ret != 0) {
            fprintf(stderr, "[%s,%d] scaling failed.\n", __func__, __LINE__);
            goto fail;
        }
    } else if (op == MY_OP_CROPPING) {
        ret = cropping(&dst, &src); // TODO
        if (ret != 0) {
            fprintf(stderr, "[%s,%d] cropping failed.\n", __func__, __LINE__);
            goto fail;
        }
    } else if (op == MY_OP_PADDING) {
        ret = padding(&dst, &src); // TODO
        if (ret != 0) {
            fprintf(stderr, "[%s,%d] padding failed.\n", __func__, __LINE__);
            goto fail;
        }
    } else /* if (op == MY_OP_PADDING) */ {
        ret = padding2(&dst, &src); // TODO
        if (ret != 0) {
            fprintf(stderr, "[%s,%d] padding failed.\n", __func__, __LINE__);
            goto fail;
        }
    }

    ret = download_data(&dst, argv[3]);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] download_data failed.\n", __func__, __LINE__);
        goto fail;
    }

fail:
    if (dst.buffer)
        bm_ion_free_buffer(dst.buffer);
    if (src.buffer)
        bm_ion_free_buffer(src.buffer);

    ret = bm_ion_allocator_close(0);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] bm_ion_allocator_close failed\n",
               __func__, __LINE__);
    }
    bmvpp_close(0);

    return 0;
}

static int scaling(frame_t* dst, frame_t* src)
{
    bmvpp_mat_t s = {0};
    bmvpp_mat_t d = {0};
    int ret;
    bmvpp_rect_t crop;

    fprintf(stderr, "src data 0: %p\n", src->data[0]);
    fprintf(stderr, "src data 1: %p\n", src->data[1]);
    fprintf(stderr, "src data 2: %p\n", src->data[2]);

    fprintf(stderr, "src line size 0: %d\n", src->linesize[0]);
    fprintf(stderr, "src line size 1: %d\n", src->linesize[1]);
    fprintf(stderr, "src line size 2: %d\n", src->linesize[2]);

    fprintf(stderr, "src width : %d\n", src->width);
    fprintf(stderr, "src height: %d\n", src->height);

    fprintf(stderr, "dst data 0: %p\n", dst->data[0]);
    fprintf(stderr, "dst data 1: %p\n", dst->data[1]);
    fprintf(stderr, "dst data 2: %p\n", dst->data[2]);

    fprintf(stderr, "dst line size 0: %d\n", dst->linesize[0]);
    fprintf(stderr, "dst line size 1: %d\n", dst->linesize[1]);
    fprintf(stderr, "dst line size 2: %d\n", dst->linesize[2]);

    fprintf(stderr, "dst width : %d\n", dst->width);
    fprintf(stderr, "dst height: %d\n", dst->height);

    s.format  = BMVPP_FMT_I420;
    s.width   = src->width;
    s.height  = src->height;
    s.stride  = src->linesize[0];
    s.c_stride= src->linesize[1];

    s.num_comp = src->num_comp;
    s.pa[0]   = (uint64_t)src->data[0];
    s.pa[1]   = (uint64_t)src->data[1];
    s.pa[2]   = (uint64_t)src->data[2];
    s.size[0] = src->size[0];
    s.size[1] = src->size[1];
    s.size[2] = src->size[2];

    d.format  = BMVPP_FMT_I420; // TODO
    d.width   = dst->width;
    d.height  = dst->height;
    d.stride  = dst->linesize[0];
    d.c_stride= dst->linesize[1];

    d.pa[0]   = (uint64_t)dst->data[0];
    d.pa[1]   = (uint64_t)dst->data[1];
    d.pa[2]   = (uint64_t)dst->data[2];
    d.size[0] = dst->size[0];
    d.size[1] = dst->size[1];
    d.size[2] = dst->size[2];

    crop.x = 0;
    crop.y = 0;
    crop.width = s.width;
    crop.height = s.height;

    ret = bmvpp_scale(0, &s, &crop, &d, 0, 0, BMVPP_RESIZE_BICUBIC);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] bmvpp_scale failed\n",
               __func__, __LINE__);
        return -1;
    }

    return 0;
}

static int cropping(frame_t* dst, frame_t* src)
{
    bmvpp_mat_t s = {0};
    bmvpp_mat_t d = {0};
    bmvpp_rect_t rect = {0};
    int ret;

    fprintf(stderr, "src data 0: %p\n", src->data[0]);
    fprintf(stderr, "src data 1: %p\n", src->data[1]);
    fprintf(stderr, "src data 2: %p\n", src->data[2]);

    fprintf(stderr, "src line size 0: %d\n", src->linesize[0]);
    fprintf(stderr, "src line size 1: %d\n", src->linesize[1]);
    fprintf(stderr, "src line size 2: %d\n", src->linesize[2]);

    fprintf(stderr, "src width : %d\n", src->width);
    fprintf(stderr, "src height: %d\n", src->height);

    fprintf(stderr, "dst data 0: %p\n", dst->data[0]);
    fprintf(stderr, "dst data 1: %p\n", dst->data[1]);
    fprintf(stderr, "dst data 2: %p\n", dst->data[2]);

    fprintf(stderr, "dst line size 0: %d\n", dst->linesize[0]);
    fprintf(stderr, "dst line size 1: %d\n", dst->linesize[1]);
    fprintf(stderr, "dst line size 2: %d\n", dst->linesize[2]);

    fprintf(stderr, "dst width : %d\n", dst->width);
    fprintf(stderr, "dst height: %d\n", dst->height);

    s.format  = BMVPP_FMT_I420;
    s.width   = src->width;
    s.height  = src->height;
    s.stride  = src->linesize[0];
    s.c_stride= src->linesize[1];

    s.pa[0]   = (uint64_t)src->data[0];
    s.pa[1]   = (uint64_t)src->data[1];
    s.pa[2]   = (uint64_t)src->data[2];
    s.size[0] = src->size[0];
    s.size[1] = src->size[1];
    s.size[2] = src->size[2];

    d.format  = BMVPP_FMT_I420; // TODO
    d.width   = dst->width;
    d.height  = dst->height;
    d.stride  = dst->linesize[0];
    d.c_stride= dst->linesize[1];

    d.pa[0]   = (uint64_t)dst->data[0];
    d.pa[1]   = (uint64_t)dst->data[1];
    d.pa[2]   = (uint64_t)dst->data[2];
    d.size[0] = dst->size[0];
    d.size[1] = dst->size[1];
    d.size[2] = dst->size[2];

    rect.x      = 300; // TODO
    rect.y      = 0;
    rect.width  = s.width  - rect.x*2;
    rect.height = s.height - rect.y*2;;

    ret = bmvpp_scale(0, &s, &rect, &d, 0, 0, BMVPP_RESIZE_BICUBIC);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] bmvpp_crop failed\n",
               __func__, __LINE__);
        return -1;
    }

    return 0;
}

static int padding(frame_t* dst, frame_t* src)
{
    bmvpp_mat_t s = {0};
    bmvpp_mat_t d = {0};
    int top = 44;
    int bot = 46;
    int ret;
    bmvpp_rect_t crop;

    fprintf(stderr, "src data 0: %p\n", src->data[0]);
    fprintf(stderr, "src data 1: %p\n", src->data[1]);
    fprintf(stderr, "src data 2: %p\n", src->data[2]);

    fprintf(stderr, "src line size 0: %d\n", src->linesize[0]);
    fprintf(stderr, "src line size 1: %d\n", src->linesize[1]);
    fprintf(stderr, "src line size 2: %d\n", src->linesize[2]);

    fprintf(stderr, "src width : %d\n", src->width);
    fprintf(stderr, "src height: %d\n", src->height);

    fprintf(stderr, "dst data 0: %p\n", dst->data[0]);
    fprintf(stderr, "dst data 1: %p\n", dst->data[1]);
    fprintf(stderr, "dst data 2: %p\n", dst->data[2]);

    fprintf(stderr, "dst line size 0: %d\n", dst->linesize[0]);
    fprintf(stderr, "dst line size 1: %d\n", dst->linesize[1]);
    fprintf(stderr, "dst line size 2: %d\n", dst->linesize[2]);

    fprintf(stderr, "dst width : %d\n", dst->width);
    fprintf(stderr, "dst height: %d\n", dst->height);

    s.format  = BMVPP_FMT_I420;
    s.width   = src->width;
    s.height  = src->height;
    s.stride  = src->linesize[0];
    s.c_stride= src->linesize[1];

    s.pa[0]   = (uint64_t)src->data[0];
    s.pa[1]   = (uint64_t)src->data[1];
    s.pa[2]   = (uint64_t)src->data[2];
    s.size[0] = src->size[0];
    s.size[1] = src->size[1];
    s.size[2] = src->size[2];

    d.format  = BMVPP_FMT_I420; // TODO
    d.stride  = dst->linesize[0];
    d.c_stride= dst->linesize[1];

    d.pa[0]   = (uint64_t)dst->data[0];
    d.pa[1]   = (uint64_t)dst->data[1];
    d.pa[2]   = (uint64_t)dst->data[2];
    d.size[0] = dst->size[0];
    d.size[1] = dst->size[1];
    d.size[2] = dst->size[2];

    crop.x = 0;
    crop.y = 0;
    crop.width = s.width;
    crop.height = s.height;

#if 0
    d.width   = dst->width;
    d.height  = dst->height;
    ret = bmvpp_scale(&s, &d, BMVPP_RESIZE_BICUBIC);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] bmvpp_scale failed\n",
               __func__, __LINE__);
        return -1;
    }

    d.width   = dst->width;
    d.height  = dst->height - bot - top;
    ret = bmvpp_pad(&s, &d, top, bot, 0, 0, BMVPP_RESIZE_BICUBIC);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] bmvpp_pad failed\n",
               __func__, __LINE__);
        return -1;
    }
#else
    {
        int total_size = d.size[0] + d.size[1] + d.size[2];
        uint8_t* yuv = calloc(1, total_size);

        memset(yuv+d.size[0], 0x80, d.size[1] + d.size[2]);

        ret = bm_ion_upload_data(yuv, dst->buffer, total_size);
        if (ret != 0) {
            fprintf(stderr, "[%s,%d] bm_ion_upload_data failed\n",
                    __func__, __LINE__);
            return -1;
        }

        d.width   = dst->width;
        d.height  = dst->height - bot - top;
        ret = bmvpp_scale(0, &s, &crop, &d, 0, top, BMVPP_RESIZE_BICUBIC);
        if (ret != 0) {
            fprintf(stderr, "[%s,%d] bmvpp_pad failed\n",
                    __func__, __LINE__);
            return -1;
        }
    }
#endif

    return 0;
}

static int padding2(frame_t* dst, frame_t* src)
{
    bmvpp_mat_t s = {0};
    bmvpp_mat_t d = {0};
    int left  = 400;
    int right = 400;
    int ret;
    bmvpp_rect_t crop;

    fprintf(stderr, "src data 0: %p\n", src->data[0]);
    fprintf(stderr, "src data 1: %p\n", src->data[1]);
    fprintf(stderr, "src data 2: %p\n", src->data[2]);

    fprintf(stderr, "src line size 0: %d\n", src->linesize[0]);
    fprintf(stderr, "src line size 1: %d\n", src->linesize[1]);
    fprintf(stderr, "src line size 2: %d\n", src->linesize[2]);

    fprintf(stderr, "src width : %d\n", src->width);
    fprintf(stderr, "src height: %d\n", src->height);

    fprintf(stderr, "dst data 0: %p\n", dst->data[0]);
    fprintf(stderr, "dst data 1: %p\n", dst->data[1]);
    fprintf(stderr, "dst data 2: %p\n", dst->data[2]);

    fprintf(stderr, "dst line size 0: %d\n", dst->linesize[0]);
    fprintf(stderr, "dst line size 1: %d\n", dst->linesize[1]);
    fprintf(stderr, "dst line size 2: %d\n", dst->linesize[2]);

    fprintf(stderr, "dst width : %d\n", dst->width);
    fprintf(stderr, "dst height: %d\n", dst->height);

    s.format  = BMVPP_FMT_I420;
    s.width   = src->width;
    s.height  = src->height;
    s.stride  = src->linesize[0];
    s.c_stride= src->linesize[1];

    s.pa[0]   = (uint64_t)src->data[0];
    s.pa[1]   = (uint64_t)src->data[1];
    s.pa[2]   = (uint64_t)src->data[2];
    s.size[0] = src->size[0];
    s.size[1] = src->size[1];
    s.size[2] = src->size[2];

    d.format  = BMVPP_FMT_I420; // TODO
    d.stride  = dst->linesize[0];
    d.c_stride= dst->linesize[1];

    d.pa[0]   = (uint64_t)dst->data[0];
    d.pa[1]   = (uint64_t)dst->data[1];
    d.pa[2]   = (uint64_t)dst->data[2];
    d.size[0] = dst->size[0];
    d.size[1] = dst->size[1];
    d.size[2] = dst->size[2];

    d.width   = dst->width-left-right;
    d.height  = dst->height;

    crop.x = 0;
    crop.y = 0;
    crop.width = s.width;
    crop.height = s.height;

    ret = bmvpp_scale(0, &s, &crop, &d, left, 0, BMVPP_RESIZE_BICUBIC);
    if (ret != 0) {
        fprintf(stderr, "[%s,%d] bmvpp_pad failed\n",
               __func__, __LINE__);
        return -1;
    }

    return 0;
}

static int upload_data(frame_t* frame, char* in_name)
{
    FILE* inf;
    uint8_t* yuv;
    int ret = 0;

    yuv = calloc(1, frame->total_size);

    inf  = fopen(in_name, "rb");
    fread(yuv, sizeof(uint8_t), frame->total_size, inf);
    fclose(inf);

    ret = bm_ion_upload_data(yuv, frame->buffer, frame->total_size);

    free(yuv);

    return ret;
}

static int download_data(frame_t* frame, char* out_name)
{
    FILE* outf;
    uint8_t* yuv, *p;
    int ret = 0;

    yuv = calloc(1, frame->total_size);

    ret = bm_ion_download_data(yuv, frame->buffer, frame->total_size);
    if (ret != 0)
        goto Exit;

    outf = fopen(out_name, "wb");
    // write yuv planes
    p = yuv;
    //Y
    for(int i = 0; i < frame->height; ++i) {
        fwrite(p, sizeof(uint8_t), frame->width, outf);
        p+=frame->linesize[0];
    }

    //U
    for(int i = 0; i < frame->height/2; ++i) {
        fwrite(p, 1, frame->width/2, outf);
        p+= frame->linesize[1];
    }

    //V
    for(int i = 0; i < frame->height/2; ++i) {
        fwrite(p, 1, frame->width/2, outf);
        p+= frame->linesize[2];
    }

    fclose(outf);

Exit:
    free(yuv);

    return ret;
}

