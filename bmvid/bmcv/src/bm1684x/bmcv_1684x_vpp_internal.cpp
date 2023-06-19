#include <vector>
#include <memory>
#include <functional>
#include "bmlib_interface.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_1684x_vpp_internal.h"
#ifdef __linux__
#include <sys/ioctl.h>
#endif

float bm1684x_csc_matrix[12][12] = {
  /*YUV2RGB 601 NOT FULL RANGE    3   YCbCr2RGB_BT601*/
  {1.1643835616438400, 0.0000000000000000, 1.5958974359999800,-222.9050087942980000,
   1.1643835616438400,-0.3936638456015240,-0.8138402992560010, 135.9303935554620000,
   1.1643835616438400, 2.0160738896544600, 0.0000000000000000,-276.6875948620730000},

  /*YUV2RGB 601 FULL RANGE    2   YPbPr2RGB_BT601*/
  {1.0000000000000000, 0.0000000000000000, 1.4018863751529200, -179.4414560195740000,
   1.0000000000000000,-0.3458066722146720,-0.7149028511111540,  135.7708189857060000 ,
   1.0000000000000000, 1.7709825540494100, 0.0000000000000000, -226.6857669183240000},

  /*RGB2YUV 601 NOT FULL RANGE   7     RGB2YCbCr_BT601*/
  {0.2568370271402130, 0.5036437166574750, 0.0983427856140760, 16.0,
  -0.1483362360666210,-0.2908794502078890, 0.4392156862745100, 128.0,
   0.4392156862745100,-0.3674637551088690,-0.0717519311656413, 128.0},

  /*YUV2RGB 709 NOT FULL RANGE     1 YCbCr2RGB_BT709*/
  {1.1643835616438400, 0.0000000000000000, 1.7926522634175300,-248.0896267037460000,
   1.1643835616438400,-0.2132370215686210,-0.5330040401422640, 76.8887189126920000,
   1.1643835616438400, 2.1124192819911800, 0.0000000000000000,-289.0198050811730000},

  /*RGB2YUV 709 NOT FULL RANGE   5   RGB2YCbCr_BT709*/
  {0.1826193815131790, 0.6142036888240730, 0.0620004590745127, 16.0,
  -0.1006613638136630,-0.3385543224608470, 0.4392156862745100, 128.0,
   0.4392156862745100,-0.3989444541822880,-0.0402712320922214, 128.0},

  /*RGB2YUV 601 FULL RANGE   6       RGB2YPbPr_BT601*/
  {0.2990568124235360, 0.5864344646011700, 0.1145087229752940, 0.0  ,
  -0.1688649115936980,-0.3311350884063020, 0.5000000000000000, 128.0,
   0.5000000000000000,-0.4183181140748280,-0.0816818859251720, 128.0},

  /*YUV2RGB 709 FULL RANGE     0   YPbPr2RGB_BT709*/
  {1.0000000000000000, 0.0000000000000000, 1.5747219882569700, -201.5644144968920000,
   1.0000000000000000,-0.1873140895347890,-0.4682074705563420,  83.90675969166480000,
   1.0000000000000000, 1.8556153692785300, 0.0000000000000000, -237.5187672676520000},

  /*RGB2YUV 709 FULL RANGE    4    RGB2YPbPr_BT709*/
 {0.2126390058715100, 0.7151686787677560, 0.0721923153607340, 0.0,
 -0.1145921775557320,-0.3854078224442680, 0.5000000000000000, 128.0,
  0.5000000000000000,-0.4541555170378730,-0.0458444829621270, 128.0},
#if 0
    1                                1.140014648       -145.921874944
    1            -0.395019531       -0.580993652       124.929687424
    1             2.031982422                          -260.093750016

  /*YUV2RGB 601 opencv: YUV2RGB  maosi simulation   8*/
  {0x3F800000, 0x00000000, 0x3F91EBFF, 0xC311EBFF,
  0x3F800000, 0xBECA3FFF, 0xBF14BBFF, 0x42F9DBFF,
  0x3F800000, 0x40020C00, 0x00000000, 0xC3820C00},

    // Coefficients for RGB to YUV420p conversion
    static const int ITUR_BT_601_CRY =  269484;
    static const int ITUR_BT_601_CGY =  528482;
    static const int ITUR_BT_601_CBY =  102760;
    static const int ITUR_BT_601_CRU = -155188;
    static const int ITUR_BT_601_CGU = -305135;
    static const int ITUR_BT_601_CBU =  460324;
    static const int ITUR_BT_601_CGV = -385875;
    static const int ITUR_BT_601_CBV = -74448;
     269484    528482   102760    16.5
    -155188   -305135   460324    128.5
     460324   -385875   -74448    128.5

    shift=1024*1024

  /*RGB2YUV420P, opencv:RGB2YUVi420 601 NOT FULL RANGE  9*/
  {0x3E83957F, 0x3F01061F, 0x3DC8B400, 0x41840000,
  0xBE178D00, 0xBE94FDE0, 0x3EE0C47F, 0x43008000,
  0x3EE0C47F, 0xBEBC6A60, 0xBD916800, 0x43008000},

   8414, 16519, 3208,     16
   -4865, -9528, 14392,   128
   14392, -12061, -2332,  128

   0.256774902     0.504119873       0.097900391          16
   -0.148468018    -0.290771484       0.439208984          128
   0.439208984    -0.36807251       -0.071166992          128

/*RGB2YUV420P,  FFMPEG : BGR2i420  601 NOT FULL RANGE    10*/
{0x3E8377FF, 0x3F010DFF, 0x3DC88000, 0x41800000,
 0xBE180800, 0xBE94DFFF, 0x3EE0DFFF, 0x43000000,
 0x3EE0DFFF, 0xBEBC7400, 0xBD91BFFF, 0x43000000},

1.163999557               1.595999718   -204.787963904
1.163999557 -0.390999794 -0.812999725    154.611938432
1.163999557  2.017999649                -258.803955072

/*YUV420P2bgr  OPENCV : i4202BGR  601   11*/
{0x3F94FDEF, 0x0,        0x3FCC49B8, 0xC34CC9B8,
 0x3F94FDEF, 0xBEC8311F, 0xBF5020BF, 0x431A9CA7,
 0x3F94FDEF, 0x400126E7, 0x0,        0xC38166E7},
#endif
};

static bm_status_t simple_check_bm1684x_input_param(
  bm_handle_t             handle,
  bm_image*               input_or_output,
  int                     frame_number
)
{
  if (handle == NULL)
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "handle is nullptr");
    return BM_ERR_FAILURE;
  }
  if (input_or_output == NULL)
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "input or output is nullptr");
    return BM_ERR_FAILURE;
  }
  if((frame_number > VPP1684X_MAX_CROP_NUM) || (frame_number <= 0))
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "input num should less than 512");
    return BM_NOT_SUPPORTED;
  }

  return BM_SUCCESS;;
}
static int is_csc_yuv_or_rgb(bm_image_format_ext format)
{
  int ret = COLOR_SPACE_YUV;

  switch(format)
  {
    case FORMAT_YUV420P:
    case FORMAT_YUV422P:
    case FORMAT_YUV444P:
    case FORMAT_NV12:
    case FORMAT_NV21:
    case FORMAT_NV16:
    case FORMAT_NV61:
    case FORMAT_NV24:
    case FORMAT_GRAY:
    case FORMAT_COMPRESSED:
    case FORMAT_YUV444_PACKED:
    case FORMAT_YVU444_PACKED:
    case FORMAT_YUV422_YUYV:
    case FORMAT_YUV422_YVYU:
    case FORMAT_YUV422_UYVY:
    case FORMAT_YUV422_VYUY:
      ret = COLOR_SPACE_YUV;
      break;
    case FORMAT_RGB_PLANAR:
    case FORMAT_BGR_PLANAR:
    case FORMAT_RGB_PACKED:
    case FORMAT_BGR_PACKED:
    case FORMAT_RGBP_SEPARATE:
    case FORMAT_BGRP_SEPARATE:
    case FORMAT_ARGB_PACKED:
    case FORMAT_ABGR_PACKED:
      ret = COLOR_SPACE_RGB;
      break;
    case FORMAT_HSV_PLANAR:
    case FORMAT_HSV180_PACKED:
    case FORMAT_HSV256_PACKED:
      ret = COLOR_SPACE_HSV;
      break;
    case FORMAT_RGBYP_PLANAR:
      ret = COLOR_SPACE_RGBY;
      break;
    default:
      ret = COLOR_NOT_DEF;
      break;
  }


  return ret;
}

static bm_status_t check_bm1684x_bm_image_param(
  int                     frame_number,
  bm_image*               input,
  bm_image*               output)
{
  int stride[4];
  int frame_idx = 0;
  int plane_num = 0;
  bm_device_mem_t device_mem[4];

  for (frame_idx = 0; frame_idx < frame_number; frame_idx++) {

    if(!bm_image_is_attached(input[frame_idx]) ||
       !bm_image_is_attached(output[frame_idx]))
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "not correctly create bm_image ,frame [%d] input or output not attache mem %s: %s: %d\n",
        frame_idx,filename(__FILE__), __func__, __LINE__);
      return BM_ERR_PARAM;
    }
    if(input[frame_idx].data_type != DATA_TYPE_EXT_1N_BYTE)
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "vpp input only support DATA_TYPE_EXT_1N_BYTE,frame [%d], %s: %s: %d\n",
        frame_idx, filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
    }

     if((output[frame_idx].data_type != DATA_TYPE_EXT_FLOAT32) &&
        (output[frame_idx].data_type != DATA_TYPE_EXT_1N_BYTE) &&
        (output[frame_idx].data_type != DATA_TYPE_EXT_1N_BYTE_SIGNED) &&
        (output[frame_idx].data_type != DATA_TYPE_EXT_FP16) &&
        (output[frame_idx].data_type != DATA_TYPE_EXT_BF16))
     {
       bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
         "bm1684x vpp[%d] output data type %d not support %s: %s: %d\n",
         frame_idx ,output[frame_idx].data_type, filename(__FILE__), __func__, __LINE__);
       return BM_NOT_SUPPORTED;
     }

    if((bm_image_get_stride(input[frame_idx], stride) != BM_SUCCESS) ||
       (bm_image_get_stride(output[frame_idx], stride) != BM_SUCCESS))
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "not correctly create input bm_image , frame [%d],input or output get stride err %s: %s: %d\n",
        frame_idx,filename(__FILE__), __func__, __LINE__);
      return BM_ERR_FAILURE;
    }

    plane_num = bm_image_get_plane_num(input[frame_idx]);
    if(plane_num == 0 || bm_image_get_device_mem(input[frame_idx], device_mem) != BM_SUCCESS)
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "not correctly create input[%d] bm_image, get plane num or device mem err %s: %s: %d\n",
        frame_idx, filename(__FILE__), __func__, __LINE__);
      return BM_ERR_FAILURE;
    }
#ifndef USING_CMODEL
    u64 device_addr = 0;
    int i = 0;

    for (i = 0; i < plane_num; i++) {
      device_addr = bm_mem_get_device_addr(device_mem[i]);
      if((device_addr > 0x4ffffffff) || (device_addr < 0x100000000))
      {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "input[%d] device memory should between 0x100000000 and 0x4ffffffff  %s: %s: %d\n",
          frame_idx, filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
      }
    }
#endif
    plane_num = bm_image_get_plane_num(output[frame_idx]);
    if(plane_num == 0 || bm_image_get_device_mem(output[frame_idx], device_mem) != BM_SUCCESS)
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "not correctly create output[%d] bm_image, get plane num or device mem err %s: %s: %d\n",
        frame_idx, filename(__FILE__), __func__, __LINE__);
      return BM_ERR_FAILURE;
    }
#ifndef USING_CMODEL
    for (i = 0; i < plane_num; i++) {
      device_addr = bm_mem_get_device_addr(device_mem[i]);
      if((device_addr > 0x4ffffffff) || (device_addr < 0x100000000))
      {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "output[%d] device memory should between 0x100000000 and 0x4ffffffff  %s: %s: %d\n",
          frame_idx, filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
      }
    }
#endif
  }

  return BM_SUCCESS;
}
static bm_status_t check_bm1684x_vpp_csctype(
  int            frame_number,
  bm_image*      input,
  bm_image*      output,
  csc_type_t     csc_type,
  csc_matrix_t*  matrix)
{
  bm_status_t ret = BM_SUCCESS;
  int input_color_space, output_color_space;
  int idx = 0;

  for (idx = 0; idx < frame_number; idx++) {

    input_color_space = is_csc_yuv_or_rgb(input[idx].image_format);
    output_color_space = is_csc_yuv_or_rgb(output[idx].image_format);

    if((is_csc_yuv_or_rgb(input[0].image_format) != is_csc_yuv_or_rgb(input[idx].image_format)) ||
      (is_csc_yuv_or_rgb(output[0].image_format) != is_csc_yuv_or_rgb(output[idx].image_format)))
    {
      ret = BM_ERR_PARAM;
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm1684x vpp Input and output color space changes will cause hardware hang,"
        " %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      break;
    }

    switch(csc_type)
    {
      case CSC_YCbCr2RGB_BT601:
      case CSC_YPbPr2RGB_BT601:
      case CSC_YPbPr2RGB_BT709:
      case CSC_YCbCr2RGB_BT709:
        if(COLOR_SPACE_YUV != input_color_space)
        {
          ret = BM_NOT_SUPPORTED;
          break;
        }
        if((COLOR_SPACE_RGB != output_color_space) &&
           (COLOR_SPACE_HSV != output_color_space) &&
           (COLOR_SPACE_RGBY != output_color_space))
        {
          ret = BM_NOT_SUPPORTED;
          break;
        }
        break;
      case CSC_RGB2YCbCr_BT709:
      case CSC_RGB2YPbPr_BT601:
      case CSC_RGB2YCbCr_BT601:
      case CSC_RGB2YPbPr_BT709:
        if(COLOR_SPACE_RGB != input_color_space)
        {
          ret = BM_NOT_SUPPORTED;
          break;
        }
        if((COLOR_SPACE_YUV != output_color_space) &&
           (COLOR_SPACE_RGBY != output_color_space))
        {
          ret = BM_NOT_SUPPORTED;
          break;
        }
        break;

      case CSC_MAX_ENUM:
        break;
      case CSC_USER_DEFINED_MATRIX:
        if(NULL == matrix)
        {
          ret = BM_NOT_SUPPORTED;
          bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
            "bm1684x vpp csc_type param %d is CSC_USER_DEFINED_MATRIX ,"
            "matrix can not be null, %s: %s: %d\n",
            csc_type, filename(__FILE__), __func__, __LINE__);
          return ret;
        }
        break;
      default:
          ret = BM_NOT_SUPPORTED;
          bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
            "bm1684x vpp csc_type param %d not support,%s: %s: %d\n",
            csc_type, filename(__FILE__), __func__, __LINE__);
          return ret;
      }
  }
  return ret;
}

static bm_status_t check_bm1684x_vpp_input_format(
  int                     frame_number,
  bm_image*               input)
{
  bm_status_t ret = BM_SUCCESS;
  int frame_idx = 0;

  for (frame_idx = 0; frame_idx < frame_number; frame_idx++) {

    switch(input[frame_idx].image_format)
    {
      case FORMAT_GRAY:
      case FORMAT_YUV420P:
      case FORMAT_YUV422P:
      case FORMAT_YUV444P:
      case FORMAT_NV12:
      case FORMAT_NV21:
      case FORMAT_NV16:
      case FORMAT_NV61:
      case FORMAT_YUV444_PACKED:
      case FORMAT_YVU444_PACKED:
      case FORMAT_YUV422_YUYV:
      case FORMAT_YUV422_YVYU:
      case FORMAT_YUV422_UYVY:
      case FORMAT_YUV422_VYUY:
      case FORMAT_RGBP_SEPARATE:
      case FORMAT_BGRP_SEPARATE:
      case FORMAT_RGB_PLANAR:
      case FORMAT_BGR_PLANAR:
      case FORMAT_RGB_PACKED:
      case FORMAT_BGR_PACKED:
      case FORMAT_COMPRESSED:
        break;
      default:
        ret = BM_NOT_SUPPORTED;
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "1684x vpp input[%d] format %d not support %s: %s: %d\n",
          frame_idx,input[frame_idx].image_format,filename(__FILE__), __func__, __LINE__);
        return ret;
    }
  }
  return ret;
}
static bm_status_t check_bm1684x_vpp_output_format(
  int                     frame_number,
  bm_image*               output)
{
  bm_status_t ret = BM_SUCCESS;
  int frame_idx = 0;

  for (frame_idx = 0; frame_idx < frame_number; frame_idx++) {

    switch(output[frame_idx].image_format)
    {
      case FORMAT_GRAY:
      case FORMAT_YUV420P:
      case FORMAT_YUV444P:
      case FORMAT_NV12:
      case FORMAT_NV21:
      case FORMAT_RGBYP_PLANAR:
      case FORMAT_RGBP_SEPARATE:
      case FORMAT_BGRP_SEPARATE:
      case FORMAT_RGB_PLANAR:
      case FORMAT_BGR_PLANAR:
      case FORMAT_RGB_PACKED:
      case FORMAT_BGR_PACKED:
      case FORMAT_HSV180_PACKED:
      case FORMAT_HSV256_PACKED:
         break;
      default:
        ret = BM_NOT_SUPPORTED;
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "1684x vpp output[%d] format %d not support %s: %s: %d\n",
          frame_idx,output[frame_idx].image_format,filename(__FILE__), __func__, __LINE__);
        return ret;
    }
  }
  return ret;
}

static bm_status_t check_bm1684x_vpp_image_param(
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  border_t*               border_param)
{
  bm_status_t ret = BM_SUCCESS;
  int frame_idx = 0;
  bmcv_rect_t  src_crop_rect, dst_crop_rect;


  if((frame_number > VPP1684X_MAX_CROP_NUM) || (frame_number <= 0))
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      " input num should less than 256  %s: %s: %d\n",
      filename(__FILE__), __func__, __LINE__);
    return BM_NOT_SUPPORTED;
  }
  for (frame_idx = 0; frame_idx < frame_number; frame_idx++)
  {
    if(NULL == input_crop_rect)
    {
      src_crop_rect.start_x = 0;
      src_crop_rect.start_y = 0;
      src_crop_rect.crop_w  = input[frame_idx].width;
      src_crop_rect.crop_h  = input[frame_idx].height;
    }
    else{
      src_crop_rect = input_crop_rect[frame_idx];
    }

    if(NULL == padding_attr)
    {
      dst_crop_rect.start_x = 0;
      dst_crop_rect.start_y = 0;
      dst_crop_rect.crop_w  = output[frame_idx].width;
      dst_crop_rect.crop_h  = output[frame_idx].height;
    }
    else{
      if((padding_attr[frame_idx].if_memset != 0) && (padding_attr[frame_idx].if_memset != 1))
      {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "frame [%d], padding_attr if_memset wrong  %s: %s: %d\n",
          frame_idx, filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
      }
      if(padding_attr[frame_idx].if_memset == 1)
      {
        if ((padding_attr[frame_idx].dst_crop_stx > 255) || (padding_attr[frame_idx].dst_crop_sty > 255) ||
            (output[frame_idx].width - padding_attr[frame_idx].dst_crop_w - padding_attr[frame_idx].dst_crop_stx > 255) ||
            (output[frame_idx].height- padding_attr[frame_idx].dst_crop_h - padding_attr[frame_idx].dst_crop_sty > 255) )
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
              "frame [%d], padding_attr  x,y,w,h may be > 255  %s: %s: %d\n",
              frame_idx, filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
        dst_crop_rect.start_x = 0;
        dst_crop_rect.start_y = 0;
        dst_crop_rect.crop_w  = output[frame_idx].width;
        dst_crop_rect.crop_h  = output[frame_idx].height;

      }else if(padding_attr[frame_idx].if_memset == 0)
      {
        dst_crop_rect.start_x = padding_attr[frame_idx].dst_crop_stx;
        dst_crop_rect.start_y = padding_attr[frame_idx].dst_crop_sty;
        dst_crop_rect.crop_w  = padding_attr[frame_idx].dst_crop_w;
        dst_crop_rect.crop_h  = padding_attr[frame_idx].dst_crop_h;
      }
    }

    if((input[frame_idx].width  > VPP1684X_MAX_W) ||
       (input[frame_idx].height > VPP1684X_MAX_H) ||
       (input[frame_idx].height < VPP1684X_MIN_H) ||
       (input[frame_idx].width  < VPP1684X_MIN_W) ||
       (src_crop_rect.crop_w > VPP1684X_MAX_W) ||
       (src_crop_rect.crop_h > VPP1684X_MAX_H) ||
       (src_crop_rect.crop_w < VPP1684X_MIN_W) ||
       (src_crop_rect.crop_h < VPP1684X_MIN_H) ||
       (output[frame_idx].width  > VPP1684X_MAX_W) ||
       (output[frame_idx].height > VPP1684X_MAX_H) ||
       (output[frame_idx].width  < VPP1684X_MIN_W) ||
       (output[frame_idx].height < VPP1684X_MIN_H) ||
       (dst_crop_rect.crop_w > VPP1684X_MAX_W) ||
       (dst_crop_rect.crop_h > VPP1684X_MAX_H) ||
       (dst_crop_rect.crop_w < VPP1684X_MIN_W) ||
       (dst_crop_rect.crop_h < VPP1684X_MIN_H) )
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,\
        "bm1684x vpp frame_idx %d, width or height abnormal,"
        "input[frame_idx].width %d,input[frame_idx].height %d,"
        "src_crop_rect[frame_idx].crop_w %d,src_crop_rect[frame_idx].crop_h %d,"
        "output[frame_idx].width %d, output[frame_idx].height %d,"
        "dst_crop_rect[frame_idx].crop_w %d, dst_crop_rect[frame_idx].crop_h %d,"
        "%s: %s: %d\n",\
        frame_idx,input[frame_idx].width,input[frame_idx].height,src_crop_rect.crop_w,
        src_crop_rect.crop_h, output[frame_idx].width, output[frame_idx].height,
        dst_crop_rect.crop_w,dst_crop_rect.crop_h,
        filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if((src_crop_rect.start_x < 0) ||
       (src_crop_rect.start_y < 0) ||
       (dst_crop_rect.start_x < 0) ||
       (dst_crop_rect.start_y < 0) ||
       (src_crop_rect.start_x + src_crop_rect.crop_w > input[frame_idx].width) ||
       (src_crop_rect.start_y + src_crop_rect.crop_h > input[frame_idx].height) ||
       (dst_crop_rect.start_x + dst_crop_rect.crop_w > output[frame_idx].width) ||
       (dst_crop_rect.start_y + dst_crop_rect.crop_h > output[frame_idx].height)
    )
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "frame [%d], input or output  crop is out of range  %s: %s: %d\n",
        frame_idx, filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
    }
  }

  ret = check_bm1684x_vpp_input_format(frame_number, input);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }
  ret = check_bm1684x_vpp_output_format(frame_number, output);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  if(NULL != border_param)
  {
    if(border_param[0].rect_border_enable == 1)
    {
      if((border_param[0].st_x > input[0].width) ||
         (border_param[0].st_y > input[0].height) ||
         (output[0].data_type != DATA_TYPE_EXT_1N_BYTE))
      {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "bm1684x vpp draw rectangle param wrong,maybe st_x,st_y wrong or data_type not supported, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
      }
    }
  }

  return ret;
}


static bm_status_t check_bm1684x_vpp_param(
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  border_t*               border_param)
{

  bm_status_t ret = BM_SUCCESS;

  if((input == NULL) || (output == NULL))
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "input or output is nullptr , %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
    return BM_ERR_PARAM;
  }

  ret = check_bm1684x_bm_image_param(frame_number, input, output);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  ret = check_bm1684x_vpp_image_param(frame_number, input, output, input_crop_rect, padding_attr, border_param);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  if((algorithm != BMCV_INTER_NEAREST) && (algorithm != BMCV_INTER_LINEAR))
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "bm1684x vpp not support algorithm %d,%s: %s: %d\n",
      algorithm, filename(__FILE__), __func__, __LINE__);
    return BM_NOT_SUPPORTED;
  }

  ret = check_bm1684x_vpp_csctype(frame_number, input,output,csc_type, matrix);
  if(ret != BM_SUCCESS)
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "bm1684x vpp csctype %d, %s: %s: %d\n",
      csc_type, filename(__FILE__), __func__, __LINE__);
    return ret;
  }

  return BM_SUCCESS;
}

static bm_status_t check_bm1684x_vpp_continuity(
  int                     frame_number,
  bm_image*               input_or_output)
{
  for (int i = 0; i < frame_number; i++)
  {
#if 0
    if((input_or_output[0].image_format!= input_or_output[i].image_format) ||
       (input_or_output[0].width!= input_or_output[i].width)||
       (input_or_output[0].height!= input_or_output[i].height))
#endif
    if(input_or_output[i].image_private== NULL)
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm_image image_private cannot be empty, %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
    }
  }
  return BM_SUCCESS;
}
static bm_status_t bm1684x_check_vpp_internal_param(
  int                     frame_number,
  bm1684x_vpp_mat         *vpp_input,
  bm1684x_vpp_mat         *vpp_output,
  bm1684x_vpp_param       *vpp_param)
{
  int idx = 0;
  float scl_x, scl_y;
  for (idx = 0; idx < frame_number; idx++) {
    if((vpp_param[idx].post_padding_param.left   == 0) &&
       (vpp_param[idx].post_padding_param.right  == 0) &&
       (vpp_param[idx].post_padding_param.top    == 0) &&
       (vpp_param[idx].post_padding_param.bottom == 0) &&
       (vpp_param[idx].post_padding_param.post_padding_enable == 1))
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm1684x vpp postpadding left right top bottom all is 0 , %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      return BM_ERR_PARAM;
    }
    if(((vpp_input[idx].cropW+ vpp_param[idx].padding_param.left + vpp_param[idx].padding_param.right) > VPP1684X_MAX_W) ||
       ((vpp_input[idx].cropH+ vpp_param[idx].padding_param.top+ vpp_param[idx].padding_param.bottom) > VPP1684X_MAX_W) ||
       ((vpp_output[idx].cropW - vpp_param[idx].post_padding_param.left - vpp_param[idx].post_padding_param.right) <= 0) ||
       ((vpp_output[idx].cropH - vpp_param[idx].post_padding_param.top - vpp_param[idx].post_padding_param.bottom) <= 0))
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm1684x vpp after padding > 8192, or after postpadding < 0 , %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      return BM_ERR_PARAM;
    }
    scl_x = (float)(vpp_input[idx].cropW+ vpp_param[idx].padding_param.left + vpp_param[idx].padding_param.right) / (float)(vpp_output[idx].cropW - vpp_param[idx].post_padding_param.left - vpp_param[idx].post_padding_param.right);
    scl_y = (float)(vpp_input[idx].cropH+ vpp_param[idx].padding_param.top+ vpp_param[idx].padding_param.bottom) / (float)(vpp_output[idx].cropH - vpp_param[idx].post_padding_param.top - vpp_param[idx].post_padding_param.bottom);
    if((scl_x < (1.0/(float)VPP1684X_MAX_SCALE_RATIO)) ||
        (scl_x > ((float)VPP1684X_MAX_SCALE_RATIO)) ||
        (scl_y < (1.0/(float)VPP1684X_MAX_SCALE_RATIO)) ||
        (scl_y > ((float)VPP1684X_MAX_SCALE_RATIO)))
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm1684x vpp not support: scaling ratio greater than 128,pay attention to postpadding, %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
    }

    if(vpp_param[idx].border_param.rect_border_enable == 1)
    {
      if((vpp_input[idx].format == IN_YUV420P) || (vpp_input[idx].format == IN_NV12) || (vpp_input[idx].format == IN_NV21))
      {
        vpp_param[idx].border_param.st_y = ((vpp_param[idx].border_param.st_y >> 1) << 1);
        vpp_param[idx].border_param.st_x = ((vpp_param[idx].border_param.st_x >> 1) << 1);
        vpp_param[idx].border_param.width     = VPPALIGN(vpp_param[idx].border_param.width, 2);
        vpp_param[idx].border_param.height    = VPPALIGN(vpp_param[idx].border_param.height, 2);
        vpp_param[idx].border_param.thickness = VPPALIGN(vpp_param[idx].border_param.thickness, 2);
      }
      if((vpp_param[idx].border_param.st_x > vpp_input[idx].cropW) ||
         (vpp_param[idx].border_param.st_y > vpp_input[idx].cropH) ||
         (vpp_input[idx].cropW != vpp_output[idx].cropW) ||
         (vpp_input[idx].cropH != vpp_output[idx].cropH) ||
         (vpp_input[idx].format != vpp_output[idx].format) ||
         (vpp_param[idx].padding_param.padding_enable == 1) ||
         (vpp_param[idx].post_padding_param.post_padding_enable== 1) ||
         (vpp_param[idx].font_param.font_enable == 1) ||
         (vpp_param[idx].conbri_shifting_param.con_bri_enable == 1) ||
         (vpp_param[idx].border_param.thickness == 0) ||
         (vpp_output[idx].wdma_form != DATA_TYPE_1N_BYTE))
      {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "bm1684x vpp border_param wrong, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
      }
    }
    if(((vpp_input[idx].format == IN_FBC) && (vpp_output[idx].format == OUT_HSV180)) ||
       ((vpp_input[idx].format == IN_FBC) && (vpp_output[idx].format == OUT_HSV256)))
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "bm1684x vpp direct conversion from compressed format to HSV is not supported, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }
    if((vpp_param[idx].csc_scale_order == 1) && (vpp_output[idx].format == OUT_RGBYP))
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "bm1684x vpp rgbyp does not support CSC before sacle, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }
    if((vpp_input[idx].format == IN_FBC) && ((vpp_input[idx].cropW % 16 != 0) ||
       (vpp_input[idx].cropH % 4 != 0) || (vpp_input[idx].axisX % 32 != 0) || (vpp_input[idx].axisY % 2 != 0)))
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "bm1684x vpp When compressing the format, cropw requires 16 alignment, croph requires 4 alignment, and start_x requires 32 alignment.start_y requires 2 alignment, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }

    if(((vpp_output[idx].wdma_form == DATA_TYPE_FP16) ||
       (vpp_output[idx].wdma_form == DATA_TYPE_BF16)||
       (vpp_output[idx].wdma_form == DATA_TYPE_FLOAT32)) &&
       ((vpp_output[idx].format != OUT_RGBP) && (vpp_output[idx].format != OUT_YUV444P) && (vpp_output[idx].format != OUT_YUV400P)))
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm1684x vpp fp32,bf16,fp16 only supprot yuv444p, yonly and rgbp, %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      return BM_ERR_PARAM;
    }
  }
  return BM_SUCCESS;
}

static void vpp1684x_dump(struct vpp_batch_n *batch)
{
  int idx = 0;
  descriptor *pdes = NULL;

  bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " %s: %s: %d\n", __FILE__, __func__, __LINE__);
  for (idx = 0; idx < batch->num; idx++) {
    pdes = (batch->cmd + idx);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " idx %d, batch->num  %d\n",idx, batch->num);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " des_head.crop_id  %d, crop_flag %d\n",
      pdes->des_head.crop_id, pdes->des_head.crop_flag);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " input_format 0x%x\n", pdes->src_ctrl.input_format);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_enable 0x%x\n", pdes->src_ctrl.post_padding_enable);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_enable 0x%x\n", pdes->src_ctrl.padding_enable);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " rect_border_enable 0x%x\n", pdes->src_ctrl.rect_border_enable);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " font_enable 0x%x\n", pdes->src_ctrl.font_enable);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_enable 0x%x\n", pdes->src_ctrl.con_bri_enable);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_scale_order 0x%x\n", pdes->src_ctrl.csc_scale_order);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " wdma_form 0x%x\n", pdes->src_ctrl.wdma_form);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " fbc_multi_map 0x%x\n", pdes->src_ctrl.fbc_multi_map);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_crop_height 0x%x\n", pdes->src_crop_size.src_crop_height);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_crop_width 0x%x\n", pdes->src_crop_size.src_crop_width);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_crop_st_x 0x%x\n", pdes->src_crop_st.src_crop_st_x);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_crop_st_y 0x%x\n", pdes->src_crop_st.src_crop_st_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_base_ext_ch0 0x%x\n", pdes->src_base_ext.src_base_ext_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_base_ext_ch1 0x%x\n", pdes->src_base_ext.src_base_ext_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_base_ext_ch2 0x%x\n", pdes->src_base_ext.src_base_ext_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_base_ch0 0x%x\n", pdes->src_base_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_base_ch1 0x%x\n", pdes->src_base_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_base_ch2 0x%x\n", pdes->src_base_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_stride_ch0 0x%x\n", pdes->src_stride_ch0.src_stride_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_stride_ch1 0x%x\n", pdes->src_stride_ch1.src_stride_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_stride_ch2 0x%x\n", pdes->src_stride_ch2.src_stride_ch2);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_value_ch0 0x%x\n", pdes->padding_value.padding_value_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_value_ch1 0x%x\n", pdes->padding_value.padding_value_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_value_ch2 0x%x\n", pdes->padding_value.padding_value_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_ext_up 0x%x\n", pdes->padding_ext.padding_ext_up);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_ext_bot 0x%x\n", pdes->padding_ext.padding_ext_bot);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_ext_left 0x%x\n", pdes->padding_ext.padding_ext_left);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " padding_ext_right 0x%x\n", pdes->padding_ext.padding_ext_right);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_value_r 0x%x\n", pdes->src_border_value.src_border_value_r);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_value_g 0x%x\n", pdes->src_border_value.src_border_value_g);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_value_b 0x%x\n", pdes->src_border_value.src_border_value_b);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_thickness 0x%x\n", pdes->src_border_value.src_border_thickness);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_height 0x%x\n", pdes->src_border_size.src_border_height);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_width 0x%x\n", pdes->src_border_size.src_border_width);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_st_y 0x%x\n", pdes->src_border_st.src_border_st_y );
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_border_st_x 0x%x\n", pdes->src_border_st.src_border_st_x);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_pitch 0x%x\n", pdes->src_font_pitch.src_font_pitch);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_type 0x%x\n", pdes->src_font_pitch.src_font_type);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_nf_range 0x%x\n", pdes->src_font_pitch.src_font_nf_range);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_dot_en 0x%x\n", pdes->src_font_pitch.src_font_dot_en);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_value_r 0x%x\n", pdes->src_font_value.src_font_value_r);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_value_g 0x%x\n", pdes->src_font_value.src_font_value_g);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_value_b 0x%x\n", pdes->src_font_value.src_font_value_b);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_ext 0x%x\n", pdes->src_font_value.src_font_ext);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_height 0x%x\n", pdes->src_font_size.src_font_height);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_width 0x%x\n", pdes->src_font_size.src_font_width);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_st_x 0x%x\n", pdes->src_font_st.src_font_st_x);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_st_y 0x%x\n", pdes->src_font_st.src_font_st_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " src_font_addr 0x%x\n", pdes->src_font_addr);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " output_format 0x%x\n", pdes->dst_ctrl.output_format);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cb_coeff_sel_tl 0x%x\n", pdes->dst_ctrl.cb_coeff_sel_tl);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cb_coeff_sel_tr 0x%x\n", pdes->dst_ctrl.cb_coeff_sel_tr);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cb_coeff_sel_bl 0x%x\n", pdes->dst_ctrl.cb_coeff_sel_bl);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cb_coeff_sel_br 0x%x\n", pdes->dst_ctrl.cb_coeff_sel_br);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cr_coeff_sel_tl 0x%x\n", pdes->dst_ctrl.cr_coeff_sel_tl);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cr_coeff_sel_tr 0x%x\n", pdes->dst_ctrl.cr_coeff_sel_tr);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cr_coeff_sel_bl 0x%x\n", pdes->dst_ctrl.cr_coeff_sel_bl);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " cr_coeff_sel_br 0x%x\n", pdes->dst_ctrl.cr_coeff_sel_br);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_crop_height 0x%x\n", pdes->dst_crop_size.dst_crop_height);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_crop_width 0x%x\n", pdes->dst_crop_size.dst_crop_width);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_crop_st_x 0x%x\n", pdes->dst_crop_st.dst_crop_st_x);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_crop_st_y 0x%x\n", pdes->dst_crop_st.dst_crop_st_y);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ext_ch0 0x%x\n", pdes->dst_base_ext.dst_base_ext_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ext_ch1 0x%x\n", pdes->dst_base_ext.dst_base_ext_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ext_ch2 0x%x\n", pdes->dst_base_ext.dst_base_ext_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ext_ch3 0x%x\n", pdes->dst_base_ext.dst_base_ext_ch3);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ch0 0x%x\n", pdes->dst_base_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ch1 0x%x\n", pdes->dst_base_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ch2 0x%x\n", pdes->dst_base_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_base_ch3 0x%x\n", pdes->dst_base_ch3);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_stride_ch0 0x%x\n", pdes->dst_stride_ch0.dst_stride_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_stride_ch1 0x%x\n", pdes->dst_stride_ch1.dst_stride_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_stride_ch2 0x%x\n", pdes->dst_stride_ch2.dst_stride_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " dst_stride_ch3 0x%x\n", pdes->dst_stride_ch3.dst_stride_ch3);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe00  %f\n", pdes->csc_coe00);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe01  %f\n", pdes->csc_coe01);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe02  %f\n", pdes->csc_coe02);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_add0  %f\n", pdes->csc_add0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe10  %f\n", pdes->csc_coe10);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe11  %f\n", pdes->csc_coe11);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe12  %f\n", pdes->csc_coe12);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_add1  %f\n", pdes->csc_add1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe20  %f\n", pdes->csc_coe20);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe21  %f\n", pdes->csc_coe21);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_coe22  %f\n", pdes->csc_coe22);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " csc_add2  %f\n", pdes->csc_add2);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_a_0  %f\n", pdes->con_bri_a_0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_a_1  %f\n", pdes->con_bri_a_1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_a_2  %f\n", pdes->con_bri_a_2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_b_0  %f\n", pdes->con_bri_b_0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_b_1  %f\n", pdes->con_bri_b_1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_b_2  %f\n", pdes->con_bri_b_2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_round 0x%x\n", pdes->con_bri_ctrl.con_bri_round);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " con_bri_full 0x%x\n", pdes->con_bri_ctrl.con_bri_full);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " hr_csc_f2i_round 0x%x\n", pdes->con_bri_ctrl.hr_csc_f2i_round);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " hr_csc_i2f_round 0x%x\n", pdes->con_bri_ctrl.hr_csc_i2f_round);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_value_ch0  %f\n", pdes->post_padding_value_ch0);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_value_ch1  %f\n", pdes->post_padding_value_ch1);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_value_ch2  %f\n", pdes->post_padding_value_ch2);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_value_ch3  %f\n", pdes->post_padding_value_ch3);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_ext_up 0x%x\n", pdes->post_padding_ext.post_padding_ext_up);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_ext_bot 0x%x\n", pdes->post_padding_ext.post_padding_ext_bot);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_ext_left 0x%x\n", pdes->post_padding_ext.post_padding_ext_left);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " post_padding_ext_right 0x%x\n", pdes->post_padding_ext.post_padding_ext_right);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " scl_ctrl 0x%x\n", pdes->scl_ctrl.scl_ctrl);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " scl_init_x 0x%x\n", pdes->scl_init.scl_init_x);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " scl_init_y 0x%x\n", pdes->scl_init.scl_init_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " scl_x %f\n", pdes->scl_x);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " scl_y %f\n", pdes->scl_y);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_ext_y 0x%x\n", pdes->map_conv_ext.map_conv_comp_ext_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_ext_c 0x%x\n", pdes->map_conv_ext.map_conv_comp_ext_c);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_off_base_ext_y 0x%x\n", pdes->map_conv_ext.map_conv_off_base_ext_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_off_base_ext_c 0x%x\n", pdes->map_conv_ext.map_conv_off_base_ext_c);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_off_base_y 0x%x\n", pdes->map_conv_off_base_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_off_base_c 0x%x\n", pdes->map_conv_off_base_c);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_pic_height_y 0x%x\n", pdes->map_conv_off_stride.map_conv_pic_height_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_pic_height_c 0x%x\n", pdes->map_conv_off_stride.map_conv_pic_height_c);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_base_y 0x%x\n", pdes->map_conv_comp_base_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_base_c 0x%x\n", pdes->map_conv_comp_base_c);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_stride_y 0x%x\n", pdes->map_conv_comp_stride.map_conv_comp_stride_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_stride_c 0x%x\n", pdes->map_conv_comp_stride.map_conv_comp_stride_c);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_crop_st_y 0x%x\n", pdes->map_conv_crop_pos.map_conv_crop_st_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_crop_st_x 0x%x\n", pdes->map_conv_crop_pos.map_conv_crop_st_x);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_crop_height 0x%x\n", pdes->map_conv_crop_size.map_conv_crop_height);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_crop_width 0x%x\n", pdes->map_conv_crop_size.map_conv_crop_width);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_out_mode_y 0x%x\n", pdes->map_conv_out_ctrl.map_conv_out_mode_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_out_mode_c 0x%x\n", pdes->map_conv_out_ctrl.map_conv_out_mode_c);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_hsync_period 0x%x\n", pdes->map_conv_out_ctrl.map_conv_hsync_period);

    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_bit_depth_y 0x%x\n", pdes->map_conv_time_dep_end.map_conv_bit_depth_y);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_bit_depth_c 0x%x\n", pdes->map_conv_time_dep_end.map_conv_bit_depth_c);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_offset_endian 0x%x\n", pdes->map_conv_time_dep_end.map_conv_offset_endian);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_comp_endian 0x%x\n", pdes->map_conv_time_dep_end.map_conv_comp_endian);
    bmlib_log("BMCV VPP DUMP", BMLIB_LOG_ERROR, " map_conv_time_out_cnt 0x%x\n", pdes->map_conv_time_dep_end.map_conv_time_out_cnt);
  }

}

static bm_status_t input_format_match(bm_image *input,uint8* input_format)
{
  bm_status_t ret = BM_SUCCESS;
  switch(input->image_format)
  {
    case FORMAT_GRAY:
      *input_format = IN_YUV400P;
      break;
    case FORMAT_YUV420P:
      *input_format = IN_YUV420P;
      break;
    case FORMAT_YUV422P:
      *input_format = IN_YUV422P;
      break;
    case FORMAT_YUV444P:
      *input_format = IN_YUV444P;
      break;
    case FORMAT_NV12:
      *input_format = IN_NV12;
      break;
    case FORMAT_NV21:
      *input_format = IN_NV21;
      break;
    case FORMAT_NV16:
      *input_format = IN_NV16;
      break;
    case FORMAT_NV61:
      *input_format = IN_NV61;
      break;
    case FORMAT_YUV444_PACKED:
      *input_format = IN_YUV444_PACKET;
      break;
    case FORMAT_YVU444_PACKED:
      *input_format = IN_YVU444_PACKET;
      break;
    case FORMAT_YUV422_YUYV:
      *input_format = IN_YUV422_YUYV;
      break;
    case FORMAT_YUV422_YVYU:
      *input_format = IN_YUV422_YVYU;
      break;
    case FORMAT_YUV422_UYVY:
      *input_format = IN_YUV422_UYVY;
      break;
    case FORMAT_YUV422_VYUY:
      *input_format = IN_YUV422_VYUY;
      break;

    case FORMAT_RGB_PLANAR:
    case FORMAT_BGR_PLANAR:
    case FORMAT_RGBP_SEPARATE:
    case FORMAT_BGRP_SEPARATE:
      *input_format = IN_RGBP;
      break;
    case FORMAT_RGB_PACKED:
      *input_format = IN_RGB24;
      break;
    case FORMAT_BGR_PACKED:
      *input_format = IN_BGR24;
      break;
    case FORMAT_COMPRESSED:
      *input_format = IN_FBC;
      break;
    default:
      ret = BM_NOT_SUPPORTED;
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "1684x vpp input format not support %s: %s: %d\n",
        __FILE__, __func__, __LINE__);
      break;
}

  return ret;
}

static bm_status_t output_format_match(bm_image *output,uint8* output_format)
{
  bm_status_t ret = BM_SUCCESS;

  switch(output->image_format)
  {
    case FORMAT_GRAY:
      *output_format = OUT_YUV400P;
      break;
    case FORMAT_YUV420P:
      *output_format = OUT_YUV420P;
      break;
    case FORMAT_YUV444P:
      *output_format = OUT_YUV444P;
      break;
    case FORMAT_NV12:
      *output_format = OUT_NV12;
      break;
    case FORMAT_NV21:
      *output_format = OUT_NV21;
      break;
    case FORMAT_RGBYP_PLANAR:
      *output_format = OUT_RGBYP;
      break;
    case FORMAT_RGB_PLANAR:
    case FORMAT_BGR_PLANAR:
    case FORMAT_RGBP_SEPARATE:
    case FORMAT_BGRP_SEPARATE:
      *output_format = OUT_RGBP;
      break;
    case FORMAT_RGB_PACKED:
      *output_format = OUT_RGB24;
      break;
    case FORMAT_BGR_PACKED:
      *output_format = OUT_BGR24;
      break;
    case FORMAT_HSV180_PACKED:
      *output_format = OUT_HSV180;
       break;
    case FORMAT_HSV256_PACKED:
      *output_format = OUT_HSV256;
       break;
    default:
      ret = BM_NOT_SUPPORTED;
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "1684x vpp output format not support %s: %s: %d\n",
        __FILE__, __func__, __LINE__);
      break;
  }

  return ret;
}

static bm_status_t default_csc_type(bm1684x_vpp_mat *input, bm1684x_vpp_mat *output, csc_type_1684x_t *csc_mode)
{
  bm_status_t ret = BM_SUCCESS;
  bm1686_vpp_color_space color_space_in = COLOR_IN_YUV, color_space_out = COLOR_OUT_YUV;
  switch(input->format)
  {
  case IN_YUV400P:
  case IN_YUV420P:
  case IN_YUV422P:
  case IN_YUV444P:
  case IN_NV12:
  case IN_NV21:
  case IN_NV16:
  case IN_NV61:
  case IN_YUV444_PACKET:
  case IN_YVU444_PACKET:
  case IN_YUV422_YUYV:
  case IN_YUV422_YVYU:
  case IN_YUV422_UYVY:
  case IN_YUV422_VYUY:
  case IN_FBC:
    color_space_in = COLOR_IN_YUV;
    break;
  case IN_RGBP:
  case IN_RGB24:
  case IN_BGR24:
    color_space_in = COLOR_IN_RGB;
    break;
  default:
    ret = BM_NOT_SUPPORTED;
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "1684x vpp input format not support %s: %s: %d\n",
      __FILE__, __func__, __LINE__);
    break;
}

    switch(output->format)
    {
    case OUT_YUV400P:
    case OUT_YUV420P:
    case OUT_YUV444P:
    case OUT_NV12:
    case OUT_NV21:
      color_space_out = COLOR_OUT_YUV;
      break;
    case OUT_RGBYP:
      if(color_space_in == COLOR_IN_YUV)
        color_space_out = COLOR_OUT_RGB;

      if(color_space_in == COLOR_IN_RGB)
        color_space_out = COLOR_OUT_YUV;

      break;
   case OUT_RGBP:
   case OUT_RGB24:
   case OUT_BGR24:
   case OUT_HSV180:
   case OUT_HSV256:
      color_space_out = COLOR_OUT_RGB;
      break;

    default:
      ret = BM_NOT_SUPPORTED;
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "1684x vpp output format not support %s: %s: %d\n",
        __FILE__, __func__, __LINE__);
      break;
  }

  if((color_space_in == COLOR_IN_YUV) && (color_space_out = COLOR_OUT_RGB))
    *csc_mode = YCbCr2RGB_BT601;

  if((color_space_in == COLOR_IN_RGB) && (color_space_out = COLOR_OUT_YUV))
    *csc_mode = RGB2YCbCr_BT601;

  return ret;
}


static bm_status_t bm_image_to_1684x_vpp_input_mat(
  bm_handle_t      handle,
  bm_image*        input,
  bm1684x_vpp_mat* mat,
  bmcv_rect_t*     crop_rect_
)
{
  bm_device_mem_t mem[4];
  int plane_num = 0, k = 0;
  uint64 pa[4] = {0};
  layout::plane_layout memory_layout[4];
  bm_status_t ret = BM_SUCCESS;
  UNUSED(handle);

  memset(mat, 0, sizeof(bm1684x_vpp_mat));

  bm_image_get_device_mem(*input, mem);
  plane_num = bm_image_get_plane_num(*input);
  for(k = 0; k < plane_num; k++)
  {
#ifdef USING_CMODEL
    ret= bm_mem_mmap_device_mem(handle, &mem[k], &pa[k]);
#else
    pa[k] = bm_mem_get_device_addr(mem[k]);
#endif
  }

  {
    std::lock_guard<std::mutex> lock(input->image_private->memory_lock);
    for (k = 0; k < input->image_private->plane_num; k++)
    {
      memory_layout[k] = input->image_private->memory_layout[k];
    }
  }

  ret = input_format_match(input, &(mat->format));
  if(BM_SUCCESS != ret ) {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "1684x vppinput not support this format %s: %s: %d\n",
              filename(__FILE__), __func__, __LINE__);
  //  return BM_NOT_SUPPORTED;
  }
  mat->frm_w  = input->width;
  mat->frm_h  = input->height;
  mat->axisX  = crop_rect_->start_x;
  mat->axisY  = crop_rect_->start_y;
  mat->cropW  = crop_rect_->crop_w;
  mat->cropH  = crop_rect_->crop_h;

  if(FORMAT_BGRP_SEPARATE == input->image_format)
  {
    mat->addr0  = pa[2];
    mat->addr1  = pa[1];
    mat->addr2  = pa[0];
    mat->addr3  = 0;
  } else if (FORMAT_RGB_PLANAR == input->image_format)
  {
    mat->addr0  = pa[0];
    mat->addr1  = pa[0] + memory_layout[0].channel_stride;
    mat->addr2  = pa[0] + memory_layout[0].channel_stride + memory_layout[0].channel_stride;
    mat->addr3  = 0;
  } else if (FORMAT_BGR_PLANAR == input->image_format)
  {
    mat->addr0  = pa[0] + memory_layout[0].channel_stride + memory_layout[0].channel_stride;
    mat->addr1  = pa[0] + memory_layout[0].channel_stride;
    mat->addr2  = pa[0];
    mat->addr3  = 0;
  }else
  {
    mat->addr0  = pa[0];
    mat->addr1  = pa[1];
    mat->addr2  = pa[2];
    mat->addr3  = pa[3];
  }

  if((FORMAT_RGB_PLANAR == input->image_format) || (FORMAT_BGR_PLANAR == input->image_format))
  {
    mat->stride_ch0 = memory_layout[0].pitch_stride;
    mat->stride_ch1 = mat->stride_ch0;
    mat->stride_ch2 = mat->stride_ch0;
    mat->stride_ch3 =0;
  }
  else
  {
    mat->stride_ch0 = memory_layout[0].pitch_stride;
    mat->stride_ch1 = memory_layout[1].pitch_stride;
    mat->stride_ch2 = memory_layout[2].pitch_stride;
    mat->stride_ch3 = memory_layout[3].pitch_stride;
  }
  mat->wdma_form = 1;

  return ret;
}
/*Due to the problem of IC hardware, the stride needs to be multiplied by 4 at fp32 and 2 at fp16*/
static bm_status_t bm_image_to_1684x_vpp_output_mat(
  bm_handle_t       handle,
  bm_image*         output,
  bm1684x_vpp_mat*  mat,
  bmcv_rect_t*      crop_rect_
)
{
  bm_device_mem_t mem[4];
  int plane_num = 0, k = 0;
  uint64 pa[4] = {0};
  layout::plane_layout memory_layout[4];
  bm_status_t ret = BM_SUCCESS;
  UNUSED(handle);

  memset(mat, 0, sizeof(bm1684x_vpp_mat));

  bm_image_get_device_mem(*output, mem);
  plane_num = bm_image_get_plane_num(*output);
  for(k = 0; k < plane_num; k++)
  {
#ifdef USING_CMODEL
    ret= bm_mem_mmap_device_mem(handle, &mem[k], &pa[k]);
#else
    pa[k] = bm_mem_get_device_addr(mem[k]);
#endif
  }


  {
    std::lock_guard<std::mutex> lock(output->image_private->memory_lock);
    for (k = 0; k < output->image_private->plane_num; k++)
    {
      memory_layout[k] = output->image_private->memory_layout[k];
    }
  }

  ret = output_format_match(output,&(mat->format));
  if(BM_SUCCESS != ret ) {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "1684x vppinput not support this format %s: %s: %d\n",
              filename(__FILE__), __func__, __LINE__);
  //  return BM_NOT_SUPPORTED;
  }

  mat->frm_w  = output->width;
  mat->frm_h  = output->height;
  mat->axisX  = crop_rect_->start_x;
  mat->axisY  = crop_rect_->start_y;
  mat->cropW  = crop_rect_->crop_w;
  mat->cropH  = crop_rect_->crop_h;
  if(FORMAT_BGRP_SEPARATE == output->image_format)
  {
    mat->addr0  = pa[2];
    mat->addr1  = pa[1];
    mat->addr2  = pa[0];
    mat->addr3  = 0;
  } else if (FORMAT_RGB_PLANAR == output->image_format)
  {
    mat->addr0  = pa[0];
    mat->addr1  = pa[0] + memory_layout[0].channel_stride;
    mat->addr2  = pa[0] + memory_layout[0].channel_stride + memory_layout[0].channel_stride;
    mat->addr3  = 0;
  } else if (FORMAT_BGR_PLANAR == output->image_format)
  {
    mat->addr0  = pa[0] + memory_layout[0].channel_stride + memory_layout[0].channel_stride;
    mat->addr1  = pa[0] + memory_layout[0].channel_stride;
    mat->addr2  = pa[0];
    mat->addr3  = 0;
  }else
  {
    mat->addr0  = pa[0];
    mat->addr1  = pa[1];
    mat->addr2  = pa[2];
    mat->addr3  = pa[3];
  }

  if((FORMAT_RGB_PLANAR == output->image_format) || (FORMAT_BGR_PLANAR == output->image_format))
  {
    mat->stride_ch0 = memory_layout[0].pitch_stride;
    mat->stride_ch1 = mat->stride_ch0;
    mat->stride_ch2 = mat->stride_ch0;
    mat->stride_ch3 =0;
  }
  else
  {
    mat->stride_ch0 = memory_layout[0].pitch_stride;
    mat->stride_ch1 = memory_layout[1].pitch_stride;
    mat->stride_ch2 = memory_layout[2].pitch_stride;
    mat->stride_ch3 = memory_layout[3].pitch_stride;
  }
  switch(output->data_type)
  {
    case DATA_TYPE_EXT_1N_BYTE_SIGNED:
      mat->wdma_form = DATA_TYPE_1N_BYTE_SIGNED;
      break;
    case DATA_TYPE_EXT_1N_BYTE:
      mat->wdma_form = DATA_TYPE_1N_BYTE;
      break;
    case DATA_TYPE_EXT_FP16:
      mat->wdma_form = DATA_TYPE_FP16;
      break;
    case DATA_TYPE_EXT_BF16:
      mat->wdma_form = DATA_TYPE_BF16;
      break;
    case DATA_TYPE_EXT_FLOAT32:
      mat->wdma_form = DATA_TYPE_FLOAT32;
      break;
    default:
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "1684x vpp output data type wrong %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      ret = BM_ERR_PARAM;
      break;
  }
 return ret;
}

bm_status_t vpp_algorithm_config(
  bmcv_resize_algorithm  algorithm,
  bm1684x_vpp_param*      vpp_param)
{
  bm_status_t ret = BM_SUCCESS;

  switch(algorithm) {
  case BMCV_INTER_NEAREST:
    vpp_param->resize_param.filter_sel = 1;
    vpp_param->resize_param.scl_init_x = 0;
    vpp_param->resize_param.scl_init_y = 0;
    break;
  case BMCV_INTER_LINEAR:
    vpp_param->resize_param.filter_sel = 0;
    vpp_param->resize_param.scl_init_x = 1;
    vpp_param->resize_param.scl_init_y = 1;
    break;
  default:
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "1684x vpp not support  algorithm %d ,%s: %s: %d\n",
      algorithm,filename(__FILE__), __func__, __LINE__);
    ret = BM_NOT_SUPPORTED;
    break;
  }
  return ret;
}
static bm_status_t bm1684x_vpp_border_font_config(
  int                   frame_number,
  border_t*             border_param,
  font_t*               font_param,
  bm1684x_vpp_param*    vpp_param,
  bmcv_convert_to_attr* convert_to_attr)
{
  int i = 0;
  if (border_param != NULL)
  {
    for(i = 0; i < frame_number; i++)
      memcpy(&vpp_param[i].border_param, &border_param[i], sizeof(border_t));
  }
  if (font_param != NULL)
  {
    for(i = 0; i < frame_number; i++)
      memcpy(&vpp_param[i].font_param, &font_param[i], sizeof(font_t));
  }
  if (convert_to_attr != NULL) {
    for(i = 0; i < frame_number; i++){
      vpp_param[i].conbri_shifting_param.con_bri_enable = 1;
      vpp_param[i].conbri_shifting_param.con_bri_a_0 = convert_to_attr[i].alpha_0;
      vpp_param[i].conbri_shifting_param.con_bri_a_1 = convert_to_attr[i].alpha_1;
      vpp_param[i].conbri_shifting_param.con_bri_a_2 = convert_to_attr[i].alpha_2;
      vpp_param[i].conbri_shifting_param.con_bri_b_0 = convert_to_attr[i].beta_0;
      vpp_param[i].conbri_shifting_param.con_bri_b_1 = convert_to_attr[i].beta_1;
      vpp_param[i].conbri_shifting_param.con_bri_b_2 = convert_to_attr[i].beta_2;
    }
  }
  return BM_SUCCESS;
}

static bm_status_t bm1684x_vpp_param_config(
  bmcv_resize_algorithm   algorithm,
  csc_matrix_t *          matrix,
  csc_type_t              csc_type,
  post_padding_t *        post_padding_param,
  bm1684x_vpp_param*      vpp_param
)
{

  bm_status_t ret = BM_SUCCESS;

  memset(vpp_param, 0, sizeof(bm1684x_vpp_param));
  vpp_algorithm_config(algorithm, vpp_param);

  if((csc_type == CSC_USER_DEFINED_MATRIX) && (matrix != NULL))
  {
    memcpy(&vpp_param->csc_matrix.csc_coe00,&matrix->csc_coe00,4);
    memcpy(&vpp_param->csc_matrix.csc_coe01,&matrix->csc_coe01,4);
    memcpy(&vpp_param->csc_matrix.csc_coe02,&matrix->csc_coe02,4);
    memcpy(&vpp_param->csc_matrix.csc_add0, &matrix->csc_add0, 4);
    memcpy(&vpp_param->csc_matrix.csc_coe10,&matrix->csc_coe10,4);
    memcpy(&vpp_param->csc_matrix.csc_coe11,&matrix->csc_coe11,4);
    memcpy(&vpp_param->csc_matrix.csc_coe12,&matrix->csc_coe12,4);
    memcpy(&vpp_param->csc_matrix.csc_add1, &matrix->csc_add1, 4);
    memcpy(&vpp_param->csc_matrix.csc_coe20,&matrix->csc_coe20,4);
    memcpy(&vpp_param->csc_matrix.csc_coe21,&matrix->csc_coe21,4);
    memcpy(&vpp_param->csc_matrix.csc_coe22,&matrix->csc_coe22,4);
    memcpy(&vpp_param->csc_matrix.csc_add2, &matrix->csc_add2, 4);
  }else
  {
    vpp_param->csc_mode = (csc_type_1684x_t)csc_type;
  }

  if (post_padding_param != NULL) {
    memcpy(&vpp_param->post_padding_param, post_padding_param, sizeof(post_padding_t));
  }

  return ret;
}

static bm_status_t bm1684x_vpp_misc(
  bm_handle_t         handle,
  int                 frame_number,
  bm1684x_vpp_mat     *vpp_input,
  bm1684x_vpp_mat     *vpp_output,
  bm1684x_vpp_param   *vpp_param,
  int                  cmodel_flag
)
{
  bm_status_t ret = BM_SUCCESS;
  struct vpp_batch_n batch;
  descriptor *pdes = NULL;
  int vpp_dev_fd = -1, idx = 0;
  csc_type_1684x_t    csc_mode = YCbCr2RGB_BT601;
  u32 src_width, src_height, height;
  u64 map_conv_off_base_y = 0, map_conv_off_base_c = 0, map_comp_base_y = 0, map_comp_base_c = 0;

  batch.num = frame_number;
  batch.cmd = new descriptor [batch.num * (sizeof(descriptor))];
  if (batch.cmd == NULL) {
    ret = BM_ERR_NOMEM;
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "vpp malloc failed %s: %s: %d\n",
      __FILE__, __func__, __LINE__);
    return ret;
  }

  memset(batch.cmd,0,(batch.num * (sizeof(descriptor))));
  for (idx = 0; idx < batch.num; idx++) {

    default_csc_type(vpp_input,vpp_output,&csc_mode);

    pdes = (batch.cmd + idx);
    if (idx == (batch.num - 1)) {
      pdes->des_head.crop_flag = 1;
    }
    else {
      pdes->des_head.crop_flag = 0;
    }
    pdes->des_head.crop_id = idx;

    pdes->src_ctrl.input_format        = vpp_input[idx].format;
    pdes->src_ctrl.post_padding_enable = vpp_param[idx].post_padding_param.post_padding_enable;
    pdes->src_ctrl.padding_enable      = 0;
    pdes->src_ctrl.rect_border_enable  = vpp_param[idx].border_param.rect_border_enable;
    pdes->src_ctrl.font_enable         = vpp_param[idx].font_param.font_enable;
    pdes->src_ctrl.con_bri_enable      = vpp_param[idx].conbri_shifting_param.con_bri_enable;
    pdes->src_ctrl.csc_scale_order     = 0;
    pdes->src_ctrl.wdma_form           = vpp_output[idx].wdma_form;;

    pdes->src_size.src_width  = vpp_input[idx].frm_w;
    pdes->src_size.src_height = vpp_input[idx].frm_h;

    pdes->src_crop_size.src_crop_height = vpp_input[idx].cropH;
    pdes->src_crop_size.src_crop_width  = vpp_input[idx].cropW;

    pdes->src_crop_st.src_crop_st_x = vpp_input[idx].axisX;
    pdes->src_crop_st.src_crop_st_y = vpp_input[idx].axisY;

    pdes->src_base_ext.src_base_ext_ch0 = vpp_input[idx].addr0>>32;
    pdes->src_base_ext.src_base_ext_ch1 = vpp_input[idx].addr1>>32;
    pdes->src_base_ext.src_base_ext_ch2 = vpp_input[idx].addr2>>32;

    pdes->src_base_ch0 = vpp_input[idx].addr0 & 0xffffffff;
    pdes->src_base_ch1 = vpp_input[idx].addr1 & 0xffffffff;
    pdes->src_base_ch2 = vpp_input[idx].addr2 & 0xffffffff;
    pdes->src_stride_ch0.src_stride_ch0 = vpp_input[idx].stride_ch0;
    pdes->src_stride_ch1.src_stride_ch1 = vpp_input[idx].stride_ch1;
    pdes->src_stride_ch2.src_stride_ch2 = vpp_input[idx].stride_ch2;

    pdes->padding_value.padding_value_ch0 = 0;
    pdes->padding_value.padding_value_ch1 = 0;
    pdes->padding_value.padding_value_ch2 = 0;
    pdes->padding_ext.padding_ext_up = 0;
    pdes->padding_ext.padding_ext_bot = 0;
    pdes->padding_ext.padding_ext_left = 0;
    pdes->padding_ext.padding_ext_right = 0;

    pdes->src_border_value.src_border_value_r = vpp_param[idx].border_param.value_r;
    pdes->src_border_value.src_border_value_g = vpp_param[idx].border_param.value_g;
    pdes->src_border_value.src_border_value_b = vpp_param[idx].border_param.value_b;
    pdes->src_border_value.src_border_thickness = vpp_param[idx].border_param.thickness;
    pdes->src_border_size.src_border_height = vpp_param[idx].border_param.height;
    pdes->src_border_size.src_border_width = vpp_param[idx].border_param.width;
    pdes->src_border_st.src_border_st_y = vpp_param[idx].border_param.st_y;
    pdes->src_border_st.src_border_st_x = vpp_param[idx].border_param.st_x;

    pdes->src_font_pitch.src_font_pitch = vpp_param[idx].font_param.font_pitch;
    pdes->src_font_pitch.src_font_type = vpp_param[idx].font_param.font_type;
    pdes->src_font_pitch.src_font_nf_range = vpp_param[idx].font_param.font_nf_range;
    pdes->src_font_pitch.src_font_dot_en = vpp_param[idx].font_param.font_dot_en;
    pdes->src_font_value.src_font_value_r = vpp_param[idx].font_param.font_value_r;
    pdes->src_font_value.src_font_value_g = vpp_param[idx].font_param.font_value_g;
    pdes->src_font_value.src_font_value_b = vpp_param[idx].font_param.font_value_b;
    pdes->src_font_value.src_font_ext = vpp_param[idx].font_param.font_addr >> 32;
    pdes->src_font_size.src_font_height = vpp_param[idx].font_param.font_height;
    pdes->src_font_size.src_font_width = vpp_param[idx].font_param.font_width;
    pdes->src_font_st.src_font_st_x = vpp_param[idx].font_param.font_st_x;
    pdes->src_font_st.src_font_st_y = vpp_param[idx].font_param.font_st_y;
    pdes->src_font_addr = vpp_param[idx].font_param.font_addr & 0xffffffff;

    pdes->dst_ctrl.output_format = vpp_output[idx].format;
    pdes->dst_ctrl.cb_coeff_sel_tl = 3;
    pdes->dst_ctrl.cb_coeff_sel_tr = 0;
    pdes->dst_ctrl.cb_coeff_sel_bl = 0;
    pdes->dst_ctrl.cb_coeff_sel_br = 0;
    pdes->dst_ctrl.cr_coeff_sel_tl = 3;
    pdes->dst_ctrl.cr_coeff_sel_tr = 0;
    pdes->dst_ctrl.cr_coeff_sel_bl = 0;
    pdes->dst_ctrl.cr_coeff_sel_br = 0;

    pdes->dst_crop_size.dst_crop_height = vpp_output[idx].cropH;
    pdes->dst_crop_size.dst_crop_width  = vpp_output[idx].cropW;

    pdes->dst_crop_st.dst_crop_st_x = vpp_output[idx].axisX;
    pdes->dst_crop_st.dst_crop_st_y = vpp_output[idx].axisY;

    pdes->dst_base_ext.dst_base_ext_ch0 = vpp_output[idx].addr0>>32;
    pdes->dst_base_ext.dst_base_ext_ch1 = vpp_output[idx].addr1>>32;
    pdes->dst_base_ext.dst_base_ext_ch2 = vpp_output[idx].addr2>>32;
    pdes->dst_base_ext.dst_base_ext_ch3 = vpp_output[idx].addr3>>32;

    pdes->dst_base_ch0 = vpp_output[idx].addr0 & 0xffffffff;
    pdes->dst_base_ch1 = vpp_output[idx].addr1 & 0xffffffff;
    pdes->dst_base_ch2 = vpp_output[idx].addr2 & 0xffffffff;
    pdes->dst_base_ch3 = vpp_output[idx].addr3 & 0xffffffff;

    pdes->dst_stride_ch0.dst_stride_ch0 = vpp_output[idx].stride_ch0;
    pdes->dst_stride_ch1.dst_stride_ch1 = vpp_output[idx].stride_ch1;
    pdes->dst_stride_ch2.dst_stride_ch2 = vpp_output[idx].stride_ch2;
    pdes->dst_stride_ch3.dst_stride_ch3 = vpp_output[idx].stride_ch3;

    if(vpp_param->csc_mode == USER_DEFINED_MATRIX)
    {
      pdes->csc_coe00 = vpp_param[idx].csc_matrix.csc_coe00;
      pdes->csc_coe01 = vpp_param[idx].csc_matrix.csc_coe01;
      pdes->csc_coe02 = vpp_param[idx].csc_matrix.csc_coe02;
      pdes->csc_add0  = vpp_param[idx].csc_matrix.csc_add0;
      pdes->csc_coe10 = vpp_param[idx].csc_matrix.csc_coe10;
      pdes->csc_coe11 = vpp_param[idx].csc_matrix.csc_coe11;
      pdes->csc_coe12 = vpp_param[idx].csc_matrix.csc_coe12;
      pdes->csc_add1  = vpp_param[idx].csc_matrix.csc_add1;
      pdes->csc_coe20 = vpp_param[idx].csc_matrix.csc_coe20;
      pdes->csc_coe21 = vpp_param[idx].csc_matrix.csc_coe21;
      pdes->csc_coe22 = vpp_param[idx].csc_matrix.csc_coe22;
      pdes->csc_add2  = vpp_param[idx].csc_matrix.csc_add2;
    }else
    {
      if(vpp_param->csc_mode != CSC_MAX)
        csc_mode = vpp_param->csc_mode;

      pdes->csc_coe00 = bm1684x_csc_matrix[csc_mode][0];
      pdes->csc_coe01 = bm1684x_csc_matrix[csc_mode][1];
      pdes->csc_coe02 = bm1684x_csc_matrix[csc_mode][2];
      pdes->csc_add0  = bm1684x_csc_matrix[csc_mode][3];
      pdes->csc_coe10 = bm1684x_csc_matrix[csc_mode][4];
      pdes->csc_coe11 = bm1684x_csc_matrix[csc_mode][5];
      pdes->csc_coe12 = bm1684x_csc_matrix[csc_mode][6];
      pdes->csc_add1  = bm1684x_csc_matrix[csc_mode][7];
      pdes->csc_coe20 = bm1684x_csc_matrix[csc_mode][8];
      pdes->csc_coe21 = bm1684x_csc_matrix[csc_mode][9];
      pdes->csc_coe22 = bm1684x_csc_matrix[csc_mode][10];
      pdes->csc_add2  = bm1684x_csc_matrix[csc_mode][11];
    }

    pdes->con_bri_a_0 = vpp_param[idx].conbri_shifting_param.con_bri_a_0;
    pdes->con_bri_a_1 = vpp_param[idx].conbri_shifting_param.con_bri_a_1;
    pdes->con_bri_a_2 = vpp_param[idx].conbri_shifting_param.con_bri_a_2;
    pdes->con_bri_b_0 = vpp_param[idx].conbri_shifting_param.con_bri_b_0;
    pdes->con_bri_b_1 = vpp_param[idx].conbri_shifting_param.con_bri_b_1;
    pdes->con_bri_b_2 = vpp_param[idx].conbri_shifting_param.con_bri_b_2;
    pdes->con_bri_ctrl.con_bri_round = 1;
    pdes->con_bri_ctrl.con_bri_full = 0;
    pdes->con_bri_ctrl.hr_csc_f2i_round = 1;
    pdes->con_bri_ctrl.hr_csc_i2f_round = 1;

    pdes->post_padding_value_ch0 = vpp_param[idx].post_padding_param.padding_ch0;
    pdes->post_padding_value_ch1 = vpp_param[idx].post_padding_param.padding_ch1;
    pdes->post_padding_value_ch2 = vpp_param[idx].post_padding_param.padding_ch2;
    pdes->post_padding_value_ch3 = vpp_param[idx].post_padding_param.padding_ch3;
    pdes->post_padding_ext.post_padding_ext_up    = vpp_param[idx].post_padding_param.top;
    pdes->post_padding_ext.post_padding_ext_bot   = vpp_param[idx].post_padding_param.bottom;
    pdes->post_padding_ext.post_padding_ext_left  = vpp_param[idx].post_padding_param.left;
    pdes->post_padding_ext.post_padding_ext_right = vpp_param[idx].post_padding_param.right;

    pdes->scl_ctrl.scl_ctrl   = vpp_param[idx].resize_param.filter_sel;
    pdes->scl_init.scl_init_x = vpp_param[idx].resize_param.scl_init_x;
    pdes->scl_init.scl_init_y = vpp_param[idx].resize_param.scl_init_y;

    pdes->scl_x = (float)(pdes->src_crop_size.src_crop_width+ pdes->padding_ext.padding_ext_left + pdes->padding_ext.padding_ext_right) / (float)(pdes->dst_crop_size.dst_crop_width- pdes->post_padding_ext.post_padding_ext_left - pdes->post_padding_ext.post_padding_ext_right);
    pdes->scl_y = (float)(pdes->src_crop_size.src_crop_height + pdes->padding_ext.padding_ext_up + pdes->padding_ext.padding_ext_bot) / (float)(pdes->dst_crop_size.dst_crop_height - pdes->post_padding_ext.post_padding_ext_up - pdes->post_padding_ext.post_padding_ext_bot);

    if(pdes->src_ctrl.input_format == 31)
    {
  //    VppAssert(pdes->MAP_CONV_CROP_SIZE.map_conv_crop_width%16 ==0);
  //    VppAssert(pdes->MAP_CONV_CROP_SIZE.map_conv_crop_height%4 ==0);
      /*offset y ,comp y, offset c, comp c*/

      pdes->src_ctrl.fbc_multi_map = 0;
      src_height = vpp_input[idx].frm_h;
      src_width  = vpp_input[idx].frm_w;
      height = ((src_height + 15) / 16) * 4 *16;

      map_conv_off_base_y = vpp_input[idx].addr0 + (pdes->src_crop_st.src_crop_st_x / 256) * height * 2 +
        (pdes->src_crop_st.src_crop_st_y / 4) * 2 * 16;
      map_conv_off_base_c = vpp_input[idx].addr2 + (pdes->src_crop_st.src_crop_st_x / 2 / 256) * height * 2 +
        (pdes->src_crop_st.src_crop_st_y / 2 / 2) * 2 * 16;
      map_comp_base_y = vpp_input[idx].addr1 + (pdes->src_crop_st.src_crop_st_y / 4)* VPPALIGN(VPPALIGN(src_width, 16) * 4, 32);
      map_comp_base_c = vpp_input[idx].addr3 + (pdes->src_crop_st.src_crop_st_y / 2 / 2) * VPPALIGN(VPPALIGN(src_width / 2, 16) * 4, 32);

      pdes->map_conv_ext.map_conv_off_base_ext_y  = (map_conv_off_base_y >> 32) & 0xff;
      pdes->map_conv_ext.map_conv_off_base_ext_c  = (map_conv_off_base_c >> 32) & 0xff;
      pdes->map_conv_ext.map_conv_comp_ext_y  =(map_comp_base_y >> 32) & 0xff;
      pdes->map_conv_ext.map_conv_comp_ext_c  =(map_comp_base_c >> 32) & 0xff;

      pdes->map_conv_off_base_y   = (u32)(map_conv_off_base_y & 0xfffffff0);
      pdes->map_conv_off_base_c   = (u32)(map_conv_off_base_c & 0xfffffff0);
      pdes->map_conv_off_stride.map_conv_pic_height_y = VPPALIGN(src_height, 16) & 0x3fff;
      pdes->map_conv_off_stride.map_conv_pic_height_c = VPPALIGN(src_height / 2, 8) & 0x3fff;
      pdes->map_conv_comp_base_y  = (u32)(map_comp_base_y & 0xfffffff0);
      pdes->map_conv_comp_base_c  = (u32)(map_comp_base_c & 0xfffffff0);
      pdes->map_conv_comp_stride.map_conv_comp_stride_y =(VPPALIGN(VPPALIGN(src_width, 16) * 4, 32))& 0xfffffff0;
      pdes->map_conv_comp_stride.map_conv_comp_stride_c =(VPPALIGN(VPPALIGN(src_width / 2, 16) * 4, 32))& 0xfffffff0;
      pdes->map_conv_crop_pos.map_conv_crop_st_x    = pdes->src_crop_st.src_crop_st_x & 0x3fff;
      pdes->map_conv_crop_pos.map_conv_crop_st_y    = pdes->src_crop_st.src_crop_st_y & 0x3fff;
      pdes->map_conv_crop_size.map_conv_crop_width  = pdes->src_crop_size.src_crop_width & 0x3fff;
      pdes->map_conv_crop_size.map_conv_crop_height = pdes->src_crop_size.src_crop_height & 0x3fff;

      /*lx_fbc big endian,  vpu  little endian*/
      pdes->map_conv_time_dep_end.map_conv_offset_endian = 0x0;
      pdes->map_conv_time_dep_end.map_conv_comp_endian = 0x0;
      pdes->map_conv_time_dep_end.map_conv_time_out_cnt = 0xfff;
    }

    pdes->src_fd0 = 0;
    pdes->src_fd1 = 0;
    pdes->src_fd2 = 0;
    pdes->src_fd3 = 0;

    pdes->dst_fd0 = 0;
    pdes->dst_fd1 = 0;
    pdes->dst_fd2 = 0;
    pdes->dst_fd3 = 0;

  }

  UNUSED(cmodel_flag);

#ifdef __linux__
#ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(vpp_dev_fd);

  bm1684x_vpp_cmodel(&batch, vpp_input, vpp_output, vpp_param);
#endif
#ifdef SOC_MODE
  if(cmodel_flag == 0)
  {
    ret = bm_get_handle_fd(handle, VPP_FD, &vpp_dev_fd);
    if ((ret != 0 ) || (vpp_dev_fd < 0)){
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "invalid vpp fd %s: %s: %d\n",
        __FILE__, __func__, __LINE__);
      delete[] batch.cmd;
      return ret;
    }

    if(0 != ioctl(vpp_dev_fd, VPP_UPDATE_BATCH_FD_PA, &batch))
    {
      printf("1684x vpp soc run failed\n");
      vpp1684x_dump(&batch);
      ret = BM_ERR_FAILURE;
    }
  } else if(cmodel_flag == 1)
  {
    bm1684x_vpp_cmodel(&batch, vpp_input, vpp_output,vpp_param);
  }
#endif

#ifdef BM_PCIE_MODE
    UNUSED(vpp_dev_fd);

    if(cmodel_flag == 0)
    {
      if(0 != bm_trigger_vpp(handle, &batch))
      {
        printf("1684x vpp pcie run failed\n");
        vpp1684x_dump(&batch);
        ret = BM_ERR_FAILURE;
      }
    }
    else if(cmodel_flag == 1)
    {
      bm1684x_vpp_cmodel(&batch, vpp_input, vpp_output,vpp_param);
    }

#endif
#endif

  delete[] batch.cmd;
  return ret;
}

static bm_status_t bm1684x_vpp_asic_and_cmodel(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr,
  border_t*               border_param,
  font_t*                 font_param,
  int                     cmodel_flag)
{
  bm_status_t ret = BM_SUCCESS;
  int i;

  bmcv_rect_t  src_crop_rect, dst_crop_rect;
  post_padding_t  post_padding_param;

  bm1684x_vpp_mat*   vpp_input  = new bm1684x_vpp_mat [frame_number];
  bm1684x_vpp_mat*   vpp_output = new bm1684x_vpp_mat [frame_number];
  bm1684x_vpp_param* vpp_param  = new bm1684x_vpp_param [frame_number];
  memset(vpp_input, 0, sizeof(bm1684x_vpp_mat) * frame_number);
  memset(vpp_output, 0, sizeof(bm1684x_vpp_mat) * frame_number);
  memset(vpp_param, 0, sizeof(bm1684x_vpp_param) * frame_number);

  for(i = 0; i< frame_number; i++)
  {
    memset(&post_padding_param, 0, sizeof(post_padding_t));
    if(NULL == input_crop_rect)
    {
      src_crop_rect.start_x = 0;
      src_crop_rect.start_y = 0;
      src_crop_rect.crop_w  = input[i].width;
      src_crop_rect.crop_h  = input[i].height;
    }
    else
    {
      src_crop_rect = input_crop_rect[i];
    }
    if(NULL == padding_attr)
    {
      dst_crop_rect.start_x = 0;
      dst_crop_rect.start_y = 0;
      dst_crop_rect.crop_w  = output[i].width;
      dst_crop_rect.crop_h  = output[i].height;
    }
    else
    {
      if (padding_attr[i].if_memset == 1)
      {

        post_padding_param.padding_ch0 = padding_attr[i].padding_r;
        post_padding_param.padding_ch1 = padding_attr[i].padding_g;
        post_padding_param.padding_ch2 = padding_attr[i].padding_b;
        post_padding_param.padding_ch3 = 0;
        post_padding_param.top         = padding_attr[i].dst_crop_sty;
        post_padding_param.bottom      = output[i].height - padding_attr[i].dst_crop_h - padding_attr[i].dst_crop_sty;
        post_padding_param.left        = padding_attr[i].dst_crop_stx;
        post_padding_param.right       = output[i].width - padding_attr[i].dst_crop_w - padding_attr[i].dst_crop_stx;
        if((post_padding_param.top == 0) && (post_padding_param.bottom == 0) &&(post_padding_param.left == 0) &&(post_padding_param.right == 0))
        {
          post_padding_param.post_padding_enable = 0;
        }
        else
        {
          post_padding_param.post_padding_enable = 1;
        }
      }
      dst_crop_rect.start_x = padding_attr[i].dst_crop_stx - post_padding_param.left;
      dst_crop_rect.start_y = padding_attr[i].dst_crop_sty - post_padding_param.top;
      dst_crop_rect.crop_w  = padding_attr[i].dst_crop_w + post_padding_param.left + post_padding_param.right;
      dst_crop_rect.crop_h  = padding_attr[i].dst_crop_h + post_padding_param.top + post_padding_param.bottom;
    }

    ret = bm_image_to_1684x_vpp_input_mat(handle, &input[i], &vpp_input[i], &src_crop_rect);
    ret = bm_image_to_1684x_vpp_output_mat(handle, &output[i], &vpp_output[i], &dst_crop_rect);
    ret = bm1684x_vpp_param_config(algorithm, matrix, csc_type, &post_padding_param, &vpp_param[i]);
  }
  ret = bm1684x_vpp_border_font_config(frame_number, border_param, font_param, vpp_param, convert_to_attr);

  if(BM_SUCCESS == bm1684x_check_vpp_internal_param(frame_number, vpp_input, vpp_output, vpp_param))
  {
    ret = bm1684x_vpp_misc(handle, frame_number, vpp_input, vpp_output, vpp_param, cmodel_flag);
  }

  delete [] vpp_param;
  delete [] vpp_output;
  delete [] vpp_input;
  return ret;

}

static bm_status_t bm1684x_vpp_multi_parameter_processing(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr,
  border_t*               border_param,
  font_t*                 font_param)
{
  bm_status_t ret = BM_SUCCESS;
  int i;

  auto free_dmem = [&]() {
    for (int i = 0; i < frame_number; i++) {
        bm_image_detach(output[i]);
    }
  };

  for (i = 0; i < frame_number; i++) {
    if (!bm_image_is_attached(output[i])) {
      if (bm_image_alloc_dev_mem(output[i], BMCV_HEAP_ANY) != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
          "output dev alloc fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
        free_dmem();
        return BM_ERR_FAILURE;
      }
    }
  }

  ret = check_bm1684x_vpp_param(
    frame_number, input, output, input_crop_rect, padding_attr, algorithm, csc_type, matrix, border_param);
  if(ret != BM_SUCCESS)
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "bm1684x vpp error parameters found,%s: %s: %d\n",
      filename(__FILE__), __func__, __LINE__);
    return ret;
  }

  ret = bm1684x_vpp_asic_and_cmodel(
    handle, frame_number, input, output, input_crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr,border_param, font_param, NOT_USE_CMODEL);

  return ret;
}


static bm_status_t check_bm1684x_convert_to_param(bm_image* input,
                                          bm_image* output,
                                          int input_num){
  for(int i=0; i<input_num; i++){
    if((input[0].image_format != input[i].image_format) || (output[0].image_format != output[i].image_format)){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "input and output different formats not supported, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
  }
  return BM_SUCCESS;
}

/*ax+b*/
bm_status_t bm1684x_vpp_convert_to(
  bm_handle_t          handle,
  int                  input_num,
  bmcv_convert_to_attr convert_to_attr,
  bm_image *           input,
  bm_image *           output)
{
  bm_status_t ret = BM_SUCCESS;

  ret = simple_check_bm1684x_input_param(handle, input, input_num);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  ret = check_bm1684x_vpp_continuity(input_num, input);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }
  ret = check_bm1684x_vpp_continuity(input_num, output);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  ret = check_bm1684x_convert_to_param(input, output, input_num);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  bmcv_convert_to_attr *convert_to_attr_inner = new bmcv_convert_to_attr [input_num];

  if(input->image_format == FORMAT_BGR_PLANAR){
    float exc = convert_to_attr.alpha_0;
    convert_to_attr.alpha_0 = convert_to_attr.alpha_2;
    convert_to_attr.alpha_2 = exc;
    exc = convert_to_attr.beta_0;
    convert_to_attr.beta_0 = convert_to_attr.beta_2;
    convert_to_attr.beta_2 = exc;
  }

  for(int i=0; i<input_num; i++)
  {
    memcpy(&convert_to_attr_inner[i], &convert_to_attr, sizeof(bmcv_convert_to_attr));
  }

  ret = bm1684x_vpp_multi_parameter_processing(
    handle, input_num, input, output, NULL, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, convert_to_attr_inner,NULL, NULL);

  delete [] convert_to_attr_inner;
  return ret;

}

static bm_status_t bm1684x_vpp_multi_input_multi_output(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix)
{
  bm_status_t ret = BM_SUCCESS;
  int i = 0;
  bmcv_convert_to_attr convert_to_attr;

  if(NULL != padding_attr)
  {
    for (i = 0; i < frame_number; i++)
    {
      if (padding_attr[i].if_memset == 1)
      {
        if ((padding_attr[i].dst_crop_stx > 255) || (padding_attr[i].dst_crop_sty > 255) ||
            (output[i].width - padding_attr[i].dst_crop_w - padding_attr[i].dst_crop_stx > 255) ||
            (output[i].height- padding_attr[i].dst_crop_h - padding_attr[i].dst_crop_sty > 255) )
        {
          convert_to_attr.alpha_0 = 0;
          convert_to_attr.alpha_1 = 0;
          convert_to_attr.alpha_2 = 0;
          convert_to_attr.beta_0 = padding_attr[i].padding_r;
          convert_to_attr.beta_1 = padding_attr[i].padding_g;
          convert_to_attr.beta_2 = padding_attr[i].padding_b;
          padding_attr[i].if_memset = 0;
          ret = bm1684x_vpp_convert_to(handle, 1,convert_to_attr, &input[i], &output[i]);
          if(ret != BM_SUCCESS)
          {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
              "bm1684x_vpp_convert_to error , %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            return BM_ERR_PARAM;
          }
        }
      }
    }
  }

  ret = bm1684x_vpp_multi_parameter_processing(
    handle, frame_number, input, output, crop_rect, padding_attr, algorithm, csc_type, matrix,NULL,NULL,NULL);

  return ret;

}

static bm_status_t bm1684x_vpp_single_input_multi_output(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image                input,
  bm_image*               output,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix)
{
  int i;
  bm_status_t ret = BM_SUCCESS;

  ret = check_bm1684x_vpp_continuity(frame_number, output);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }


  bm_image *input_inner = new bm_image [frame_number];

  for(i = 0; i< frame_number; i++)
  {
    input_inner[i] = input;
  }

  ret = bm1684x_vpp_multi_input_multi_output(
    handle, frame_number, input_inner, output, crop_rect, padding_attr, algorithm, csc_type, matrix);

  delete [] input_inner;
  return ret;
}

static bm_status_t bm1684x_vpp_multi_input_single_output(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image                output,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix)
{
  int i;
  bm_status_t ret = BM_SUCCESS;
  ret = check_bm1684x_vpp_continuity( frame_number, input);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  bm_image *output_inner = new bm_image [frame_number];

  for(i = 0; i< frame_number; i++)
  {
    output_inner[i] = output;
  }

  ret = bm1684x_vpp_multi_input_multi_output(
    handle, frame_number, input, output_inner, crop_rect, padding_attr, algorithm, csc_type, matrix);

  delete [] output_inner;

  return ret;
}

bm_status_t bm1684x_vpp_compressed2yuv(
  bm_handle_t           handle,
  int                   output_num,
  bm_image*             input,
  bm_image*             input_temp,
  csc_matrix_t*         matrix,
  bmcv_resize_algorithm algorithm,
  bmcv_rect_t *         crop_rect,
  int*                  compressed_flag)
{
  bm_status_t ret = BM_SUCCESS;
  int i = 0;
  for (i = 0; i < output_num; i++)
  {
    if(NULL != crop_rect)
    {
      if((input->image_format == FORMAT_COMPRESSED) && ((crop_rect[i].crop_w % 16 != 0) ||
       (crop_rect[i].crop_h % 4 != 0) || (crop_rect[i].start_x % 32 != 0) || (crop_rect[i].start_y % 2 != 0)))
      {
        *compressed_flag = 1;
        break;
      }
    }
  }

  if(1 == *compressed_flag)
  {
    bm_image_create(handle, input->height, input->width, FORMAT_YUV420P, DATA_TYPE_EXT_1N_BYTE, input_temp);
    if(BM_SUCCESS != bm_image_alloc_dev_mem(input_temp[0])) {
      BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
      return BM_ERR_FAILURE;
    }

    ret = bm1684x_vpp_single_input_multi_output(handle, 1, input[0], input_temp, NULL, NULL, algorithm, CSC_MAX_ENUM, matrix);
    if(ret != BM_SUCCESS)
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "bm1684x_vpp_single_input_multi_output error , %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
      return BM_ERR_FAILURE;
    }
    *input = *input_temp;
  }
  return ret;
}

bm_status_t bm1684x_vpp_csc_matrix_convert(
  bm_handle_t           handle,
  int                   output_num,
  bm_image              input,
  bm_image*             output,
  csc_type_t            csc,
  csc_matrix_t*         matrix,
  bmcv_resize_algorithm algorithm,
  bmcv_rect_t *         crop_rect)
{
  bm_status_t ret = BM_SUCCESS;

  ret = simple_check_bm1684x_input_param(handle, output, output_num);
  if(ret != BM_SUCCESS)
    return ret;

  int compressed_flag = 0;
  bm_image input_temp;
  ret = bm1684x_vpp_compressed2yuv(handle, output_num, &input, &input_temp, matrix, algorithm, crop_rect, &compressed_flag);
  if(ret != BM_SUCCESS)
    goto failed;

  ret = bm1684x_vpp_single_input_multi_output(
    handle, output_num, input, output, crop_rect, NULL, algorithm, csc, matrix);
failed:
  if(1 == compressed_flag)
  {
    bm_image_destroy(input_temp);
    input_temp.image_private = NULL;
  }
  return ret;
}

bm_status_t bm1684x_vpp_cvt_padding(
  bm_handle_t             handle,
  int                     output_num,
  bm_image                input,
  bm_image*               output,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_rect_t*            crop_rect,
  bmcv_resize_algorithm   algorithm,
  csc_matrix_t*           matrix)
{
  bm_status_t ret = BM_SUCCESS;
  csc_type_t csc_type = CSC_MAX_ENUM;

  ret = simple_check_bm1684x_input_param(handle, output, output_num);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  if (padding_attr == NULL) {
      bmlib_log("VPP_PADDING", BMLIB_LOG_ERROR, "vpp padding info is nullptr");
      return BM_ERR_FAILURE;
  }

  int compressed_flag = 0;
  bm_image input_temp;
  ret = bm1684x_vpp_compressed2yuv(handle, output_num, &input, &input_temp, matrix, algorithm, crop_rect, &compressed_flag);
  if(ret != BM_SUCCESS)
    goto failed;

  ret = bm1684x_vpp_single_input_multi_output(
    handle, output_num, input, output, crop_rect, padding_attr, algorithm, csc_type, matrix);

failed:
  if(1 == compressed_flag)
  {
    bm_image_destroy(input_temp);
    input_temp.image_private = NULL;
  }
  return ret;
}

bm_status_t bm1684x_vpp_convert_internal(
  bm_handle_t             handle,
  int                     output_num,
  bm_image                input,
  bm_image*               output,
  bmcv_rect_t*            crop_rect_,
  bmcv_resize_algorithm   algorithm,
  csc_matrix_t *          matrix
  )
{
  bm_status_t ret = BM_SUCCESS;
  csc_type_t csc_type = CSC_MAX_ENUM;

  ret = bm1684x_vpp_csc_matrix_convert(
    handle, output_num, input, output, csc_type, matrix, algorithm, crop_rect_);

  return ret;
}

bm_status_t bm1684x_vpp_basic(
  bm_handle_t             handle,
  int                     in_img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix) {

  int out_img_num = 0, i = 0, j = 0;
  bm_image *input_inner;
  int out_idx = 0;
  bm_status_t ret = BM_SUCCESS;

  ret = simple_check_bm1684x_input_param(handle, input, in_img_num);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  if (crop_rect == NULL) {
    if (crop_num_vec != NULL) {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
        "crop_num_vec should be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
      return BM_ERR_FAILURE;
    }

    out_img_num = in_img_num;
    input_inner = input;

  } else {
    if (crop_num_vec == NULL) {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
        "crop_num_vec should not be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
      return BM_ERR_FAILURE;
    }

    for (i = 0; i < in_img_num; i++) {
      out_img_num += crop_num_vec[i];
    }
    input_inner = new bm_image [out_img_num];

    for (i = 0; i < in_img_num; i++) {
      for (j = 0; j < crop_num_vec[i]; j++) {
        input_inner[out_idx + j]= input[i];
      }
      out_idx += crop_num_vec[i];
    }
  }

  ret = check_bm1684x_vpp_continuity( in_img_num, input);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }
  ret = check_bm1684x_vpp_continuity( out_img_num, output);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  ret = bm1684x_vpp_multi_input_multi_output(
    handle, out_img_num, input_inner, output, crop_rect, padding_attr, algorithm, csc_type, matrix);

  if (crop_rect != NULL)
  {
    delete [] input_inner;
  }
  return ret;
}


bm_status_t bm1684x_vpp_stitch(
  bm_handle_t             handle,
  int                     input_num,
  bm_image*               input,
  bm_image                output,
  bmcv_rect_t*            dst_crop_rect,
  bmcv_rect_t*            src_crop_rect,
  bmcv_resize_algorithm   algorithm)
{

  int i;
  bm_status_t ret = BM_SUCCESS;

  ret = simple_check_bm1684x_input_param(handle, input, input_num);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  if (dst_crop_rect == NULL) {
    bmlib_log("VPP-STITCH", BMLIB_LOG_ERROR, "dst_crop_rect is nullptr");
    return BM_ERR_FAILURE;
  }



  bmcv_padding_atrr_t *padding_attr = new bmcv_padding_atrr_t [input_num];

  memset( padding_attr, 0, input_num *sizeof(bmcv_padding_atrr_t));
  for(i = 0; i < input_num; i++)
  {
    padding_attr[i].dst_crop_stx = dst_crop_rect[i].start_x;
    padding_attr[i].dst_crop_sty = dst_crop_rect[i].start_y;
    padding_attr[i].dst_crop_w   = dst_crop_rect[i].crop_w;
    padding_attr[i].dst_crop_h   = dst_crop_rect[i].crop_h;
  }
  ret = bm1684x_vpp_multi_input_single_output(
    handle, input_num, input, output, src_crop_rect, padding_attr, algorithm, CSC_MAX_ENUM, NULL);

  delete [] padding_attr;
  return ret;
}

/*draw  rectangle , border */
bm_status_t bm1684x_vpp_draw_rectangle(
  bm_handle_t   handle,
  bm_image      image,
  int           rect_num,
  bmcv_rect_t * rects,
  int           line_width,
  unsigned char r,
  unsigned char g,
  unsigned char b)
{
  int i = 0;
  bm_status_t ret = BM_SUCCESS;
  /*check border param*/

  border_t* border_param = new border_t [rect_num];
  for(i = 0; i < rect_num; i++)
  {
    border_param[i].rect_border_enable = 1;
    border_param[i].st_x = rects[i].start_x;
    border_param[i].st_y = rects[i].start_y;
    border_param[i].width = rects[i].crop_w;
    border_param[i].height = rects[i].crop_h;
    border_param[i].value_r = r;
    border_param[i].value_g = g;
    border_param[i].value_b = b;
    border_param[i].thickness = line_width;
  }

  bm_image *border_image = new bm_image [rect_num];

  for(i = 0; i< rect_num; i++)
  {
    border_image[i] = image;
  }

  ret = bm1684x_vpp_multi_parameter_processing(
    handle, rect_num, border_image, border_image, NULL, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, NULL, border_param,NULL);

  delete [] border_image;
  delete [] border_param;
  return ret;
}

bm_status_t bm1684x_vpp_yuv2bgr_ext(
  bm_handle_t  handle,
  int          image_num,
  bm_image *   input,
  bm_image *   output)
{
  bm_status_t ret = BM_SUCCESS;

  ret = bm1684x_vpp_multi_parameter_processing(
    handle, image_num, input, output, NULL, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, NULL, NULL,NULL);

  return ret;
}

bm_status_t bm1684x_vpp_crop(
  bm_handle_t  handle,
  int          crop_num,
  bmcv_rect_t* rects,
  bm_image     input,
  bm_image*    output)
{
  bm_status_t ret = BM_SUCCESS;

  ret = bm1684x_vpp_single_input_multi_output(
    handle, crop_num, input, output, rects, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL);

  return ret;
}

bm_status_t bm1684x_vpp_resize(
  bm_handle_t        handle,
  int                input_num,
  bmcv_resize_image* resize_attr,
  bm_image *         input,
  bm_image *         output)
{
  bm_status_t ret = BM_SUCCESS;
  int input_idx = 0, output_num = 0, i = 0, output_idx = 0;

  for (int i = 0; i < input_num; i++)
  {
    if(resize_attr[0].stretch_fit!= resize_attr[input_idx].stretch_fit)
    {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
        "expected consistant input image stretch_fit %s: %s: %d\n",
        filename(__FILE__), __func__, __LINE__);
      return BM_NOT_SUPPORTED;
    }
  }

  for (input_idx = 0; input_idx < input_num; input_idx++)
  {
    output_num += resize_attr[input_idx].roi_num;
  }

  bm_image* input_inner = new bm_image [output_num];
  bmcv_rect_t* src_crop_rect = new bmcv_rect_t[output_num];
  bmcv_padding_atrr_t* padding_attr = new bmcv_padding_atrr_t[output_num];

  for (input_idx = 0; input_idx < input_num; input_idx++) {
    for (i = 0; i < resize_attr[input_idx].roi_num; i++) {
      input_inner[output_idx]= input[input_idx];
      src_crop_rect[output_idx].start_x = resize_attr[input_idx].resize_img_attr[i].start_x;
      src_crop_rect[output_idx].start_y = resize_attr[input_idx].resize_img_attr[i].start_y;
      src_crop_rect[output_idx].crop_w  = resize_attr[input_idx].resize_img_attr[i].in_width;
      src_crop_rect[output_idx].crop_h  = resize_attr[input_idx].resize_img_attr[i].in_height;

      if(resize_attr[0].stretch_fit == 1)
      {
        padding_attr[output_idx].dst_crop_stx = 0;
        padding_attr[output_idx].dst_crop_sty = 0;
        padding_attr[output_idx].dst_crop_w   = output[output_idx].width;
        padding_attr[output_idx].dst_crop_h   = output[output_idx].height;
        padding_attr[output_idx].if_memset = 0;
      }
      else if(resize_attr[0].stretch_fit == 0)
      {
        if(output[output_idx].width *src_crop_rect[output_idx].crop_h < output[output_idx].height * src_crop_rect[output_idx].crop_w)
        {
          padding_attr[output_idx].dst_crop_stx = 0;
          padding_attr[output_idx].dst_crop_w   = output[output_idx].width;
          padding_attr[output_idx].dst_crop_h   = output[output_idx].width * src_crop_rect[output_idx].crop_h / src_crop_rect[output_idx].crop_w ;
          padding_attr[output_idx].dst_crop_sty = (output[output_idx].height - padding_attr[output_idx].dst_crop_h)/2;
          padding_attr[output_idx].if_memset = 1;
        }
        else if(output[output_idx].width *src_crop_rect[output_idx].crop_h > output[output_idx].height * src_crop_rect[output_idx].crop_w)
        {
          padding_attr[output_idx].dst_crop_sty = 0;
          padding_attr[output_idx].dst_crop_h   = output[output_idx].height;
          padding_attr[output_idx].dst_crop_w   = output[output_idx].height * src_crop_rect[output_idx].crop_w / src_crop_rect[output_idx].crop_h ;
          padding_attr[output_idx].dst_crop_stx = (output[output_idx].width - padding_attr[output_idx].dst_crop_w)/2;
          padding_attr[output_idx].if_memset = 1;
        }
        else
        {
          padding_attr[output_idx].dst_crop_stx = 0;
          padding_attr[output_idx].dst_crop_sty = 0;
          padding_attr[output_idx].dst_crop_w   = output[output_idx].width;
          padding_attr[output_idx].dst_crop_h   = output[output_idx].height;
          padding_attr[output_idx].if_memset = 0;
        }
        padding_attr[output_idx].padding_r = resize_attr[input_idx].padding_r;
        padding_attr[output_idx].padding_g = resize_attr[input_idx].padding_g;
        padding_attr[output_idx].padding_b = resize_attr[input_idx].padding_b;
      }
      else
      {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
          "resize_attr.stretch_fit must be 0 or 1 %s: %s: %d\n", __FILE__, __func__, __LINE__);
      }
      output_idx++;
    }
  }

  ret = bm1684x_vpp_multi_input_multi_output(
    handle, output_num, input_inner, output, src_crop_rect, padding_attr, (bmcv_resize_algorithm)resize_attr[0].interpolation, CSC_MAX_ENUM, NULL);

  delete [] padding_attr;
  delete [] src_crop_rect;
  delete [] input_inner;
  return ret;
}


bm_status_t  bm1684x_vpp_bgrsplit(
  bm_handle_t handle,
  bm_image input,
  bm_image output)
{
  bm_status_t ret = BM_SUCCESS;

  ret = bm1684x_vpp_single_input_multi_output(
    handle, 1, input, &output, NULL, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL);

  return ret;
}

bm_status_t bm1684x_vpp_storage_convert(
  bm_handle_t      handle,
  int              image_num,
  bm_image*        input_,
  bm_image*        output_,
  csc_type_t       csc_type)
{
  bm_status_t ret = BM_SUCCESS;

  if((input_[0].data_type == DATA_TYPE_EXT_4N_BYTE || input_[0].data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED) ||
    (output_[0].data_type == DATA_TYPE_EXT_4N_BYTE || output_[0].data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED))
  {
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "1684x vpp not support 4N mode %s: %s: %d\n", __FILE__, __func__, __LINE__);
    ret = BM_NOT_SUPPORTED;
    goto done;
  }

  ret = bm1684x_vpp_multi_input_multi_output(
    handle, image_num, input_, output_, NULL, NULL, BMCV_INTER_LINEAR, csc_type, NULL);

done:
  return ret;
}

bm_status_t bm1684x_vpp_yuv2hsv(
  bm_handle_t     handle,
  bmcv_rect_t     rect,
  bm_image        input,
  bm_image        output)
{
  bm_status_t ret = BM_SUCCESS;

  ret = bm1684x_vpp_single_input_multi_output(
    handle, 1, input, &output, &rect, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL);

  return ret;
}

bm_status_t bm1684x_vpp_put_text(
  bm_handle_t       handle,
  bm_image *        image,
  int               font_num,
  bmcv_rect_t *     font_rects,
  int               font_pitch,
  bm_device_mem_t * font_mem,
  int               font_type,
  unsigned char     r,
  unsigned char     g,
  unsigned char     b)
{
  int i = 0;
  bm_status_t ret = BM_SUCCESS;

  font_t* font_param = new font_t [font_num];
  for(i = 0; i < font_num; i++)
  {
    font_param[i].font_enable = 1;
    font_param[i].font_type = font_type; //alpha or binary
    font_param[i].font_nf_range = 0;
    font_param[i].font_dot_en = 0;
    font_param[i].font_pitch = font_pitch;
    font_param[i].font_st_x = font_rects[i].start_x;
    font_param[i].font_st_y = font_rects[i].start_y;
    font_param[i].font_width = font_rects[i].crop_w;
    font_param[i].font_height = font_rects[i].crop_h;
    if(font_rects[i].start_x < 0 || font_rects[i].start_y < 0 || font_rects[i].start_x + font_rects[i].crop_w > image[i].width || \
          font_rects[i].start_y + font_rects[i].crop_h > image[i].height){
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "rects out of range, idx = %d, stx = %d, sty = %d, crop_w = %d, crop_h = %d, %s: %s: %d\n", i, font_rects[i].start_x, font_rects[i].start_y, \
            font_rects[i].crop_w, font_rects[i].crop_h, __FILE__, __func__, __LINE__);
      ret = BM_ERR_FAILURE;
      goto fail;
    }
#ifdef USING_CMODEL
    ret= bm_mem_mmap_device_mem(handle, font_mem, &font_param[i].font_addr);
#else
    font_param[i].font_addr = bm_mem_get_device_addr(font_mem[i]);
#endif

    font_param[i].font_value_r = r;
    font_param[i].font_value_g = g;
    font_param[i].font_value_b = b;
  }

  ret = bm1684x_vpp_multi_parameter_processing(
    handle, font_num, image, image, NULL, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, NULL, NULL, font_param);
fail:
  delete [] font_param;
  return ret;
}

DECL_EXPORT bm_status_t bm1684x_vpp_copy_to(
  bm_handle_t         handle,
  bmcv_copy_to_atrr_t copy_to_attr,
  bm_image            input,
  bm_image            output)
{
  bm_status_t ret = BM_SUCCESS;

  bmcv_padding_atrr_t padding_attr;

  padding_attr.dst_crop_stx = copy_to_attr.start_x;
  padding_attr.dst_crop_sty = copy_to_attr.start_y;
  padding_attr.dst_crop_w   = input.width;
  padding_attr.dst_crop_h   = input.height;
  padding_attr.padding_r = copy_to_attr.padding_r;
  padding_attr.padding_g = copy_to_attr.padding_g;
  padding_attr.padding_b = copy_to_attr.padding_b;
  padding_attr.if_memset = copy_to_attr.if_padding;

  ret = bm1684x_vpp_cvt_padding(handle, 1, input, &output, &padding_attr, NULL, BMCV_INTER_LINEAR, NULL);

  return ret;
}

void bm1684x_vpp_write_bin(bm_image dst, const char *output_name)
{
  int image_byte_size[4] = {0};
  bm_image_get_byte_size(dst, image_byte_size);
#if 0
  printf("dst plane0 size: %d\n", image_byte_size[0]);
  printf("dst plane1 size: %d\n", image_byte_size[1]);
  printf("dst plane2 size: %d\n", image_byte_size[2]);
  printf("dst plane3 size: %d\n", image_byte_size[3]);
#endif

  int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
  char* output_ptr = (char *)malloc(byte_size);

  void* out_ptr[4] = {(void*)output_ptr,
                      (void*)((char*)output_ptr + image_byte_size[0]),
                      (void*)((char*)output_ptr + image_byte_size[0] + image_byte_size[1]),
                      (void*)((char*)output_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};

  bm_image_copy_device_to_host(dst, (void **)out_ptr);

  FILE *fp_dst = fopen(output_name, "wb");

  fwrite((void *)output_ptr, 1, byte_size, fp_dst);

  fclose(fp_dst);
  free(output_ptr);

  return;
}

void bm1684x_vpp_read_bin(bm_image src, const char *input_name)
{
  int image_byte_size[4] = {0};
  bm_image_get_byte_size(src, image_byte_size);

#if 0
  printf("src plane0 size: %d\n", image_byte_size[0]);
  printf("src plane1 size: %d\n", image_byte_size[1]);
  printf("src plane2 size: %d\n", image_byte_size[2]);
  printf("src plane3 size: %d\n", image_byte_size[3]);
#endif

  int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2] + image_byte_size[3];
  char* input_ptr = (char *)malloc(byte_size);

  void* in_ptr[4] = {(void*)input_ptr,
                     (void*)((char*)input_ptr + image_byte_size[0]),
                     (void*)((char*)input_ptr + image_byte_size[0] + image_byte_size[1]),
                     (void*)((char*)input_ptr + image_byte_size[0] + image_byte_size[1] + image_byte_size[2])};


  FILE *fp_src = fopen(input_name, "rb+");

  if (fread((void *)input_ptr, 1, byte_size, fp_src) < (unsigned int)byte_size){
      printf("file size is less than %d required bytes\n", byte_size);
  };

  fclose(fp_src);

  bm_image_copy_host_to_device(src, (void **)in_ptr);
  free(input_ptr);
  return;
}

bm_status_t bm1684x_vpp_cmodel_calc(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr,
  border_t*               border_param,
  font_t*                 font_param)
{
  bm_status_t ret = BM_SUCCESS;

  ret = bm1684x_vpp_asic_and_cmodel(
    handle, frame_number, input, output, input_crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr,border_param, font_param, USE_CMODEL);

  return ret;

}

bm_status_t bm1684x_vpp_yuv_resize(
  bm_handle_t       handle,
  int               input_num,
  bm_image *        input,
  bm_image *        output)
{
  bm_status_t ret = BM_SUCCESS;

  ret = bm1684x_vpp_storage_convert(handle, input_num, input, output, CSC_MAX_ENUM);

  return ret;

}


bm_status_t bm1684x_vpp_fill_rectangle(
  bm_handle_t          handle,
  int                  input_num,
  bm_image *           input,
  bm_image *           output,
  bmcv_rect_t*         input_crop_rect,
  unsigned char        r,
  unsigned char        g,
  unsigned char        b)
{
  bm_status_t ret = BM_SUCCESS;
  ret = simple_check_bm1684x_input_param(handle, input, input_num);
  if(ret != BM_SUCCESS)
    return ret;
  bm_image *output_inner = new bm_image [input_num];
  bmcv_padding_atrr_t *padding_attr = new bmcv_padding_atrr_t [input_num];
  bmcv_convert_to_attr *convert_to_attr = new bmcv_convert_to_attr [input_num];
  for(int i=0;i<input_num;i++){
    output_inner[i]=output[0];
    padding_attr[i].dst_crop_w = input_crop_rect[i].crop_w;
    padding_attr[i].dst_crop_h = input_crop_rect[i].crop_h;
    padding_attr[i].dst_crop_stx = input_crop_rect[i].start_x;
    padding_attr[i].dst_crop_sty = input_crop_rect[i].start_y;
    if((input_crop_rect[i].crop_w + input_crop_rect[i].start_x > output->width)||
      (input_crop_rect[i].crop_h + input_crop_rect[i].start_y > output->height)){
    padding_attr[i].dst_crop_w = bm_min((output->width - input_crop_rect[i].start_x),input_crop_rect[i].crop_w);
    padding_attr[i].dst_crop_h = bm_min((output->height - input_crop_rect[i].start_y),input_crop_rect[i].crop_h);
    }
    padding_attr[i].padding_b = 0;
    padding_attr[i].padding_g = 0;
    padding_attr[i].padding_r = 0;
    padding_attr[i].if_memset = 0;
    convert_to_attr[i].alpha_0 = 0;
    convert_to_attr[i].alpha_1 = 0;
    convert_to_attr[i].alpha_2 = 0;
    convert_to_attr[i].beta_0 = (float)r;
    convert_to_attr[i].beta_1 = (float)g;
    convert_to_attr[i].beta_2 = (float)b;}
  ret = bm1684x_vpp_multi_parameter_processing(
     handle, input_num, input, output_inner, NULL, padding_attr, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, convert_to_attr, NULL, NULL);
  delete [] padding_attr;
  delete [] convert_to_attr;
  delete [] output_inner;
  return ret;
}

bm_status_t bm1684x_vpp_cmodel_csc_resize_convert_to(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr)
{
  bm_status_t ret = BM_SUCCESS;
  ret = bm1684x_vpp_asic_and_cmodel(
    handle, frame_number, input, output, input_crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr, NULL, NULL, USE_CMODEL);
  return ret;
}

bm_status_t bm1684x_vpp_cmodel_border(
  bm_handle_t             handle,
  int                     rect_num,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            rect,
  int                     line_width,
  unsigned char           r,
  unsigned char           g,
  unsigned char           b)
{
  bm_status_t ret = BM_SUCCESS;
  border_t* border_param = new border_t [rect_num];
  for(int i = 0; i < rect_num; i++){
    border_param[i].rect_border_enable = 1;
    border_param[i].st_x = rect[i].start_x;
    border_param[i].st_y = rect[i].start_y;
    border_param[i].width = rect[i].crop_w;
    border_param[i].height = rect[i].crop_h;
    if(is_csc_yuv_or_rgb(input[i].image_format) != COLOR_SPACE_YUV){
      border_param[i].value_r = r;
      border_param[i].value_g = g;
      border_param[i].value_b = b;
    }
    else
      calculate_yuv(r, g, b, &border_param[i].value_r, &border_param[i].value_g, &border_param[i].value_b);
    border_param[i].thickness = line_width;
  }
  ret = bm1684x_vpp_asic_and_cmodel(
    handle, rect_num, input, output, NULL, NULL, BMCV_INTER_LINEAR, CSC_MAX_ENUM, NULL, NULL, border_param, NULL, USE_CMODEL);
  delete [] border_param;
  return ret;
}

bm_status_t bm1684x_vpp_mosaic_special(bm_handle_t          handle,
                                      int                   mosaic_num,
                                      bm_image              image,
                                      bmcv_rect_t *         mosaic_rect){
  bm_status_t ret = BM_SUCCESS;
  bm_image * input = new bm_image [mosaic_num];
  bm_image * output = new bm_image [mosaic_num];
  bm_image * masaic_pad = new bm_image [mosaic_num];
  bm_image * masaic_narrow = new bm_image [mosaic_num];
  bmcv_padding_atrr_t * padding_pad = new bmcv_padding_atrr_t [mosaic_num];
  bmcv_padding_atrr_t * padding_enlarge = new bmcv_padding_atrr_t [mosaic_num];
  bmcv_rect_t * crop_pad = new bmcv_rect_t [mosaic_num];
  bmcv_rect_t * crop_enlarge = new bmcv_rect_t [mosaic_num];
  for(int i=0; i<mosaic_num; i++){
    input[i] = image;
    crop_pad[i].start_x = mosaic_rect[i].start_x;
    crop_pad[i].start_y = mosaic_rect[i].start_y;
    crop_pad[i].crop_w = mosaic_rect[i].crop_w;
    crop_pad[i].crop_h = mosaic_rect[i].crop_h;
    padding_pad[i].dst_crop_stx = 0;
    padding_pad[i].dst_crop_sty = 0;
    padding_pad[i].dst_crop_w = crop_pad[i].crop_w;
    padding_pad[i].dst_crop_h = crop_pad[i].crop_h;
    padding_pad[i].if_memset = 1;
    padding_pad[i].padding_r = 0;
    padding_pad[i].padding_g = 0;
    padding_pad[i].padding_b = 0;
    int masaic_pad_h = mosaic_rect[i].crop_h > 64 ? mosaic_rect[i].crop_h : 64;
    int masaic_pad_w = mosaic_rect[i].crop_w > 64 ? mosaic_rect[i].crop_w : 64;
    bm_image_create(handle, masaic_pad_h, masaic_pad_w, image.image_format, image.data_type, &masaic_pad[i]);
    ret = bm_image_alloc_dev_mem(masaic_pad[i]);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "bm_image alloc dev mem fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
      goto fail2;
    }
  }
  ret = bm1684x_vpp_multi_parameter_processing(handle, mosaic_num, input, masaic_pad, crop_pad,
                                                padding_pad, BMCV_INTER_NEAREST, CSC_MAX_ENUM, NULL, NULL, NULL, NULL);
  if(ret != BM_SUCCESS)
    goto fail2;
  for(int i=0; i<mosaic_num; i++){
    output[i] = image;
    int masaic_narrow_h = masaic_pad[i].height >> 3;
    int masaic_narrow_w = masaic_pad[i].width >> 3;
    bm_image_create(handle, masaic_narrow_h, masaic_narrow_w, image.image_format, image.data_type, &masaic_narrow[i]);
    ret = bm_image_alloc_dev_mem(masaic_narrow[i]);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "bm_image alloc dev mem fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
      goto fail1;
    }
    padding_enlarge[i].dst_crop_stx = mosaic_rect[i].start_x;
    padding_enlarge[i].dst_crop_sty = mosaic_rect[i].start_y;
    padding_enlarge[i].dst_crop_w = crop_pad[i].crop_w;
    padding_enlarge[i].dst_crop_h = crop_pad[i].crop_h;
    padding_enlarge[i].if_memset = 0;
    crop_enlarge[i].start_x = 0;
    crop_enlarge[i].start_y = 0;
    crop_enlarge[i].crop_w = crop_pad[i].crop_w;
    crop_enlarge[i].crop_h = crop_pad[i].crop_h;
  }
  ret = bm1684x_vpp_multi_parameter_processing(handle, mosaic_num, masaic_pad, masaic_narrow, NULL,
                                                     NULL, BMCV_INTER_NEAREST, CSC_MAX_ENUM, NULL, NULL, NULL, NULL);
  if(ret != BM_SUCCESS)
    goto fail1;
  ret = bm1684x_vpp_multi_parameter_processing(handle, mosaic_num, masaic_narrow, masaic_pad, NULL,
                                                    NULL, BMCV_INTER_NEAREST, CSC_MAX_ENUM, NULL, NULL, NULL, NULL);
  if(ret != BM_SUCCESS)
    goto fail1;
  ret = bm1684x_vpp_multi_parameter_processing(handle, mosaic_num, masaic_pad, output, crop_enlarge,
                                                    padding_enlarge, BMCV_INTER_NEAREST, CSC_MAX_ENUM, NULL, NULL, NULL, NULL);
fail1:
  for(int i=0; i<mosaic_num; i++){
    bm_image_destroy(masaic_narrow[i]);
  }
fail2:
  for(int i=0; i<mosaic_num; i++){
    bm_image_destroy(masaic_pad[i]);
  }
  delete [] masaic_narrow;
  delete [] masaic_pad;
  delete [] padding_pad;
  delete [] padding_enlarge;
  delete [] crop_pad;
  delete [] crop_enlarge;
  delete [] input;
  delete [] output;
  return ret;
}

bm_status_t bm1684x_vpp_mosaic_normal(bm_handle_t           handle,
                                      int                   mosaic_num,
                                      bm_image              input,
                                      bmcv_rect_t *         mosaic_rect){
  bm_status_t ret = BM_SUCCESS;
  bm_image * image = new bm_image [mosaic_num];
  bm_image * masaic_narrow = new bm_image [mosaic_num];
  bmcv_rect_t * crop_narrow = new bmcv_rect_t [mosaic_num];
  bmcv_padding_atrr_t * padding_enlarge = new bmcv_padding_atrr_t [mosaic_num];
  for(int i=0; i<mosaic_num; i++){
    image[i] = input;
    crop_narrow[i].start_x = mosaic_rect[i].start_x;
    crop_narrow[i].start_y = mosaic_rect[i].start_y;
    crop_narrow[i].crop_w = mosaic_rect[i].crop_w;
    crop_narrow[i].crop_h = mosaic_rect[i].crop_h;
    padding_enlarge[i].dst_crop_stx = mosaic_rect[i].start_x;
    padding_enlarge[i].dst_crop_sty = mosaic_rect[i].start_y;
    padding_enlarge[i].dst_crop_w = mosaic_rect[i].crop_w;
    padding_enlarge[i].dst_crop_h = mosaic_rect[i].crop_h;
    padding_enlarge[i].if_memset = 0;
    bm_image_create(handle, mosaic_rect[i].crop_h >> 3, mosaic_rect[i].crop_w >> 3, input.image_format, input.data_type, &masaic_narrow[i]);
    ret = bm_image_alloc_dev_mem(masaic_narrow[i]);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
      "bm_image alloc dev mem fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
      goto fail;
    }
  }
  ret = bm1684x_vpp_multi_parameter_processing(handle, mosaic_num, image, masaic_narrow, crop_narrow, NULL, BMCV_INTER_NEAREST, CSC_MAX_ENUM, NULL, NULL, NULL, NULL);
  if(ret != BM_SUCCESS)
    goto fail;
  ret = bm1684x_vpp_multi_parameter_processing(handle, mosaic_num, masaic_narrow, image, NULL, padding_enlarge, BMCV_INTER_NEAREST, CSC_MAX_ENUM, NULL, NULL, NULL, NULL);
fail:
  for(int i=0; i<mosaic_num; i++)
    bm_image_destroy(masaic_narrow[i]);
  delete [] image;
  delete [] masaic_narrow;
  delete [] crop_narrow;
  delete [] padding_enlarge;
  return ret;
}

bm_status_t bm1684x_vpp_mosaic(bm_handle_t          handle,
                              int                   mosaic_num,
                              bm_image              image,
                              bmcv_rect_t *         mosaic_rect){
  bm_status_t ret = BM_SUCCESS;
  int normal_num = 0, special_num = 0;
  bmcv_rect_t * normal_masaic = new bmcv_rect_t [mosaic_num];
  bmcv_rect_t * special_masaic = new bmcv_rect_t [mosaic_num];
  for(int i=0; i<mosaic_num; i++){
    if(mosaic_rect[i].crop_w < 64 || mosaic_rect[i].crop_h < 64){
        special_masaic[special_num] = mosaic_rect[i];
        special_num += 1;
    }
    else{
        normal_masaic[normal_num] = mosaic_rect[i];
        normal_num += 1;
    }
  }
  if(special_num > 0){
    ret = bm1684x_vpp_mosaic_special(handle, special_num, image, special_masaic);
  }
  if(normal_num > 0){
    ret = bm1684x_vpp_mosaic_normal(handle, normal_num, image, normal_masaic);
  }
  delete [] normal_masaic;
  delete [] special_masaic;
  return ret;
}

bm_status_t bm1684x_vpp_basic_v2(
  bm_handle_t             handle,
  int                     img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr_){

  int out_crop_num = 0, i = 0, j = 0;
  bm_image *input_inner, *output_inner;
  bmcv_convert_to_attr *convert_to_attr;
  int out_idx = 0;
  bm_status_t ret = BM_SUCCESS;

  ret = simple_check_bm1684x_input_param(handle, input, img_num);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  ret = check_bm1684x_vpp_continuity(img_num, input);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }
  ret = check_bm1684x_vpp_continuity(out_crop_num, output);
  if(ret != BM_SUCCESS)
  {
    return ret;
  }

  if (crop_rect == NULL) {
    if (crop_num_vec != NULL) {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
        "crop_num_vec should be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
      return BM_ERR_FAILURE;
    }

    out_crop_num = img_num;
  } else {
    if (crop_num_vec == NULL) {
      bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
        "crop_num_vec should not be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
      return BM_ERR_FAILURE;
    }
    for (i = 0; i < img_num; i++) {
      out_crop_num += crop_num_vec[i];
    }
  }

  bmcv_convert_to_attr black_attr;

  if(NULL != padding_attr)
  {
    for (i = 0; i < out_crop_num; i++)
    {
      padding_attr[i].padding_r = 0;
      padding_attr[i].padding_g = 0;
      padding_attr[i].padding_b = 0;
      padding_attr[i].if_memset = 1;
      if ((padding_attr[i].dst_crop_stx > 255) || (padding_attr[i].dst_crop_sty > 255) ||
          (output[i].width - padding_attr[i].dst_crop_w - padding_attr[i].dst_crop_stx > 255) ||
          (output[i].height- padding_attr[i].dst_crop_h - padding_attr[i].dst_crop_sty > 255) )
      {
        black_attr.alpha_0 = 0;
        black_attr.alpha_1 = 0;
        black_attr.alpha_2 = 0;
        black_attr.beta_0 = padding_attr[i].padding_r;
        black_attr.beta_1 = padding_attr[i].padding_g;
        black_attr.beta_2 = padding_attr[i].padding_b;
        padding_attr[i].if_memset = 0;
        ret = bm1684x_vpp_convert_to(handle, 1, black_attr, input, &output[i]);
        if(ret != BM_SUCCESS)
        {
          bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
            "bm1684x_vpp_convert_to error , %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
          return BM_ERR_PARAM;
        }
      }
    }
  }

  if (crop_rect != NULL) {
    input_inner = new bm_image [out_crop_num];
    output_inner = new bm_image [out_crop_num];

    for (i = 0; i < img_num; i++) {
      for (j = 0; j < crop_num_vec[i]; j++) {
          input_inner[out_idx + j]= input[i];
          output_inner[out_idx + j]= output[out_idx + j];
      }
      out_idx += crop_num_vec[i];
    }
  } else {
    input_inner = input;
    output_inner = output;
  }

  convert_to_attr = new bmcv_convert_to_attr [out_crop_num];
  for(int i=0; i<out_crop_num; i++)
    convert_to_attr[i] = convert_to_attr_[0];

  ret = bm1684x_vpp_multi_parameter_processing(
    handle, out_crop_num, input_inner, output_inner, crop_rect, padding_attr, algorithm, csc_type, matrix, convert_to_attr, NULL, NULL);

  if (crop_rect != NULL)
  {
    delete [] input_inner;
    delete [] output_inner;
  }
  delete [] convert_to_attr;
  return ret;
}