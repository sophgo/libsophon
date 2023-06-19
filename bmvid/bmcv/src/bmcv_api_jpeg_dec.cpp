//
// Created by nan.wu on 2019-08-25.
//
#ifndef USING_CMODEL

#include "bmcv_api_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "bmcv_internal.h"

#include <setjmp.h>
extern "C" {
#include "jpeglib.h"
}


typedef struct bmcv_jpeg_decoder_struct {
    BmJpuJPEGDecoder *decoder_;
} bmcv_jpeg_decoder_t;

static int format_switch(BmJpuJPEGDecInfo info) {
    BmJpuColorFormat jpu_format = info.color_format;
    int chroma_itlv = info.chroma_interleave;
    int bmcv_fmt = -1;
    switch(jpu_format) {
        case BM_JPU_COLOR_FORMAT_YUV420:
            bmcv_fmt = (chroma_itlv == 0) ? FORMAT_YUV420P :
                       ((chroma_itlv == 1) ? FORMAT_NV12 :
                        ((chroma_itlv == 2) ? FORMAT_NV21 : -1));
            break;
        case BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL:
            bmcv_fmt = (chroma_itlv == 0) ? FORMAT_YUV422P :
                       ((chroma_itlv == 1) ? FORMAT_NV16 :
                        ((chroma_itlv == 2) ? FORMAT_NV61 : -1));
            break;
        case BM_JPU_COLOR_FORMAT_YUV422_VERTICAL:
            bmcv_fmt = -1;
            break;
        case BM_JPU_COLOR_FORMAT_YUV400:
            bmcv_fmt = FORMAT_GRAY;
            break;
        case BM_JPU_COLOR_FORMAT_YUV444:
            bmcv_fmt = FORMAT_YUV444P;
            break;
        default:
            break;
    }
    return bmcv_fmt;
}

static bm_status_t bmcv_jpeg_dec_check(bm_image*        src,
                                       BmJpuJPEGDecInfo info) {

    if (src->data_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("JEPG-DEC", BMLIB_LOG_ERROR,
                  "data type only support 1N_BYTE %s: %s: %d\n",
                   filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if ((format_switch(info) == -1) ||
        (format_switch(info) != src->image_format)) {
        bmlib_log("JEPG-DEC", BMLIB_LOG_ERROR,
                  "bm_image format should be same with image %s: %s: %d\n",
                   filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    // only support yuv420p, yuv422p, yuv444p, nv12, nv16, gray.
    if (!((src->image_format == FORMAT_YUV420P) ||
          (src->image_format == FORMAT_YUV444P) ||
          (src->image_format == FORMAT_YUV422P) ||
          (src->image_format == FORMAT_NV12) ||
          (src->image_format == FORMAT_NV16) ||
          (src->image_format == FORMAT_GRAY))) {
        bmlib_log("JEPG-DEC", BMLIB_LOG_ERROR,
                  "dst image format only support those format now:\n \
                   FORMAT_YUV420P/FORMAT_YUV444P/FORMAT_YUV422P/FORMAT_NV12/FORMAT_NV16/FORMAT_GRAY");
        return BM_NOT_SUPPORTED;
    }
    // width and height should be same
    if (((unsigned int)src->width != info.actual_frame_width) &&
        ((unsigned int)src->height!= info.actual_frame_height)) {
        bmlib_log("JEPG-DEC", BMLIB_LOG_ERROR,
                  "bm_image width and height should be same with image %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

static int get_image_buffer_size(bm_image* img) {
    int buffer_size = 0;
    int w = img->width;
    int h = img->height;
    int w_align = (w + 63) / 64 * 64;
    int h_align = (h + 63) / 64 * 64;
    switch (img->image_format) {
        case FORMAT_NV12:
        case FORMAT_NV21:
        case FORMAT_YUV420P: {
            buffer_size = w_align * h_align * 1.5;
            break;
        }
        case FORMAT_NV61:
        case FORMAT_NV16:
        case FORMAT_YUV422P: {
            buffer_size = w_align * h_align * 2;
            break;
        }
        case FORMAT_YUV444P: {
            buffer_size = w_align * h_align * 3;
            break;
        }
        case FORMAT_GRAY: {
            buffer_size = w_align * h_align;
            break;
        }
        default:
            break;
    }
    return buffer_size;
}

static int bmcv_jpeg_decoder_create(bmcv_jpeg_decoder_t** p_jpeg_decoder,
                                    bm_image* dst,
                                    int bs_size,
                                    int devid) {
    BmJpuDecReturnCodes ret;
    BmJpuDecOpenParams open_params;
    bmcv_jpeg_decoder_t *dec = NULL;
    dec = (bmcv_jpeg_decoder_t*)malloc(sizeof(bmcv_jpeg_decoder_t));
    if (dec == NULL) {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "internal malloc failed!\r\n");
    }

    memset(dec, 0, sizeof(*dec));
    /* Open the JPEG decoder */
    memset(&open_params, 0, sizeof(BmJpuDecOpenParams));
    open_params.frame_width  = 0;
    open_params.frame_height = 0;
    open_params.chroma_interleave = 0;
    /* avoid the false alarm that bs buffer is empty */
    open_params.bs_buffer_size = (bs_size + 256);
    open_params.device_index = devid;
    if (dst->image_private != NULL &&
        ((dst->image_format == FORMAT_NV12) || dst->image_format == FORMAT_NV16)) {
        open_params.chroma_interleave = 1;
    }

    ret = bm_jpu_dec_load(devid);
    if (BM_JPU_DEC_RETURN_CODE_OK != ret) {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "load jpeg decoder failed!\r\n");
        goto End;
    }

    ret = bm_jpu_jpeg_dec_open(&dec->decoder_, &open_params, 0);
    if (BM_JPU_DEC_RETURN_CODE_OK != ret) {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "open jpeg decoder failed!\r\n");
       goto End;
    }

    *p_jpeg_decoder = dec;
    return BM_JPU_ENC_RETURN_CODE_OK;

End:
    if (dec != NULL)
    {
        /* Shut down the JPEG decoder */
        if (dec->decoder_)
            bm_jpu_jpeg_dec_close(dec->decoder_);
        free(dec);
        /* Unload JPU */
        bm_jpu_dec_unload(devid);
    }
    return BM_JPU_DEC_RETURN_CODE_ERROR;
}

static int bmcv_jpeg_decoder_destroy(bmcv_jpeg_decoder_t *jpeg_decoder)
{
    if (jpeg_decoder && jpeg_decoder->decoder_) {
        bm_jpu_jpeg_dec_close(jpeg_decoder->decoder_);
    }
    free(jpeg_decoder);
    return 0;
}

/*
 * import libjpeg-turbo for soft jpeg decoding
 */
struct JpegErrorMgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

METHODDEF(void)
error_exit( j_common_ptr cinfo )
{
    JpegErrorMgr* err_mgr = (JpegErrorMgr*)(cinfo->err);

    /* Return control to the setjmp point */
    longjmp( err_mgr->setjmp_buffer, 1 );
}

/***************************************************************************
 * end of code for supportting MJPEG image files
 * based on a message of Laurent Pinchart on the video4linux mailing list
 ***************************************************************************/
/***************************************************************************
 * following code is for supporting MJPEG image files
 * based on a message of Laurent Pinchart on the video4linux mailing list
 ***************************************************************************/

/* JPEG DHT Segment for YCrCb omitted from MJPEG data */
static uint8_t bmcv_jpeg_odml_dht[0x1a4] = {
    0xff, 0xc4, 0x01, 0xa2,

    0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,

    0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,

    0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04,
    0x04, 0x00, 0x00, 0x01, 0x7d,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06,
    0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1,
    0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65,
    0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85,
    0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
    0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
    0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4,
    0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa,

    0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04,
    0x04, 0x00, 0x01, 0x02, 0x77,
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41,
    0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09,
    0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17,
    0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64,
    0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
    0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
    0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4,
    0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

/*
 * Parse the DHT table.
 * This code comes from jpeg6b (jdmarker.c).
 */
static int bmcv_jpeg_load_dht(struct jpeg_decompress_struct *info, uint8_t *dht,
                            JHUFF_TBL *ac_tables[], JHUFF_TBL *dc_tables[])
{
    unsigned int length = (dht[2] << 8) + dht[3] - 2;
    unsigned int pos = 4;
    unsigned int count, i;
    int index;

    JHUFF_TBL **hufftbl;
    uint8_t bits[17];
    uint8_t huffval[256] = {0};

    while (length > 16)
    {
        bits[0] = 0;
        index = dht[pos++];
        count = 0;
        for (i = 1; i <= 16; ++i)
        {
            bits[i] = dht[pos++];
            count += bits[i];
        }
        length -= 17;

        if (count > 256 || count > length)
            return -1;

        for (i = 0; i < count; ++i)
            huffval[i] = dht[pos++];
        length -= count;

        if (index & 0x10)
        {
            index &= ~0x10;
            hufftbl = &ac_tables[index];
        }
        else
            hufftbl = &dc_tables[index];

        if (index < 0 || index >= NUM_HUFF_TBLS)
            return -1;

        if (*hufftbl == NULL)
            *hufftbl = jpeg_alloc_huff_table ((j_common_ptr)info);
        if (*hufftbl == NULL)
            return -1;

        memcpy ((*hufftbl)->bits, bits, sizeof (*hufftbl)->bits);
        memcpy ((*hufftbl)->huffval, huffval, sizeof (*hufftbl)->huffval);
    }

    if (length != 0)
        return -1;
    return 0;
}

static int check_jpeg_baseline(jpeg_decompress_struct* cinfo)
{
    if (cinfo->arith_code || cinfo->progressive_mode || cinfo->data_precision != 8)
        return 0;

    jpeg_component_info *p = cinfo->comp_info;
    for (int ci=0; ci < cinfo->num_components; ci++)
    {
        if (p->dc_tbl_no > 1 || p->ac_tbl_no > 1)
            return 0;

        JQUANT_TBL *qtbl = cinfo->quant_tbl_ptrs[p->quant_tbl_no];
        if (qtbl == NULL) return 0;
        for (int i = 0; i < DCTSIZE2; i++)
        {
            if (qtbl->quantval[i] > 255)
                return 0;
        }

        p++;
    }

    return 1;
}


static int determine_hw_decoding(jpeg_decompress_struct* cinfo)
{
    /* A. Baseline ISO/IEC 10918-1 JPEG compliance */
    int is_baseline = check_jpeg_baseline(cinfo);
    if (!is_baseline)
        return 0;

#if 0
    printf("cinfo->num_components=%d\n", cinfo->num_components);
    printf("cinfo->comps_in_scan=%d\n", cinfo->comps_in_scan);
#endif

    /* B. Support 1 or 3 color components */
    if (cinfo->num_components != 1 && cinfo->num_components != 3)
        return 0;

    /* C. 3 component in a scan (interleaved only) */
    if (cinfo->num_components == 3 && cinfo->comps_in_scan != 3)
        return 0;

    /* The following cases are NOT supported by JPU */
    if (cinfo->comp_info[0].h_samp_factor>2 ||
        cinfo->comp_info[0].v_samp_factor>2)
        return 0;

    if (cinfo->num_components == 3 &&
        (cinfo->comp_info[1].h_samp_factor!=1 ||
         cinfo->comp_info[2].h_samp_factor!=1 ||
         cinfo->comp_info[1].v_samp_factor!=1 ||
         cinfo->comp_info[2].v_samp_factor!=1))
        return 0;

    int sampleFactor;
    if (cinfo->num_components == 1)
        sampleFactor = 0x1;
    else
        sampleFactor = (((cinfo->comp_info[0].h_samp_factor&3)<<2) |
                        ( cinfo->comp_info[0].v_samp_factor&3));

    /* For now, yuv420/yuv422/yuv444/yuv400 pictures ared decoded by JPU. */
    if ((0xA != sampleFactor) && (0x9 != sampleFactor) &&
        (0x5 != sampleFactor) && (0x1 != sampleFactor))
        return 0;

    if (cinfo->jpeg_color_space != JCS_YCbCr &&
        cinfo->jpeg_color_space != JCS_GRAYSCALE &&
        cinfo->jpeg_color_space != JCS_RGB)
        return 0;

    if (cinfo->image_width < 16 || cinfo->image_height < 16)
        return 0;

    return 1;
}

static int try_soft_decoding(bm_handle_t  handle,
                           void*        buf,
                           size_t       size,
                           bm_image     *dst)
{
    jpeg_decompress_struct cinfo;
    struct JpegErrorMgr errorMgr;
    int soft_decoding = 1;
    volatile int bmimage_created = 0;
    volatile int bmimage_allocated = 0;

    cinfo.err = jpeg_std_error(&errorMgr.pub);
    errorMgr.pub.error_exit = error_exit;

    if (setjmp(errorMgr.setjmp_buffer))
    {
        BMCV_ERR_LOG("jpeg-turbo read header failed!\r\n");
        soft_decoding = -1;
        goto END;
    }

    /* initial jpeg decompressor */
    jpeg_create_decompress( &cinfo );

    jpeg_mem_src(&cinfo, (const unsigned char*)buf, size);

    jpeg_read_header(&cinfo, TRUE);

    if (!determine_hw_decoding(&cinfo)){
        unsigned char *p_raw_pic;
        int row_stride;
        int pic_size;
        JSAMPROW row_pointer[1];
        int bmcv_stride[4];

        soft_decoding = 1;
        bmlib_log("JPEG-DEC", BMLIB_LOG_INFO, "use soft jpeg decoding!\r\n");

        if (dst == NULL){
            BMCV_ERR_LOG("jpeg-turbo bm_image pointer is null!\n");
            soft_decoding = -BM_ERR_FAILURE;
            goto END;
        }
        else if (dst->image_private != NULL){
            if (dst->width != (int)cinfo.image_width || dst->height != (int)cinfo.image_height){
                bmlib_log("JEPG-DEC", BMLIB_LOG_ERROR,
                    "bm_image width and height should be same with image %s: %s: %d: [%dx%d] != [%dx%d]\n",
                    filename(__FILE__), __func__, __LINE__,
                    dst->width, dst->height, cinfo.image_width, cinfo.image_height);
                soft_decoding = -BM_NOT_SUPPORTED;
                goto END;
            }

            if (dst->data_type != DATA_TYPE_EXT_1N_BYTE) {
                bmlib_log("JEPG-DEC", BMLIB_LOG_ERROR, "data type only support 1N_BYTE %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
                soft_decoding = -BM_NOT_SUPPORTED;
                goto END;
            }

            if (dst->image_format == FORMAT_GRAY) cinfo.out_color_space = JCS_GRAYSCALE;
            else if (dst->image_format == FORMAT_BGR_PACKED) cinfo.out_color_space = JCS_EXT_BGR;
            else if (dst->image_format == FORMAT_RGB_PACKED) cinfo.out_color_space = JCS_EXT_RGB;
            else if (dst->image_format == FORMAT_ARGB_PACKED) cinfo.out_color_space = JCS_EXT_XRGB;
            else if (dst->image_format == FORMAT_ABGR_PACKED) cinfo.out_color_space = JCS_EXT_XBGR;
            else {
                bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "unsupported format %s: %s: %d: %d\n",
                    filename(__FILE__), __func__, __LINE__, dst->image_format);
                soft_decoding = -BM_NOT_SUPPORTED;
                goto END;
            }
        } else
            cinfo.out_color_space = JCS_EXT_BGR;

        // update output informatoin
        cinfo.scale_num = cinfo.scale_denom = 1;
        jpeg_calc_output_dimensions(&cinfo);

        // create bm_image if bm_image is null
        if (dst->image_private == NULL){
            int stride = (cinfo.output_width * 3 + 63) & ~63; // BGR packed
            if(BM_SUCCESS != bm_image_create(handle, cinfo.output_height, cinfo.output_width,
                        (bm_image_format_ext)FORMAT_BGR_PACKED, DATA_TYPE_EXT_1N_BYTE, dst, &stride)) {
                BMCV_ERR_LOG("bm_image_create error\r\n");
                soft_decoding = -BM_ERR_FAILURE;
                goto END;
            } else
                bmimage_created = 1;
        }

        // allocate bmimage memory
        if (!bm_image_is_attached(*dst)){
            // not alloc in heap0, because it maybe use by VPP after this operation
            if (BM_SUCCESS != bm_image_alloc_dev_mem_heap_mask(*dst, 6)) {
                BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");
                soft_decoding = -BM_ERR_FAILURE;
                goto END;
            } else
                bmimage_allocated = 1;
        }

        bm_image_get_stride(*dst, bmcv_stride); // only packed mode supported.
        row_stride = bmcv_stride[0]; // stride[0] can guarantee enough for output_width*output_components
        pic_size = (row_stride * cinfo.output_height + 63) & ~63; // aligned with 64 for SIMD instruction

        // allocate memory
        p_raw_pic = (unsigned char *)malloc(pic_size);
        if (p_raw_pic == NULL){
            BMCV_ERR_LOG("jpeg-turbo allocate memory failed!\n");
            return -1;
        }
        row_pointer[0] = p_raw_pic;

        /* check if this is a mjpeg image format, add huff table */
        if (cinfo.ac_huff_tbl_ptrs[0] == NULL &&
            cinfo.ac_huff_tbl_ptrs[1] == NULL &&
            cinfo.dc_huff_tbl_ptrs[0] == NULL &&
            cinfo.dc_huff_tbl_ptrs[1] == NULL)
        {
            /* yes, this is a mjpeg image format, so load the correct
             * huffman table */
            bmcv_jpeg_load_dht(&cinfo,
                             bmcv_jpeg_odml_dht,
                             cinfo.ac_huff_tbl_ptrs,
                             cinfo.dc_huff_tbl_ptrs);
        }

        jpeg_start_decompress(&cinfo);

        while(cinfo.output_scanline < cinfo.output_height){
            row_pointer[0] = p_raw_pic + cinfo.output_scanline*row_stride;
            jpeg_read_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_decompress(&cinfo);

        // flush to bm_image dev_mem
        if (BM_SUCCESS != bm_image_copy_host_to_device(*dst, (void**)&p_raw_pic)){
            BMCV_ERR_LOG("bm_image_copy_host_to_device error\r\n");
            soft_decoding = -BM_ERR_FAILURE;
            if (p_raw_pic) free(p_raw_pic);
            goto END;
        }

        // release resource
        if (p_raw_pic) free(p_raw_pic);
    } else
        soft_decoding = 0;

END:
    jpeg_destroy_decompress(&cinfo);
    if (soft_decoding < 0){
        if (bmimage_allocated){bm_image_detach(*dst); bmimage_allocated = 0;}
        if (bmimage_created) {bm_image_destroy(*dst); bmimage_created = 0;}
    }
    return soft_decoding;

}

bm_status_t bmcv_jpeg_dec_one_image(bm_handle_t          handle,
                                    bmcv_jpeg_decoder_t* jpeg_dec,
                                    void*                buf,
                                    size_t               in_size,
                                    bm_image*            dst) {
    BmJpuDecReturnCodes ret;
    BmJpuJPEGDecInfo info;

    ret = bm_jpu_jpeg_dec_decode(jpeg_dec->decoder_, (const unsigned char*)buf, in_size);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK)
    {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "jpeg decode failed!\r\n");
        return BM_ERR_FAILURE;
    }
    /* Get some information about the frame
     * Note that the info is only available *after* calling bm_jpu_jpeg_dec_decode() */
    bm_jpu_jpeg_dec_get_info(jpeg_dec->decoder_, &info);

    bmlib_log("JPEG-DEC", BMLIB_LOG_INFO,
           "aligned frame size: %u x %u\n"
           "pixel actual frame size: %u x %u\n"
           "pixel Y/Cb/Cr stride: %u/%u/%u\n"
           "pixel Y/Cb/Cr size: %u/%u/%u\n"
           "pixel Y/Cb/Cr offset: %u/%u/%u\n"
           "color format: %s\n"
           "chroma interleave: %d\n",
           info.aligned_frame_width, info.aligned_frame_height,
           info.actual_frame_width, info.actual_frame_height,
           info.y_stride, info.cbcr_stride, info.cbcr_stride,
           info.y_size, info.cbcr_size, info.cbcr_size,
           info.y_offset, info.cb_offset, info.cr_offset,
           bm_jpu_color_format_string(info.color_format),
           info.chroma_interleave);

    if (info.framebuffer == NULL)
    {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "could not decode this JPEG image : no framebuffer returned!\r\n");
        return BM_ERR_FAILURE;
    }

    bm_jpu_phys_addr_t phys_addr = bm_mem_get_device_addr(*(info.framebuffer->dma_buffer));

    bool need_convert = false;
    if (dst->image_private != NULL) {
        if(BM_SUCCESS != bmcv_jpeg_dec_check(dst, info)) {
            BMCV_ERR_LOG("bmcv_jpeg_dec_check error\r\n");

            return BM_ERR_FAILURE;
        }
        // if stride is not same, need to convert
        int jpu_stride[3] = {(int)info.y_stride, (int)info.cbcr_stride, (int)info.cbcr_stride};
        int bmcv_stride[3];
        int plane_num = bm_image_get_plane_num(*dst);
        bm_image_get_stride(*dst, bmcv_stride);
        for (int i = 0; i < plane_num; i++) {
            if (jpu_stride[i] != bmcv_stride[i]) {
                need_convert = true;
            }
        }
    } else {
        int format = format_switch(info);
        int stride[3] = {(int)info.y_stride, (int)info.cbcr_stride, (int)info.cbcr_stride};
        if(BM_SUCCESS != bm_image_create(handle, info.actual_frame_height, info.actual_frame_width,
                                     (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, dst, stride)) {
            BMCV_ERR_LOG("bm_image_create error\r\n");
            return BM_ERR_FAILURE;
        }
    }
    // create device mem, and attach it to bm_image
    bm_device_mem_t dev_mem[3];
    int img_size[3] = {0};
    bm_image_get_byte_size(*dst, img_size);
    dev_mem[0] = bm_mem_from_device(phys_addr, img_size[0]);
    dev_mem[1] = bm_mem_from_device(phys_addr + info.cb_offset, img_size[1]);
    dev_mem[2] = bm_mem_from_device(phys_addr + info.cr_offset, img_size[2]);
    if (!need_convert) {
        if(BM_SUCCESS != bm_image_attach(*dst, dev_mem)) {
            BMCV_ERR_LOG("bm_image_attach error\r\n");
            return BM_ERR_FAILURE;
        }
    } else {
        bm_image dst_tmp;
        int stride[3] = {(int)info.y_stride, (int)info.cbcr_stride, (int)info.cbcr_stride};
        if (BM_SUCCESS != bm_image_create(handle, info.actual_frame_height, info.actual_frame_width,
                                     dst->image_format, DATA_TYPE_EXT_1N_BYTE, &dst_tmp, stride)) {
            BMCV_ERR_LOG("bm_image_create error\r\n");
            return BM_ERR_FAILURE;
        }
        if (BM_SUCCESS != bm_image_attach(dst_tmp, dev_mem)) {
            BMCV_ERR_LOG("bm_image_attach error\r\n");
            bm_image_destroy(dst_tmp);
            return BM_ERR_FAILURE;
        }
        if (!bm_image_is_attached(*dst)) {
            // not alloc in heap0, because it maybe use by VPP after this operation
            if (BM_SUCCESS != bm_image_alloc_dev_mem_heap_mask(*dst, 6)) {
                BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");
                bm_image_destroy(dst_tmp);
                return BM_ERR_FAILURE;
            }
        }
        if (BM_SUCCESS != bmcv_width_align(handle, dst_tmp, *dst)) {
            BMCV_ERR_LOG("bmcv_width_align error\r\n");
            bm_image_destroy(dst_tmp);
            return BM_ERR_FAILURE;
        }
        bm_image_destroy(dst_tmp);
    }

#if 0

     size_t out_size = info.framebuffer->dma_buffer.size;
     unsigned long long mapped_virtual_address = 0;
     bm_mem_mmap_device_mem(handle, info.framebuffer->dma_buffer, &mapped_virtual_address);
     if (mapped_virtual_address==NULL)
     {
         fprintf(stderr, "bm_jpu_dma_buffer_map failed\n");
         return BM_ERR_FAILURE;
     }

     char* filename = (char*)"test_dbg.dec";
     FILE *fp = fopen(filename, "wb+");
     fwrite(&mapped_virtual_address, 1, out_size, fp);

     bm_mem_unmap_device_mem(handle, &mapped_virtual_address, out_size);
     printf("save to %s %ld bytes\n", filename, out_size);
     fclose(fp);
#endif

    return BM_SUCCESS;
}

bm_status_t bmcv_image_jpeg_dec(bm_handle_t  handle,
                                void**       p_jpeg_data,
                                size_t*      in_size,
                                int          image_num,
                                bm_image*    dst) {
    if (handle == NULL) {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (in_size == NULL || p_jpeg_data == NULL) {
        bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "The pointer of data and size should not be NULL!\r\n");
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < image_num; i++) {
        if (p_jpeg_data[i] == NULL) {
            bmlib_log("JPEG-DEC", BMLIB_LOG_ERROR, "The pointer of data should not be NULL!\r\n");
            return BM_ERR_FAILURE;
        }
    }

    int devid = bm_get_devid(handle);
    bmcv_jpeg_decoder_t *jpeg_dec[4];
    int use_soft_jpeg[4];
    for (int i = 0; i < image_num; i++) {
        use_soft_jpeg[i] = try_soft_decoding(handle, p_jpeg_data[i], in_size[i], dst+i);

        if (use_soft_jpeg[i] < 0)
            return BM_ERR_FAILURE;
        else if (use_soft_jpeg[i] == 1)
            continue;

        if(bmcv_jpeg_decoder_create(&(jpeg_dec[i]), dst + i, in_size[i], devid)
           != BM_JPU_ENC_RETURN_CODE_OK) {
            return BM_ERR_FAILURE;
        }

        bm_status_t ret;
        ret = bmcv_jpeg_dec_one_image(handle, jpeg_dec[i], p_jpeg_data[i], in_size[i], dst + i);
        if (ret != BM_SUCCESS) {
            for (int j = 0; j < i; j++) {
                BmJpuJPEGDecInfo info;
                bm_jpu_jpeg_dec_get_info(jpeg_dec[i]->decoder_, &info);
                if (info.framebuffer)
                    bm_jpu_jpeg_dec_frame_finished(jpeg_dec[i]->decoder_, info.framebuffer);
                else
                    return BM_ERR_FAILURE;
                bmcv_jpeg_decoder_destroy(jpeg_dec[i]);
            }
            return BM_ERR_FAILURE;
        }
    }

    for (int i = 0; i < image_num; i++) {
        if (use_soft_jpeg[i] == 1) continue;
        //BmJpuJPEGDecInfo info;
        //bm_jpu_jpeg_dec_get_info(jpeg_dec[i]->decoder_, &info);
        //bm_jpu_jpeg_dec_frame_finished(jpeg_dec[i]->decoder_, info.framebuffer);
        //bmcv_jpeg_decoder_destroy(jpeg_dec[i]);
        // decoder should be close(free dev mem) when bm_image destory
        dst[i].image_private->decoder = jpeg_dec[i]->decoder_;
        free(jpeg_dec[i]);
    }
    return BM_SUCCESS;
}

#endif
