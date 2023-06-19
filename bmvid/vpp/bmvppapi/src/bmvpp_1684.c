//
// Created by yuan on 8/24/21.
//

#include "bmvpp_int.h"

/**
 * YUV colorspace type.
 * These values match the ones defined by ISO/IEC 23001-8_2013 § 7.3.
 */
enum AVColorSpace {
    AVCOL_SPC_RGB         = 0,  ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
    AVCOL_SPC_BT709       = 1,  ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
    AVCOL_SPC_UNSPECIFIED = 2,
    AVCOL_SPC_RESERVED    = 3,
    AVCOL_SPC_FCC         = 4,  ///< FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
    AVCOL_SPC_BT470BG     = 5,  ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
    AVCOL_SPC_SMPTE170M   = 6,  ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
    AVCOL_SPC_SMPTE240M   = 7,  ///< functionally identical to above
    AVCOL_SPC_YCGCO       = 8,  ///< Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
    AVCOL_SPC_YCOCG       = AVCOL_SPC_YCGCO,
    AVCOL_SPC_BT2020_NCL  = 9,  ///< ITU-R BT2020 non-constant luminance system
    AVCOL_SPC_BT2020_CL   = 10, ///< ITU-R BT2020 constant luminance system
    AVCOL_SPC_SMPTE2085   = 11, ///< SMPTE 2085, Y'D'zD'x
    AVCOL_SPC_CHROMA_DERIVED_NCL = 12, ///< Chromaticity-derived non-constant luminance system
    AVCOL_SPC_CHROMA_DERIVED_CL = 13, ///< Chromaticity-derived constant luminance system
    AVCOL_SPC_ICTCP       = 14, ///< ITU-R BT.2100-0, ICtCp
    AVCOL_SPC_NB                ///< Not part of ABI
};

/**
 * MPEG vs JPEG YUV range.
 */
enum AVColorRange {
    AVCOL_RANGE_UNSPECIFIED = 0,
    AVCOL_RANGE_MPEG        = 1, ///< the normal 219*2^(n-8) "MPEG" YUV ranges
    AVCOL_RANGE_JPEG        = 2, ///< the normal     2^n-1   "JPEG" YUV ranges
    AVCOL_RANGE_NB               ///< Not part of ABI
};


struct ColorSapce2CSCMap {
    enum AVColorSpace color_space;
    enum AVColorRange color_range;
    vpp_csc_type csc_type_yuv2rgb;
    vpp_csc_type csc_type_rgb2yuv;
};

#define BMVPP_ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])
#define BMVPP_ALIGN(x, a) (((x)+(a)-1)&~((a)-1))

static struct ColorSapce2CSCMap g_csc_map[] = {
        {AVCOL_SPC_BT709,     AVCOL_RANGE_MPEG, YCbCr2RGB_BT709, RGB2YCbCr_BT709},
        {AVCOL_SPC_BT709,     AVCOL_RANGE_JPEG, YPbPr2RGB_BT709, RGB2YPbPr_BT709},
        {AVCOL_SPC_BT470BG,   AVCOL_RANGE_MPEG, YCbCr2RGB_BT601, RGB2YCbCr_BT601},
        {AVCOL_SPC_BT470BG,   AVCOL_RANGE_JPEG, YPbPr2RGB_BT601, RGB2YPbPr_BT601},
        {AVCOL_SPC_SMPTE170M, AVCOL_RANGE_MPEG, YCbCr2RGB_BT601, RGB2YCbCr_BT601},
        {AVCOL_SPC_SMPTE170M, AVCOL_RANGE_JPEG, YPbPr2RGB_BT601, RGB2YPbPr_BT601},
        {AVCOL_SPC_SMPTE240M, AVCOL_RANGE_MPEG, YCbCr2RGB_BT601, RGB2YCbCr_BT601},
        {AVCOL_SPC_SMPTE240M, AVCOL_RANGE_JPEG, YPbPr2RGB_BT601, RGB2YPbPr_BT601},
};

static int bmvpp_get_csc_type_by_colorinfo(int color_space, int color_range, int fmt, vpp_csc_type *p_csc_type)
{
    int ret = BMVPP_ERR;
    int inRGB = 0;
    if (fmt == BMVPP_FMT_BGR ||
        fmt == BMVPP_FMT_BGRP ||
        fmt == BMVPP_FMT_RGB ||
        fmt == BMVPP_FMT_ARGB ||
        fmt == BMVPP_FMT_ABGR ||
        fmt == BMVPP_FMT_RGBP) {
        inRGB = 1;
    }

    for(int i = 0; i< BMVPP_ARRAY_SIZE(g_csc_map); ++i) {
        if (g_csc_map[i].color_range == color_range && g_csc_map[i].color_space == color_space) {
            ret = BMVPP_OK;
            *p_csc_type = (inRGB == 0 ? g_csc_map[i].csc_type_yuv2rgb:g_csc_map[i].csc_type_rgb2yuv);
            break;
        }
    }
    return ret;
}

static int bmpvpp_resize_type (int scale_method) {
    switch (scale_method){
        case BMVPP_RESIZE_BILINEAR: return VPP_SCALE_BILINEAR;
        case BMVPP_RESIZE_NEAREST: return VPP_SCALE_NEAREST;
        case BMVPP_RESIZE_BICUBIC: return VPP_SCALE_BICUBIC;
    }

    return VPP_SCALE_BILINEAR;
}

static void vppmat_from(vpp_mat *d, bmvpp_mat_t *s)
{
    d->format = s->format;
    d->num_comp = s->num_comp;
    for(int i = 0;i < 4; i++) {
        d->pa[i] = s->pa[i];
        d->ion_len[i] = s->size[i];
    }

    d->stride = s->stride;
    d->uv_stride = s->c_stride;
    d->is_pa = 1;
    d->cols = s->width;
    d->rows = s->height;
    d->axisX = 0;
    d->axisY = 0;

    bmvpp_get_vppfd(s->soc_idx, &d->vpp_fd);
}

static void vppmat_from_plane(vpp_mat *d, bmvpp_mat_t *s, int idx)
{
    d->format = BMVPP_FMT_GREY;
    d->num_comp = 1;

    d->pa[0] = s->pa[idx];
    d->ion_len[0] = s->size[idx];

    if (s->format == BMVPP_FMT_YUV422P) {

        switch (idx) {
            case 0:
                d->stride = s->stride;
                d->cols = s->width;
                d->rows = (s->height+1)&(~1);
                break;
            case 1:
            case 2:
                d->stride = s->c_stride;
                d->cols = s->c_stride;
                d->rows = (s->height+1)&(~1);
                break;
        }
    }else if (s->format == BMVPP_FMT_I420) {
        switch (idx) {
            case 0:
                d->stride = s->stride;
                d->cols = s->width;
                d->rows = (s->height+1)&(~1);
                break;
            case 1:
            case 2:
                d->stride = s->c_stride;
                d->cols = s->c_stride;
                d->rows = (s->height+1)/2;
                break;
        }
    }else{
        assert(0);
    }

    d->uv_stride = s->c_stride;
    d->is_pa = 1;
    d->axisX = 0;
    d->axisY = 0;

    bmvpp_get_vppfd(s->soc_idx, &d->vpp_fd);
}

int bmvpp_J422pToJ420p(int is_compressed, bmvpp_mat_t* src, bmvpp_rect_t* loca,  bmvpp_mat_t* dst, int dst_x, int dst_y, int resize_method)
{
    int ret = 0;
    vpp_mat s;
    vpp_mat d;
    BMVPP_UNUSED(is_compressed);
    BMVPP_UNUSED(loca);
    BMVPP_UNUSED(dst_x);
    BMVPP_UNUSED(dst_y);


    memset(&s, 0, sizeof(s));
    memset(&d, 0, sizeof(d));

    vpp_csc_type csc_type = CSC_MAX;
    bmvpp_get_csc_type_by_colorinfo(src->colorspace, src->color_range, src->format, &csc_type);

    for(int i = 0;i < 3; i++) {
        vpp_mat ps={0};
        vpp_mat pd={0};

        vppmat_from_plane(&ps, src, i);
        vppmat_from_plane(&pd, dst, i);

        vpp_rect crop1;
        crop1.x = 0;
        crop1.y = 0;
        crop1.width = ps.cols;
        crop1.height = ps.rows;

        ret = vpp_misc(&ps, &crop1, &pd, 1, csc_type, bmpvpp_resize_type(resize_method));
        if (ret < 0) {
            bmvpp_log_err("vpp_misc errr");
            return ret;
        }
    }

    return 0;

}

int bmvpp_scale(int is_compressed, bmvpp_mat_t* src, bmvpp_rect_t* loca, bmvpp_mat_t* dst, int dst_x, int dst_y, int resize_method)
{
    vpp_mat s;
    vpp_mat d;

    memset(&s, 0, sizeof(s));
    memset(&d, 0, sizeof(d));
    vppmat_from(&s, src);
    vppmat_from(&d, dst);

    vpp_rect crop;
    crop.x = loca->x;
    crop.y = loca->y;
    crop.width = loca->width;
    crop.height = loca->height;

    /* border use */
    d.axisX = dst_x;
    d.axisY = dst_y;

    vpp_csc_type csc_type = CSC_MAX;
    bmvpp_get_csc_type_by_colorinfo(src->colorspace, src->color_range, src->format, &csc_type);

    if (!is_compressed) {
        return vpp_misc(&s, &crop, &d, 1, csc_type, bmpvpp_resize_type(resize_method));
    }

    return fbd_csc_crop_multi_resize_ctype_stype(&s, &crop, &d, 1, csc_type, bmpvpp_resize_type(resize_method));
}



const char *bmvpp_get_format_desc(int format)
{
    switch (format)
    {
        case BMVPP_FMT_GREY: return "Y only";
        case BMVPP_FMT_I420: return "yuv420p";
        case BMVPP_FMT_NV12: return "nv12";
        case BMVPP_FMT_BGR:  return "bgr";
        case BMVPP_FMT_RGB:  return "rgb";
        case BMVPP_FMT_RGBP: return "rgbp";
        case BMVPP_FMT_BGRP: return "bgrp";
        case BMVPP_FMT_YUV444P:  return "yuv444p";
        case BMVPP_FMT_YUV422P: return "yuv422p";
        case BMVPP_FMT_YUV444:  return "yuv444";
    }

    return "bmvpp_format_unknown";
}