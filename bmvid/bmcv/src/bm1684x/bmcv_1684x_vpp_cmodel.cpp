/*
*    Copyright (c) 2019-2022 by Bitmain Technologies Inc. All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Bitmain Technologies Inc. This is proprietary information owned by
*    Bitmain Technologies Inc. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Bitmain Technologies Inc.
*
*/
#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <iostream>
#include <algorithm>
#include "stdlib.h"
#include "stdio.h"
#include "math.h"
#include "bmcv_1684x_vpp_cmodel.h"
#include "bmcv_1684x_vpp_internal.h"

#ifdef __x86_64__
#include <xmmintrin.h>
#endif

//------------------------------------------------------------------------------
#if 0
csc mode    standard    Full range flag         floating-point coe matrix
sRGB2YCbCr  BT709   1
Y:0~255
CbCr: 0~255
            0.2126390058715100      0.7151686787677560      0.0721923153607340      0
            -0.1145921775557320     -0.3854078224442680     0.5000000000000000      0
            0.5000000000000000      -0.4541555170378730     -0.0458444829621270     0
YCbCr2sRGB  BT709   1
            1.0000000000000000      0.0000000000000000      1.5747219882569700      0
            1.0000000000000000      -0.1873140895347890     -0.4682074705563420     0
            1.0000000000000000      1.8556153692785300      0.0000000000000000      0
sRGB2YCbCr  BT709   0
Y:16~235
CbCr: 16~240
            0.1826193815131790      0.6142036888240730      0.0620004590745127      16
            -0.1006613638136630     -0.3385543224608470     0.4392156862745100      128
            0.4392156862745100      -0.3989444541822880     -0.0402712320922214     128
YCbCr2sRGB  BT709   0
            1.1643835616438400      0.0000000000000000      1.7926522634175300      -248.0896267037460000
            1.1643835616438400      -0.2132370215686210     -0.5330040401422640     76.8887189126920000
            1.1643835616438400      2.1124192819911800      0.0000000000000000      -289.0198050811730000

sRGB2YCbCr  BT601   1
            0.2990568124235360      0.5864344646011700      0.1145087229752940      0
            -0.1688649115936980     -0.3311350884063020     0.5000000000000000      0
            0.5000000000000000      -0.4183181140748280     -0.0816818859251720     0
YCbCr2sRGB  BT601   1
            1.0000000000000000      0.0000000000000000      1.4018863751529200      0
            1.0000000000000000      -0.3458066722146720     -0.7149028511111540     0
            1.0000000000000000      1.7709825540494100      0.0000000000000000      0
sRGB2YCbCr  BT601   0
            0.2568370271402130      0.5036437166574750      0.0983427856140760      16
            -0.1483362360666210     -0.2908794502078890     0.4392156862745100      128
            0.4392156862745100      -0.3674637551088690     -0.0717519311656413     128
YCbCr2sRGB  BT601   0
            1.1643835616438400      0.0000000000000000      1.5958974359999800      -222.9050087942980000
            1.1643835616438400      -0.3936638456015240     -0.8138402992560010     135.9303935554620000
            1.1643835616438400      2.0160738896544600      0.0000000000000000      -276.6875948620730000

uint32 csc_matrix_list_1686[12][12] = {
  /*YUV2RGB 709 FULL RANGE     0 */
  {0x3F800000, 0x00000000, 0x3FC9907D, 0x0,
  0x3F800000, 0xBE3FCF43, 0xBEEFB8E3, 0x0,
  0x3F800000, 0x3FED84CD, 0x00000000, 0x0},

  /*YUV2RGB 709 NOT FULL RANGE     1*/
  {0x3F950A85, 0x00000000, 0x3FE575A1, 0xC37816F1,
  0x3F950A85, 0xBE5A5ACE, 0xBF0872F3, 0x4299C706,
  0x3F950A85, 0x400731E0, 0x00000000, 0xC3908288},

  /*YUV2RGB 601 FULL RANGE    2*/
  {0x3F800000, 0x00000000, 0x3FB37103, 0x00000000,
  0x3F800000, 0xBEB10D92, 0xBF3703DF, 0x00000000,
  0x3F800000, 0x3FE2AF8E, 0x00000000, 0x00000000},

  /*YUV2RGB 601 NOT FULL RANGE    3*/
  {0x3F950A85, 0x00000000, 0x3FCC465D, 0xC35EE7AE,
  0x3F950A85, 0xBEC98E4E, 0xBF5057D6, 0x4307EE2E,
  0x3F950A85, 0x4001075A, 0x00000000, 0xC38A5803},

  /*RGB2YUV 709 FULL RANGE    4*/
  {0x3E59BE0A, 0x3F37154B, 0x3D93D990, 0x00000000,
  0xBDEAAF4D, 0xBEC5542C, 0x3F000000, 0x00000000,
  0x3F000000, 0xBEE88712, 0xBD3BC76C, 0x00000000},

  /*RGB2YUV 709 NOT FULL RANGE   5*/
  {0x3E3B0093, 0x3F1D3C73, 0x3D7DF431, 0x41800000,
  0xBDCE278B, 0xBEAD56FD, 0x3EE0E0E0, 0x43000000,
  0x3EE0E0E0, 0xBECC4272, 0xBD24F372, 0x43000000},

  /*RGB2YUV 601 FULL RANGE   6*/
  {0x3E991DF9, 0x3F162091, 0x3DEA838C, 0x00000000,
  0xBE2CEAEC, 0xBEA98A89, 0x3F000000, 0x00000000,
  0x3F000000, 0xBED62DCA, 0xBDA748D5, 0x00000000},

  /*RGB2YUV 601 NOT FULL RANGE   7*/
  {0x3E838024, 0x3F00EECB, 0x3DC967F1, 0x41800000,
  0xBE17E574, 0xBE94EE26, 0x3EE0E0E0, 0x43000000,
  0x3EE0E0E0, 0xBEBC2435, 0xBD92F2AD, 0x43000000},

#if 0
    1                                1.140014648       -145.921874944
    1            -0.395019531       -0.580993652       124.929687424
    1             2.031982422                          -260.093750016
#endif
  /*YUV2RGB 601 opencv: YUV2RGB  maosi simulation   8*/
  {0x3F800000, 0x00000000, 0x3F91EBFF, 0xC311EBFF,
  0x3F800000, 0xBECA3FFF, 0xBF14BBFF, 0x42F9DBFF,
  0x3F800000, 0x40020C00, 0x00000000, 0xC3820C00},
#if 0
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
#endif
  /*RGB2YUV420P, opencv:RGB2YUVi420 601 NOT FULL RANGE  9*/
  {0x3E83957F, 0x3F01061F, 0x3DC8B400, 0x41840000,
  0xBE178D00, 0xBE94FDE0, 0x3EE0C47F, 0x43008000,
  0x3EE0C47F, 0xBEBC6A60, 0xBD916800, 0x43008000},
#if 0
        8414, 16519, 3208,     16
        -4865, -9528, 14392,   128
        14392, -12061, -2332,  128

        0.256774902     0.504119873       0.097900391          16
       -0.148468018    -0.290771484       0.439208984          128
        0.439208984    -0.36807251       -0.071166992          128
#endif
/*RGB2YUV420P,  FFMPEG : BGR2i420  601 NOT FULL RANGE    10*/
{0x3E8377FF, 0x3F010DFF, 0x3DC88000, 0x41800000,
 0xBE180800, 0xBE94DFFF, 0x3EE0DFFF, 0x43000000,
 0x3EE0DFFF, 0xBEBC7400, 0xBD91BFFF, 0x43000000},

#if 0
1.163999557               1.595999718   -204.787963904
1.163999557 -0.390999794 -0.812999725    154.611938432
1.163999557  2.017999649                -258.803955072
#endif

/*YUV420P2bgr  OPENCV : i4202BGR  601   11*/
{0x3F94FDEF, 0x0,        0x3FCC49B8, 0xC34CC9B8,
 0x3F94FDEF, 0xBEC8311F, 0xBF5020BF, 0x431A9CA7,
 0x3F94FDEF, 0x400126E7, 0x0,        0xC38166E7},

};
#endif
/********************************************************************************/

static void set_denorm(void)
{

#ifdef __x86_64__
    const int MXCSR_DAZ = (1<<6);
    const int MXCSR_FTZ = (1<<15);

    unsigned int mxcsr = __builtin_ia32_stmxcsr ();
    mxcsr |= MXCSR_DAZ | MXCSR_FTZ;
    __builtin_ia32_ldmxcsr (mxcsr);
#endif

}

float clip_f (float in,float min, float max){
    float res;
    if(in<min) res = 0.0f;
    else if(in>max) res = 255.0f;
    else            res = in;
    return res;
}
uint16_t fp32idata_to_fp16idata(uint32_t f,int round_method) {
  uint32_t f_exp;
  uint32_t f_sig;
  uint32_t h_sig=0x0u;
  uint16_t h_sgn, h_exp;

  h_sgn = (uint16_t)((f & 0x80000000u) >> 16);
  f_exp = (f & 0x7f800000u);

  /* Exponent overflow/NaN converts to signed inf/NaN */
  if (f_exp >= 0x47800000u) {
    if (f_exp == 0x7f800000u) {
      /* Inf or NaN */
      f_sig = (f & 0x007fffffu);
      if (f_sig != 0) {
        /* NaN */
        return 0x7fffu;
      } else {
        /* signed inf */
        return (uint16_t)(h_sgn + 0x7c00u);
      }
    } else {
      /* overflow to signed inf */
      return (uint16_t)(h_sgn + 0x7c00u);
    }
  }

/* Exponent underflow converts to a subnormal half or signed zero */
#ifdef CUDA_T
  if (f_exp <= 0x38000000u) ////exp= -15, fp16is denormal ,the smallest value of fp16 normal = 1x2**(-14)
#else
  if (f_exp < 0x38000000u)
#endif
  {
    /*
     * Signed zeros, subnormal floats, and floats with small
     * exponents all convert to signed zero half-floats.
     */
    if (f_exp < 0x33000000u) {
      return h_sgn;
    }
    /* Make the subnormal significand */
    f_exp >>= 23;
    f_sig = (0x00800000u + (f & 0x007fffffu));

/* Handle rounding by adding 1 to the bit beyond half precision */
    switch (round_method) {
      case 0:
// ROUND_TO_NEAREST_EVEN
    /*
     * If the last bit in the half significand is 0 (already even), and
     * the remaining bit pattern is 1000...0, then we do not add one
     * to the bit after the half significand.  In all other cases, we do.
     */
    if ((f_sig & ((0x01u << (127 - f_exp)) - 1)) != (0x01u << (125 - f_exp))) {
      f_sig += 1 << (125 - f_exp);
    }
    f_sig >>= (126 - f_exp);
    h_sig = (uint16_t)(f_sig);
    break;
  case 1:
    //JUST ROUND
    f_sig += 1 << (125 - f_exp);
    f_sig >>= (126 - f_exp);
    h_sig = (uint16_t)(f_sig);
    break;
   case 2:
// ROUND_TO_ZERO
    f_sig >>= (126 - f_exp);
    h_sig = (uint16_t)(f_sig);
    break;
   case 3:
    //JUST_FLOOR
      if ((f_sig & ((0x01u << (126 - f_exp)) - 1)) != 0x0u ) {
        if(h_sgn)
          f_sig  += 1 << (126 - f_exp);
      }
      f_sig >>= (126 - f_exp);
      h_sig = (uint16_t)(f_sig);
    break;
   case 4:
    //just_ceil
      if ((f_sig & ((0x01u << (126 - f_exp)) - 1)) != 0x0u ) {
        if( !h_sgn)
          f_sig  += 1 << (126 - f_exp);
      }
      f_sig >>= (126 - f_exp);
      h_sig = (uint16_t)(f_sig);
    break;
    //
   default:
// ROUND_TO_ZERO
      f_sig >>= (126 - f_exp);
      h_sig = (uint16_t)(f_sig);
      break;
    }
    /*
     * If the rounding causes a bit to spill into h_exp, it will
     * increment h_exp from zero to one and h_sig will be zero.
     * This is the correct result.
     */
    return (uint16_t)(h_sgn + h_sig);
  }

  /* Regular case with no overflow or underflow */
  h_exp = (uint16_t)((f_exp - 0x38000000u) >> 13);
  /* Handle rounding by adding 1 to the bit beyond half precision */
  f_sig = (f & 0x007fffffu);

    switch (round_method) {
      case 0:
          // ROUND_TO_NEAREST_EVEN
            /*
             * If the last bit in the half significand is 0 (already even), and
             * the remaining bit pattern is 1000...0, then we do not add one
             * to the bit after the half significand.  In all other cases, we do.
             */
            if ((f_sig & 0x00003fffu) != 0x00001000u) {
              f_sig = f_sig + 0x00001000u;
            }
              h_sig = (uint16_t)(f_sig >> 13);
              break;
      case 1:
          // JUST_ROUND
              f_sig = f_sig + 0x00001000u;
              h_sig = (uint16_t)(f_sig >> 13);
              break;
      case 2:
          // ROUND_TO_ZERO    
            h_sig = (uint16_t)(f_sig >> 13);
            break;
      case 3:
          // JUST_FLOOR
            if ((f_sig & 0x00001fffu) != 0x00000000u) {
              if ( h_sgn ) {
                f_sig = f_sig + 0x00002000u;
              }
            }
            h_sig = (uint16_t)(f_sig >> 13);
            break;
      case 4:
          // JUST_CEIL
            if ((f_sig & 0x00001fffu) != 0x00000000u) {
              if ( !h_sgn )
                f_sig = f_sig + 0x00002000u;
            }
            h_sig = (uint16_t)(f_sig >> 13);
            break;
      default:
          // ROUND_TO_ZERO    
            h_sig = (uint16_t)(f_sig >> 13);
            break;
    }
  /*
   * If the rounding causes a bit to spill into h_exp, it will
   * increment h_exp by one and h_sig will be zero.  This is the
   * correct result.  h_exp may increment to 15, at greatest, in
   * which case the result overflows to a signed inf.
   */

  return h_sgn + h_exp + h_sig;
}

fp16 fp32tofp16 (float A,int round_method) {
    fp16_data res;
    fp32_data ina;

    ina.fdata = A;
    //// VppInfo("round_method =%d  \n",round_method);  
    set_denorm();
    res.idata = fp32idata_to_fp16idata(ina.idata, round_method);
    return res.ndata;

}

bf16 fp32tobf16(float srca,int round_method)
{
    fp32_data ina;
    bf16_data res;

    set_denorm();

    ina.fdata = srca;

    uint32_t f_sig =  (uint32_t) (ina.idata & 0x007fffff);
    uint16_t h_sig, h_exp,h_sgn;
    h_sgn = (uint16_t)((ina.idata & 0x80000000u) >>16);
    h_exp = (uint16_t)((ina.idata & 0x7f800000u) >>16);

    res.idata = ina.idata>>16;
    //zero
    if(ina.ndata.exp == 0 && ina.ndata.manti == 0) return res.ndata;
    //infinity
    if(ina.ndata.exp == 0xff && ina.ndata.manti == 0) return res.ndata;
    //NaN
    if(ina.ndata.exp == 0xff && ina.ndata.manti !=0){
        //return (res.ndata.manti==0 ? (res.ndata | 0x1) : res.ndata);
        res.ndata.manti=0xff>>1 ;
        res.ndata.exp=0xff ;
        res.ndata.sign=0x0 ;
        return res.ndata;
    }

  switch( round_method) {
case 0: //ROUND_TO_NEAREST_EVEN
  if( ( f_sig & 0x0001ffffu ) != 0x00008000u) {
    f_sig  = f_sig + 0x00008000u;  
  }
    h_sig = (uint16_t)(f_sig>>16);
    break;
case 1:  //JUST_ROUND
    f_sig = f_sig + 0x00008000u;  
    h_sig = (uint16_t)(f_sig>>16);
    break;
case 2: //ROUND_TO_ZERO
    h_sig = (uint16_t)(f_sig>>16);
    break;    
case 3: //JUST_FLOOR
  if( ( f_sig & (0x0000ffffu)) != 0x00000000u) {
    if (h_sgn)
    f_sig = f_sig + 0x00010000u;  
  }
    h_sig = (uint16_t)(f_sig>>16);
    break;
case 4: //JUST_CEIL
  if( ( f_sig & (0x0000ffffu)) != 0x00000000u) {
    if (!h_sgn)
    f_sig = f_sig + 0x00010000u;  
  }
    h_sig = (uint16_t)(f_sig>>16);
    break;

default: //ROUND_TO_ZERO
    h_sig = (uint16_t)(f_sig>>16);
    break;   
  } 


    res.idata = h_sgn + h_exp + h_sig; 
    return res.ndata;
}

int8  float2int8(float A,int round_method) {
  fp32_data x;
  fp32_data res;
  x.fdata = A;
  uint32_t f_sig;
  uint8_t pos,neg; 

    float round2zero_f ;

  set_denorm();

  if(x.ndata.exp == 0xff && x.ndata.manti != 0) //nan
        return 0; 
//  else if(x.ndata.exp <= 0x66u)
//        return 0;
	else if ( (x.fdata) >= 127.f || (x.fdata) <= -128.f )
        return x.ndata.sign ? 0x80 : 0x7f;
	else{

     round2zero_f = x.ndata.sign ? ceil(x.fdata) : floor(x.fdata);

    switch (round_method) {
      case 0:
      //  ROUND_TO_NEAREST_EVEN
        //0: round to nearest even
            res.fdata = ( (floor(x.fdata) + 0.5 == x.fdata) &&  (int(round2zero_f) %2 ==0 ) )?  round2zero_f : round(x.fdata) ;  
          break;
      case 1:
// JUST_ROUND
  //1 round 
        res.fdata = round(x.fdata);
        break;
      case 2:
// ROUND_TO_ZERO
  //2 round to zero
        res.fdata = x.ndata.sign ? ceil(x.fdata) : floor(x.fdata);
        break;
      case 3:
  //3:floor
// JUST_FLOOR
        res.fdata = floor(x.fdata);
        break;
      case 4:
// JUST_CEIL
  //4:ceil
        res.fdata = ceil(x.fdata);
         break;
    default: 
         res.fdata = x.ndata.sign ? ceil(x.fdata) : floor(x.fdata);
         break;
    }

  if(res.fdata == 0.0f) {
    return 0;
  }
  else {
    f_sig = ( 0x00800000u + res.ndata.manti );
    pos = uint8_t(f_sig>> (24-(res.ndata.exp-126)) );
    neg = uint8_t(~(pos) + 0x01u) ;

    //vppinfo("fvalue = %f to f_sig =%x pos = %x neg =%x  int8(neg)=%x res=%d \n",res.fdata,f_sig,pos,neg,int8(pos),int8(neg));  
    return int8( res.ndata.sign ? neg : pos );
  }
  
  }
}   

uint8 float2uint8(float A,int round_method){
  fp32_data x;
  fp32_data res;
  x.fdata = A;
  uint32_t f_sig;
  uint8_t pos; 

  set_denorm();

    if(x.ndata.exp == 0xff && x.ndata.manti != 0) //nan
        return 0; 
    if(x.ndata.sign == 0x1 || x.fdata == 0)
        return 0;    
  	else if (x.ndata.exp > 0x86)
        return  0xff;
    else {

      float round2zero_f = floor(x.fdata);

      switch (round_method) {
      case 0:
//  ROUND_TO_NEAREST_EVEN
  //0: round to nearest even
    res.fdata = (  ( round2zero_f + 0.5f == x.fdata) &&  (int(round2zero_f) %2 ==0 ) )?  round2zero_f : round(x.fdata) ;  
    break;
      case 1:
// JUST_ROUND
  //1 round 
    res.fdata = round(x.fdata);
    break;
      case 2:
// ROUND_TO_ZERO
  //2 round to zero
    res.fdata = floor(x.fdata);
    break;
      case 3:
  //3:floor
// JUST_FLOOR
    res.fdata = floor(x.fdata);
    break;
      case 4:
// JUST_CEIL
  //4:ceil
    res.fdata = ceil(x.fdata);
    break;
      default:
    res.fdata =  floor(x.fdata);
    break;
      } 


    if (res.fdata>= 255.f)
      return 0xff;
    else if (res.fdata == 0)
      return 0;
    else {
      f_sig = ( 0x00800000u + res.ndata.manti );
      pos = uint8_t(f_sig>> (24-(res.ndata.exp-126)) );
      //VppInfo("fvalue = %f to f_sig =%x pos = %d \n",res.fdata,f_sig,pos);  
      return uint8( pos );
    }      
  }       
    
    
}

/********************************************************************************/

static int input_format_reserved(const int src_fmt) {
  int ret = 0;

  VppAssert(src_fmt < IN_FORMAT_MAX);

  switch (src_fmt) {

    case IN_RSV5:
    case IN_RSV6:
    case IN_RSV7:
    case IN_RSV8:
    case IN_RSV9:
    case IN_RSV10:
    case IN_RSV11:
    case IN_RSV12:
    case IN_RSV13:
    case IN_RSV14:
    case IN_RSV15:
    case IN_RSV16:
    case IN_RSV17:
    case IN_RSV18:
      ret = 1;
    break;
    default:
      ret = 0;
    break;
  }

  return ret;
}

void assert_input_format(const uint8 in_fmt) {
  VppAssert(in_fmt < IN_FORMAT_MAX);
  VppAssert(!input_format_reserved(in_fmt));
}

void assert_output_format(const uint8 out_fmt) {
  VppAssert(out_fmt < OUT_FORMAT_MAX);
  VppAssert(((out_fmt <= OUT_NV21) && (out_fmt != 2)) ||
    ((out_fmt >= OUT_RGBYP) && (out_fmt <= OUT_HSV256)));
}

static void vpp_src_crop_yuv400p(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert(p_param->processor_param[0].SRC_CTRL[crop_idx].input_format == IN_YUV400P);
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width);

  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w + w] = video_in[(crop_st_y + h) * stride_c0 + crop_st_x + w];
    }
  }

#ifdef WRITE_SRC_CROP_FILE
  uint32 len = crop_sz_h * crop_sz_w;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_yuv420p(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_sz_h_even;
  int crop_st_x_even;
  int crop_st_y_even;
  int src_height_u_v;
  int ofs_in;
  int ofs_crop;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;
  const int stride_c1 = p_param->processor_param[0].SRC_STRIDE_CH1.src_stride_ch1;
  const int stride_c2 = p_param->processor_param[0].SRC_STRIDE_CH2.src_stride_ch2;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  src_height_u_v = VPPALIGN(src_height, 2) / 2;

  crop_st_x_even = (crop_st_x % 2 == 0) ? crop_st_x : crop_st_x - 1;
  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ? crop_sz_w :
  crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);
  crop_st_y_even = (crop_st_y % 2 == 0) ? crop_st_y : crop_st_y - 1;
  crop_sz_h_even = (crop_st_y % 2 == 0) ? ((crop_sz_h % 2 == 0) ? crop_sz_h :
  crop_sz_h + 1) : ((crop_sz_h % 2 == 0) ? crop_sz_h + 2: crop_sz_h + 1);
  
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  VppAssert((format_in == IN_YUV420P)||(format_in == IN_FBC));
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width);
  VppAssert(stride_c1 >= crop_sz_w_even / 2);
  VppAssert(stride_c2 >= crop_sz_w_even / 2);

  /*crop y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w + w] = video_in[(crop_st_y + h) * stride_c0 + crop_st_x + w];
    }
  }

  /*crop u planar*/
  ofs_in = src_height * stride_c0;
  ofs_crop = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h_even / 2; h++) {
    for (w = 0; w < crop_sz_w_even / 2; w++) {
      crop_data[ofs_crop + h * crop_sz_w_even /2 + w] = video_in[ofs_in +
        (crop_st_y_even / 2 + h) * stride_c1 + crop_st_x_even / 2 + w];
    }
  }

/*crop v planar*/
  ofs_in += src_height_u_v * stride_c1;
  ofs_crop += (crop_sz_h_even / 2) * (crop_sz_w_even / 2);

  for (h = 0; h < crop_sz_h_even / 2; h++) {
    for (w = 0; w < crop_sz_w_even / 2; w++) {
      crop_data[ofs_crop + h * crop_sz_w_even /2 + w] = video_in[ofs_in +
        (crop_st_y_even / 2 + h) * stride_c2 + crop_st_x_even / 2 + w];
    }
  }

#ifdef WRITE_SRC_CROP_FILE
  uint32 len = crop_sz_h * crop_sz_w + (crop_sz_h_even / 2) * (crop_sz_w_even / 2) * 2;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_420sp(int crop_idx, uint8 *video_in,
    uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_sz_h_even;
  int crop_st_x_even;
  int crop_st_y_even;
  int ofs_in;
  int ofs_crop;

  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;
  const int stride_c1 = p_param->processor_param[0].SRC_STRIDE_CH1.src_stride_ch1;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_st_x_even = (crop_st_x % 2 == 0) ? crop_st_x : crop_st_x - 1;
  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);
  crop_st_y_even = (crop_st_y % 2 == 0) ? crop_st_y : crop_st_y - 1;
  crop_sz_h_even = (crop_st_y % 2 == 0) ? ((crop_sz_h % 2 == 0) ?
    crop_sz_h : crop_sz_h + 1) : ((crop_sz_h % 2 == 0) ? crop_sz_h + 2: crop_sz_h + 1);

  VppAssert((format_in == IN_NV12) || (format_in == IN_NV21));//ori
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width);
  VppAssert(stride_c1 >= crop_sz_w_even);

  VppInfo("stride_c0 %d, stride_c1 %d\n", stride_c0, stride_c1);
  VppInfo("crop_st_x %d, crop_st_y %d, crop_sz_w %d, crop_sz_h %d\n",
    crop_st_x, crop_st_y, crop_sz_w, crop_sz_h);
  VppInfo("crop_st_x_e %d, crop_st_y_e %d, crop_sz_w_e %d, crop_sz_h_e %d\n",
    crop_st_x_even, crop_st_y_even, crop_sz_w_even, crop_sz_h_even);

  /*crop y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w + w] = video_in[(crop_st_y + h) * stride_c0 + crop_st_x + w];
    }
  }

  /*crop uv planar*/
  ofs_in = src_height * stride_c0;
  ofs_crop = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h_even / 2; h++) {
    for (w = 0; w < crop_sz_w_even; w++) {
      crop_data[ofs_crop + h * crop_sz_w_even + w] = video_in[ofs_in +
        (crop_st_y_even / 2 + h) * stride_c1 + crop_st_x_even + w];
    }
  }

#ifdef WRITE_SRC_CROP_FILE
  uint32 len = crop_sz_h * crop_sz_w + (crop_sz_h_even / 2) * crop_sz_w_even;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_yuv422p(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_st_x_even;
  uint32 ofs_in;
  uint32 ofs_crop;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;
  const int stride_c1 = p_param->processor_param[0].SRC_STRIDE_CH1.src_stride_ch1;
  const int stride_c2 = p_param->processor_param[0].SRC_STRIDE_CH2.src_stride_ch2;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert(p_param->processor_param[0].SRC_CTRL[crop_idx].input_format == IN_YUV422P);
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width);

  crop_st_x_even = (crop_st_x % 2 == 0) ? crop_st_x : crop_st_x - 1;
  crop_sz_w_even =  (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  VppAssert(stride_c1 >= (crop_sz_w_even / 2));
  VppAssert(stride_c2 >= (crop_sz_w_even / 2));

  /*crop y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w + w] = video_in[(crop_st_y + h) * stride_c0 + crop_st_x + w];
    }
  }

  /*crop u planar*/
  ofs_in = src_height * stride_c0;
  ofs_crop = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w_even / 2; w++) {
      crop_data[ofs_crop + h * crop_sz_w_even / 2 + w] =
        video_in[ofs_in + (crop_st_y + h) * stride_c1 + crop_st_x_even / 2 + w];
    }
  }

  /*crop v planar*/
  ofs_in += src_height * stride_c1;
  ofs_crop += crop_sz_h * crop_sz_w_even / 2;

  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w_even / 2; w++) {
      crop_data[ofs_crop + h * crop_sz_w_even / 2 + w] =
        video_in[ofs_in + (crop_st_y + h) * stride_c2 + crop_st_x_even / 2 + w];
    }
  }
#ifdef WRITE_SRC_CROP_FILE
  uint32 len = ofs_crop + crop_sz_h * crop_sz_w_even / 2;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_422sp(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_st_x_even;
  uint32 ofs_in;
  uint32 ofs_crop;

  const uint8 src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;
  const int stride_c1 = p_param->processor_param[0].SRC_STRIDE_CH1.src_stride_ch1;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert((src_fmt == IN_NV16) || (src_fmt == IN_NV61));
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width);

  crop_st_x_even = (crop_st_x % 2 == 0) ? crop_st_x : crop_st_x - 1;
  crop_sz_w_even =  (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  VppAssert(stride_c1 >= crop_sz_w_even);

  /*crop y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w + w] = video_in[(crop_st_y + h) * stride_c0 + crop_st_x + w];
    }
  }

  /*crop uv planar*/
  ofs_in = src_height * stride_c0;
  ofs_crop = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w_even; w++) {
      crop_data[ofs_crop + h * crop_sz_w_even + w] = video_in[ofs_in +
        (crop_st_y + h) * stride_c1 + crop_st_x_even + w];
    }
  }
#ifdef WRITE_SRC_CROP_FILE
  uint32 len = ofs_crop + crop_sz_h * crop_sz_w_even;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}


static void vpp_src_crop_planar(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  uint32 ofs_in;
  uint32 ofs_crop;
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;
  const int stride_c1 = p_param->processor_param[0].SRC_STRIDE_CH1.src_stride_ch1;
  const int stride_c2 = p_param->processor_param[0].SRC_STRIDE_CH2.src_stride_ch2;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert((format_in == IN_YUV444P) || (format_in == IN_RGBP));
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width);
  VppAssert(stride_c1 >= src_width);
  VppAssert(stride_c2 >= src_width);

  /*crop planar 0*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w + w] = video_in[(crop_st_y + h) * stride_c0 + crop_st_x + w];
    }
  }

  /*crop planar 1*/
  ofs_in = src_height * stride_c0;
  ofs_crop = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[ofs_crop + h * crop_sz_w + w] =
        video_in[ofs_in + (crop_st_y + h) * stride_c1 + crop_st_x + w];
    }
  }

/*crop planar 2*/
  ofs_in += src_height * stride_c1;
  ofs_crop += crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[ofs_crop + h * crop_sz_w + w] =
        video_in[ofs_in + (crop_st_y + h) * stride_c2 + crop_st_x + w];
    }
  }
#ifdef WRITE_SRC_CROP_FILE
  uint32 len = ofs_crop + crop_sz_h * crop_sz_w;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_packet(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int ch = 3;
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert((format_in == IN_YUV444_PACKET) || (format_in == IN_YVU444_PACKET) ||
    (format_in == IN_RGB24) || (format_in == IN_BGR24));
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width * 3);

  /*crop*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      crop_data[h * crop_sz_w * ch + w * ch + 0] =
        video_in[(crop_st_y + h) * stride_c0 + (crop_st_x + w) * ch + 0];
      crop_data[h * crop_sz_w * ch + w * ch + 1] =
        video_in[(crop_st_y + h) * stride_c0 + (crop_st_x + w) * ch + 1];
      crop_data[h * crop_sz_w * ch + w * ch + 2] =
        video_in[(crop_st_y + h) * stride_c0 + (crop_st_x + w) * ch + 2];
    }
  }

#ifdef WRITE_SRC_CROP_FILE
  uint32 len = crop_sz_h * crop_sz_w * 3;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_422pkt(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_st_x_even;
  int ch = 4;
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert((format_in == IN_YUV422_YUYV) || (format_in ==IN_YUV422_YVYU ) ||
    (format_in ==IN_YUV422_UYVY ) || (format_in == IN_YUV422_VYUY));
  VppAssert(crop_st_x + crop_sz_w <= src_width);
  VppAssert(crop_st_y + crop_sz_h <= src_height);
  VppAssert(stride_c0 >= src_width * 2);


  crop_st_x_even = (crop_st_x % 2 == 0) ? crop_st_x : crop_st_x - 1;
  crop_sz_w_even =  (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  /*crop*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w_even /2 ; w++) {
      crop_data[h * crop_sz_w_even/2 * ch + w * ch + 0] =
        video_in[(crop_st_y + h) * stride_c0 + crop_st_x_even*2 + w * ch + 0];
      crop_data[h * crop_sz_w_even/2 * ch + w * ch + 1] =
        video_in[(crop_st_y + h) * stride_c0 + crop_st_x_even*2 + w * ch + 1];
      crop_data[h * crop_sz_w_even/2 * ch + w * ch + 2] =
        video_in[(crop_st_y + h) * stride_c0 + crop_st_x_even*2 + w * ch + 2];
      crop_data[h * crop_sz_w_even/2 * ch + w * ch + 3] =
        video_in[(crop_st_y + h) * stride_c0 + crop_st_x_even*2 + w * ch + 3];
    }
  }

#ifdef WRITE_SRC_CROP_FILE
  uint32 len = crop_sz_h * crop_sz_w_even /2 * ch;
  write_crop_data_to_file(crop_idx, crop_data, len);
#endif
}

static void vpp_src_crop_top(int crop_idx, uint8 *video_in,
  uint8 *crop_data, VPP1686_PARAM *p_param) {
  const uint8 src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  assert_input_format(src_fmt);
  VppAssert(p_param != NULL);
  VppAssert(video_in != NULL);
  VppAssert(crop_data != NULL);

  switch (src_fmt) {
  case IN_YUV400P:
    vpp_src_crop_yuv400p(crop_idx, video_in, crop_data, p_param);
    break;

  case IN_YUV420P:
  case IN_FBC:
    vpp_src_crop_yuv420p(crop_idx, video_in, crop_data, p_param);
    break;

  case IN_YUV422P:
    vpp_src_crop_yuv422p(crop_idx, video_in, crop_data, p_param);
    break;

  case IN_YUV444P:
  case IN_RGBP:
    vpp_src_crop_planar(crop_idx, video_in, crop_data, p_param);
    break;

  case IN_NV12:
  case IN_NV21:
    vpp_src_crop_420sp(crop_idx, video_in, crop_data, p_param);
    break;

  case IN_NV16:
  case IN_NV61:
    vpp_src_crop_422sp(crop_idx, video_in, crop_data, p_param);
    break;

  case IN_YUV444_PACKET:
  case IN_YVU444_PACKET:
  case IN_RGB24:
  case IN_BGR24:
    vpp_src_crop_packet(crop_idx, video_in, crop_data, p_param);
    break;
  case IN_YUV422_YUYV:
  case IN_YUV422_YVYU:
  case IN_YUV422_UYVY:
  case IN_YUV422_VYUY:
    vpp_src_crop_422pkt(crop_idx, video_in, crop_data, p_param );
    break;
  default:  // error input format
    VppErr("src_fmt:%d\nget src crop input format err!\n",src_fmt);
    break;
  }
}

static void vpp_up_sampling_yuv400p(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  VppAssert(p_param->processor_param[0].SRC_CTRL[crop_idx].input_format == IN_YUV400P);

  memcpy(up_sampling_data, crop_data, crop_sz_h * crop_sz_w);
  //memset(&up_sampling_data[crop_sz_h * crop_sz_w], 0x80, crop_sz_h * crop_sz_w);
  //memset(&up_sampling_data[crop_sz_h * crop_sz_w * 2], 0x80, crop_sz_h * crop_sz_w);
}
static void vpp_up_sampling_yuv420p_stx_even(int crop_idx, int w, int h,
  uint8 data, uint8 *pdata, VPP1686_PARAM *p_param) {
  int crop_sz_w_even;
  int crop_sz_h_even;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ? crop_sz_w :
    crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);
  crop_sz_h_even = (crop_st_y % 2 == 0) ? ((crop_sz_h % 2 == 0) ? crop_sz_h :
    crop_sz_h + 1) : ((crop_sz_h % 2 == 0) ? crop_sz_h + 2: crop_sz_h + 1);

  if ((crop_st_x % 2 == 0) && (crop_st_y % 2 == 0) && (crop_sz_w % 2 == 0) &&
    (crop_sz_h % 2 == 0)) {
    pdata[h * 2 * crop_sz_w + w * 2] = data;
    pdata[h * 2 * crop_sz_w + w * 2 + 1] = data;
    pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
    pdata[(h *2 + 1) * crop_sz_w + w * 2 + 1] = data;
  } else if ((crop_st_x % 2 == 0) && (crop_st_y % 2 == 0) &&
  (crop_sz_w % 2 == 0) && (crop_sz_h % 2 != 0)) {
    if (h < crop_sz_h_even / 2 - 1) {
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[h * 2 * crop_sz_w + w * 2 + 1] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      pdata[(h *2 + 1) * crop_sz_w + w *2 + 1] = data;
    } else {
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[h * 2 * crop_sz_w + w * 2 + 1] = data;
    }
  } else if ((crop_st_x % 2 == 0) && (crop_st_y % 2 == 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 == 0)) {
    if (w < crop_sz_w_even / 2 - 1) {
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[h * 2 * crop_sz_w + w * 2 + 1] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      pdata[(h *2 + 1) * crop_sz_w + w *2 + 1] = data;
    } else {
      pdata[h * 2 * crop_sz_w + w *2] = data;
      pdata[(h * 2 + 1) * crop_sz_w + w * 2] = data;
    }
  } else if ((crop_st_x % 2 == 0) && (crop_st_y % 2 == 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 != 0)) {
    if (h < crop_sz_h_even / 2 - 1) {  // not last row
      if (w < crop_sz_w_even / 2 - 1) {  // not last col
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[h * 2 * crop_sz_w + w * 2 + 1] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2 + 1) * crop_sz_w + w *2 + 1] = data;
      } else {  // last col
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      }
    } else {  // last row
      if (w < crop_sz_w_even / 2 - 1) {  // not last col
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[h * 2 * crop_sz_w + w * 2 + 1] = data;
      } else {  // last col
        pdata[h * 2 * crop_sz_w + w * 2] = data;
      }
    }
  } else if ((crop_st_x % 2 == 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 == 0) && (crop_sz_h % 2 == 0)) {
    if (h == 0) {  // first row
      pdata[h * crop_sz_w + w * 2] = data;
      pdata[h * crop_sz_w + w * 2 + 1] = data;
    } else if (h < crop_sz_h_even / 2 - 1) {
      pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      pdata[(h * 2 - 1) * crop_sz_w + w * 2 + 1] = data;
      pdata[(h *2) * crop_sz_w + w * 2] = data;
      pdata[(h *2) * crop_sz_w + w * 2 + 1] = data;
    } else {  // last row
      pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      pdata[(h * 2 - 1) * crop_sz_w + w * 2 + 1] = data;
    }
  } else if ((crop_st_x % 2 == 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 == 0) && (crop_sz_h % 2 != 0)) {
    if (h == 0) {  // first row
      pdata[h * crop_sz_w + w * 2] = data;
      pdata[h * crop_sz_w + w * 2 + 1] = data;
    } else {
      pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      pdata[(h * 2 - 1) * crop_sz_w + w * 2 + 1] = data;
      pdata[(h *2) * crop_sz_w + w * 2] = data;
      pdata[(h *2) * crop_sz_w + w * 2 + 1] = data;
    }
  } else if ((crop_st_x % 2 == 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 == 0)) {
    if (h == 0) {  // first row
      if (w < crop_sz_w_even / 2 - 1) {  // not last col
        pdata[h * crop_sz_w + w * 2] = data;
        pdata[h * crop_sz_w + w * 2 + 1] = data;
      } else {  // last col
        pdata[h * crop_sz_w + w * 2] = data;
      }
    } else if (h < crop_sz_h_even / 2 - 1) {
      if (w < crop_sz_w_even / 2 - 1) {  // not last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 + 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2 + 1] = data;
      } else {  // last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      }
    } else {  // last row
      if (w < crop_sz_w_even / 2 - 1) {  // not last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 + 1] = data;
      } else {  // last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      }
    }
  } else if ((crop_st_x % 2 == 0) &&
  (crop_st_y % 2 != 0) && (crop_sz_w % 2 != 0) && (crop_sz_h % 2 != 0)) {
    if (w < crop_sz_w_even / 2 - 1) {  // not last col
      if (h == 0) {  // first row
        pdata[h * crop_sz_w + w * 2] = data;
        pdata[h * crop_sz_w + w * 2 + 1] = data;
      } else {  // not firt row
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 + 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w *2 + 1] = data;
      }
    } else {  // last col
      if (h == 0) {  // first row
        pdata[h * crop_sz_w + w * 2] = data;
      } else {  // not last row
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      }
    }
  } else {
    VppErr("fatal err!!!\n");
  }
}

static void vpp_up_sampling_yuv420p_stx_odd(int crop_idx, int w, int h,
  uint8 data, uint8 *pdata, VPP1686_PARAM *p_param) {
  int crop_sz_w_even;
  int crop_sz_h_even;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);
  crop_sz_h_even = (crop_st_y % 2 == 0) ? ((crop_sz_h % 2 == 0) ?
    crop_sz_h : crop_sz_h + 1) : ((crop_sz_h % 2 == 0) ? crop_sz_h + 2: crop_sz_h + 1);

  if ((crop_st_x % 2 != 0) && (crop_st_y % 2 == 0) &&
    (crop_sz_w % 2 == 0) && (crop_sz_h % 2 == 0)) {
    if (w == 0) {  // first col
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
    } else if (w < crop_sz_w_even / 2 - 1) {
      pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2 - 1] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
    } else {  // last col
      pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2 - 1] = data;
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 == 0) &&
  (crop_sz_w % 2 == 0) && (crop_sz_h % 2 != 0)) {
    if (w == 0) {  // first col
      if (h < crop_sz_h_even / 2 - 1) {  // not last row
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      } else {  // last row
        pdata[h * 2 * crop_sz_w + w * 2] = data;
      }
    } else if (w < crop_sz_w_even / 2 - 1) {
      if (h < crop_sz_h_even / 2 - 1) {  // not last row
        pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      } else {  // last row
        pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
        pdata[h * 2 * crop_sz_w + w * 2] = data;
      }
    } else {  // last col
      if (h < crop_sz_h_even / 2 - 1) {  // not last row
        pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2 - 1] = data;
      } else {  // last col
        pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
      }
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 == 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 == 0)) {
    if (w == 0) {  // first col
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
    } else {
      pdata[h * 2 * crop_sz_w + w * 2 -1 ] = data;
      pdata[h * 2 * crop_sz_w + w * 2] = data;
      pdata[(h *2 + 1) * crop_sz_w + w * 2 - 1] = data;
      pdata[(h *2 + 1) * crop_sz_w + w *2] = data;
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 == 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 != 0)) {
    if (w == 0) {  // first col
      if (h < crop_sz_h_even / 2 - 1) {  // first col but not last row
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      } else {  // first col and last row
        pdata[h * 2 * crop_sz_w + w * 2] = data;
      }
    } else {  // not first col
      if (h < crop_sz_h_even / 2 - 1) {  // not first col and not last row
        pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
        pdata[h * 2 * crop_sz_w + w * 2] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2 + 1) * crop_sz_w + w * 2] = data;
      } else {  // not first col but last row
        pdata[h * 2 * crop_sz_w + w * 2 - 1] = data;
        pdata[h * 2 * crop_sz_w + w * 2] = data;
      }
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 == 0) && (crop_sz_h % 2 == 0)) {
    if (h == 0) {  // first row
      if (w == 0) {  // first col
        pdata[h * crop_sz_w + w * 2] = data;
      } else if (w < crop_sz_w_even / 2 - 1) {
        pdata[h * crop_sz_w + w * 2 - 1] = data;
        pdata[h * crop_sz_w + w * 2] = data;
      } else {  // last col
        pdata[h * crop_sz_w + w * 2 - 1] = data;
      }
    } else if (h < crop_sz_h_even / 2 - 1) {
      if (w == 0) {  // first col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      } else if (w < crop_sz_w_even / 2 - 1) {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      } else {  // last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2 - 1] = data;
      }
    } else {  // last row
      if (w == 0) {  // first col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      } else if (w < crop_sz_w_even / 2 - 1) {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      } else {  // last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
      }
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 == 0) && (crop_sz_h % 2 != 0)) {
    if (h == 0) {  // first row
      if (w == 0) {  // first col
        pdata[h * crop_sz_w + w * 2] = data;
      } else if (w < crop_sz_w_even / 2 - 1) {
        pdata[h * crop_sz_w + w * 2 - 1] = data;
        pdata[h * crop_sz_w + w * 2] = data;
      } else {  // last col
        pdata[h * crop_sz_w + w * 2 - 1] = data;
      }
    } else {
      if (w == 0) {  // first col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      } else if (w < crop_sz_w_even / 2 - 1) {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      } else {  // last col
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2 - 1] = data;
      }
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 == 0)) {
    if (h == 0) {  // first row
      if (w == 0) {  // first col
        pdata[h * crop_sz_w + w * 2] = data;
      } else {  // not last col
        pdata[h * crop_sz_w + w * 2 - 1] = data;
        pdata[h * crop_sz_w + w * 2] = data;
      }
    } else if (h < crop_sz_h_even / 2 - 1) {
      if (w == 0) {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      } else {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      }
    } else {  // last row
      if (w == 0) {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      } else {
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
      }
    }
  } else if ((crop_st_x % 2 != 0) && (crop_st_y % 2 != 0) &&
  (crop_sz_w % 2 != 0) && (crop_sz_h % 2 != 0)) {
    if (w == 0) {  // first col
      if (h == 0) {  // first row
        pdata[h * crop_sz_w + w * 2] = data;
      } else {  // not firt row
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      }
    } else {  // not first col
      if (h == 0) {  // first row
        pdata[h * crop_sz_w + w * 2 - 1] = data;
        pdata[h * crop_sz_w + w * 2] = data;
      } else {  // not last row
        pdata[(h * 2 - 1) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h * 2 - 1) * crop_sz_w + w * 2] = data;
        pdata[(h *2) * crop_sz_w + w * 2 - 1] = data;
        pdata[(h *2) * crop_sz_w + w * 2] = data;
      }
    }
  } else {
    VppErr("fatal err!!!\n");
  }
}

static void vpp_up_sampling_yuv420p(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_sz_h_even;
  int ofs_u;
  int ofs_v;
  uint8 u_data;
  uint8 v_data;

  const int crop_st_x = p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y = p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);
  crop_sz_h_even = (crop_st_y % 2 == 0) ? ((crop_sz_h % 2 == 0) ?
    crop_sz_h : crop_sz_h + 1) : ((crop_sz_h % 2 == 0) ? crop_sz_h + 2: crop_sz_h + 1);
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  VppAssert((format_in == IN_YUV420P)||(format_in == IN_FBC));

  uint8 *py = &up_sampling_data[0];
  uint8 *pu = &up_sampling_data[crop_sz_h * crop_sz_w];
  uint8 *pv = &up_sampling_data[crop_sz_h * crop_sz_w * 2];

  /*copy y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      py[h * crop_sz_w + w] = crop_data[h * crop_sz_w + w];
    }
  }

  /*upsampling u and v planar*/
  ofs_u = crop_sz_h * crop_sz_w;
  ofs_v = ofs_u + (crop_sz_h_even / 2) * (crop_sz_w_even / 2);

  for (h = 0; h < crop_sz_h_even / 2; h ++) {
    for (w = 0; w < crop_sz_w_even / 2; w ++) {
      u_data = crop_data[ofs_u + h * crop_sz_w_even / 2 + w];
      v_data = crop_data[ofs_v + h * crop_sz_w_even / 2 + w];

      if (crop_st_x % 2 == 0) {
        vpp_up_sampling_yuv420p_stx_even(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv420p_stx_even(crop_idx, w, h, v_data, pv, p_param);
      } else {
        vpp_up_sampling_yuv420p_stx_odd(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv420p_stx_odd(crop_idx, w, h, v_data, pv, p_param);
      }
    }
  }
}

static void vpp_up_sampling_yuv420sp(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int crop_sz_h_even;
  int ofs_u_or_v;
  uint8 u_data=0;
  uint8 v_data=0;

  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int crop_st_x = p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_st_y = p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_y;
  const int crop_sz_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);
  crop_sz_h_even = (crop_st_y % 2 == 0) ? ((crop_sz_h % 2 == 0) ?
    crop_sz_h : crop_sz_h + 1) : ((crop_sz_h % 2 == 0) ? crop_sz_h + 2: crop_sz_h + 1);

  VppAssert((format_in == IN_NV12) || (format_in == IN_NV21)); //tmp

  uint8 *py = &up_sampling_data[0];
  uint8 *pu = &up_sampling_data[crop_sz_h * crop_sz_w];
  uint8 *pv = &up_sampling_data[crop_sz_h * crop_sz_w * 2];

  /*copy y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      py[h * crop_sz_w + w] = crop_data[h * crop_sz_w + w];
    }
  }

  /*upsampling u and v planar*/
  ofs_u_or_v = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h_even / 2; h ++) {
    for (w = 0; w < crop_sz_w_even / 2; w ++) {
      if (format_in == IN_NV12) {
        u_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2];
        v_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2 + 1];
      } else if (format_in == IN_NV21) {
        v_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2];
        u_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2 + 1];
      }

      if (crop_st_x % 2 == 0) {
        vpp_up_sampling_yuv420p_stx_even(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv420p_stx_even(crop_idx, w, h, v_data, pv, p_param);
      } else {
        vpp_up_sampling_yuv420p_stx_odd(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv420p_stx_odd(crop_idx, w, h, v_data, pv, p_param);
      }
    }
  }
}

static void vpp_up_sampling_yuv422p_stx_even(int crop_idx, int w, int h,
  uint8 data, uint8 *pdata, VPP1686_PARAM *p_param) {
  int crop_sz_w_even;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  if ((crop_st_x % 2 == 0) && (crop_sz_w % 2 == 0)) {
    pdata[h * crop_sz_w + w * 2] = data;
    pdata[h * crop_sz_w + w * 2 + 1] = data;
  } else if ((crop_st_x % 2 == 0) && (crop_sz_w % 2 != 0)) {
    if (w < crop_sz_w_even / 2 - 1) {  // not last col
      pdata[h * crop_sz_w + w * 2] = data;
      pdata[h * crop_sz_w + w * 2 + 1] = data;
    } else {  // last col
      pdata[h * crop_sz_w + w *2] = data;
    }
  } else {
    VppErr("fatal err!!!\n");
  }
}

static void vpp_up_sampling_yuv422p_stx_odd(int crop_idx, int w, int h,
  uint8 data, uint8 *pdata, VPP1686_PARAM *p_param) {
  int crop_sz_w_even;

  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  if ((crop_st_x % 2 != 0) && (crop_sz_w % 2 == 0)) {
    if (w == 0) {  // first col
      pdata[h * crop_sz_w + w * 2] = data;
    } else if (w < crop_sz_w_even / 2 - 1) {
      pdata[h * crop_sz_w + w * 2 - 1] = data;
      pdata[h * crop_sz_w + w * 2] = data;
    } else {  // last col
      pdata[h * crop_sz_w + w * 2 - 1] = data;
    }
  } else if ((crop_st_x % 2 != 0) && (crop_sz_w % 2 != 0)) {
    if (w == 0) {  // first col
      pdata[h * crop_sz_w + w * 2] = data;
    } else {
      pdata[h * crop_sz_w + w * 2 -1 ] = data;
      pdata[h * crop_sz_w + w * 2] = data;
    }
  } else {
    VppErr("fatal err!!!\n");
  }
}

static void vpp_up_sampling_yuv422p(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int ofs_u;
  int ofs_v;
  uint8 u_data;
  uint8 v_data;

  uint8 format_in =
    p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  VppAssert(format_in == IN_YUV422P);

  uint8 *py = &up_sampling_data[0];
  uint8 *pu = &up_sampling_data[crop_sz_h * crop_sz_w];
  uint8 *pv = &up_sampling_data[crop_sz_h * crop_sz_w * 2];

  /*copy y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      py[h * crop_sz_w + w] = crop_data[h * crop_sz_w + w];
    }
  }

  /*upsampling u and v planar*/
  ofs_u = crop_sz_h * crop_sz_w;
  ofs_v = ofs_u + crop_sz_h * crop_sz_w_even / 2;

  for (h = 0; h < crop_sz_h; h ++) {
    for (w = 0; w < crop_sz_w_even / 2; w ++) {
      u_data = crop_data[ofs_u + h * crop_sz_w_even / 2 + w];
      v_data = crop_data[ofs_v + h * crop_sz_w_even / 2 + w];

      if (crop_st_x % 2 == 0) {
        vpp_up_sampling_yuv422p_stx_even(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv422p_stx_even(crop_idx, w, h, v_data, pv, p_param);
      } else {
        vpp_up_sampling_yuv422p_stx_odd(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv422p_stx_odd(crop_idx, w, h, v_data, pv, p_param);
      }
    }
  }
}

static void vpp_up_sampling_422pkt(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;

  uint8 chnnl = 4;
  uint8 even_edge ;
  uint8 y_data = 0;
  uint8 u_data = 0;
  uint8 v_data = 0;

  uint8 format_in =
    p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int crop_st_x =
    p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_sz_w =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h =
    p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  if( (crop_st_x % 2 == 0) && (crop_sz_w % 2 == 0) )
    even_edge = 0;  //no need crop
  else if( (crop_st_x % 2 == 0) && (crop_sz_w % 2 == 1) )
    even_edge = 1;  //crop right
  else if( (crop_st_x % 2 == 1) && (crop_sz_w % 2 == 0) )
    even_edge = 2;  //crop left and right
  else if( (crop_st_x % 2 == 1) && (crop_sz_w % 2 == 1) )
    even_edge = 3;  //crop left
  

  uint8 *py = &up_sampling_data[0];
  uint8 *pu = &up_sampling_data[crop_sz_h * crop_sz_w];
  uint8 *pv = &up_sampling_data[crop_sz_h * crop_sz_w * 2];
  /*copy y planar*/    //for  Y in YUYV
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w_even ; w++) {
      if ( (format_in == IN_YUV422_YUYV) || (format_in == IN_YUV422_YVYU) )
        y_data = crop_data[h * crop_sz_w_even *2 +  w * 2 ];
      else if ( (format_in == IN_YUV422_UYVY) || (format_in == IN_YUV422_VYUY) )
        y_data = crop_data[h * crop_sz_w_even *2 +  w * 2 + 1 ];
      switch (even_edge) { 
        case 0:
           py[h * crop_sz_w + w] = y_data;
           break;
        case 1:
          if ( w == crop_sz_w_even -1 ) 
            continue;
          else 
            py[h * crop_sz_w + w] = y_data;
          break;
        case 2:
          if( (w==0) || (w == crop_sz_w_even-1)) 
             continue;
          else
             py[h * crop_sz_w + w -1] = y_data;
          break;
        case 3:
          if (w==0 ) 
             continue;
          else 
              py[h * crop_sz_w + w -1] = y_data;
          
          break;
      } 
    }
  }
 
    VppInfo("crop_sz_w_even = %d",crop_sz_w_even);
  /*copy u planar*/    //for  U in YUYV
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w_even /2 ; w++) {
       if( format_in == IN_YUV422_YUYV) {
        u_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 1];
        v_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 3];
       }
       else if( format_in == IN_YUV422_YVYU) {
        v_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 1];
        u_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 3];
       }
       else if( format_in == IN_YUV422_UYVY) {
        u_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 0];
        v_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 2];
       }
       else if( format_in == IN_YUV422_VYUY) {
        v_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 0];
        u_data = crop_data[h * crop_sz_w_even/2*chnnl +  w * 4 + 2];
       }

      if (crop_st_x % 2 == 0) {
        vpp_up_sampling_yuv422p_stx_even(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv422p_stx_even(crop_idx, w, h, v_data, pv, p_param);
      } else {
        vpp_up_sampling_yuv422p_stx_odd(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv422p_stx_odd(crop_idx, w, h, v_data, pv, p_param);
      }
    }
  }


}


static void vpp_up_sampling_yuv422sp(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  int crop_sz_w_even;
  int ofs_u_or_v;
  uint8 u_data;
  uint8 v_data;

  VppInfo("crop_idx %d\n", crop_idx);
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int crop_st_x = p_param->processor_param[0].SRC_CROP_ST[crop_idx].src_crop_st_x;
  const int crop_sz_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  crop_sz_w_even = (crop_st_x % 2 == 0) ? ((crop_sz_w % 2 == 0) ?
    crop_sz_w : crop_sz_w + 1) : ((crop_sz_w % 2 == 0) ? crop_sz_w + 2: crop_sz_w + 1);

  VppAssert((format_in == IN_NV16) || (format_in == IN_NV61));

  uint8 *py = &up_sampling_data[0];
  uint8 *pu = &up_sampling_data[crop_sz_h * crop_sz_w];
  uint8 *pv = &up_sampling_data[crop_sz_h * crop_sz_w * 2];

  /*copy y planar*/
  for (h = 0; h < crop_sz_h; h++) {
    for (w = 0; w < crop_sz_w; w++) {
      py[h * crop_sz_w + w] = crop_data[h * crop_sz_w + w];
    }
  }

  /*upsampling u and v planar*/
  ofs_u_or_v = crop_sz_h * crop_sz_w;

  for (h = 0; h < crop_sz_h; h ++) {
    for (w = 0; w < crop_sz_w_even / 2; w ++) {
      if (format_in == IN_NV16) {
        u_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2];
        v_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2 + 1];
      } else if (format_in == IN_NV61) {
        v_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2];
        u_data = crop_data[ofs_u_or_v + h * crop_sz_w_even + w * 2 + 1];
      }
      if (crop_st_x % 2 == 0) {
        vpp_up_sampling_yuv422p_stx_even(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv422p_stx_even(crop_idx, w, h, v_data, pv, p_param);
      } else {
        vpp_up_sampling_yuv422p_stx_odd(crop_idx, w, h, u_data, pu, p_param);
        vpp_up_sampling_yuv422p_stx_odd(crop_idx, w, h, v_data, pv, p_param);
      }
    }
  }
}

static void vpp_up_sampling_planar(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  const int crop_sz_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;


  VppAssert((format_in == IN_YUV444P) || (format_in == IN_RGBP));

  memcpy(up_sampling_data, crop_data, crop_sz_h * crop_sz_w * 3);
}
static void vpp_up_sampling_packet(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  int h, w;
  uint8 format_in = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  const int crop_sz_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int crop_sz_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  uint8 *pch0 = &up_sampling_data[0];
  uint8 *pch1 = &up_sampling_data[crop_sz_w * crop_sz_h];
  uint8 *pch2 = &up_sampling_data[crop_sz_w * crop_sz_h * 2];

  VppAssert((format_in == IN_YUV444_PACKET) || (format_in == IN_YVU444_PACKET) ||
    (format_in == IN_RGB24) || (format_in == IN_BGR24));

  if ((format_in == IN_YUV444_PACKET) || (format_in == IN_RGB24)) {
    for (h = 0 ; h < crop_sz_h; h++) {
      for (w = 0; w < crop_sz_w; w++) {
        pch0[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 0];
        pch1[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 1];
        pch2[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 2];
      }
    }
  } else if (format_in == IN_YVU444_PACKET) {
    for (h = 0 ; h < crop_sz_h; h++) {
      for (w = 0; w < crop_sz_w; w++) {
        pch0[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 0];
        pch1[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 2];
        pch2[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 1];
      }
    }
  } else if (format_in == IN_BGR24) {
    for (h = 0 ; h < crop_sz_h; h++) {
      for (w = 0; w < crop_sz_w; w++) {
        pch0[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 2];
        pch1[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 1];
        pch2[h * crop_sz_w + w] = crop_data[h * crop_sz_w * 3 + w * 3 + 0];
      }
    }
  }
}

static void vpp_up_sampling_top(int crop_idx,
  uint8 *up_sampling_data, uint8 *crop_data, VPP1686_PARAM *p_param) {
  const uint8 src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  assert_input_format(src_fmt);
  VppAssert(p_param != NULL);
  VppAssert(up_sampling_data != NULL);
  VppAssert(crop_data != NULL);

  switch (src_fmt) {
  case IN_YUV400P:
    vpp_up_sampling_yuv400p(crop_idx, up_sampling_data, crop_data, p_param);
    break;

  case IN_YUV420P:
  case IN_FBC:
    vpp_up_sampling_yuv420p(crop_idx, up_sampling_data, crop_data, p_param);
    break;

  case IN_NV12:
  case IN_NV21:
    vpp_up_sampling_yuv420sp(crop_idx, up_sampling_data, crop_data, p_param);
    break;

  case IN_YUV422P:
    vpp_up_sampling_yuv422p(crop_idx, up_sampling_data, crop_data, p_param);
    break;

    case IN_NV16:
    case IN_NV61:
      vpp_up_sampling_yuv422sp(crop_idx, up_sampling_data, crop_data, p_param);
      break;

  case IN_YUV444P:
  case IN_RGBP:
    vpp_up_sampling_planar(crop_idx, up_sampling_data, crop_data, p_param);
    break;

  case IN_YUV444_PACKET:
  case IN_YVU444_PACKET:
  case IN_RGB24:
  case IN_BGR24:
    vpp_up_sampling_packet(crop_idx, up_sampling_data, crop_data, p_param);
    break;

  case IN_YUV422_YUYV:
  case IN_YUV422_YVYU:
  case IN_YUV422_UYVY:
  case IN_YUV422_VYUY:
    vpp_up_sampling_422pkt(crop_idx, up_sampling_data, crop_data, p_param );
    break;

  default:  // error input format
    VppErr("get src crop input format err!\n");
    break;
  }

#ifdef WRITE_UP_SAMPLING_FILE
  write_upsampling_data_to_file(crop_idx, up_sampling_data, p_param);
#endif
}


static void vpp_padding_top(int crop_idx, int padding_enable,
  uint8 *padding_data, uint8 *up_sampling_data, VPP1686_PARAM *p_param) {
  int top, bottom, left, right;
  uint8 ch0, ch1, ch2;
  uint8 *pch0;
  uint8 *pch1;
  uint8 *pch2;

  uint8 *p_us_ch0;
  uint8 *p_us_ch1;
  uint8 *p_us_ch2;

  int padding_w;
  int padding_h;
  int h, w;
  uint32 len;

  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;

  top = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
  bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
  left = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
  right = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;

  VppInfo("padding_enable %d, top %d, bottom %d, left %d, right %d\n", padding_enable, top, bottom, left, right);
  if (padding_enable) {
    ch0 = p_param->processor_param[0].PADDING_VALUE[crop_idx].padding_value_ch0;
    ch1 = p_param->processor_param[0].PADDING_VALUE[crop_idx].padding_value_ch1;
    ch2 = p_param->processor_param[0].PADDING_VALUE[crop_idx].padding_value_ch2;

    padding_w = src_crop_w + left + right;
    padding_h = src_crop_h + top + bottom;

    pch0 = &padding_data[0];
    p_us_ch0 = &up_sampling_data[0];

    pch1 = NULL;
    pch2 = NULL;
    p_us_ch1 = NULL;
    p_us_ch2 = NULL;

    if (src_fmt != IN_YUV400P) {
      pch1 = &padding_data[padding_h * padding_w];
      pch2 = &padding_data[padding_h * padding_w * 2];

      p_us_ch1 = &up_sampling_data[src_crop_h * src_crop_w];
      p_us_ch2 = &up_sampling_data[src_crop_h * src_crop_w * 2];
    }

    for (h = 0; h < padding_h; h++) {
      for (w = 0; w < padding_w; w++) {
        if ((h < top) || (h > top + src_crop_h - 1)) {
          pch0[h * padding_w + w] = ch0;

          if (src_fmt != IN_YUV400P) {
            pch1[h * padding_w + w] = ch1;
            pch2[h * padding_w + w] = ch2;
          }
        } else {
          if ((w < left) || (w > left + src_crop_w - 1)) {
            pch0[h * padding_w + w] = ch0;

            if (src_fmt != IN_YUV400P) {
              pch1[h * padding_w + w] = ch1;
              pch2[h * padding_w + w] = ch2;
            }
          } else {
            pch0[h * padding_w + w] = p_us_ch0[(h - top) * src_crop_w + w - left];

            if (src_fmt != IN_YUV400P) {
              pch1[h * padding_w + w] = p_us_ch1[(h - top) * src_crop_w + w - left];
              pch2[h * padding_w + w] = p_us_ch2[(h - top) * src_crop_w + w - left];
            }
          }
        }
      }
    }
  } else {
    top = 0;
    bottom = 0;
    left = 0;
    right = 0;

    if (src_fmt == IN_YUV400P) {
      len = src_crop_h * src_crop_w;
    } else {
      len = src_crop_h * src_crop_w * 3;
    }
    memcpy(padding_data, up_sampling_data, len);
  }

}

void uv_coe_legality_judgment(uint8 x_tl, uint8 x_tr, uint8 x_bl, uint8 x_br) {
  VppAssert((x_tl + x_tr + x_bl + x_br) == 4);
}

void vpp_ds_uv_coe_modify(uint8 *coe) {
  if (coe[0] == 3) {
    coe[0] = 4;
  }
}

void vpp_yuv444p_ds_yuv420p(int crop_idx, uint8 *down_sample_data,
  uint8 *csc_data, VPP1686_PARAM *p_param) {
  uint8 u_tl, u_tr, u_bl, u_br;
  uint8 v_tl, v_tr, v_bl, v_br;

  uint8 *p_src_y;
  uint8 *p_src_u;
  uint8 *p_src_v;
  int offset_u;
  int offset_v;

  uint8 *p_dst_y;
  uint8 *p_dst_u;
  uint8 *p_dst_v;

  int h, w;
  int step_uv;

  /*
  p00  p01
  p10  p11
  */

  int p00, p01, p10, p11;

  /*00:0, 01:0.25,10:0.5, 11:1*/
  /*
  bit1 bit0    decimal
   0     0         0
   0      1        0.25 * 4 = 1
   1      0        0.5 * 4 = 2
   1      1        3       1 * 4 = 4, must be modified
  */

  u_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tl;
  u_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tr;
  u_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_bl;
  u_br = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_br;

  v_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tl;
  v_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tr;
  v_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_bl;
  v_br = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_br;

  vpp_ds_uv_coe_modify(&u_tl);
  vpp_ds_uv_coe_modify(&u_tr);
  vpp_ds_uv_coe_modify(&u_bl);
  vpp_ds_uv_coe_modify(&u_br);
  vpp_ds_uv_coe_modify(&v_tl);
  vpp_ds_uv_coe_modify(&v_tr);
  vpp_ds_uv_coe_modify(&v_bl);
  vpp_ds_uv_coe_modify(&v_br);

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_h_even = VPPALIGN(dst_crop_h, 2);
  const int dst_crop_w_even = VPPALIGN(dst_crop_w, 2);

  p_src_y = &csc_data[0];
  p_src_u = &csc_data[dst_crop_h * dst_crop_w];
  p_src_v = &csc_data[dst_crop_h * dst_crop_w * 2];

  step_uv = dst_crop_w_even / 2;
  offset_u = dst_crop_h * dst_crop_w;
  offset_v = offset_u + dst_crop_h_even / 2 * step_uv;

  p_dst_y = &down_sample_data[0];
  p_dst_u = &down_sample_data[offset_u];
  p_dst_v = &down_sample_data[offset_v];

  /*y data*/
  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_y[h * dst_crop_w + w] = p_src_y[h * dst_crop_w + w];
    }
  }

  /*u data*/
  if ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 == 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 == 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
    }
    if (h == (dst_crop_h / 2)) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
    }
  } else if  ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
    }

    if (h == (dst_crop_h / 2)) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
      if (w == (dst_crop_w / 2)) {  // last row and last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = p00;
        p11 = p00;
        p_dst_u[h * step_uv + w] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);
      }
    }
  } else {
    VppErr("error !!\n");
  }

  /*v data*/
  if ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 == 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 == 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
    }
    if (h == (dst_crop_h / 2)) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
    }
  } else if  ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
    }
  } else if  ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
    }

    if (h == (dst_crop_h / 2)) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
      if (w == (dst_crop_w / 2)) {  // last row and last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = p00;
        p11 = p00;
        p_dst_v[h * step_uv + w] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);
      }
    }
  } else {
    VppErr("error !!\n");
  }
}

void vpp_yuv444p_ds_nv12(int crop_idx, uint8 *down_sample_data,
  uint8 *csc_data, VPP1686_PARAM *p_param) {
  uint8 u_tl, u_tr, u_bl, u_br;
  uint8 v_tl, v_tr, v_bl, v_br;

  uint8 *p_src_y;
  uint8 *p_src_u;
  uint8 *p_src_v;

  uint8 *p_dst_y;
  uint8 *p_dst_uv;

  int h, w;
  int step_uv;
  int offset_uv;
  /*
  p00  p01
  p10  p11
  */

  int p00, p01, p10, p11;

  /*00:0, 01:0.25,10:0.5, 11:1*/
  /*
  bit1 bit0    decimal
   0     0         0
   0      1        0.25 * 4 = 1
   1      0        0.5 * 4 = 2
   1      1        3       1 * 4 = 4, must be modified
  */

  u_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tl;
  u_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tr;
  u_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_bl;
  u_br = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_br;

  v_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tl;
  v_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tr;
  v_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_bl;
  v_br = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_br;

  vpp_ds_uv_coe_modify(&u_tl);
  vpp_ds_uv_coe_modify(&u_tr);
  vpp_ds_uv_coe_modify(&u_bl);
  vpp_ds_uv_coe_modify(&u_br);
  vpp_ds_uv_coe_modify(&v_tl);
  vpp_ds_uv_coe_modify(&v_tr);
  vpp_ds_uv_coe_modify(&v_bl);
  vpp_ds_uv_coe_modify(&v_br);

  //VppInfo("after modify u_tl %d, u_tr %d, u_bl %d, u_br %d\n", u_tl, u_tr, u_bl, u_br);
  //VppInfo("after modify v_tl %d, v_tr %d, v_bl %d, v_br %d\n", v_tl, v_tr, v_bl, v_br);

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w_even = VPPALIGN(dst_crop_w, 2);
  //VppInfo("dst_crop_w_even %d\n", dst_crop_w_even);

  p_src_y = &csc_data[0];
  p_src_u = &csc_data[dst_crop_h * dst_crop_w];
  p_src_v = &csc_data[dst_crop_h * dst_crop_w * 2];

  step_uv = dst_crop_w_even;
  offset_uv = dst_crop_h * dst_crop_w;

  p_dst_y = &down_sample_data[0];
  p_dst_uv = &down_sample_data[offset_uv];

  /*y data*/
  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_y[h * dst_crop_w + w] = p_src_y[h * dst_crop_w + w];
    }
  }

  /*uv data*/
  if ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 ==0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] *u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
          if (p_dst_uv[h * step_uv + w * 2] != p_src_u[p00]) {
            //VppInfo("[%dx%d]:p_dst_uv[%d] %d,p_src_u[%d], %d\n", h, w, h * step_uv + w * 2, p_dst_uv[h * step_uv + w * 2], p00, p_src_u[p00]);
          }
         if (p_dst_uv[h * step_uv + w * 2 + 1] != p_src_v[p00]) {
            //VppInfo("[%dx%d]:p_dst_uv[%d] %d,p_src_u[%d], %d\n", h, w, h * step_uv + w * 2 + 1, p_dst_uv[h * step_uv + w * 2 + 1], p00, p_src_v[p00]);
          }

      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 == 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
    }
    if (h == dst_crop_h / 2) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
    }
  } else if ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
    }

    if (h == dst_crop_h / 2) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
      if (w == (dst_crop_w / 2)) {  // last row and last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = p00;
        p11 = p00;

        p_dst_uv[h * step_uv + w * 2] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
        p_dst_uv[h * step_uv + w * 2 + 1] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
      }
    }
  } else {
    VppErr("error!!\n");
  }
}

void vpp_yuv444p_ds_nv21(int crop_idx, uint8 *down_sample_data,
  uint8 *csc_data, VPP1686_PARAM *p_param) {
  uint8 u_tl, u_tr, u_bl, u_br;
  uint8 v_tl, v_tr, v_bl, v_br;

  uint8 *p_src_y;
  uint8 *p_src_u;
  uint8 *p_src_v;

  uint8 *p_dst_y;
  uint8 *p_dst_vu;

  int h, w;
  int step_vu;
  int offset_vu;

  /*
  p00  p01
  p10  p11
  */

  int p00, p01, p10, p11;

  /*00:0, 01:0.25,10:0.5, 11:1*/
  /*
  bit1 bit0    decimal
   0     0         0
   0      1        0.25 * 4 = 1
   1      0        0.5 * 4 = 2
   1      1        3       1 * 4 = 4, must be modified
  */

  u_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tl;
  u_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tr;
  u_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_bl;
  u_br = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_br;

  v_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tl;
  v_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tr;
  v_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_bl;
  v_br = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_br;

  vpp_ds_uv_coe_modify(&u_tl);
  vpp_ds_uv_coe_modify(&u_tr);
  vpp_ds_uv_coe_modify(&u_bl);
  vpp_ds_uv_coe_modify(&u_br);
  vpp_ds_uv_coe_modify(&v_tl);
  vpp_ds_uv_coe_modify(&v_tr);
  vpp_ds_uv_coe_modify(&v_bl);
  vpp_ds_uv_coe_modify(&v_br);

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w_even = VPPALIGN(dst_crop_w, 2);

  p_src_y = &csc_data[0];
  p_src_u = &csc_data[dst_crop_h * dst_crop_w];
  p_src_v = &csc_data[dst_crop_h * dst_crop_w * 2];

  step_vu = dst_crop_w_even;
  offset_vu = dst_crop_h * dst_crop_w;
  p_dst_y = &down_sample_data[0];
  p_dst_vu = &down_sample_data[offset_vu];

  /*y data*/
  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_y[h * dst_crop_w + w] = p_src_y[h * dst_crop_w + w];
    }
  }

  /*uv data*/
  if ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 ==0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] *u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 == 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
    }
    if (h == dst_crop_h / 2) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
    }
  } else if ((dst_crop_h % 2 == 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
    }
  } else if ((dst_crop_h % 2 != 0) && (dst_crop_w % 2 != 0)) {
    for (h = 0; h < dst_crop_h / 2; h++) {
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = (h * 2 + 1) * dst_crop_w + w * 2 + 1;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
      if (w == (dst_crop_w / 2)) {  // last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = (h * 2 + 1) * dst_crop_w + w * 2;
        p11 = p10;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
    }

    if (h == dst_crop_h / 2) {  // last row
      for (w = 0; w < dst_crop_w / 2; w++) {
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = h * 2 * dst_crop_w + w * 2 + 1;
        p10 = p00;
        p11 = p01;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
      if (w == (dst_crop_w / 2)) {  // last row and last col
        p00 = h * 2 * dst_crop_w + w * 2;
        p01 = p00;
        p10 = p00;
        p11 = p00;

        p_dst_vu[h * step_vu + w * 2] = (uint8)((float)(p_src_v[p00] * v_tl +
          p_src_v[p01] * v_tr + p_src_v[p10] * v_bl + p_src_v[p11] * v_br) / 4.0 + 0.5);  // data v
        p_dst_vu[h * step_vu + w * 2 + 1] = (uint8)((float)(p_src_u[p00] * u_tl +
          p_src_u[p01] * u_tr + p_src_u[p10] * u_bl + p_src_u[p11] * u_br) / 4.0 + 0.5);  // data u
      }
    }
  } else {
    VppErr("error!!\n");
  }
}

void vpp_yuv444p_down_sample_top(int crop_idx, uint8 *down_sample_data, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  uint8 u_tl, u_tr, u_bl, u_br;
  uint8 v_tl, v_tr, v_bl, v_br;

  VppAssert((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) ||
    (dst_fmt == OUT_NV21));

  /*00:0, 01:0.25,10:0.5, 11:1*/
  u_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tl;
  u_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_tr;
  u_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_bl;
  u_br = p_param->processor_param[0].DST_CTRL[crop_idx].cb_coeff_sel_br;

  v_tl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tl;
  v_tr = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_tr;
  v_bl = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_bl;
  v_br = p_param->processor_param[0].DST_CTRL[crop_idx].cr_coeff_sel_br;

  VppInfo("u_tl %d, u_tr %d, u_bl %d, u_br %d\n", u_tl, u_tr, u_bl, u_br);
  VppInfo("v_tl %d, v_tr %d, v_bl %d, v_br %d\n", v_tl, v_tr, v_bl, v_br);
  vpp_ds_uv_coe_modify(&u_tl);
  vpp_ds_uv_coe_modify(&u_tr);
  vpp_ds_uv_coe_modify(&u_bl);
  vpp_ds_uv_coe_modify(&u_br);
  vpp_ds_uv_coe_modify(&v_tl);
  vpp_ds_uv_coe_modify(&v_tr);
  vpp_ds_uv_coe_modify(&v_bl);
  vpp_ds_uv_coe_modify(&v_br);

  uv_coe_legality_judgment(u_tl, u_tr, u_bl, u_br);
  uv_coe_legality_judgment(v_tl, v_tr, v_bl, v_br);

  switch (dst_fmt) {
  case OUT_YUV420P:
    vpp_yuv444p_ds_yuv420p(crop_idx, down_sample_data, csc_data, p_param);
  break;
  case OUT_NV12:
    vpp_yuv444p_ds_nv12(crop_idx, down_sample_data, csc_data, p_param);
  break;
  case OUT_NV21:
    vpp_yuv444p_ds_nv21(crop_idx, down_sample_data, csc_data, p_param);
  break;
  default:
    VppErr("err format in vpp_yuv444p_down_sample_top dst_fmt %d\n", dst_fmt);
  break;
}

}


int get_yuv444_down_sample_flag(int dst_fmt) {
  int need_yuv444p_down_sample = 0;

  if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
    need_yuv444p_down_sample = 1;
  }

  return need_yuv444p_down_sample;
}

void get_len_based_on_dst_fmt(int crop_idx,
  int *vpp_out_len, VPP1686_PARAM *p_param) {
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  const int dst_crop_h =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;

  const int dst_crop_h_even = VPPALIGN(dst_crop_h, 2);
  const int dst_crop_w_even = VPPALIGN(dst_crop_w, 2);
  int step_u = dst_crop_w_even / 2;
  int step_v = dst_crop_w_even / 2;
  int step_uv = dst_crop_w_even;

  /*TOOD: dst_crop_st_x and dst_crop_st_y must be considered*/
  switch (dst_fmt) {
    case OUT_YUV400P:
      vpp_out_len[0] = dst_crop_h * dst_crop_w;
      break;

    case OUT_YUV420P:
      vpp_out_len[0] = dst_crop_h * dst_crop_w + dst_crop_h_even /2 * (step_u + step_v);
      break;

    case OUT_NV12:
    case OUT_NV21:
      vpp_out_len[0] = dst_crop_h * dst_crop_w + dst_crop_h_even /2 * step_uv;
      break;

    case OUT_YUV444P:
    case OUT_RGBP:
      vpp_out_len[0] = dst_crop_h * dst_crop_w * 3;
      break;

    case OUT_RGB24:
    case OUT_BGR24:
    case OUT_HSV180:
    case OUT_HSV256:
      vpp_out_len[0] = dst_crop_h * dst_crop_w * 3;
      break;

    case OUT_RGBYP:
      vpp_out_len[0] = dst_crop_h * dst_crop_w * 4;
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }
}

void vpp_out_yuv400p(int crop_idx,
  uint8 *vpp_out_data_u8, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  int h, w;

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      vpp_out_data_u8[h * dst_crop_w + w] = csc_data[h * dst_crop_w + w];
    }
  }
}

void vpp_out_yuv420p(int crop_idx,
  uint8 *vpp_out_data_u8, uint8 *down_sample_data, VPP1686_PARAM *p_param) {
  const int dst_crop_h =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;

  const int dst_crop_h_even = VPPALIGN(dst_crop_h, 2);
  const int dst_crop_w_even = VPPALIGN(dst_crop_w, 2);

  int len_y;
  int len_u;
  int len_v;
  int len_all;

  uint8 *p_dst = &vpp_out_data_u8[0];
  uint8 *p_src = &down_sample_data[0];

  len_y = dst_crop_h * dst_crop_w;
  len_u = dst_crop_h_even / 2 * dst_crop_w_even / 2;
  len_v = dst_crop_h_even / 2 * dst_crop_w_even / 2;
  len_all = len_y + len_u + len_v;

  /*copy down_sample_data to vpp_out_data_u8*/
  memcpy(p_dst, p_src, len_all);
}

void vpp_out_yuv420sp(int crop_idx,
  uint8 *vpp_out_data_u8, uint8 *down_sample_data, VPP1686_PARAM *p_param) {
  const int dst_crop_h =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w =
    p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;

  const int dst_crop_h_even = VPPALIGN(dst_crop_h, 2);
  const int dst_crop_w_even = VPPALIGN(dst_crop_w, 2);

  int len_y;
  int len_uv;
  int len_all;

  uint8 *p_dst = &vpp_out_data_u8[0];
  uint8 *p_src = &down_sample_data[0];

  len_y = dst_crop_h * dst_crop_w;
  len_uv = dst_crop_h_even / 2 * dst_crop_w_even;

  len_all = len_y + len_uv;

  /*copy down_sample_data to vpp_out_data_u8*/
  memcpy(p_dst, p_src, len_all);

}

void vpp_out_three_planar(int crop_idx, uint8 *vpp_out_data_u8, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  int h, w;

  uint8 *p_src_ch0;
  uint8 *p_src_ch1;
  uint8 *p_src_ch2;

  uint8 *p_dst_ch0;
  uint8 *p_dst_ch1;
  uint8 *p_dst_ch2;

  int offset_ch1 = dst_crop_h * dst_crop_w;
  int offset_ch2 = offset_ch1 + dst_crop_h * dst_crop_w;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];

  p_dst_ch0 = &vpp_out_data_u8[0];
  p_dst_ch1 = &vpp_out_data_u8[offset_ch1];
  p_dst_ch2 = &vpp_out_data_u8[offset_ch2];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_ch0[h * dst_crop_w + w] = p_src_ch0[h * dst_crop_w + w];
      p_dst_ch1[h * dst_crop_w + w] = p_src_ch1[h * dst_crop_w + w];
      p_dst_ch2[h * dst_crop_w + w] = p_src_ch2[h * dst_crop_w + w];
    }
  }
}

void vpp_out_four_planar(int crop_idx, uint8 *vpp_out_data_u8, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  int h, w;

  VppAssert(dst_fmt == OUT_RGBYP);

  uint8 *p_src_ch0;
  uint8 *p_src_ch1;
  uint8 *p_src_ch2;
  uint8 *p_src_ch3;

  uint8 *p_dst_ch0;
  uint8 *p_dst_ch1;
  uint8 *p_dst_ch2;
  uint8 *p_dst_ch3;

  int offset_ch1 = dst_crop_h * dst_crop_w;
  int offset_ch2 = offset_ch1 + dst_crop_h * dst_crop_w;
  int offset_ch3 = offset_ch2 + dst_crop_h * dst_crop_w;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];
  p_src_ch3 = &csc_data[dst_crop_h * dst_crop_w * 3];

  p_dst_ch0 = &vpp_out_data_u8[0];
  p_dst_ch1 = &vpp_out_data_u8[offset_ch1];
  p_dst_ch2 = &vpp_out_data_u8[offset_ch2];
  p_dst_ch3 = &vpp_out_data_u8[offset_ch3];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_ch0[h * dst_crop_w + w] = p_src_ch0[h * dst_crop_w + w];
      p_dst_ch1[h * dst_crop_w + w] = p_src_ch1[h * dst_crop_w + w];
      p_dst_ch2[h * dst_crop_w + w] = p_src_ch2[h * dst_crop_w + w];
      p_dst_ch3[h * dst_crop_w + w] = p_src_ch3[h * dst_crop_w + w];
    }
  }

}

void vpp_out_planar_int8(int crop_idx,
  char *vpp_out_data_int8, char *nor_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  int h, w;

  char *p_src_ch0;
  char *p_src_ch1;
  char *p_src_ch2;

  char *p_dst_ch0;
  char *p_dst_ch1;
  char *p_dst_ch2;

  int offset_ch1 = dst_crop_h * dst_crop_w;
  int offset_ch2 = offset_ch1 + dst_crop_h * dst_crop_w;

  p_src_ch0 = &nor_data[0];
  p_src_ch1 = &nor_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &nor_data[dst_crop_h * dst_crop_w * 2];

  p_dst_ch0 = &vpp_out_data_int8[0];
  p_dst_ch1 = &vpp_out_data_int8[offset_ch1];
  p_dst_ch2 = &vpp_out_data_int8[offset_ch2];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_ch0[h * dst_crop_w + w] = p_src_ch0[h * dst_crop_w + w];
      p_dst_ch1[h * dst_crop_w + w] = p_src_ch1[h * dst_crop_w + w];
      p_dst_ch2[h * dst_crop_w + w] = p_src_ch2[h * dst_crop_w + w];
    }
  }

}

void vpp_out_rgb24(int crop_idx,
  uint8 *vpp_out_data_u8, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;

  int h, w;

  uint8 *p_src_ch0;
  uint8 *p_src_ch1;
  uint8 *p_src_ch2;

  uint8 *p_dst;
  int step = dst_crop_w * 3;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];

  p_dst = &vpp_out_data_u8[0];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst[h * step + w * 3 + 0] = p_src_ch0[h * dst_crop_w + w];  // r channel
      p_dst[h * step + w * 3 + 1] = p_src_ch1[h * dst_crop_w + w];  // g channel
      p_dst[h * step + w * 3 + 2] = p_src_ch2[h * dst_crop_w + w];  // b channel
    }
  }

}

void vpp_out_rgb24_int8(int crop_idx,
  char *vpp_out_data_int8, char *nor_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;

  int h, w;

  char *p_src_ch0;
  char *p_src_ch1;
  char *p_src_ch2;

  char *p_dst;
  int step = dst_crop_w * 3;

  p_src_ch0 = &nor_data[0];
  p_src_ch1 = &nor_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &nor_data[dst_crop_h * dst_crop_w * 2];

  p_dst = &vpp_out_data_int8[0];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst[h * step + w * 3 + 0] = p_src_ch0[h * dst_crop_w + w];  // r channel
      p_dst[h * step + w * 3 + 1] = p_src_ch1[h * dst_crop_w + w];  // g channel
      p_dst[h * step + w * 3 + 2] = p_src_ch2[h * dst_crop_w + w];  // b channel
    }
  }

}

void vpp_out_bgr24(int crop_idx,
  uint8 *vpp_out_data_u8, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;

  int h, w;

  uint8 *p_src_ch0;
  uint8 *p_src_ch1;
  uint8 *p_src_ch2;

  uint8 *p_dst;
  int step = dst_crop_w * 3;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];

  p_dst = &vpp_out_data_u8[0];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst[h * step + w * 3 + 0] = p_src_ch2[h * dst_crop_w + w];  // b channel
      p_dst[h * step + w * 3 + 1] = p_src_ch1[h * dst_crop_w + w];  // g channel
      p_dst[h * step + w * 3 + 2] = p_src_ch0[h * dst_crop_w + w];  // r channel
    }
  }

}

void vpp_out_bgr24_int8(int crop_idx,
  char *vpp_out_data_int8, char *nor_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;

  int h, w;

  char *p_src_ch0;
  char *p_src_ch1;
  char *p_src_ch2;

  char *p_dst;
  int step = dst_crop_w * 3;

  p_src_ch0 = &nor_data[0];
  p_src_ch1 = &nor_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &nor_data[dst_crop_h * dst_crop_w * 2];

  p_dst = &vpp_out_data_int8[0];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst[h * step + w * 3 + 0] = p_src_ch2[h * dst_crop_w + w];  // b channel
      p_dst[h * step + w * 3 + 1] = p_src_ch1[h * dst_crop_w + w];  // g channel
      p_dst[h * step + w * 3 + 2] = p_src_ch0[h * dst_crop_w + w];  // r channel
    }
  }

}

/*
*1.if dst fmt is yuv400p,yuv420p, nv12, nv21 and yuv444p, csc_data's format is yuv444p;
*2.if dst fmt is rgb24, bgr24 and rgbp, csc-data's format is rgbp;
*3.if dst fmt is rgby, TODO......;
*4.if dst fmt is yuv420p, nv12 and nv21, down_sample_data is the result of csc_data doing the downsampling
*/
void vpp_out_top_u8(int crop_idx, uint8 *vpp_out_data_u8,
  uint8 *down_sample_data, uint8 *csc_data, VPP1686_PARAM *p_param) {
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  //const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  //const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  //const int con_bri_en = p_param->processor_param[0].SRC_CTRL[crop_idx].con_bri_enable;


  switch (dst_fmt) {
    case OUT_YUV400P:
      vpp_out_yuv400p(crop_idx, vpp_out_data_u8, csc_data, p_param);
      break;

    case OUT_YUV420P:
      vpp_out_yuv420p(crop_idx, vpp_out_data_u8, down_sample_data, p_param);
      break;

    case OUT_NV12:
    case OUT_NV21:
      vpp_out_yuv420sp(crop_idx, vpp_out_data_u8, down_sample_data, p_param);
      break;

    case OUT_YUV444P:
    case OUT_RGBP:
      vpp_out_three_planar(crop_idx, vpp_out_data_u8, csc_data, p_param);
      break;

    case IN_RGB24:
      vpp_out_rgb24(crop_idx, vpp_out_data_u8, csc_data, p_param);
      break;

    case IN_BGR24:
      vpp_out_bgr24(crop_idx, vpp_out_data_u8, csc_data, p_param);
      break;

    case OUT_RGBYP:
      vpp_out_four_planar(crop_idx, vpp_out_data_u8, csc_data, p_param);
      break;

    case OUT_HSV180:
    case OUT_HSV256:
      //if(con_bri_en) vpp_out_rgb24(crop_idx, vpp_out_data_u8, csc_data, p_param);
      //else memcpy(vpp_out_data_u8, csc_data, dst_crop_h * dst_crop_w * 3);
      vpp_out_rgb24(crop_idx, vpp_out_data_u8, csc_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }

}

/*
*1.if dst fmt is yuv400p,yuv420p, nv12, nv21 and yuv444p, csc_data's format is yuv444p;
*2.if dst fmt is rgb24, bgr24 and rgbp, csc-data's format is rgbp;
*3.if dst fmt is rgby, TODO......;
*4.if dst fmt is yuv420p, nv12 and nv21, down_sample_data is the result of csc_data doing the downsampling
*/
void vpp_out_top_int8(int crop_idx,
char *vpp_out_data_int8, char *nor_data, VPP1686_PARAM *p_param) {
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  VppAssert((dst_fmt == OUT_RGB24) || (dst_fmt == OUT_BGR24) ||
    (dst_fmt == OUT_RGBP));

  switch (dst_fmt) {
    case OUT_RGBP:
      vpp_out_planar_int8(crop_idx, vpp_out_data_int8, nor_data, p_param);
      break;

    case IN_RGB24:
      vpp_out_rgb24_int8(crop_idx, vpp_out_data_int8, nor_data, p_param);
      break;

    case IN_BGR24:
      vpp_out_bgr24_int8(crop_idx, vpp_out_data_int8, nor_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }
}

int input_color_space_judgment(uint8 src_fmt) {
  uint8 color_space_in = 0;
  assert_input_format(src_fmt);

  color_space_in = ((src_fmt >= IN_RGBP) && (src_fmt <= IN_BGR24)) ? 0 : 1;
  return color_space_in;
}

void vpp_font_alpha_blending(uint64 font_addr, int crop_idx, uint8 *padding_data, VPP1686_PARAM *p_param) {
  uint32 src_font_pitch = 0;
  uint32 src_font_nf_range = 0;  // not consider this flag now
  uint32 src_font_value_r = 0;
  uint32 src_font_value_g = 0;
  uint32 src_font_value_b = 0;
  int src_font_size_w = 0;
  int src_font_size_h = 0;
  int src_font_st_x = 0;
  int src_font_st_y = 0;
  // uint32 src_font_addr = 0;
  // uint32 src_font_ext = 0;
  uint8 *p_ch0;
  uint8 *p_ch1;
  uint8 *p_ch2;
  uint32 data_offset;
  uint32 data_offset_uv;
  uint8 data_ch0;
  uint8 data_ch1;
  uint8 data_ch2;
  int row, col;
  //int bit, cl;
  int cl;
  int color_space_in;

  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  color_space_in = input_color_space_judgment(src_fmt);

  VppAssert(src_fmt == dst_fmt);

  src_font_pitch = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_pitch;
  src_font_nf_range = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_nf_range;
  src_font_value_r = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_r;
  src_font_value_g = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_g;
  src_font_value_b = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_b;
  src_font_size_w = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_width;
  src_font_size_h = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_height;
  src_font_st_x = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_x;
  src_font_st_y = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_y;
#if 1
  if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
    src_font_st_x = ((src_font_st_x >> 1) << 1);
    src_font_st_y = ((src_font_st_y >> 1) << 1);
    VppAssert(src_font_st_x % 2 == 0);
    VppAssert(src_font_st_y % 2 == 0);
  }
#endif
  p_ch0 = &padding_data[0];
  p_ch1 = &padding_data[src_crop_h * src_crop_w];
  p_ch2 = &padding_data[src_crop_h * src_crop_w * 2];

  int f_len = src_font_size_h * src_font_pitch;
 // unsigned char font_bitmap[f_len];
  unsigned char *font_bitmap;
  font_bitmap = new unsigned char[f_len];

  memcpy((void *)font_bitmap, (void *)font_addr,f_len);

  for (row = 0; row < src_font_size_h; row ++) {
    if (src_font_st_y + row < 0) {
      continue;
    }
    if (src_font_st_y + row >= src_crop_h) {
      break;
    }

    for (col = 0; col < src_font_size_w; col++) {
      cl = font_bitmap[row * src_font_pitch + col];
      if (cl == 0) {
        continue;
      }
      if (src_font_st_x + col < 0) {
        continue;
      }
      if (src_font_st_x + col >= src_crop_w) {
        break;
      }

      int src_row = src_font_st_y + row;
      int src_col = src_font_st_x + col;

      if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
        //int src_row_even = ((src_row >> 1) << 1);
        //int src_col_even  = ((src_col >> 1) << 1);
        data_offset = src_row * src_crop_w + src_col;
        //data_offset_uv = src_row_even * src_crop_w + src_col_even;
        data_offset_uv = src_row * src_crop_w + src_col;
      } else {
        data_offset = src_row * src_crop_w + src_col;
        data_offset_uv = src_row * src_crop_w + src_col;
      }

      data_ch0 = p_ch0[data_offset];
      p_ch0[data_offset] = ((src_font_value_r - data_ch0) * cl + (data_ch0 << 8) + (1 << 7)) >> 8;

      if (src_fmt != IN_YUV400P) {
        data_ch1 = p_ch1[data_offset_uv];
        data_ch2 = p_ch2[data_offset_uv];
        p_ch1[data_offset_uv] = ((src_font_value_g - data_ch1) * cl + (data_ch1 << 8) + (1 << 7)) >> 8;
        p_ch2[data_offset_uv] = ((src_font_value_b - data_ch2) * cl + (data_ch2 << 8) + (1 << 7)) >> 8;
      }

      //VppInfo("color_space_in %d, src_font_nf_range %d\n", color_space_in, src_font_nf_range);
      /*out data clip according src fmt and data range*/
      if (src_font_nf_range == 0) {  // full range
        VppInfo("0 <= uint8 <=255\n");
#if 0
        p_ch0[data_offset] = clip(p_ch0[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
        if (src_fmt != IN_YUV400P) {
          p_ch1[data_offset_uv] = clip(p_ch1[data_offset_uv], FULL_RANGE_MIN, FULL_RANGE_MAX);
          p_ch2[data_offset_uv] = clip(p_ch2[data_offset_uv], FULL_RANGE_MIN, FULL_RANGE_MAX);
        }
#endif
      } else if ((src_font_nf_range == 1) && (color_space_in == COLOR_SPACE_IN_YUV)) {  // not full range and src color space is yuv
        int temp = p_ch0[data_offset];
        p_ch0[data_offset] = clip(p_ch0[data_offset], NOT_FULL_RANGE_MIN_YUV, NOT_FULL_RANGE_MAX_Y);
        if ((temp < NOT_FULL_RANGE_MIN_YUV) || (temp > NOT_FULL_RANGE_MAX_Y)) {
          VppInfo("bf clip %d, after clip p_ch0[data_offset] %d, data_offset %d\n", temp, p_ch0[data_offset], data_offset);
        }

        if (src_fmt != IN_YUV400P) {
          p_ch1[data_offset_uv] = clip(p_ch1[data_offset_uv], NOT_FULL_RANGE_MIN_YUV, NOT_FULL_RANGE_MAX_UV);
          p_ch2[data_offset_uv] = clip(p_ch2[data_offset_uv], NOT_FULL_RANGE_MIN_YUV, NOT_FULL_RANGE_MAX_UV);
        }
      } else {
        VppInfo("0 <= uint8 <=255\n");
#if 0
        p_ch0[data_offset] = clip(p_ch0[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
        p_ch1[data_offset_uv] = clip(p_ch1[data_offset_uv], FULL_RANGE_MIN, FULL_RANGE_MAX);
        p_ch2[data_offset_uv] = clip(p_ch2[data_offset_uv], FULL_RANGE_MIN, FULL_RANGE_MAX);
#endif
      }
    }
  }
  delete [] font_bitmap;
}

void vpp_font_alpha_blending_yuv420_dot(uint64 font_addr, int crop_idx, uint8 *padding_data,
  VPP1686_PARAM *p_param) {
  uint32 src_font_pitch = 0;
  uint32 src_font_nf_range = 0;  // not consider this flag now
  uint32 src_font_value_r = 0;
  uint32 src_font_value_g = 0;
  uint32 src_font_value_b = 0;
  int src_font_size_w = 0;
  int src_font_size_h = 0;
  int src_font_st_x = 0;
  int src_font_st_y = 0;
  uint8 *p_ch0;
  uint8 *p_ch1;
  uint8 *p_ch2;
  uint32 data_offset;
  uint8 data_ch0;
  uint8 data_ch1;
  uint8 data_ch2;
  int row, col;
  int cl;
  int color_space_in;
  int src_row_even;
  int src_col_even;

  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  color_space_in = input_color_space_judgment(src_fmt);

  VppAssert(src_fmt == dst_fmt);
  VppAssert((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21));

  src_font_pitch = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_pitch;
  src_font_nf_range = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_nf_range;
  src_font_value_r = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_r;
  src_font_value_g = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_g;
  src_font_value_b = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_b;
  src_font_size_w = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_width;
  src_font_size_h = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_height;
  src_font_st_x = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_x;
  src_font_st_y = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_y;

  src_font_st_x = ((src_font_st_x >> 1) << 1);
  src_font_st_y = ((src_font_st_y >> 1) << 1);
  VppAssert(src_font_st_x % 2 == 0);
  VppAssert(src_font_st_y % 2 == 0);

  p_ch0 = &padding_data[0];
  p_ch1 = &padding_data[src_crop_h * src_crop_w];
  p_ch2 = &padding_data[src_crop_h * src_crop_w * 2];

  int f_len = src_font_size_h * src_font_pitch;
  //unsigned char font_bitmap[f_len];
  unsigned char *font_bitmap;
  font_bitmap = new unsigned char[f_len];

  memcpy((void *)font_bitmap, (void *)font_addr,f_len);

  for (row = 0; row < src_font_size_h; row ++) {
    if (src_font_st_y + row < 0) {
      continue;
    }
    if (src_font_st_y + row >= src_crop_h) {
      break;
    }

    for (col = 0; col < src_font_size_w; col++) {
      cl = font_bitmap[row * src_font_pitch + col];
      if (cl == 0) {
        continue;
      }
      if (src_font_st_x + col < 0) {
        continue;
      }
      if (src_font_st_x + col >= src_crop_w) {
        break;
      }

      int src_row = src_font_st_y + row;
      int src_col = src_font_st_x + col;

      if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
        src_row_even = ((src_row >> 1) << 1);
        src_col_even  = ((src_col >> 1) << 1);

        if ((src_row_even != src_row) || (src_col_even != src_col)) {
          continue;
        }
      }
      data_offset = src_row * src_crop_w + src_col;

      data_ch0 = p_ch0[data_offset];
      p_ch0[data_offset] = ((src_font_value_r - data_ch0) * cl + (data_ch0 << 8) + (1 << 7)) >> 8;

      if (src_fmt != IN_YUV400P) {
        data_ch1 = p_ch1[data_offset];
        data_ch2 = p_ch2[data_offset];
        p_ch1[data_offset] = ((src_font_value_g - data_ch1) * cl + (data_ch1 << 8) + (1 << 7)) >> 8;
        p_ch2[data_offset] = ((src_font_value_b - data_ch2) * cl + (data_ch2 << 8) + (1 << 7)) >> 8;
      }

      //VppInfo("color_space_in %d, src_font_nf_range %d\n", color_space_in, src_font_nf_range);
      /*out data clip according src fmt and data range*/
      if (src_font_nf_range == 0) {  // full range
        VppInfo("0 <= uint8 <=255\n");
#if 0
        p_ch0[data_offset] = clip(p_ch0[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
        if (src_fmt != IN_YUV400P) {
          p_ch1[data_offset] = clip(p_ch1[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
          p_ch2[data_offset] = clip(p_ch2[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
        }
#endif
      } else if ((src_font_nf_range == 1) && (color_space_in == COLOR_SPACE_IN_YUV)) {  // not full range and src color space is yuv
        int temp = p_ch0[data_offset];
        p_ch0[data_offset] = clip(p_ch0[data_offset], NOT_FULL_RANGE_MIN_YUV, NOT_FULL_RANGE_MAX_Y);
        if ((temp < NOT_FULL_RANGE_MIN_YUV) || (temp > NOT_FULL_RANGE_MAX_Y)) {
          VppInfo("bf clip %d, after clip p_ch0[data_offset] %d, data_offset %d\n", temp, p_ch0[data_offset], data_offset);
        }

        if (src_fmt != IN_YUV400P) {
          p_ch1[data_offset] = clip(p_ch1[data_offset], NOT_FULL_RANGE_MIN_YUV, NOT_FULL_RANGE_MAX_UV);
          p_ch2[data_offset] = clip(p_ch2[data_offset], NOT_FULL_RANGE_MIN_YUV, NOT_FULL_RANGE_MAX_UV);
        }
      } else {
        VppInfo("0 <= uint8 <=255\n");
#if 0
        p_ch0[data_offset] = clip(p_ch0[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
        p_ch1[data_offset] = clip(p_ch1[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
        p_ch2[data_offset] = clip(p_ch2[data_offset], FULL_RANGE_MIN, FULL_RANGE_MAX);
#endif
      }
    }
  }
    delete [] font_bitmap;
}

void vpp_font_binary(uint64 font_addr, int crop_idx, uint8 *padding_data, VPP1686_PARAM *p_param) {
  uint32 src_font_pitch = 0;
  uint32 src_font_value_r = 0;
  uint32 src_font_value_g = 0;
  uint32 src_font_value_b = 0;
  int src_font_size_w = 0;
  int src_font_size_h = 0;
  int src_font_st_x = 0;
  int src_font_st_y = 0;
  // uint32 src_font_addr = 0;
  // uint32 src_font_ext = 0;
  uint8 *p_ch0;
  uint8 *p_ch1;
  uint8 *p_ch2;
  uint32 data_offset;
  uint32 data_offset_uv;
  int row, col;
  //int bit, cl;
  //int cl;
  uint8 cl;

  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  VppAssert(src_fmt == dst_fmt);

  src_font_pitch = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_pitch;
  src_font_value_r = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_r;
  src_font_value_g = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_g;
  src_font_value_b = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_b;
  src_font_size_w = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_width;
  src_font_size_h = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_height;

  src_font_st_x = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_x;
  src_font_st_y = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_y;

  if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
    src_font_st_x = ((src_font_st_x >> 1) << 1);
    src_font_st_y = ((src_font_st_y >> 1) << 1);
    VppAssert(src_font_st_x % 2 == 0);
    VppAssert(src_font_st_y % 2 == 0);
  }

  p_ch0 = &padding_data[0];
  p_ch1 = &padding_data[src_crop_h * src_crop_w];
  p_ch2 = &padding_data[src_crop_h * src_crop_w * 2];

  int f_len = src_font_size_h * src_font_pitch;
  //unsigned char font_bitmap[f_len];
  unsigned char *font_bitmap;
  font_bitmap = new unsigned char[f_len];

  memcpy((void *)font_bitmap, (void *)font_addr,f_len);

  for (row = 0; row < src_font_size_h; row++) {
    if (src_font_st_y + row < 0) {
      continue;
    }
    if (src_font_st_y + row >= src_crop_h) {
      break;
    }

    for (col = 0; col < src_font_size_w; col++) {
      cl = font_bitmap[row * src_font_pitch + col / 8];
      if (cl == 0) {
        continue;
      }

      if (cl & (0x1 << (col % 8))) {
        int src_row = src_font_st_y + row;
        int src_col = src_font_st_x + col;

        if (src_row >= 0 && src_row < src_crop_h && src_col >= 0 && src_col < src_crop_w) {
          if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
            //int src_row_even = ((src_row >> 1) << 1);
            //int src_col_even  = ((src_col >> 1) << 1);
            data_offset = src_row * src_crop_w + src_col;
            //data_offset_uv = src_row_even * src_crop_w + src_col_even;
            data_offset_uv = src_row * src_crop_w + src_col;
          } else {
            data_offset = src_row * src_crop_w + src_col;
            data_offset_uv = src_row * src_crop_w + src_col;
          }
          p_ch0[data_offset] = src_font_value_r;

          if (src_fmt != IN_YUV400P) {
            p_ch1[data_offset_uv] = src_font_value_g;
            p_ch2[data_offset_uv] = src_font_value_b;
          }
        }
      }
    }
  }
    delete [] font_bitmap;
}

/*when src_fmt is yuv420, nv12 or nv21:
Change the value of y, u and v only when the coordinates are even rows and even columns*/
void vpp_font_binary_yuv420_dot(uint64 font_addr, int crop_idx, uint8 *padding_data, VPP1686_PARAM *p_param) {
  uint32 src_font_pitch = 0;
  uint32 src_font_value_r = 0;
  uint32 src_font_value_g = 0;
  uint32 src_font_value_b = 0;
  int src_font_size_w = 0;
  int src_font_size_h = 0;
  int src_font_st_x = 0;
  int src_font_st_y = 0;
  uint8 *p_ch0;
  uint8 *p_ch1;
  uint8 *p_ch2;
  uint32 data_offset;
  int row, col;
  uint8 cl;
  int src_row_even;
  int src_col_even;

  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  VppAssert(src_fmt == dst_fmt);
  VppAssert((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21));

  src_font_pitch = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_pitch;
  src_font_value_r = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_r;
  src_font_value_g = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_g;
  src_font_value_b = p_param->processor_param[0].SRC_FONT_VALUE[crop_idx].src_font_value_b;
  src_font_size_w = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_width;
  src_font_size_h = p_param->processor_param[0].SRC_FONT_SIZE[crop_idx].src_font_height;

  src_font_st_x = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_x;
  src_font_st_y = p_param->processor_param[0].SRC_FONT_ST[crop_idx].src_font_st_y;

  src_font_st_x = ((src_font_st_x >> 1) << 1);
  src_font_st_y = ((src_font_st_y >> 1) << 1);

  VppAssert(src_font_st_x % 2 == 0);
  VppAssert(src_font_st_y % 2 == 0);

  p_ch0 = &padding_data[0];
  p_ch1 = &padding_data[src_crop_h * src_crop_w];
  p_ch2 = &padding_data[src_crop_h * src_crop_w * 2];

  int f_len = src_font_size_h * src_font_pitch;
  //unsigned char font_bitmap[f_len];
  unsigned char *font_bitmap;
  font_bitmap = new unsigned char[f_len];

  memcpy((void *)font_bitmap, (void *)font_addr,f_len);

  for (row = 0; row < src_font_size_h; row++) {
    if (src_font_st_y + row < 0) {
      continue;
    }
    if (src_font_st_y + row >= src_crop_h) {
      break;
    }

    for (col = 0; col < src_font_size_w; col++) {
      cl = font_bitmap[row * src_font_pitch + col / 8];
      if (cl == 0) {
        continue;
      }

      if (cl & (0x1 << (col % 8))) {
        int src_row = src_font_st_y + row;
        int src_col = src_font_st_x + col;

        if (src_row >= 0 && src_row < src_crop_h && src_col >= 0 && src_col < src_crop_w) {
          if ((dst_fmt == OUT_YUV420P) || (dst_fmt == OUT_NV12) || (dst_fmt == OUT_NV21)) {
            src_row_even = ((src_row >> 1) << 1);
            src_col_even = ((src_col >> 1) << 1);

            if ((src_row_even != src_row) || (src_col_even != src_col)) {
              continue;
            }
          }

          data_offset = src_row * src_crop_w + src_col;
          p_ch0[data_offset] = src_font_value_r;

          if (src_fmt != IN_YUV400P) {
            p_ch1[data_offset] = src_font_value_g;
            p_ch2[data_offset] = src_font_value_b;
          }
        }
      }
    }
  }
    delete [] font_bitmap;
}


void vpp_font_top(int crop_idx, uint8 *padding_data, VPP1686_PARAM *p_param) {
  uint32 src_font_type = 0;
  uint32 src_font_dot_en = 0;
  uint64 font_addr;

  font_addr = p_param->processor_param[0].SRC_FONT_ADDR;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  src_font_type = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_type;
  src_font_dot_en = p_param->processor_param[0].SRC_FONT_PITCH[crop_idx].src_font_dot_en;

  VppAssert(src_fmt == dst_fmt);
  VppAssert((src_font_type == 1) || (src_font_type == 0));


  if (src_font_type == 0) {
    if (((src_fmt == IN_YUV420P) || (src_fmt == IN_NV12) || (src_fmt == IN_NV21)) && (src_font_dot_en == 1)) {
      vpp_font_alpha_blending_yuv420_dot(font_addr, crop_idx, padding_data, p_param);
    } else {
      vpp_font_alpha_blending(font_addr, crop_idx, padding_data, p_param);
    }
  } else {
    if (((src_fmt == IN_YUV420P) || (src_fmt == IN_NV12) || (src_fmt == IN_NV21)) && (src_font_dot_en == 1)) {
      vpp_font_binary_yuv420_dot(font_addr, crop_idx, padding_data, p_param);
    } else {
      vpp_font_binary(font_addr, crop_idx, padding_data, p_param);
    }
  }

}

void vpp_rect_top(int crop_idx, uint8 *padding_data, VPP1686_PARAM *p_param) {
  uint32 src_border_value_r;
  uint32 src_border_value_g;
  uint32 src_border_value_b;
  int src_border_thickness;
  int src_border_height;
  int src_border_width;
  int src_border_st_y;
  int src_border_st_x;
  uint8 *p_ch0;
  uint8 *p_ch1;
  uint8 *p_ch2;

  int src_border_height_ext;
  int src_border_width_ext;
  int src_border_st_y_ext;
  int src_border_st_x_ext;
  int h, w, data_offset;

  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  VppAssert(src_fmt == dst_fmt);

  src_border_value_r = p_param->processor_param[0].SRC_BORDER_VALUE[crop_idx].src_border_value_r;
  src_border_value_g = p_param->processor_param[0].SRC_BORDER_VALUE[crop_idx].src_border_value_g;
  src_border_value_b = p_param->processor_param[0].SRC_BORDER_VALUE[crop_idx].src_border_value_b;
  src_border_thickness = p_param->processor_param[0].SRC_BORDER_VALUE[crop_idx].src_border_thickness;
  src_border_height = p_param->processor_param[0].SRC_BORDER_SIZE[crop_idx].src_border_height;
  src_border_width = p_param->processor_param[0].SRC_BORDER_SIZE[crop_idx].src_border_width;
  src_border_st_y = p_param->processor_param[0].SRC_BORDER_ST[crop_idx].src_border_st_y;
  src_border_st_x = p_param->processor_param[0].SRC_BORDER_ST[crop_idx].src_border_st_x;

  p_ch0 = &padding_data[0];
  p_ch1 = NULL;
  p_ch2 = NULL;

  if (src_fmt != IN_YUV400P) {
    p_ch1 = &padding_data[src_crop_h * src_crop_w];
    p_ch2 = &padding_data[src_crop_h * src_crop_w * 2];
  }

  /*src_border_st_y_ext may be < 0;
  *src_border_st_y_ext may not be inside src crop;
  *
  *src_border_st_x_ext is the same as src_border_st_y_ext;
  */
#if 0
  if ((src_fmt == IN_YUV420P) || (src_fmt == IN_NV12) || (src_fmt == IN_NV21)) {
    src_border_st_y = ((src_border_st_y >> 1) << 1);
    src_border_st_x = ((src_border_st_x >> 1) << 1);
    src_border_thickness = VPPALIGN(src_border_thickness, 2);
    src_border_height = VPPALIGN(src_border_height, 2);
    src_border_width = VPPALIGN(src_border_width, 2);

    VppAssert(src_border_st_y % 2 == 0);
    VppAssert(src_border_st_x % 2 == 0);
    VppAssert(src_border_thickness % 2 == 0);
    VppAssert(src_border_height % 2 == 0);
    VppAssert(src_border_width % 2 == 0);
  }
#endif
  src_border_st_y_ext = src_border_st_y - src_border_thickness;
  src_border_st_x_ext = src_border_st_x - src_border_thickness;


  /*src_border_height_ext may be bigger than src_crop_h;
  *src_border_height_ext may not be inside src crop;
  *src_border_width_ext may not be inside src crop;
  */

  src_border_height_ext = src_border_height + src_border_thickness * 2;
  src_border_width_ext = src_border_width + src_border_thickness * 2;

  VppInfo("src_border_st_y %d, src_border_height %d, src_border_thickness %d, src_border_st_y_ext %d\n",
    src_border_st_y, src_border_height, src_border_thickness, src_border_st_y_ext);
    VppInfo("src_border_st_x %d, src_border_width %d, src_border_thickness %d, src_border_st_x_ext %d\n",
    src_border_st_x, src_border_width, src_border_thickness, src_border_st_x_ext);

  for (h = src_border_st_y_ext; h < (src_border_st_y_ext + src_border_height_ext); h++) {
    for (w = src_border_st_x_ext; w < (src_border_st_x_ext + src_border_width_ext); w++) {
      if ((h < 0) || (h >= src_crop_h)) {
        continue;
      }
      if ((w < 0) || (w >= src_crop_w)) {
        continue;
      }

#if 1
      data_offset = h * src_crop_w + w;
#else
      int h_even = (h % 2 == 0) ? h : (h - 1);
      int w_even  = (w % 2 == 0) ? w : (w - 1);
      data_offset = h_even * src_crop_w + w_even;
#endif

      if (h < src_border_st_y) {
              //printf("h %d, w %d\n", h, w);
        p_ch0[data_offset] = src_border_value_r;

        if (src_fmt != IN_YUV400P) {
          p_ch1[data_offset] = src_border_value_g;
          p_ch2[data_offset] = src_border_value_b;
        }
      } else if ((h >= src_border_st_y) && (h < (src_border_st_y + src_border_height))) {
        if ((w < src_border_st_x) || (w >= (src_border_st_x + src_border_width))) {
                //printf("h %d, w %d\n", h, w);
          p_ch0[data_offset] = src_border_value_r;

          if (src_fmt != IN_YUV400P) {
            p_ch1[data_offset] = src_border_value_g;
            p_ch2[data_offset] = src_border_value_b;
          }
        }
      }else if (h >= (src_border_st_y + src_border_height)) {
            //printf("h %d, w %d\n", h, w);
        p_ch0[data_offset] = src_border_value_r;

        if (src_fmt != IN_YUV400P) {
          p_ch1[data_offset] = src_border_value_g;
          p_ch2[data_offset] = src_border_value_b;
        }
      }
    }
  }
}


int two_ch(int format_in) {
  int is_two_ch = 0;

  switch (format_in) {
  case IN_YUV400P:
  case IN_YUV420P:
  case IN_FBC:
  case IN_YUV422P:
  case IN_YUV444P:
  case IN_RGBP:
  case IN_YUV444_PACKET:
  case IN_YVU444_PACKET:
  case IN_RGB24:
  case IN_BGR24:
  case IN_YUV422_YUYV:
  case IN_YUV422_YVYU:
  case IN_YUV422_UYVY:
  case IN_YUV422_VYUY:    
    is_two_ch = 0;
    break;

  case IN_NV12:
  case IN_NV21:
  case IN_NV16:
  case IN_NV61:
    is_two_ch = 1;
    break;

  default:  // error input format
    VppErr("get src crop input format err!\n");
    break;
  }
  return is_two_ch;
}

int three_ch(int format_in) {
  int is_three_ch = 0;

  switch (format_in) {
  case IN_YUV400P:
  case IN_YUV444_PACKET:
  case IN_YVU444_PACKET:
  case IN_RGB24:
  case IN_BGR24:
  case IN_NV12:
  case IN_NV21:
  case IN_NV16:
  case IN_NV61:
  case IN_YUV422_YUYV:
  case IN_YUV422_YVYU:
  case IN_YUV422_UYVY:
  case IN_YUV422_VYUY:    
    is_three_ch = 0;
    break;

  case IN_YUV420P:
  case IN_FBC:
  case IN_YUV422P:
  case IN_YUV444P:
  case IN_RGBP:
    is_three_ch = 1;
    break;

  default:  // error input format
    VppErr("get src crop input format err!\n");
    break;
  }
  return is_three_ch;
}


static void gen_input_bin_file_yuv420p(uint8 * video_in, uint8 *in_ch0,
  uint8 *in_ch1, uint8 *in_ch2, int height, int stride_c0, int stride_c1, int stride_c2) {
  int h, w;

  VppAssert(video_in != NULL);
  VppAssert(in_ch0 != NULL);
  VppAssert(in_ch1 != NULL);
  VppAssert(in_ch2 != NULL);

  int height_u_v = VPPALIGN(height, 2) / 2;
#ifdef WRITE_IN_FILE
  FILE *fp_c0;
  FILE *fp_c1;
  FILE *fp_c2;

  fp_c0 = fopen(IN_FILE_CH_0, "wb");
  if (!fp_c0) {
    VppErr("Unable to open the input channal 0 file\n");
    exit(-1);
  }
  fwrite(in_ch0, 1, height * stride_c0, fp_c0);
  fclose(fp_c0);
#endif
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c0; w++) {
      video_in[h * stride_c0 + w] = in_ch0[h * stride_c0 + w];
    }
  }
#ifdef WRITE_IN_FILE
  fp_c1 = fopen(IN_FILE_CH_1, "wb");
  if (!fp_c1) {
    VppErr("Unable to open the input channal 1 file\n");
    exit(-1);
  }
  fwrite(in_ch1, 1, height_u_v * stride_c1, fp_c1);
  fclose(fp_c1);
#endif

  int ofs = height * stride_c0;
  for (h = 0; h < height_u_v; h++) {
    for (w = 0; w < stride_c1; w++) {
      video_in[ofs + h * stride_c1 + w] = in_ch1[h * stride_c1 + w];
    }
  }
#ifdef WRITE_IN_FILE
  fp_c2 = fopen(IN_FILE_CH_2, "wb");
  if (!fp_c2) {
    VppErr("Unable to open the input channal 2 file\n");
    exit(-1);
  }
  fwrite(in_ch2, 1, height_u_v * stride_c2, fp_c2);
  fclose(fp_c2);
#endif
  ofs += height_u_v * stride_c1;
  for (h = 0; h < height_u_v; h++) {
    for (w = 0; w < stride_c2; w++) {
      video_in[ofs + h * stride_c2 + w] = in_ch2[h * stride_c2 + w];
    }
  }
}

static void gen_input_bin_file_yuv420sp(uint8 * video_in, uint8 *in_ch0,
  uint8 *in_ch1, int height, int stride_c0, int stride_c1) {
  int h, w;

  VppAssert(video_in != NULL);
  VppAssert(in_ch0 != NULL);
  VppAssert(in_ch1 != NULL);

  int height_uv = VPPALIGN(height, 2) / 2;
#ifdef WRITE_IN_FILE
  FILE *fp_c0;
  FILE *fp_c1;

  fp_c0 = fopen(IN_FILE_CH_0, "wb");
  if (!fp_c0) {
    VppErr("Unable to open the input channal 0 file\n");
    exit(-1);
  }
  fwrite(in_ch0, 1, height * stride_c0, fp_c0);
  fclose(fp_c0);
#endif
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c0; w++) {
      video_in[h * stride_c0 + w] = in_ch0[h * stride_c0 + w];
    }
  }
#ifdef WRITE_IN_FILE
  fp_c1 = fopen(IN_FILE_CH_1, "wb");
  if (!fp_c1) {
    VppErr("Unable to open the input channal 1 file\n");
    exit(-1);
  }
  fwrite(in_ch1, 1, height_uv * stride_c1, fp_c1);
  fclose(fp_c1);
#endif
  int ofs = height * stride_c0;
  for (h = 0; h < height_uv; h++) {
    for (w = 0; w < stride_c1; w++) {
      video_in[ofs + h * stride_c1 + w] = in_ch1[h * stride_c1 + w];
    }
  }
}

static void gen_input_bin_file_yuv422sp(uint8 * video_in, uint8 *in_ch0,
  uint8 *in_ch1, int height, int stride_c0, int stride_c1) {
  int h, w;

  VppAssert(video_in != NULL);
  VppAssert(in_ch0 != NULL);
  VppAssert(in_ch1 != NULL);
#ifdef WRITE_IN_FILE
  FILE *fp_c0;
  FILE *fp_c1;

  fp_c0 = fopen(IN_FILE_CH_0, "wb");
  if (!fp_c0) {
    VppErr("Unable to open the input channal 0 file\n");
    exit(-1);
  }
  fwrite(in_ch0, 1, height * stride_c0, fp_c0);
  fclose(fp_c0);
#endif
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c0; w++) {
      video_in[h * stride_c0 + w] = in_ch0[h * stride_c0 + w];
    }
  }
#ifdef WRITE_IN_FILE
  fp_c1 = fopen(IN_FILE_CH_1, "wb");
  if (!fp_c1) {
    VppErr("Unable to open the input channal 1 file\n");
    exit(-1);
  }
  fwrite(in_ch1, 1, height * stride_c1, fp_c1);
  fclose(fp_c1);
#endif
  int ofs = height * stride_c0;
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c1; w++) {
      video_in[ofs + h * stride_c1 + w] = in_ch1[h * stride_c1 + w];
    }
  }
}

static void gen_input_bin_file_packet(uint8 * video_in, uint8 *in_ch0,
  int height, int stride_c0) {
  int h, w;

  VppAssert(video_in != NULL);
  VppAssert(in_ch0 != NULL);
#ifdef WRITE_IN_FILE
  FILE *fp;
  fp = fopen(IN_FILE_CH_0, "wb");
  if (!fp) {
    VppErr("Unable to open the input channal 0 file\n");
    exit(-1);
  }
  fwrite(in_ch0, 1, height * stride_c0, fp);
  fclose(fp);
#endif
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c0; w++) {
      video_in[h * stride_c0 + w] = in_ch0[h * stride_c0 + w];
    }
  }
}

static void gen_input_bin_file_planar(uint8 * video_in, uint8 *in_ch0,
  uint8 *in_ch1, uint8 *in_ch2, int height, int stride_c0, int stride_c1, int stride_c2) {
  int h, w;

  VppAssert(video_in != NULL);
  VppAssert(in_ch0 != NULL);
  VppAssert(in_ch1 != NULL);
  VppAssert(in_ch2 != NULL);
#ifdef WRITE_IN_FILE
  FILE *fp_c0;
  FILE *fp_c1;
  FILE *fp_c2;

  fp_c0 = fopen(IN_FILE_CH_0, "wb");
  if (!fp_c0) {
    VppErr("Unable to open the input channal 0 file\n");
    exit(-1);
  }
  fwrite(in_ch0, 1, height * stride_c0, fp_c0);
  fclose(fp_c0);
#endif
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c0; w++) {
      video_in[h * stride_c0 + w] = in_ch0[h * stride_c0 + w];
    }
  }
#ifdef WRITE_IN_FILE
  fp_c1 = fopen(IN_FILE_CH_1, "wb");
  if (!fp_c1) {
    VppErr("Unable to open the input channal 1 file\n");
    exit(-1);
  }
  fwrite(in_ch1, 1, height * stride_c1, fp_c1);
  fclose(fp_c1);
#endif
  int ofs = height * stride_c0;
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c1; w++) {
      video_in[ofs + h * stride_c1 + w] = in_ch1[h * stride_c1 + w];
    }
  }
#ifdef WRITE_IN_FILE
  fp_c2 = fopen(IN_FILE_CH_2, "wb");
  if (!fp_c2) {
    VppErr("Unable to open the input channal 2 file\n");
    exit(-1);
  }
  fwrite(in_ch2, 1, height * stride_c2, fp_c2);
  fclose(fp_c2);
#endif
  ofs += height * stride_c1;
  for (h = 0; h < height; h++) {
    for (w = 0; w < stride_c2; w++) {
      video_in[ofs + h * stride_c2 + w] = in_ch2[h * stride_c2 + w];
    }
  }
}

void read_src_data(
  int format_in, 
  uint8 * video_in, uint8 * in_ch0, uint8 * in_ch1, uint8 * in_ch2, 
  int src_height, int stride_c0, int stride_c1, int stride_c2){
  assert_input_format(format_in);

  if (format_in == IN_YUV400P) {  // yuv400p
    gen_input_bin_file_packet(video_in, in_ch0, src_height, stride_c0);
  } else if ((format_in == IN_YUV420P) || (format_in == IN_FBC)) {  // yuv420p(i420)
    gen_input_bin_file_yuv420p(video_in, in_ch0, in_ch1, in_ch2, src_height, stride_c0, stride_c1, stride_c2);
  } else if ((format_in == IN_NV12) || (format_in == IN_NV21)) {  // nv12 nv21
    gen_input_bin_file_yuv420sp(video_in, in_ch0, in_ch1, src_height, stride_c0, stride_c1);
  } else if (format_in == IN_YUV422P) {  // yuv422p
    gen_input_bin_file_planar(video_in, in_ch0, in_ch1, in_ch2, src_height, stride_c0, stride_c1, stride_c2);
  } else if ((format_in == IN_NV16) || (format_in == IN_NV61)) {  // nv16 nv61
    gen_input_bin_file_yuv422sp(video_in, in_ch0, in_ch1, src_height, stride_c0, stride_c1);
  } else if ((format_in == IN_RGBP) || (format_in == IN_YUV444P)) {  // rgbp yuv444p
    gen_input_bin_file_planar(video_in, in_ch0, in_ch1, in_ch2, src_height, stride_c0, stride_c1, stride_c2);
  } else {  // rgb24 packet, bgr24 packet yuv444 packet, yvu444 packet
    gen_input_bin_file_packet(video_in, in_ch0, src_height, stride_c0);
  }

}

/***************************************************************************************/
void bilinear_float(float deta_x, float deta_y, float p00, float p01,
  float p10, float p11, float *pout) {
  // Calculate the weights for each pixel

  //uint32 deta_x1 = (1 << FP_WL) - deta_x;
  //uint32 deta_y1 = (1 << FP_WL) - deta_y;

  float deta_x1 = 1 - deta_x;
  float deta_y1 = 1 - deta_y;
  // We perform the actual weighting of pixel values with fixed-point arithmetic
  //uint32 w00 = (deta_x1 * deta_y1 + (1 << (FP_WL - 1))) >> FP_WL;  // rounding
  //uint32 w01 = (deta_x  * deta_y1 + (1 << (FP_WL - 1))) >> FP_WL;
  //uint32 w10 = (deta_x1 * deta_y + (1 << (FP_WL - 1))) >> FP_WL;
  //uint32 w11 = (deta_x  * deta_y + (1 << (FP_WL - 1))) >> FP_WL;

  float w00 = deta_x1 * deta_y1; 
  float w01 = deta_x  * deta_y1;
  float w10 = deta_x1 * deta_y ;
  float w11 = deta_x  * deta_y ;
  // Calculate the weighted sum of pixels

  float out_data_p00p01 = p00 * w00 + p01 * w01 ;
  float out_data_p10p11 = p10 * w10 + p11 * w11;
  float out_data        = out_data_p00p01 + out_data_p10p11;
  //out_data = (out_data + (1 << (FP_WL-1))) >> FP_WL;
  //float out_data_f32 = float(out_data/16384.f);
  float out_data_f32 = clip_f(out_data, 0.0f, 255.0f);
  pout[0] = out_data_f32;
}

void vpp_scaling_bilinear_float(int crop_idx, float *scaling_data,
  float *padding_data, VPP1686_PARAM *p_param) {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;

  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;  

  int h;
  int w;
  int py, px, py_plus, px_plus;
  float deta_x, deta_y;

  float out_ch0;
  float out_ch1;
  float out_ch2;
  float *p_src_ch0;
  float *p_src_ch1;
  float *p_src_ch2;
  float *p_dst_ch0;
  float *p_dst_ch1;
  float *p_dst_ch2;
  
  float f_scale_h;
  float f_scale_w;

  float float_py,float_px;

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;

  const uint32 scl_x = p_param->processor_param[0].SCL_X[crop_idx].scl_x;
  const uint32 scl_y = p_param->processor_param[0].SCL_Y[crop_idx].scl_y;
  const uint32 scl_init_x = p_param->processor_param[0].SCL_INIT[crop_idx].scl_init_x;
  const uint32 scl_init_y = p_param->processor_param[0].SCL_INIT[crop_idx].scl_init_y;
  
  memcpy(&f_scale_w, &scl_x, 4);
  memcpy(&f_scale_h, &scl_y, 4);

  if (padding_enable) {
    top = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
    bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
    left = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
    right = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;
  }

  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  const int src_w = src_crop_w + left + right;
  const int src_h = src_crop_h + top + bottom;
  const int dst_w = dst_crop_w - post_left - post_right;
  const int dst_h = dst_crop_h - post_top  - post_bottom;
  

  VppInfo("Enter bilinear scaling!\n");
  VppInfo("Enter  bilinear scaling!,src_h[%d] / dst_h[%d] =fscalh[%f]\n;src_w[%d] / dst_w[%d]=fscalw[%f]\n",src_h,dst_h,f_scale_h,src_w,dst_w,f_scale_w);

  p_src_ch0 = &padding_data[0];
  p_dst_ch0 = &scaling_data[0];

  p_src_ch1 = NULL;
  p_src_ch2 = NULL;
  p_dst_ch1 = NULL;
  p_dst_ch2 = NULL;

    p_src_ch1 = &padding_data[src_h * src_w];
    p_src_ch2 = &padding_data[src_h * src_w * 2];

    p_dst_ch1 = &scaling_data[dst_h * dst_w];
    p_dst_ch2 = &scaling_data[dst_h * dst_w * 2];
  for (h = 0; h < dst_h; h++) {
    for (w = 0; w < dst_w; w++) {
      // py = (y+0.5)*scale_h-0.5 = (y+0.5)*(a+o.b)-0.5 = y*a + 0.5*[(a-1)+(2y+1)*0.b]
      // px = (x+0.5)*scale_w-0.5 = (x+0.5)*(c+o.d)-0.5 = x*c + 0.5*[(c-1)+(2x+1)*0.d]
      if(scl_init_y) {
        float_py =  (h+0.5f)*f_scale_h - 0.5f ;  
      }
      else {
        float_py = ( h*f_scale_h );  
      }      
        py = int(float_py);
      


      if(scl_init_x) {
        float_px =  (w+0.5f)*f_scale_w - 0.5f ;  
      }
      else {
        float_px = ( w*f_scale_w );  
      }

      px = int(float_px);     

     // py = (h * 2 + 1) * (scl_y >> SCALING_RATIO_FRACTION_BITS) - 1;
     // py_tmp = ((h * 2 + 1) * (scl_y & SCALING_RATIO_FRACTION_MASK)) >>
     //   SCALING_RATIO_FRACTION_BITS;
     // py = (py + py_tmp) >> 1;

     // px = (w * 2 + 1) * (scl_x >> SCALING_RATIO_FRACTION_BITS) - 1;
     // px_tmp = ((w * 2 + 1) * (scl_x & SCALING_RATIO_FRACTION_MASK)) >>
     //   SCALING_RATIO_FRACTION_BITS;
     // px = (px + px_tmp) >> 1;

      if (float_py <= 0) {
        deta_y = 0;
        py = 0;
        py_plus = 0;
      } else {
        //deta_y = ((scl_y & SCALING_RATION_INTEGER_MASK) -
        //  (1 << SCALING_RATIO_FRACTION_BITS) +
        //  (h * 2 + 1) * (scl_y & SCALING_RATIO_FRACTION_MASK)) >> 1;
        deta_y = float_py - py;
        py = (py < src_h) ? py : (src_h - 1);
        py_plus = (py < src_h - 1) ? (py + 1) : py;
      }

      if (float_px <= 0) {
        deta_x = 0;

        px = 0;
        px_plus = 0;
      } else {
        //deta_x = ((scl_x & SCALING_RATION_INTEGER_MASK) -
        //  (1 << SCALING_RATIO_FRACTION_BITS) +
        //  (w * 2 + 1) * (scl_x & SCALING_RATIO_FRACTION_MASK)) >> 1;

        deta_x = float_px - px;  
        px = (px < src_w) ? px : (src_w - 1);
        px_plus = (px < src_w - 1) ? (px + 1) : px;
      }

      //for py debug begin
      //if(h==0 || h==1) {
        //if(w >=256 && w <= 300 )
           //VppInfo("@dst[%d,%d],src_p=[%d,%d] px_float=%0f \n",w,h,px,py,float_px);
      //}
      //for py debug end
     
      //bilinear_float(deta_x & SCALING_RATIO_FRACTION_MASK, deta_y & SCALING_RATIO_FRACTION_MASK, p_src_ch0[py * src_w + px],
      //  p_src_ch0[py * src_w + px_plus], p_src_ch0[py_plus * src_w + px],
      //  p_src_ch0[py_plus * src_w + px_plus], &out_ch0);
      //

      bilinear_float(deta_x , deta_y , p_src_ch0[py * src_w + px],
        p_src_ch0[py * src_w + px_plus], p_src_ch0[py_plus * src_w + px],
        p_src_ch0[py_plus * src_w + px_plus], &out_ch0);

      p_dst_ch0[h * dst_w + w] = out_ch0;

      //bilinear_float(deta_x & SCALING_RATIO_FRACTION_MASK, deta_y & SCALING_RATIO_FRACTION_MASK, p_src_ch1[py * src_w + px],
      bilinear_float(deta_x , deta_y , p_src_ch1[py * src_w + px],
        p_src_ch1[py * src_w + px_plus], p_src_ch1[py_plus * src_w + px],
        p_src_ch1[py_plus * src_w + px_plus], &out_ch1);

      //bilinear_float(deta_x & SCALING_RATIO_FRACTION_MASK, deta_y & SCALING_RATIO_FRACTION_MASK, p_src_ch2[py * src_w + px],
      bilinear_float(deta_x , deta_y , p_src_ch2[py * src_w + px],
        p_src_ch2[py * src_w + px_plus], p_src_ch2[py_plus * src_w + px],
        p_src_ch2[py_plus * src_w + px_plus], &out_ch2);

      p_dst_ch1[h * dst_w + w] = out_ch1;
      p_dst_ch2[h * dst_w + w] = out_ch2;
    }
  }

}

void vpp_scaling_nearest_floor_float(int crop_idx,
  float *scaling_data, float *padding_data, VPP1686_PARAM *p_param) {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;  
  int h;
  int w;
  int py, px ;
  float *p_src_ch0;
  float *p_src_ch1;
  float *p_src_ch2;
  float *p_dst_ch0;
  float *p_dst_ch1;
  float *p_dst_ch2;

  float py_f, px_f;
  
  float f_scale_h;
  float f_scale_w;


  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;

  const uint32 scl_x = p_param->processor_param[0].SCL_X[crop_idx].scl_x;
  const uint32 scl_y = p_param->processor_param[0].SCL_Y[crop_idx].scl_y;
  const uint32 scl_init_x = p_param->processor_param[0].SCL_INIT[crop_idx].scl_init_x;
  const uint32 scl_init_y = p_param->processor_param[0].SCL_INIT[crop_idx].scl_init_y;

  memcpy(&f_scale_w, &scl_x, 4);
  memcpy(&f_scale_h, &scl_y, 4);

  if (padding_enable) {
    top = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
    bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
    left = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
    right = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;
  }

  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  const int src_w = src_crop_w + left + right;
  const int src_h = src_crop_h + top + bottom;
  const int dst_w = dst_crop_w - post_left - post_right;
  const int dst_h = dst_crop_h - post_top  - post_bottom;

    VppInfo("Enter vpp_scaling_nearest_floor_float scaling!,fscalh = %f,fscalw=%f\n",f_scale_h,f_scale_w);
  p_src_ch0 = &padding_data[0];
  p_dst_ch0 = &scaling_data[0];
  p_src_ch1 = NULL;
  p_src_ch2 = NULL;
  p_dst_ch1 = NULL;
  p_dst_ch2 = NULL;

    p_src_ch1 = &padding_data[src_h * src_w];
    p_src_ch2 = &padding_data[src_h * src_w * 2];

    p_dst_ch1 = &scaling_data[dst_h * dst_w];
    p_dst_ch2 = &scaling_data[dst_h * dst_w * 2];

  for (h = 0; h < dst_h; h++) {
    for (w = 0; w < dst_w; w++) {
      //py = h * (scl_y >> SCALING_RATIO_FRACTION_BITS);
      //py_tmp = h * (scl_y & SCALING_RATIO_FRACTION_MASK) >> SCALING_RATIO_FRACTION_BITS;
      //py = (py + py_tmp);
      if(scl_init_y) {
        py_f = (h+0.5f)*f_scale_h - 0.5f ;  
        if (py_f <= 0.0f)
          py = 0;
        else 
          py = (int)( (h+0.5f)*f_scale_h - 0.5f );  
      }
      else {
        py = (int)( h*f_scale_h );  
      }
      

      //px = w * (scl_x >> SCALING_RATIO_FRACTION_BITS);
      //px_tmp = w * (scl_x & SCALING_RATIO_FRACTION_MASK) >> SCALING_RATIO_FRACTION_BITS;
      //px = (px + px_tmp);
      //
      if(scl_init_x) {
        px_f =  (w+0.5f)*f_scale_w - 0.5f ;  
        if (px_f <= 0.0f)
          px = 0;
        else
          px = (int)( (w+0.5f)*f_scale_w - 0.5f );  
      }
      else {
        px = (int)( w*f_scale_w );  
      }

      if(h ==0 && w==0) {
        VppInfo("vpp_scaling_nearest_floor_float px0 = %d,py0=%d \n",px,py);
      }

      py = (py < src_h) ? py : (src_h - 1);

      px = (px < src_w) ? px : (src_w - 1);

      p_dst_ch0[h * dst_w + w] = (p_src_ch0[py * src_w + px]);

        p_dst_ch1[h * dst_w + w] = (p_src_ch1[py * src_w + px]);
        p_dst_ch2[h * dst_w + w] = (p_src_ch2[py * src_w + px]);

    }

  }
}


void vpp_scaling_nearest_round_float(int crop_idx,
  float *scaling_data, float *padding_data, VPP1686_PARAM *p_param) {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;  
  int h;
  int w;
  int py, px;
  float *p_src_ch0;
  float *p_src_ch1;
  float *p_src_ch2;
  float *p_dst_ch0;
  float *p_dst_ch1;
  float *p_dst_ch2;

  float f_scale_h;
  float f_scale_w;

  //int py_f, px_f;

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
  const uint32 scl_init_x = p_param->processor_param[0].SCL_INIT[crop_idx].scl_init_x;
  const uint32 scl_init_y = p_param->processor_param[0].SCL_INIT[crop_idx].scl_init_y;

  const uint32 scl_x = p_param->processor_param[0].SCL_X[crop_idx].scl_x;
  const uint32 scl_y = p_param->processor_param[0].SCL_Y[crop_idx].scl_y;
  memcpy(&f_scale_w, &scl_x, 4);
  memcpy(&f_scale_h, &scl_y, 4);

  if (padding_enable) {
    top = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
    bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
    left = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
    right = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;
  }

  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  const int src_w = src_crop_w + left + right;
  const int src_h = src_crop_h + top + bottom;
  const int dst_w = dst_crop_w - post_left - post_right;
  const int dst_h = dst_crop_h - post_top  - post_bottom;

    VppInfo("Enter vpp_scaling_nearest_round_float scaling!,src_h=%d,src_w=%d,dst_h=%d,dst_w=%d,fscalh = %f,fscalw=%f\n",src_h,src_w,dst_h,dst_w,f_scale_h,f_scale_w);

  p_src_ch0 = &padding_data[0];
  p_dst_ch0 = &scaling_data[0];

  p_src_ch1 = NULL;
  p_src_ch2 = NULL;
  p_dst_ch1 = NULL;
  p_dst_ch2 = NULL;

    p_src_ch1 = &padding_data[src_h * src_w];
    p_src_ch2 = &padding_data[src_h * src_w * 2];

    p_dst_ch1 = &scaling_data[dst_h * dst_w];
    p_dst_ch2 = &scaling_data[dst_h * dst_w * 2];

  float __attribute__ ((unused)) px_float;

  for (h = 0; h < dst_h; h++) {
    for (w = 0; w < dst_w; w++) {
      //py = h * (scl_y >> SCALING_RATIO_FRACTION_BITS);
      //py_tmp = (h * (scl_y & SCALING_RATIO_FRACTION_MASK) + (1 << (SCALING_RATIO_FRACTION_BITS - 1))) >>
      //  SCALING_RATIO_FRACTION_BITS;
      //py = (py + py_tmp);
      if(scl_init_y) {
        py = (int)( (h+0.5f)*f_scale_h );  
      }
      else {
        py = (int)( h*f_scale_h + 0.5f);  
      }
      //px = w * (scl_x >> SCALING_RATIO_FRACTION_BITS);
      //px_tmp = (w * (scl_x & SCALING_RATIO_FRACTION_MASK) + (1 << (SCALING_RATIO_FRACTION_BITS - 1))) >>
      //  SCALING_RATIO_FRACTION_BITS;
      //px = (px + px_tmp);
      if(scl_init_x) {
        px = (int)( (w+0.5f)*f_scale_w );  
      }
      else {
        px = (int)( w*f_scale_w + 0.5f );  
      }
     
     // py_f = (int)(h * ((float)src_h / (float)dst_h) + 0.5);  // round
     // px_f = (int)(w * ((float)src_w / (float)dst_w) + 0.5);

     // if ((py_f - py > 1) || (py - py_f > 1)) {
     //   VppInfo("h %d, w %d, py %d, py_f %d, px %d, px_f %d\n", h , w, py, py_f, px, px_f);
     //   VppErr("Such a big difference between py and py_f!!\n");
     // }
     // if ((px_f - px > 1) || (px - px_f > 1)) {
     //   VppInfo("h %d, w %d, py %d, py_f %d, px %d, px_f %d\n", h , w, py, py_f, px, px_f);
     //   VppErr("Such a big difference between px and px_f!!\n");
     // }


      py = (py < src_h) ? py : (src_h - 1);

      px = (px < src_w) ? px : (src_w - 1);


      //for py debug
      if(scl_init_x) {
        px_float = ( (w+0.5f)*f_scale_w );  
      }
      else {
        px_float = ( w*f_scale_w + 0.5f );  
      }

//      if(h==0 || h==1) {
//        if(w >=256 && w <= 300 )
//           VppInfo("@dst[%d,%d],src_p=[%d,%d] px_float=%0f \n",w,h,px,py,px_float);
//      }
      //for py debug end
      p_dst_ch0[h * dst_w + w] = (p_src_ch0[py * src_w + px]);
      p_dst_ch1[h * dst_w + w] = (p_src_ch1[py * src_w + px]);
      p_dst_ch2[h * dst_w + w] = (p_src_ch2[py * src_w + px]);

    }
  }
}

int vpp_scaling_top_float(int crop_idx, float *scaling_data,
  float *padding_data, VPP1686_PARAM *p_param) {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;  
  uint32 len = 0;
  
  float f_scale_h;
  float f_scale_w;

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int csc_scale_order = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_scale_order;
  const uint32 scl_x = p_param->processor_param[0].SCL_X[crop_idx].scl_x;
  const uint32 scl_y = p_param->processor_param[0].SCL_Y[crop_idx].scl_y;
  const uint8 filter_sel = p_param->processor_param[0].SCL_CTRL[crop_idx].scl_ctrl;

  memcpy(&f_scale_w, &scl_x, 4);
  memcpy(&f_scale_h, &scl_y, 4);

  if (padding_enable) {
    top = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
    bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
    left = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
    right = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;
  }

  if(post_padding_enable){
    post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
    post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
    post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
    post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;
  }

  const int src_w = src_crop_w + left + right;
  const int src_h = src_crop_h + top + bottom;
  const int dst_w = dst_crop_w - post_left - post_right;
  const int dst_h = dst_crop_h - post_top  - post_bottom;
    VppInfo("Enter vpp_scaling_top_float!,src_h[%d] / dst_h[%d] =fscalh[%f]\n;src_w[%d] / dst_w[%d]=fscalw[%f]\n",src_h,dst_h,f_scale_h,src_w,dst_w,f_scale_w);

  if (src_fmt == IN_YUV400P && (!csc_scale_order) ) {
        //memcpy(csc_data, scaling_data, (dst_h * dst_w * sizeof(float)));
        //need to fill dummy data to 2chs
      std::fill(padding_data + src_h * src_w , padding_data + src_h * src_w *3 , 128.f);
   }

  /*no need to do scaling*/
  if ((src_w == dst_w) && (src_h == dst_h) && (scl_x == 0x3F800000) && (scl_y == 0x3F800000)) {    //change to float 1
      len = src_h * src_w * 3;
      memcpy(scaling_data,padding_data,len * sizeof(float));
  }

  else {
     switch (filter_sel) {
       case BILINEAR_CENTER:
         vpp_scaling_bilinear_float(crop_idx, scaling_data, padding_data, p_param);
         break;
       case NEAREST_FLOOR:
         vpp_scaling_nearest_floor_float(crop_idx, scaling_data, padding_data, p_param);
         break;
       case NEAREST_ROUND:
         vpp_scaling_nearest_round_float(crop_idx, scaling_data, padding_data, p_param);
         break;
       default:
         VppErr("filter_sel err!");
         break;
     }
      len =  dst_h * dst_w * 3;
  }

  return VPP_OK;
}

void csc_float(
  struct CSC_MATRIX_S csc_matrix,
  float src_p0,
  float src_p1,
  float src_p2,
  float *dst_p0,
  float *dst_p1,
  float *dst_p2,
  FILE* fpw00, FILE* fpw01,FILE* fout) 

{
  float temp_d0 , temp_d1 , temp_d2;
  float temp00, temp01, temp02;
  float temp10, temp11, temp12;
  float temp20, temp21, temp22;
  uint32 val;

  uint32 coe00_i, coe01_i, coe02_i,  add0_i,
         coe10_i, coe11_i, coe12_i,  add1_i,
         coe20_i, coe21_i, coe22_i,  add2_i;

  float coe00, coe01, coe02,  add0,
        coe10, coe11, coe12,  add1,
        coe20, coe21, coe22,  add2;
  
  coe00_i = csc_matrix.CSC_COE00;
  coe01_i = csc_matrix.CSC_COE01;
  coe02_i = csc_matrix.CSC_COE02;
  coe10_i = csc_matrix.CSC_COE10;
  coe11_i = csc_matrix.CSC_COE11;
  coe12_i = csc_matrix.CSC_COE12;
  coe20_i = csc_matrix.CSC_COE20;
  coe21_i = csc_matrix.CSC_COE21;
  coe22_i = csc_matrix.CSC_COE22;
  add0_i  = csc_matrix.CSC_ADD0 ;
  add1_i  = csc_matrix.CSC_ADD1 ;
  add2_i  = csc_matrix.CSC_ADD2 ;

  memcpy(&coe00, &coe00_i, 4);
  memcpy(&coe01, &coe01_i, 4);
  memcpy(&coe02, &coe02_i, 4);
  memcpy(&coe10, &coe10_i, 4);
  memcpy(&coe11, &coe11_i, 4);
  memcpy(&coe12, &coe12_i, 4);
  memcpy(&coe20, &coe20_i, 4);
  memcpy(&coe21, &coe21_i, 4);
  memcpy(&coe22, &coe22_i, 4);
  memcpy(&add0,  &add0_i , 4);
  memcpy(&add1,  &add1_i , 4);
  memcpy(&add2,  &add2_i , 4);

  temp00 = src_p0 * coe00;
  temp10 = src_p0 * coe10;
  temp20 = src_p0 * coe20;

  temp01 = src_p1 * coe01;
  temp11 = src_p1 * coe11;
  temp21 = src_p1 * coe21;

  temp02 = src_p2 * coe02;
  temp12 = src_p2 * coe12;
  temp22 = src_p2 * coe22;
  
  float inter_d0_tmp1 = temp00 + temp01;
  float inter_d0_tmp2 = temp02 + add0;

  float inter_d1_tmp1 = temp10 + temp11;
  float inter_d1_tmp2 = temp12 + add1;

  float inter_d2_tmp1 = temp20 + temp21;
  float inter_d2_tmp2 = temp22 + add2;


//  temp_d0 = temp00 + temp01 + temp02 + add0;
//  temp_d1 = temp10 + temp11 + temp12 + add1;
//  temp_d2 = temp20 + temp21 + temp22 + add2;
  temp_d0 = inter_d0_tmp1 + inter_d0_tmp2;
  temp_d1 = inter_d1_tmp1 + inter_d1_tmp2;
  temp_d2 = inter_d2_tmp1 + inter_d2_tmp2;

  dst_p0[0] = clip_f(temp_d0, 0.0f, 255.0f);
  dst_p1[0] = clip_f(temp_d1, 0.0f, 255.0f);
  dst_p2[0] = clip_f(temp_d2, 0.0f, 255.0f);

      memcpy(&val, &inter_d0_tmp1, 4);
      fprintf(fpw00, "%08x ",val);
      memcpy(&val, &inter_d0_tmp2, 4);
      fprintf(fpw01, "%08x ",val);
      memcpy(&val, &temp00, 4);
      fprintf(fout, "%08x ",val);

  //extern int kk;
  //kk++;
}
void csc_float(
  struct CSC_MATRIX_S csc_matrix,
  float src_p0,
  float src_p1,
  float src_p2,
  float *dst_p0,
  float *dst_p1,
  float *dst_p2) 

{
  float temp_d0 , temp_d1 , temp_d2;
  float temp00, temp01, temp02;
  float temp10, temp11, temp12;
  float temp20, temp21, temp22;

  uint32 coe00_i, coe01_i, coe02_i,  add0_i,
         coe10_i, coe11_i, coe12_i,  add1_i,
         coe20_i, coe21_i, coe22_i,  add2_i;

  float coe00, coe01, coe02,  add0,
        coe10, coe11, coe12,  add1,
        coe20, coe21, coe22,  add2;
  
  coe00_i = csc_matrix.CSC_COE00;
  coe01_i = csc_matrix.CSC_COE01;
  coe02_i = csc_matrix.CSC_COE02;
  coe10_i = csc_matrix.CSC_COE10;
  coe11_i = csc_matrix.CSC_COE11;
  coe12_i = csc_matrix.CSC_COE12;
  coe20_i = csc_matrix.CSC_COE20;
  coe21_i = csc_matrix.CSC_COE21;
  coe22_i = csc_matrix.CSC_COE22;
  add0_i  = csc_matrix.CSC_ADD0 ;
  add1_i  = csc_matrix.CSC_ADD1 ;
  add2_i  = csc_matrix.CSC_ADD2 ;

  memcpy(&coe00, &coe00_i, 4);
  memcpy(&coe01, &coe01_i, 4);
  memcpy(&coe02, &coe02_i, 4);
  memcpy(&coe10, &coe10_i, 4);
  memcpy(&coe11, &coe11_i, 4);
  memcpy(&coe12, &coe12_i, 4);
  memcpy(&coe20, &coe20_i, 4);
  memcpy(&coe21, &coe21_i, 4);
  memcpy(&coe22, &coe22_i, 4);
  memcpy(&add0,  &add0_i , 4);
  memcpy(&add1,  &add1_i , 4);
  memcpy(&add2,  &add2_i , 4);

  temp00 = src_p0 * coe00;
  temp10 = src_p0 * coe10;
  temp20 = src_p0 * coe20;

  temp01 = src_p1 * coe01;
  temp11 = src_p1 * coe11;
  temp21 = src_p1 * coe21;

  temp02 = src_p2 * coe02;
  temp12 = src_p2 * coe12;
  temp22 = src_p2 * coe22;
  
  float inter_d0_tmp1 = temp00 + temp01;
  float inter_d0_tmp2 = temp02 + add0;

  float inter_d1_tmp1 = temp10 + temp11;
  float inter_d1_tmp2 = temp12 + add1;

  float inter_d2_tmp1 = temp20 + temp21;
  float inter_d2_tmp2 = temp22 + add2;


//  temp_d0 = temp00 + temp01 + temp02 + add0;
//  temp_d1 = temp10 + temp11 + temp12 + add1;
//  temp_d2 = temp20 + temp21 + temp22 + add2;
  temp_d0 = inter_d0_tmp1 + inter_d0_tmp2;
  temp_d1 = inter_d1_tmp1 + inter_d1_tmp2;
  temp_d2 = inter_d2_tmp1 + inter_d2_tmp2;

  dst_p0[0] = clip_f(temp_d0, 0.0f, 255.0f);
  dst_p1[0] = clip_f(temp_d1, 0.0f, 255.0f);
  dst_p2[0] = clip_f(temp_d2, 0.0f, 255.0f);

  //extern int kk;
  //kk++;
}

void vpp_yuv_hsv_float(float *hsv_float,  float *scaling_data,  int crop_idx, VPP1686_PARAM *p_param) {
  static int sdiv_table[256];
  static int hdiv_table180[256];
  static int hdiv_table256[256];
  
  int w_idx,h_idx;
  const int hsv_shift = 12;
  int row, col;
  int i;
  int b, g, r;
  int h, s, v;
  int vmin;
  int vr, vg;
  uint8 diff;

  float u_temp = 1.0;
  float v_temp = 1.0;

  float *rgb_planar_float=NULL;  //input data float , yuv convert to rgb
  uint8 *rgb_planar_uint8=NULL;  //input data float convert to uint8
  uint8 *hsv = NULL;       //output data will be convert to float

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;   
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  const int csc_scale_order = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_scale_order;
  const int round_method = p_param->processor_param[0].CON_BRI_CTRL[crop_idx].hr_csc_f2i_round;

  int top = 0;  
  int bottom = 0;  
  int left = 0;  
  int right = 0;

  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;
  
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
  if(padding_enable){
  top    = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
  bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
  left   = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
  right  = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;}


  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  int dst_w = 0;
  int dst_h = 0;


  //csc_ float param  begin
  float src_ch0, src_ch1, src_ch2;  
  float *p_dst0;
  float *p_dst1;
  float *p_dst2;
#ifdef WRITE_CSC_FILE
  char file_w00[100],file_w01[100],file_out[100]; //,file_rgb_uint[100];
  sprintf(file_w00, "%s%s", "csc_d0tmp0_ch0", ".dat");
  sprintf(file_w01, "%s%s", "csc_d0tmp0_ch0", ".dat");
  sprintf(file_out, "%s%s", "csc_srcp0xcoe00", ".dat");
//  sprintf(file_rgb_uint, "%s%s", "rgb2hsv_rgbch0_uint8", ".dat");
  FILE *fpw00 = fopen(file_w00, "w+");
  FILE *fpw01 = fopen(file_w01, "w+");
  FILE *fpout = fopen(file_out, "w+");  
  //csc_ float param  end
#endif

  if(!csc_scale_order){
    dst_w = dst_crop_w - post_left - post_right;
    dst_h = dst_crop_h - post_top  - post_bottom;
  }
  else{
    dst_w = src_crop_w + left + right;
    dst_h = src_crop_h + top  + bottom; 
  }

  rgb_planar_float = new float[dst_h * dst_w *3];
  rgb_planar_uint8 = new uint8[dst_h * dst_w *3];
  hsv = new uint8[dst_h * dst_w *3];
//

  uint32 offset_ch1 = dst_h * dst_w;
  uint32 offset_ch2 = dst_h * dst_w * 2;
  uint32 pix_len = dst_h * dst_w *3;

  VppAssert((src_fmt <= 13) || (src_fmt == 31)); //all input_format yuv will be ok;
  VppAssert((dst_fmt == OUT_HSV180) || (dst_fmt == OUT_HSV256));

  //***************************************************//
  ///step 1 , convet yuv to rgb_planar 
  //***************************************************//
      for (h_idx = 0; h_idx < dst_h; h_idx++) {
        for (w_idx = 0; w_idx < dst_w; w_idx++) {
          src_ch0 = scaling_data[h_idx * dst_w + w_idx];

          if (src_fmt == IN_YUV400P && csc_scale_order) {
            src_ch1 = float(128);
            src_ch2 = float(128);
          } else {
            src_ch1 = scaling_data[offset_ch1 + h_idx * dst_w + w_idx];
            src_ch2 = scaling_data[offset_ch2 + h_idx * dst_w + w_idx];
          }

          p_dst0 = &rgb_planar_float[h_idx * dst_w + w_idx];

          if (dst_fmt == OUT_YUV400P) {
            p_dst1 = &u_temp;
            p_dst2 = &v_temp;
          } else {
            p_dst1 = &rgb_planar_float[offset_ch1 + h_idx * dst_w + w_idx];
            p_dst2 = &rgb_planar_float[offset_ch2 + h_idx * dst_w + w_idx];
          }

          csc_float(p_param->processor_param[0].CSC_MATRIX,src_ch0, src_ch1, src_ch2, p_dst0, p_dst1, p_dst2);
        }
#ifdef WRITE_CSC_FILE

      fprintf(fpw00, "\n");
      fprintf(fpw01, "\n");
      fprintf(fpout, "\n");
#endif
      }


  //***************************************************//
  ///step 2 , convet  rgb_planar_float to uint8 format 
  //***************************************************//
      for (int i=0;i<dst_h * dst_w *3; i++) {
            rgb_planar_uint8[i] = float2uint8( rgb_planar_float[i], round_method );
      }
      delete [] rgb_planar_float;
      rgb_planar_float=NULL;
  //***************************************************//
  ///step 3 , convet  rgb_planar_uint8 to hsv
  //***************************************************//

  const int hrange = (dst_fmt == OUT_HSV180) ? 180 : 256;
  /*build table*/
  sdiv_table[0] = hdiv_table180[0] = hdiv_table256[0] = 0;
  for(i = 1; i < 256; i++) {
    sdiv_table[i] = (int)((255 << hsv_shift) / (1.* i) + 0.5);  // need clip?
    hdiv_table180[i] = (int)((180 << hsv_shift) / (6.* i) + 0.5);
    hdiv_table256[i] = (int)((256 << hsv_shift) / (6.* i) + 0.5);
  }
  const int* hdiv_table = (hrange == 180) ? hdiv_table180 : hdiv_table256;


  for (row = 0; row < dst_h; row++) {  // rows
    for( col = 0; col < dst_w; col++) {  // cols

      r = rgb_planar_uint8[row * dst_w + col];
      g = rgb_planar_uint8[offset_ch1 + row * dst_w + col];
      b = rgb_planar_uint8[offset_ch2 + row * dst_w + col];
      v = b;
      vmin = b;

      VPP_MAX(v, g);
      VPP_MAX(v, r);
      VPP_MIN(vmin, g);
      VPP_MIN(vmin, r);

      diff = (uint8)(v - vmin);

      vr = v == r ? -1 : 0;
      vg = v == g ? -1 : 0;
      s = (diff * sdiv_table[v] + (1 << (hsv_shift-1))) >> hsv_shift;

      h = (vr & (g - b)) + (~vr & ((vg & (b - r + 2 * diff)) + ((~vg) & (r - g + 4 * diff))));
      h = (h * hdiv_table[diff] + (1 << (hsv_shift-1))) >> hsv_shift;
      h += h < 0 ? hrange : 0;

      hsv[row * dst_w + col ] = (uint8)(h);
      hsv[dst_h* dst_w + row * dst_w + col ]= (uint8)s;
      hsv[dst_h* dst_w *2 + row * dst_w + col] = (uint8)v;

    }
  }
  
  delete []  rgb_planar_uint8;
  rgb_planar_uint8 = NULL;

  for (uint32 i=0;i< pix_len; i++) {
       hsv_float[i] = float( hsv[i] );
  }

  delete [] hsv ;
  hsv = NULL;

}

void vpp_rgb_planar_hsv_float(float *hsv_float, float *scaling_data,  int crop_idx, VPP1686_PARAM *p_param) {
  static int sdiv_table[256];
  static int hdiv_table180[256];
  static int hdiv_table256[256];

  const int hsv_shift = 12;
  int row, col;
  int i;
  int b, g, r;
  int h, s, v;
  int vmin;
  int vr, vg;
  uint8 diff;

  uint8 *rgb_planar=NULL;
  uint8 *hsv = NULL;
//  int row, col;
//  float b, g, r;
//  float h, s, v;
//  float vmin, vmax;
//  float diff, rev_diff;
//  float hscale;
//
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;   
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  const int csc_scale_order = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_scale_order;
  const int round_method = p_param->processor_param[0].CON_BRI_CTRL[crop_idx].hr_csc_f2i_round;

  int top = 0;  
  int bottom = 0;  
  int left = 0;  
  int right = 0;

  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;
  
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
  if(padding_enable){
  top    = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
  bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
  left   = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
  right  = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;}


  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  int dst_w = 0;
  int dst_h = 0;

//  char file_w00[100];
//  sprintf(file_w00, "%s%s", "csc_hsvout_ch0", ".dat");
//  FILE *fpout = fopen(file_w00, "w+");  

  if(!csc_scale_order){
    dst_w = dst_crop_w - post_left - post_right;
    dst_h = dst_crop_h - post_top  - post_bottom;
  }
  else{
    dst_w = src_crop_w + left + right;
    dst_h = src_crop_h + top  + bottom; 
  }

  uint32 offset_ch1 = dst_h * dst_w;
  uint32 offset_ch2 = dst_h * dst_w * 2;
  uint32 pix_len = dst_h * dst_w *3;

  rgb_planar = new uint8[dst_h * dst_w *3];
  hsv = new uint8[dst_h * dst_w *3];

      for (int i=0;i<dst_h * dst_w *3; i++) {
            //rgb_planar[i] = float2uint8( scaling_data[i], 1 );
            rgb_planar[i] = float2uint8( scaling_data[i], round_method );
      }

  VppAssert((src_fmt == IN_RGB24) || (src_fmt == IN_BGR24) || (src_fmt == IN_RGBP));
  VppAssert((dst_fmt == OUT_HSV180) || (dst_fmt == OUT_HSV256));
  const int hrange = (dst_fmt == OUT_HSV180) ? 180 : 256;
  /*build table*/
  sdiv_table[0] = hdiv_table180[0] = hdiv_table256[0] = 0;
  for(i = 1; i < 256; i++) {
    sdiv_table[i] = (int)((255 << hsv_shift) / (1.* i) + 0.5);  // need clip?
    hdiv_table180[i] = (int)((180 << hsv_shift) / (6.* i) + 0.5);
    hdiv_table256[i] = (int)((256 << hsv_shift) / (6.* i) + 0.5);
  }
  const int* hdiv_table = (hrange == 180) ? hdiv_table180 : hdiv_table256;


  for (row = 0; row < dst_h; row++) {  // rows
    for( col = 0; col < dst_w; col++) {  // cols
      r = rgb_planar[row * dst_w + col];
      g = rgb_planar[offset_ch1 + row * dst_w + col];
      b = rgb_planar[offset_ch2 + row * dst_w + col];
      v = b;
      vmin = b;

      VPP_MAX(v, g);
      VPP_MAX(v, r);
      VPP_MIN(vmin, g);
      VPP_MIN(vmin, r);

      diff = (uint8)(v - vmin);

      vr = v == r ? -1 : 0;
      vg = v == g ? -1 : 0;
      s = (diff * sdiv_table[v] + (1 << (hsv_shift-1))) >> hsv_shift;

      h = (vr & (g - b)) + (~vr & ((vg & (b - r + 2 * diff)) + ((~vg) & (r - g + 4 * diff))));
      h = (h * hdiv_table[diff] + (1 << (hsv_shift-1))) >> hsv_shift;
      h += h < 0 ? hrange : 0;

    //  hsv[row * dst_w * 3 + col * 3 + 0] = (uint8)(h);
    //  hsv[row * dst_w * 3 + col * 3 + 1] = (uint8)s;
    //  hsv[row * dst_w * 3 + col * 3 + 2] = (uint8)v;

      hsv[row * dst_w + col ] = (uint8)(h);
      hsv[dst_h* dst_w + row * dst_w + col ]= (uint8)s;
      hsv[dst_h* dst_w *2 + row * dst_w + col] = (uint8)v;

    //  fprintf(fpout,"%02x ",hsv[row * dst_w + col] );
    }
   // fprintf(fpout,"\n");
  }
  delete [] rgb_planar;
  rgb_planar = NULL;

  for (uint32 i=0;i< pix_len; i++) {
       hsv_float[i] = float( hsv[i] );
  }
  
  delete [] hsv;
  hsv = NULL;
  //for (row = 0; row < dst_h; row++) {  // rows
  //  for( col = 0; col < dst_w; col++) {  // cols
  //    r = rgb_planar[row * dst_w + col];
  //    g = rgb_planar[offset_ch1 + row * dst_w + col];
  //    b = rgb_planar[offset_ch2 + row * dst_w + col];
  //    
  //    vmin = min_f(r,min(g,b));
  //    vmax = max_f(r,max(g,b));
  //    diff = (vmax - vmin);
  //    rev_diff = 60.f/(diff+FLT_MIN); //avoid /0,maybe different with rtl realization
  //  
  //    v = vmax;
  //    s = diff/(vmax+FLT_MIN);        //avoid /0,maybe different with rtl realization
  //    if(vmax == r){
  //      h = (g - b) * rev_diff;
  //    }
  //    else if(vmax == g){
  //      h = (b - r) * rev_diff + 120.f;
  //    }
  //    else {
  //      h = (r - g) * rev_diff + 240.f;
  //    }

  //    if(h<0) h += 360.f;

  //    hscale = hrange*(1.f/360.f);    //maybe different with rtl realization  
  //  
  //    hsv[row * dst_w * 3 + col * 3 + 0] = h*hscale;
  //    hsv[row * dst_w * 3 + col * 3 + 1] = s;
  //    hsv[row * dst_w * 3 + col * 3 + 2] = v;
  //  }
  //}
}

void vpp_csc_float(int crop_idx, float *csc_data, float *scaling_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;  
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  const int csc_path = p_param->USER_DEF.csc_path;
  const int csc_scale_order = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_scale_order;

  int h, w;
  float src_ch0, src_ch1, src_ch2;
  float *p_dst0;
  float *p_dst1;
  float *p_dst2;

  float u_temp = 1.0;
  float v_temp = 1.0;

  int top = 0;  
  int bottom = 0;  
  int left = 0;  
  int right = 0;

  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;
  

  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
  if(padding_enable){
  top    = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
  bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
  left   = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
  right  = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;}


  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  int dst_w = 0;
  int dst_h = 0;

#ifdef WRITE_CSC_FILE
  char file_w00[100],file_w01[100],file_out[100]; //,file_rgb_uint[100];
  sprintf(file_w00, "%s%s", "csc_d0tmp0_ch0", ".dat");
  sprintf(file_w01, "%s%s", "csc_d0tmp0_ch1", ".dat");
  sprintf(file_out, "%s%s", "csc_srcp0xcoe00", ".dat");
//  sprintf(file_rgb_uint, "%s%s", "rgb2hsv_rgbch0_uint8", ".dat");
  FILE *fpw00 = fopen(file_w00, "w+");
  FILE *fpw01 = fopen(file_w01, "w+");
  FILE *fpout = fopen(file_out, "w+");  
 // FILE *frgb  = fopen(file_rgb_uint,"w+");
#endif

  if(!csc_scale_order){
    dst_w = dst_crop_w - post_left - post_right;
    dst_h = dst_crop_h - post_top  - post_bottom;
  }
  else{
    dst_w = src_crop_w + left + right;
    dst_h = src_crop_h + top  + bottom; 
  }

  uint32 offset_ch1 = dst_h * dst_w;
  uint32 offset_ch2 = dst_h * dst_w * 2;


  VppAssert((csc_path == CSC_PATH_RGB_RGB) || (csc_path == CSC_PATH_YUV_YUV) || (csc_path == CSC_PATH_YUV_RGB) ||
    (csc_path == CSC_PATH_RGB_YUV) || (csc_path == CSC_PATH_RGB_HSV)|| (csc_path == CSC_PATH_YUV_HSV));

  //int csc_mode = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_mode;
  //

  switch (csc_path) {
    case CSC_PATH_YUV_RGB:
    case CSC_PATH_RGB_YUV:
      for (h = 0; h < dst_h; h++) {
        for (w = 0; w < dst_w; w++) {
          src_ch0 = scaling_data[h * dst_w + w];

          if (src_fmt == IN_YUV400P && csc_scale_order) {
            src_ch1 = float(128);
            src_ch2 = float(128);
          } else {
            src_ch1 = scaling_data[offset_ch1 + h * dst_w + w];
            src_ch2 = scaling_data[offset_ch2 + h * dst_w + w];
          }

          p_dst0 = &csc_data[h * dst_w + w];

          if (dst_fmt == OUT_YUV400P) {
            p_dst1 = &u_temp;
            p_dst2 = &v_temp;
          } else {
            p_dst1 = &csc_data[offset_ch1 + h * dst_w + w];
            p_dst2 = &csc_data[offset_ch2 + h * dst_w + w];
          }

          csc_float(p_param->processor_param[0].CSC_MATRIX,src_ch0, src_ch1, src_ch2, p_dst0, p_dst1, p_dst2);
          //csc_float((struct CSC_MATRIX_S *)&csc_matrix_list[csc_mode],src_ch0, src_ch1, src_ch2, p_dst0, p_dst1, p_dst2);
        }
#ifdef WRITE_CSC_FILE
      fprintf(fpw00, "\n");
      fprintf(fpw01, "\n");
      fprintf(fpout, "\n");
#endif
      }
    break;
    case CSC_PATH_RGB_HSV:

        vpp_rgb_planar_hsv_float(csc_data, scaling_data,  crop_idx, p_param);
    break;

    case CSC_PATH_YUV_HSV: 
        vpp_yuv_hsv_float(csc_data, scaling_data, crop_idx, p_param);
        break;

    default:  // error input format
      VppErr("no need to do color space convert!\n");
    break;
  }
#ifdef WRITE_CSC_FILE
  fclose(fpw00);
  fclose(fpw01);
  fclose(fpout);
#endif
}

/*csc_path: 0: yuv to yuv, or rgb to rgb; 1: yuv to rgb; 2: rgb to yuv; 3: rgb to hsv*/
void vpp_csc_top_float(int crop_idx, float *csc_data, float *scaling_data, VPP1686_PARAM *p_param) {
  const int csc_path = p_param->USER_DEF.csc_path;
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
  const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  const int src_fmt = p_param->processor_param[0].SRC_CTRL[crop_idx].input_format;
  const int csc_scale_order = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_scale_order;

  int top = 0;
  int bottom = 0;  
  int left = 0;  
  int right = 0;
  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;
  
  const int padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
  if(padding_enable){
  top    = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
  bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
  left   = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
  right  = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;}


  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}
  
  int dst_w = 0;
  int dst_h = 0;

     VppInfo("Enter vpp_csc_top_float ,csc_path = %d!\n", csc_path);

  if(!csc_scale_order){
    dst_w = dst_crop_w - post_left - post_right;
    dst_h = dst_crop_h - post_top  - post_bottom;
  }
  else{
    dst_w = src_crop_w + left + right;
    dst_h = src_crop_h + top  + bottom;
  }


  VppAssert((csc_path == CSC_PATH_RGB_RGB) || (csc_path == CSC_PATH_YUV_YUV) || (csc_path == CSC_PATH_YUV_RGB) ||
    (csc_path == CSC_PATH_RGB_YUV) || (csc_path == CSC_PATH_RGB_HSV)|| (csc_path == CSC_PATH_YUV_HSV));


  switch (csc_path) {
    case CSC_PATH_RGB_RGB:  // yuv to yuv, or rgb to rgb, no need to do csc
      if (src_fmt == IN_YUV400P && csc_scale_order) {
        memcpy(csc_data, scaling_data, (dst_h * dst_w * sizeof(float)));
        //memset(&csc_data[dst_h * dst_w ], 128, (dst_h * dst_w * 2 *sizeof(float)));
        std::fill(csc_data + dst_h * dst_w, csc_data + dst_h * dst_w *3, 128.f);
      } else {      
        memcpy(csc_data, scaling_data, (dst_h * dst_w * 3 *sizeof(float) ));       
      }
      VppInfo("vpp_csc_top_float no need to csc, csc_path = %d!\n", csc_path);
    break;

    case CSC_PATH_YUV_RGB:  // yuv to rgb
    vpp_csc_float(crop_idx, csc_data, scaling_data, p_param);

    if (dst_fmt == OUT_RGBYP) {
      uint32 offset_y = dst_h * dst_w * 3;
      memcpy(&csc_data[offset_y], scaling_data, dst_h * dst_w * sizeof(float));
    }
    break;

    case CSC_PATH_RGB_YUV:  // rgb to yuv
      vpp_csc_float(crop_idx, csc_data, scaling_data, p_param);

      if (dst_fmt == OUT_RGBYP) {
        uint32 offset_y = dst_h * dst_w * 3;
        /*copy the data of the channel y to the corresponding position*/
        memcpy(&csc_data[offset_y], csc_data, dst_h * dst_w * sizeof(float));
        /*copy the data of the three channels of r g b to the corresponding position*/
        memcpy(csc_data, scaling_data, dst_h * dst_w * 3 * sizeof(float));
      }
    break;

    case CSC_PATH_RGB_HSV: // rgb to hsv
      vpp_csc_float(crop_idx, csc_data, scaling_data, p_param);
    break;

    case CSC_PATH_YUV_HSV: // yuv to hsv
      vpp_csc_float(crop_idx, csc_data, scaling_data, p_param);
    break;


    default:  // error input format
      VppErr("vpp_csc_top csc_path err, csc_path = %d!\n", csc_path);
    break;
  }

#ifdef WRITE_CSC_FILE
  char file_name[100];
  sprintf(file_name, "%s%s%d%s", CSC, "crop", crop_idx, ".bin");

  FILE *fp = fopen(file_name, "wb");
  if (!fp) {
    printf("ERROR: Unable to open the csc file\n");
    exit(-1);
  }

  VppInfo("csc top  dst_h %d, dst_w %d, dst_h * dst_w * 3   %d\n",
    dst_h, dst_w, dst_h * dst_w * 3);

  uint32 len;

  if (dst_fmt == OUT_RGBYP) {
    len = dst_h * dst_w * 4;
  } 
  else if (dst_fmt == OUT_YUV400P ) {
    len = dst_h * dst_w ;
  }
  else {
    len = dst_h * dst_w * 3;
  }

  for(uint32 i=0; i<len; i++){
    float2char.f = csc_data[i];
    fwrite(&float2char.c[3], 1, 1, fp);
    fwrite(&float2char.c[2], 1, 1, fp);
    fwrite(&float2char.c[1], 1, 1, fp);
    fwrite(&float2char.c[0], 1, 1, fp);
  }
  fclose(fp);

#endif
}


void vpp_con_bri_top_float(int crop_idx, float *con_bri_data,
  float *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w  = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h  = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  const int output_format = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0;
  if(post_padding_enable){
  post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;}

  int dst_w = 0;
  int dst_h = 0;

  dst_w = dst_crop_w - post_left - post_right;
  dst_h = dst_crop_h - post_top  - post_bottom;

  //bool  packet_en = (format_out == OUT_HSV180) || (format_out == OUT_HSV256);
  float con_bri_a_0, con_bri_a_1, con_bri_a_2;
  float con_bri_b_0, con_bri_b_1, con_bri_b_2;
  int h, w;

  float con_bri_ch0;
  float con_bri_ch1;
  float con_bri_ch2;

  float *p_csc_ch0;
  float *p_csc_ch1;
  float *p_csc_ch2;
  float *p_csc_ch3;

  float *p_con_bri_ch0;
  float *p_con_bri_ch1;
  float *p_con_bri_ch2;
  float *p_con_bri_ch3;


  p_csc_ch0 = &csc_data[0];
  p_csc_ch1 = &csc_data[dst_h * dst_w];
  p_csc_ch2 = &csc_data[dst_h * dst_w * 2];
  p_csc_ch3 = &csc_data[dst_h * dst_w * 3];

  p_con_bri_ch0 = &con_bri_data[0];
  p_con_bri_ch1 = &con_bri_data[dst_h * dst_w];
  p_con_bri_ch2 = &con_bri_data[dst_h * dst_w * 2];
  p_con_bri_ch3 = &con_bri_data[dst_h * dst_w * 3];

  con_bri_a_0 = p_param->processor_param[0].CON_BRI_A_0[crop_idx].con_bri_a_0;
  con_bri_a_1 = p_param->processor_param[0].CON_BRI_A_1[crop_idx].con_bri_a_1;
  con_bri_a_2 = p_param->processor_param[0].CON_BRI_A_2[crop_idx].con_bri_a_2;  
  con_bri_b_0 = p_param->processor_param[0].CON_BRI_B_0[crop_idx].con_bri_b_0;
  con_bri_b_1 = p_param->processor_param[0].CON_BRI_B_1[crop_idx].con_bri_b_1;
  con_bri_b_2 = p_param->processor_param[0].CON_BRI_B_2[crop_idx].con_bri_b_2;
    
  /*
  printf("con_bri_ch0: a:%f b:%f \n",con_bri_a_0,con_bri_b_0);
  printf("con_bri_ch1: a:%f b:%f \n",con_bri_a_1,con_bri_b_1);
  printf("con_bri_ch2: a:%f b:%f \n",con_bri_a_2,con_bri_b_2);
  */

//  VppAssert((con_bri_a_0 >= 0.0f) && (con_bri_a_0<=255.0f) );
//  VppAssert((con_bri_a_1 >= 0.0f) && (con_bri_a_1<=255.0f) );
//  VppAssert((con_bri_a_2 >= 0.0f) && (con_bri_a_2<=255.0f) ); 
//  VppAssert((con_bri_b_0 >= -255.0f) && (con_bri_a_2<=255.0f) ); 
//  VppAssert((con_bri_b_1 >= -255.0f) && (con_bri_a_2<=255.0f) ); 
//  VppAssert((con_bri_b_2 >= -255.0f) && (con_bri_a_2<=255.0f) ); 


    for (h = 0; h < dst_h; h++) {
      for (w = 0; w < dst_w; w++) {
        con_bri_ch0 = ( p_csc_ch0[h * dst_w + w] * con_bri_a_0 ) + con_bri_b_0;
        con_bri_ch1 = ( p_csc_ch1[h * dst_w + w] * con_bri_a_1 ) + con_bri_b_1;
        con_bri_ch2 = ( p_csc_ch2[h * dst_w + w] * con_bri_a_2 ) + con_bri_b_2;

        p_con_bri_ch0[h * dst_w + w] = (con_bri_ch0);
        p_con_bri_ch1[h * dst_w + w] = (con_bri_ch1);
        p_con_bri_ch2[h * dst_w + w] = (con_bri_ch2);
      }
    }

    if(output_format == OUT_RGBYP){
     memcpy(p_con_bri_ch3, p_csc_ch3, dst_h * dst_w * sizeof(float)); 
    }
#ifdef WRITE_CON_BRI_FILE
  char file_name[100];
  sprintf(file_name, "%s%s%d%s", CON_BRI, "crop", crop_idx, ".bin");

  FILE *fp = fopen(file_name, "wb");
  if (!fp) {
    printf("ERROR: Unable to open the con_bri file\n");
    exit(-1);
  }

  uint32 len = (output_format == OUT_RGBYP)? dst_h * dst_w * 4 : dst_h * dst_w * 3;
  for(uint32 i=0; i<len; i++){
    float2char.f = con_bri_data[i];
    fwrite(&float2char.c[3], 1, 1, fp);
    fwrite(&float2char.c[2], 1, 1, fp);
    fwrite(&float2char.c[1], 1, 1, fp);
    fwrite(&float2char.c[0], 1, 1, fp);
  }
  fclose(fp);
#endif  
}


void vpp_post_padding_float(int crop_idx, float*post_pad_data, float *csc_data, VPP1686_PARAM *p_param){

  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  //const uint32_t post_value = p_param->processor_param[0].POST_PADDING_VALUE[crop_idx].post_padding_value;
  const uint32 ch0 = p_param->processor_param[0].POST_PADDING_VALUE[crop_idx].post_padding_value_ch0;
  const uint32 ch1 = p_param->processor_param[0].POST_PADDING_VALUE[crop_idx].post_padding_value_ch1;
  const uint32 ch2 = p_param->processor_param[0].POST_PADDING_VALUE[crop_idx].post_padding_value_ch2;
  const uint32 ch3 = p_param->processor_param[0].POST_PADDING_VALUE[crop_idx].post_padding_value_ch3;
  
  const int post_padding_enable = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;
  const int post_top    = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
  const int post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
  const int post_left   = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
  const int post_right  = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;

  float post_val_ch0;
  float post_val_ch1;
  float post_val_ch2;
  float post_val_ch3;
  memcpy(&post_val_ch0,&ch0,4); 
  memcpy(&post_val_ch1,&ch1,4); 
  memcpy(&post_val_ch2,&ch2,4); 
  memcpy(&post_val_ch3,&ch3,4); 

  float *psrc0;
  float *psrc1;
  float *psrc2;
  float *psrc3;

  float *pdst0;
  float *pdst1;
  float *pdst2;
  float *pdst3;

  VppInfo("the ch3= %x, post_value_ch3 =%f\n",ch3,post_val_ch3);
  int src_w, src_h;
  int dst_w, dst_h;
  
  int h;


  src_w = dst_crop_w - (post_padding_enable ? (post_left + post_right):0);
  src_h = dst_crop_h - (post_padding_enable ? (post_top + post_bottom):0);
  dst_w = dst_crop_w;
  dst_h = dst_crop_h;
  
  psrc0 = &csc_data[0];
  pdst0 = &post_pad_data[0];

  psrc1 = NULL;
  psrc2 = NULL;
  psrc3 = NULL;
  pdst1 = NULL;
  pdst2 = NULL;
  pdst3 = NULL;
   
    psrc1 = &csc_data[src_h * src_w];
    psrc2 = &csc_data[src_h * src_w * 2];

    pdst1 = &post_pad_data[dst_h * dst_w];
    pdst2 = &post_pad_data[dst_h * dst_w * 2];

    if(dst_fmt == OUT_RGBYP){
    psrc3 = &csc_data[src_h * src_w * 3];    
    pdst3 = &post_pad_data[dst_h * dst_w * 3];      
    }
  //padding top
  std::fill(pdst0, pdst0+(dst_w*post_top), (float)post_val_ch0);
  std::fill(pdst1, pdst1+(dst_w*post_top), (float)post_val_ch1);
  std::fill(pdst2, pdst2+(dst_w*post_top), (float)post_val_ch2);
  if (dst_fmt == OUT_RGBYP){
  std::fill(pdst3, pdst3+(dst_w*post_top), (float)post_val_ch3);
  }
  //padding left + valid + right
  for(h=0; h<src_h; h++){ 
    std::fill(  pdst0 + (dst_w*(post_top+h) ), pdst0 + (dst_w*(post_top+h)) + post_left, (float)post_val_ch0);
    memcpy(pdst0 + (dst_w*(post_top+h) ) + post_left, psrc0 + src_w*h, sizeof(float)*src_w);
    std::fill(  pdst0 + (dst_w*(post_top+h+1) - post_right), pdst0 + (dst_w*(post_top+h+1)), (float)post_val_ch0); 

    std::fill(  pdst1 + (dst_w*(post_top+h) ), pdst1 + (dst_w*(post_top+h)) + post_left, (float)post_val_ch1);
    memcpy(pdst1 + (dst_w*(post_top+h) ) + post_left, psrc1 + src_w*h, sizeof(float)*src_w);
    std::fill(  pdst1 + (dst_w*(post_top+h+1) - post_right), pdst1 + (dst_w*(post_top+h+1)), (float)post_val_ch1); 

    std::fill(  pdst2 + (dst_w*(post_top+h) ), pdst2 + (dst_w*(post_top+h)) + post_left, (float)post_val_ch2);
    memcpy(pdst2 + (dst_w*(post_top+h) ) + post_left, psrc2 + src_w*h, sizeof(float)*src_w);
    std::fill(  pdst2 + (dst_w*(post_top+h+1) - post_right), pdst2 + (dst_w*(post_top+h+1)), (float)post_val_ch2); 

    if(dst_fmt == OUT_RGBYP){ 
    std::fill(  pdst3 + (dst_w*(post_top+h) ), pdst3 + (dst_w*(post_top+h)) + post_left, (float)post_val_ch3);
    memcpy(pdst3 + (dst_w*(post_top+h) ) + post_left, psrc3 + src_w*h, sizeof(float)*src_w);
    std::fill(  pdst3 + (dst_w*(post_top+h+1) - post_right), pdst3 + (dst_w*(post_top+h+1)), (float)post_val_ch3); 
    }
  }  
  //padding bottom
  std::fill(pdst0 + (dst_w*(dst_h-post_bottom)), pdst0+(dst_w*dst_h), (float)post_val_ch0);
  std::fill(pdst1 + (dst_w*(dst_h-post_bottom)), pdst1+(dst_w*dst_h), (float)post_val_ch1);
  std::fill(pdst2 + (dst_w*(dst_h-post_bottom)), pdst2+(dst_w*dst_h), (float)post_val_ch2);  
  if (dst_fmt == OUT_RGBYP){
  std::fill(pdst3 + (dst_w*(dst_h-post_bottom)), pdst3+(dst_w*dst_h), (float)post_val_ch3);  
  }
}

void vpp_out_three_planar_fp16(int crop_idx, fp16 *vpp_out_data_u8, float *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int round_method = p_param->processor_param[0].CON_BRI_CTRL[crop_idx].con_bri_round;
  int h, w;

  float *p_src_ch0;
  float *p_src_ch1;
  float *p_src_ch2;

  fp16 *p_dst_ch0;
  fp16 *p_dst_ch1;
  fp16 *p_dst_ch2;

  int offset_ch1 = dst_crop_h * dst_crop_w;
  int offset_ch2 = offset_ch1 + dst_crop_h * dst_crop_w;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];

  p_dst_ch0 = &vpp_out_data_u8[0];
  p_dst_ch1 = &vpp_out_data_u8[offset_ch1];
  p_dst_ch2 = &vpp_out_data_u8[offset_ch2];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_ch0[h * dst_crop_w + w] = fp32tofp16(p_src_ch0[h * dst_crop_w + w],round_method);
      p_dst_ch1[h * dst_crop_w + w] = fp32tofp16(p_src_ch1[h * dst_crop_w + w],round_method);
      p_dst_ch2[h * dst_crop_w + w] = fp32tofp16(p_src_ch2[h * dst_crop_w + w],round_method);
    }
  }
}


void vpp_out_top_fp16(int crop_idx, float *post_pad_data, VPP1686_PARAM *p_param) {

  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;

  fp16 *vpp_tmp_float = new fp16[dst_crop_h * dst_crop_w * 3];

  //const int con_bri_en = p_param->processor_param[0].SRC_CTRL[crop_idx].con_bri_enable;

  switch (dst_fmt) {
    case OUT_YUV444P:
    case OUT_RGBP:
      vpp_out_three_planar_fp16(crop_idx, vpp_tmp_float, post_pad_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }

#ifdef WRITE_OUT_FILE
  int vpp_out_len;
  vpp_out_len = dst_crop_h * dst_crop_w * 3;

  char file_name[100];
  sprintf(file_name, "%s%s%d%s", OUT, "crop", crop_idx, "fp16.bin");

  FILE *fp = fopen(file_name, "wb");
  if (!fp) {
    printf("ERROR: Unable to open the out file\n");
    exit(-1);
  }

  fwrite(vpp_tmp_float, 1, vpp_out_len *sizeof(fp16), fp);
  fclose(fp);
#endif
  delete [] vpp_tmp_float;

}

void vpp_out_three_planar_bf16(int crop_idx, bf16 *vpp_out_data_u8, float *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int round_method = p_param->processor_param[0].CON_BRI_CTRL[crop_idx].con_bri_round;
  int h, w;

  float *p_src_ch0;
  float *p_src_ch1;
  float *p_src_ch2;

  bf16 *p_dst_ch0;
  bf16 *p_dst_ch1;
  bf16 *p_dst_ch2;

  int offset_ch1 = dst_crop_h * dst_crop_w;
  int offset_ch2 = offset_ch1 + dst_crop_h * dst_crop_w;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];

  p_dst_ch0 = &vpp_out_data_u8[0];
  p_dst_ch1 = &vpp_out_data_u8[offset_ch1];
  p_dst_ch2 = &vpp_out_data_u8[offset_ch2];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_ch0[h * dst_crop_w + w] = fp32tobf16(p_src_ch0[h * dst_crop_w + w],round_method);
      p_dst_ch1[h * dst_crop_w + w] = fp32tobf16(p_src_ch1[h * dst_crop_w + w],round_method);
      p_dst_ch2[h * dst_crop_w + w] = fp32tobf16(p_src_ch2[h * dst_crop_w + w],round_method);
    }
  }
}

void vpp_out_top_bf16(int crop_idx, float *post_pad_data, VPP1686_PARAM *p_param) {

  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;

  bf16 *vpp_tmp_float = new bf16[dst_crop_h * dst_crop_w * 3];

  //const int con_bri_en = p_param->processor_param[0].SRC_CTRL[crop_idx].con_bri_enable;

  switch (dst_fmt) {
    case OUT_YUV444P:
    case OUT_RGBP:
      vpp_out_three_planar_bf16(crop_idx, vpp_tmp_float, post_pad_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }

#ifdef WRITE_OUT_FILE
  int vpp_out_len;
  vpp_out_len = dst_crop_h * dst_crop_w * 3;

  char file_name[100];
  sprintf(file_name, "%s%s%d%s", OUT, "crop", crop_idx, "bf16.bin");

  FILE *fp = fopen(file_name, "wb");
  if (!fp) {
    printf("ERROR: Unable to open the out file\n");
    exit(-1);
  }

  fwrite(vpp_tmp_float, 1, vpp_out_len *sizeof(bf16), fp);
  fclose(fp);
#endif
  delete [] vpp_tmp_float;

}
void vpp_out_three_planar_fp32(int crop_idx, float *vpp_out_data_u8, float *csc_data, VPP1686_PARAM *p_param) {
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  int h, w;

  float *p_src_ch0;
  float *p_src_ch1;
  float *p_src_ch2;

  float *p_dst_ch0;
  float *p_dst_ch1;
  float *p_dst_ch2;

  int offset_ch1 = dst_crop_h * dst_crop_w;
  int offset_ch2 = offset_ch1 + dst_crop_h * dst_crop_w;

  p_src_ch0 = &csc_data[0];
  p_src_ch1 = &csc_data[dst_crop_h * dst_crop_w];
  p_src_ch2 = &csc_data[dst_crop_h * dst_crop_w * 2];

  p_dst_ch0 = &vpp_out_data_u8[0];
  p_dst_ch1 = &vpp_out_data_u8[offset_ch1];
  p_dst_ch2 = &vpp_out_data_u8[offset_ch2];

  for (h = 0; h < dst_crop_h; h++) {
    for (w = 0; w < dst_crop_w; w++) {
      p_dst_ch0[h * dst_crop_w + w] = p_src_ch0[h * dst_crop_w + w];
      p_dst_ch1[h * dst_crop_w + w] = p_src_ch1[h * dst_crop_w + w];
      p_dst_ch2[h * dst_crop_w + w] = p_src_ch2[h * dst_crop_w + w];
    }
  }
}

void vpp_out_top_fp32(int crop_idx, float *vpp_tmp_float,
      float *post_pad_data, VPP1686_PARAM *p_param) {
  const int dst_fmt = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;

  //const int con_bri_en = p_param->processor_param[0].SRC_CTRL[crop_idx].con_bri_enable;

  switch (dst_fmt) {
    case OUT_YUV444P:
    case OUT_RGBP:
      vpp_out_three_planar_fp32(crop_idx, vpp_tmp_float, post_pad_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }

#ifdef WRITE_OUT_FILE
  const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
  const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;

  int vpp_out_len;
  vpp_out_len = dst_crop_h * dst_crop_w * 3;

  char file_name[100];
  sprintf(file_name, "%s%s%d%s", OUT, "crop", crop_idx, "fp32.bin");

  FILE *fp = fopen(file_name, "wb");
  if (!fp) {
    printf("ERROR: Unable to open the out file\n");
    exit(-1);
  }
#if 1
  for(int i=0; i<vpp_out_len; i++){
    float2char.f = vpp_tmp_float[i];

    fwrite(&float2char.c[0], 1, 1, fp);
    fwrite(&float2char.c[1], 1, 1, fp);
    fwrite(&float2char.c[2], 1, 1, fp);
    fwrite(&float2char.c[3], 1, 1, fp);
  }
#endif
//  fwrite(vpp_tmp_float, 1, vpp_out_len *sizeof(float), fp);

  fclose(fp);
#endif
}

/****************************************************************************/
void write_yuv400p_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param)
{
  uint32 dst_stride_c0 = p_param->processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0;
  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;

  uint8 *out_ch0 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH0);

  uint32 i = 0, j = 0;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_stride_c0 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j];
    }
  }
}

void write_yuv420p_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param)
{
  uint32 dst_stride_c0 = p_param->processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0;
  uint32 dst_stride_c1 = p_param->processor_param[0].DST_STRIDE_CH1[0].dst_stride_ch1;
  uint32 dst_stride_c2 = p_param->processor_param[0].DST_STRIDE_CH2[0].dst_stride_ch2;
  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;

  uint8 *out_ch0 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH0);
  uint8 *out_ch1 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH1);
  uint8 *out_ch2 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH2);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;
  uint32 dst_uv_h_even = VPPALIGN(dst_crop_h, 2)/2;
  uint32 dst_uv_w_even = VPPALIGN(dst_crop_w, 2)/2;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_stride_c0 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for(i = 0; i < dst_uv_h_even; i++)
  {
    for(j = 0; j < dst_uv_w_even; j++)
    {
      out_ch1[(dst_crop_st_y/2 + i) * dst_stride_c1 + dst_crop_st_x/2 + j]= vpp_tmp_uint8[i * dst_uv_w_even + j + offset_tmp];
    }
  }

  offset_tmp = offset_tmp + dst_uv_h_even * dst_uv_w_even;

  for(i = 0; i < dst_uv_h_even; i++)
  {
    for(j = 0; j < dst_uv_w_even; j++)
    {
      out_ch2[(dst_crop_st_y/2 + i) * dst_stride_c2 + dst_crop_st_x/2 + j]= vpp_tmp_uint8[i * dst_uv_w_even + j + offset_tmp];
    }
  }

}

void write_yuv420sp_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param)
{
  uint32 dst_stride_c0 = p_param->processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0;
  uint32 dst_stride_c1 = p_param->processor_param[0].DST_STRIDE_CH1[0].dst_stride_ch1;
  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;

  uint8 *out_ch0 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH0);
  uint8 *out_ch1 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH1);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;
  uint32 dst_crop_sz_h_even = VPPALIGN(dst_crop_h, 2);
  uint32 dst_crop_sz_w_even = VPPALIGN(dst_crop_w, 2);

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_stride_c0 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for (i = 0; i < dst_crop_sz_h_even/2; i++)
  {
    for (j = 0; j < dst_crop_sz_w_even; j++)
    {
      out_ch1[(dst_crop_st_y/2 + i) * dst_stride_c1 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_sz_w_even + j + offset_tmp];
    }
  }

}

void write_yuv444p_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param)
{
  uint32 dst_stride_c0 = p_param->processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0;
  uint32 dst_stride_c1 = p_param->processor_param[0].DST_STRIDE_CH1[0].dst_stride_ch1;
  uint32 dst_stride_c2 = p_param->processor_param[0].DST_STRIDE_CH2[0].dst_stride_ch2;
  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;

  uint8 *out_ch0 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH0);
  uint8 *out_ch1 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH1);
  uint8 *out_ch2 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH2);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_stride_c0 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch1[(dst_crop_st_y + i) * dst_stride_c1 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j + offset_tmp];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w * 2;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch2[(dst_crop_st_y + i) * dst_stride_c2 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j + offset_tmp];
    }
  }

}

void write_rgbyp_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param)
{
  uint32 dst_stride_c0 = p_param->processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0;
  uint32 dst_stride_c1 = p_param->processor_param[0].DST_STRIDE_CH1[0].dst_stride_ch1;
  uint32 dst_stride_c2 = p_param->processor_param[0].DST_STRIDE_CH2[0].dst_stride_ch2;
  uint32 dst_stride_c3 = p_param->processor_param[0].DST_STRIDE_CH3[0].dst_stride_ch3;
  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;

  uint8 *out_ch0 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH0);
  uint8 *out_ch1 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH1);
  uint8 *out_ch2 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH2);
  uint8 *out_ch3 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH3);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_stride_c0 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch1[(dst_crop_st_y + i) * dst_stride_c1 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j + offset_tmp];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w * 2;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch2[(dst_crop_st_y + i) * dst_stride_c2 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j + offset_tmp];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w * 3;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch3[(dst_crop_st_y + i) * dst_stride_c3 + dst_crop_st_x + j]= vpp_tmp_uint8[i * dst_crop_w + j + offset_tmp];
    }
  }

}
void write_rgb24_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param)
{
  uint32 dst_stride_c0 = p_param->processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0;
  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;
  uint8 *out_ch0 = (uint8 *)(p_param->processor_param[0].DST_BASE_CH0);

  uint32 i = 0, j = 0;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w * 3; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_stride_c0 + dst_crop_st_x * 3 + j]= vpp_tmp_uint8[i * dst_crop_w *3 + j];
    }
  }
}

void write_dst_data_u8(uint8 * vpp_tmp_uint8, VPP1686_PARAM *p_param){

  const int  format_out = p_param->processor_param[0].DST_CTRL[0].output_format;

  switch (format_out) {
    case OUT_YUV400P:
      write_yuv400p_u8(vpp_tmp_uint8, p_param);
      break;

    case OUT_YUV420P:
      write_yuv420p_u8(vpp_tmp_uint8, p_param);
      break;

    case OUT_NV12:
    case OUT_NV21:
      write_yuv420sp_u8(vpp_tmp_uint8, p_param);
      break;

    case OUT_YUV444P:
    case OUT_RGBP:
      write_yuv444p_u8(vpp_tmp_uint8, p_param);
      break;

    case IN_RGB24:
    case IN_BGR24:
    case OUT_HSV180:
    case OUT_HSV256:
      write_rgb24_u8(vpp_tmp_uint8, p_param);
      break;
    case OUT_RGBYP:
      write_rgbyp_u8(vpp_tmp_uint8, p_param);
      break;
    default:
      VppErr("dst format err!\n")
      break;
  }

}
void write_yuv444p_fp16(float *post_pad_data , VPP1686_PARAM *p_param){

  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;
  const int round_method = p_param->processor_param[0].CON_BRI_CTRL[0].con_bri_round;

  fp16 *out_ch0 = (fp16 *)(p_param->processor_param[0].DST_BASE_CH0);
  fp16 *out_ch1 = (fp16 *)(p_param->processor_param[0].DST_BASE_CH1);
  fp16 *out_ch2 = (fp16 *)(p_param->processor_param[0].DST_BASE_CH2);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= fp32tofp16(post_pad_data[i * dst_crop_w + j],round_method);
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch1[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= fp32tofp16(post_pad_data[i * dst_crop_w + j + offset_tmp],round_method);
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w * 2;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch2[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= fp32tofp16(post_pad_data[i * dst_crop_w + j + offset_tmp],round_method);
    }
  }

}

void write_dst_data_fp16(float *post_pad_data , VPP1686_PARAM *p_param){

  const int  format_out = p_param->processor_param[0].DST_CTRL[0].output_format;

  switch (format_out) {
    case OUT_YUV444P:
    case OUT_RGBP:
      write_yuv444p_fp16(post_pad_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }
}

void write_yuv444p_bf16(float *post_pad_data , VPP1686_PARAM *p_param){

  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;
  const int round_method = p_param->processor_param[0].CON_BRI_CTRL[0].con_bri_round;

  bf16 *out_ch0 = (bf16 *)(p_param->processor_param[0].DST_BASE_CH0);
  bf16 *out_ch1 = (bf16 *)(p_param->processor_param[0].DST_BASE_CH1);
  bf16 *out_ch2 = (bf16 *)(p_param->processor_param[0].DST_BASE_CH2);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= fp32tobf16(post_pad_data[i * dst_crop_w + j],round_method);
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch1[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= fp32tobf16(post_pad_data[i * dst_crop_w + j + offset_tmp],round_method);
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w * 2;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch2[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= fp32tobf16(post_pad_data[i * dst_crop_w + j + offset_tmp],round_method);
    }
  }

}

void write_dst_data_bf16(float *post_pad_data , VPP1686_PARAM *p_param){

  const int  format_out = p_param->processor_param[0].DST_CTRL[0].output_format;

  switch (format_out) {
    case OUT_YUV444P:
    case OUT_RGBP:
      write_yuv444p_bf16(post_pad_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }
}

void write_yuv444p_fp32(float *post_pad_data , VPP1686_PARAM *p_param){

  uint32 dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_width;
  uint32 dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[0].dst_crop_height;
  uint32 dst_crop_st_x = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_x;
  uint32 dst_crop_st_y = p_param->processor_param[0].DST_CROP_ST[0].dst_crop_st_y;

  float *out_ch0 = (float *)(p_param->processor_param[0].DST_BASE_CH0);
  float *out_ch1 = (float *)(p_param->processor_param[0].DST_BASE_CH1);
  float *out_ch2 = (float *)(p_param->processor_param[0].DST_BASE_CH2);

  uint32 i = 0, j = 0;
  uint32 offset_tmp;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch0[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= post_pad_data[i * dst_crop_w + j];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch1[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= post_pad_data[i * dst_crop_w + j + offset_tmp];
    }
  }

  offset_tmp = dst_crop_h * dst_crop_w * 2;

  for(i = 0; i < dst_crop_h; i++)
  {
    for(j = 0; j < dst_crop_w; j++)
    {
      out_ch2[(dst_crop_st_y + i) * dst_crop_w + dst_crop_st_x + j]= post_pad_data[i * dst_crop_w + j + offset_tmp];
    }
  }

}

void write_dst_data_fp32(float *post_pad_data , VPP1686_PARAM *p_param){

  const int  format_out = p_param->processor_param[0].DST_CTRL[0].output_format;

  switch (format_out) {
    case OUT_YUV444P:
    case OUT_RGBP:
      write_yuv444p_fp32(post_pad_data, p_param);
      break;

    default:
      VppErr("dst format err!\n")
      break;
  }
}


int test_vpp_1686(VPP1686_PARAM *p_param) {
  VppInfo("\n----- Start VPP-1686 Test -----\n");

  uint8 *in_ch0 = NULL;
  uint8 *in_ch1 = NULL;
  uint8 *in_ch2 = NULL;
  uint8 *vpp_in = NULL;
  int vpp_out_len;
  uint8 post_padding_en;
  uint8 padding_en;
  uint8 rect_en;
  uint8 font_en;  
  uint8 con_bri_en;
  uint8 csc_scale_order;
  uint8 wdma_form;
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
  int post_top = 0;
  int post_bottom = 0;
  int post_left = 0;
  int post_right = 0; 

  unsigned int pix_num;

  int round_method;

  const int  format_in = p_param->processor_param[0].SRC_CTRL[0].input_format;

  assert_input_format(format_in);

  const int src_width = (p_param->processor_param[0].SRC_SIZE >> 16) & 0xffff;
  const int src_height = p_param->processor_param[0].SRC_SIZE & 0xffff;

  const int src_stride_c0 = p_param->processor_param[0].SRC_STRIDE_CH0.src_stride_ch0;
  const int src_stride_c1 = p_param->processor_param[0].SRC_STRIDE_CH1.src_stride_ch1;
  const int src_stride_c2 = p_param->processor_param[0].SRC_STRIDE_CH2.src_stride_ch2;

  VppAssert(src_stride_c0 >= src_width);

  VppInfo("Param: SRC video size w x h = %d x %d\n", src_width, src_height);

  in_ch0 = (uint8 *)(p_param->processor_param[0].SRC_BASE_CH0);
  in_ch1 = (uint8 *)(p_param->processor_param[0].SRC_BASE_CH1);
  in_ch2 = (uint8 *)(p_param->processor_param[0].SRC_BASE_CH2);

  pix_num = src_height * (src_stride_c0 + src_stride_c1 + src_stride_c2);

  vpp_in = new uint8[pix_num];  // process

  /*read src data and gen cm_chxx_in.dat for vpp fpga verification*/
  read_src_data(format_in,
    vpp_in, in_ch0, in_ch1, in_ch2, 
    src_height, src_stride_c0, src_stride_c1, src_stride_c2);

  const int crop_num = p_param->processor_param[0].CROP_NUM;

  /*input fmt -> crop -> resample to yuv444p or rgbp(up sampling) ->padding->
  resieze -> csc to dst fmt(down sampling) ->normalization*/

  for (int crop_idx = 0; crop_idx < crop_num; crop_idx++) {
    const int  format_out = p_param->processor_param[0].DST_CTRL[crop_idx].output_format;
    const int dst_crop_w = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_width;
    const int dst_crop_h = p_param->processor_param[0].DST_CROP_SIZE[crop_idx].dst_crop_height;
    const int src_crop_w = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_width;
    const int src_crop_h = p_param->processor_param[0].SRC_CROP_SIZE[crop_idx].src_crop_height;

    post_padding_en = p_param->processor_param[0].SRC_CTRL[crop_idx].post_padding_enable;    
    padding_en = p_param->processor_param[0].SRC_CTRL[crop_idx].padding_enable;
    font_en = p_param->processor_param[0].SRC_CTRL[crop_idx].font_enable;
    rect_en = p_param->processor_param[0].SRC_CTRL[crop_idx].rect_border_enable;
    con_bri_en = p_param->processor_param[0].SRC_CTRL[crop_idx].con_bri_enable;
    csc_scale_order = p_param->processor_param[0].SRC_CTRL[crop_idx].csc_scale_order;
    wdma_form = p_param->processor_param[0].SRC_CTRL[crop_idx].wdma_form;    

    round_method = p_param->processor_param[0].CON_BRI_CTRL[crop_idx].con_bri_round ;
    assert_output_format(format_out);
    VppInfo("crop_idx %d, format_in %d, format_out %d\n", crop_idx, format_in, format_out);

 //   int extrabytes = get_extrabytes(format_out, dst_crop_w);
  //  VppInfo("extrabytes %d\n", extrabytes);

    uint8 *crop_data = new uint8[src_crop_h * src_crop_w * 4];

    vpp_src_crop_top(crop_idx, vpp_in, crop_data, p_param);

    /*rgbxxx upsampling to rgbp, and yuvxxx upsampling to yuv444p except yuv400p*/
    uint8 *up_sampling_data = new uint8[src_crop_h * src_crop_w * 3];
    vpp_up_sampling_top(crop_idx, up_sampling_data, crop_data, p_param);
   
    delete [] crop_data;
    crop_data = NULL;

    if (padding_en) {
      VppAssert((font_en == 0) && (rect_en == 0));
      top = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_up;
      bottom = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_bot;
      left = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_left;
      right = p_param->processor_param[0].PADDING_EXT[crop_idx].padding_ext_right;
    }
    else{
      top    = 0;
      bottom = 0;
      left   = 0;
      right  = 0;     
    }
    if (post_padding_en){
      post_top = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_up;
      post_bottom = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_bot;
      post_left = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_left;
      post_right = p_param->processor_param[0].POST_PADDING_EXT[crop_idx].post_padding_ext_right;      
    }
    else{
      post_top = 0;
      post_bottom =0;
      post_left = 0;
      post_right =0;        
    }
    uint8 *padding_data = new uint8[(src_crop_h + top + bottom) * (src_crop_w + left + right) * 3];

    vpp_padding_top(crop_idx, padding_en, padding_data, up_sampling_data, p_param);

    delete [] up_sampling_data;
    up_sampling_data = NULL;



    if (font_en) {
      VppAssert((padding_en == 0)&& (post_padding_en == 0)&&(con_bri_en==0)&& (rect_en == 0)&& (wdma_form==1));
      //// if mult crop and font/rect enable ;should reload font_rect_crop[crop_idx-1] data
      vpp_font_top(crop_idx, padding_data, p_param);
    }
    if (rect_en) {
      VppAssert((padding_en == 0)&& (post_padding_en == 0)&&(con_bri_en==0) && (font_en == 0)&& (wdma_form==1));
      //// if mult crop and font/rect enable ;should reload font_rect_crop[crop_idx-1] data
      vpp_rect_top(crop_idx, padding_data, p_param);
    }

    uint32 pix_num_ch_before_scal = (src_crop_h + top + bottom) * (src_crop_w + left + right);
    uint32 pix_num_ch_after_scal = (dst_crop_h - post_top - post_bottom) * (dst_crop_w - post_left - post_right);
    get_len_based_on_dst_fmt(crop_idx, &vpp_out_len, p_param);

    VppInfo("pix_num_ch_before_scal = %d pix_num_ch_after_scal= %d\n",pix_num_ch_before_scal,pix_num_ch_after_scal);     

    float *tmp_data     = new float[pix_num_ch_before_scal * 3];
    float *scaling_data = new float[pix_num_ch_after_scal * 3];
    float *csc_data     = new float[pix_num_ch_after_scal * 4];
    float *con_bri_data = new float[pix_num_ch_after_scal * 4];

    float *post_pad_data= new float[dst_crop_h * dst_crop_w * 4];
    uint8 *tmp_data_i   = new uint8[dst_crop_h * dst_crop_w * 4];
    int8  *tmp_data_c   = new int8[dst_crop_h * dst_crop_w * 4];

    char *vpp_tmp_int8 = new char[dst_crop_h * dst_crop_w * 4];
    uint8 *vpp_tmp_uint8 = new uint8[dst_crop_h * dst_crop_w * 4];

    if(csc_scale_order){//float fp16 bf16 only support three plannar, no RGBY
      delete[]csc_data     ;  
      csc_data = NULL;
      csc_data     = new float[pix_num_ch_before_scal * 4];      
    }

   uint32  pixlen_allch_before_scal = (format_in == IN_YUV400P) ? pix_num_ch_before_scal  : pix_num_ch_before_scal * 3;
 
    for (uint32 i=0; i < pixlen_allch_before_scal; i++){
      tmp_data[i] = float(padding_data[i]);
    }

    delete [] padding_data;    
    padding_data = NULL;

    if(!csc_scale_order){
        //scal
        vpp_scaling_top_float(crop_idx, scaling_data, tmp_data, p_param);   
        delete [] tmp_data;
        tmp_data = NULL;
        //csc
        VppInfo("process before  vpp_csc_top_float\n");     

        vpp_csc_top_float(crop_idx, csc_data, scaling_data, p_param); 
        delete [] scaling_data;
        scaling_data = NULL;

        VppInfo("process after  vpp_csc_top_float\n");     

        //conbri_en
        if(con_bri_en) vpp_con_bri_top_float(crop_idx, con_bri_data, csc_data, p_param);  
        else 
        {
          if ( format_out ==OUT_RGBYP )
            memcpy(con_bri_data,csc_data,pix_num_ch_after_scal * 4 * sizeof(float));
          else
            memcpy(con_bri_data,csc_data,pix_num_ch_after_scal * 3 * sizeof(float));
        }

        delete [] csc_data;
        csc_data = NULL;
    }
    else{
        //csc
        vpp_csc_top_float(crop_idx, csc_data, tmp_data, p_param);    
        delete [] tmp_data;
        tmp_data = NULL;
        //scaling
        vpp_scaling_top_float(crop_idx, scaling_data, csc_data, p_param);
        delete [] csc_data;
        csc_data = NULL;

        //conbri_en
        if(con_bri_en) vpp_con_bri_top_float(crop_idx, con_bri_data, scaling_data, p_param);  
        else 
        {
         memcpy(con_bri_data,scaling_data,pix_num_ch_after_scal * 3 * sizeof(float));
        }

        delete [] scaling_data;
        scaling_data = NULL;
    }

    //post_padding
    if(post_padding_en) 
      vpp_post_padding_float(crop_idx, post_pad_data, con_bri_data, p_param);
    else {
      if ( format_out ==OUT_RGBYP && (! csc_scale_order) )
        memcpy(post_pad_data,con_bri_data,dst_crop_h * dst_crop_w  * 4 * sizeof(float));
      else 
        memcpy(post_pad_data,con_bri_data,dst_crop_h * dst_crop_w  * 3 * sizeof(float));
    }
    delete [] con_bri_data;
    con_bri_data     = NULL;

    VppInfo(" --- **** ---- \n");


    VppInfo("process after  post_padding \n");

    /*down_sample_data is continuous,
    that is to say down_sample_data is not considering stride*/
    uint8 *down_sample_data = new uint8[dst_crop_h * dst_crop_w * 4];


  //wdma form
    int pixlen;
    pixlen = ( format_out ==OUT_RGBYP ) ? dst_crop_h * dst_crop_w * 4 : dst_crop_h * dst_crop_w * 3;

    //TODO
    //if (wdma_form == 0 )
    //  VppAssert(con_bri_en == 1);

    //if(wdma_form == 0 && con_bri_en == 1){ //int8
    if (wdma_form == 0 ) {
        for (int i=0; i < pixlen; i++){
          tmp_data_c[i] = float2int8(post_pad_data[i],round_method);
        }
        ////change tmp_dat_c to uint8 type to use vpp_out functions
        memcpy(tmp_data_i,tmp_data_c, pixlen);
      if ( get_yuv444_down_sample_flag(format_out) ) {
    /*only dst fmt yuv420p nv12 and nv21 need to do down sample*/
        vpp_yuv444p_down_sample_top(crop_idx, down_sample_data, tmp_data_i, p_param);
      }
     // else {
     //   memcpy(down_sample_data,tmp_data_i,pixlen);
     // }
      vpp_out_top_u8(crop_idx, vpp_tmp_uint8, down_sample_data, tmp_data_i,p_param);
      write_dst_data_u8(vpp_tmp_uint8, p_param);
    }

    else if(wdma_form == 1){//uint8
      for (int i=0; i < pixlen; i++){
        tmp_data_i[i] = float2uint8(post_pad_data[i],round_method);
      }

      if ( get_yuv444_down_sample_flag(format_out) ) {
    /*only dst fmt yuv420p nv12 and nv21 need to do down sample*/
        vpp_yuv444p_down_sample_top(crop_idx, down_sample_data, tmp_data_i, p_param);
      }
      //else {
      //  memcpy(down_sample_data,tmp_data_i,pixlen);
      //}
      vpp_out_top_u8(crop_idx, vpp_tmp_uint8, down_sample_data, tmp_data_i,p_param);
      write_dst_data_u8(vpp_tmp_uint8, p_param);
    }

    else if(wdma_form == 2){//fp16
   //   vpp_out_top_fp16(crop_idx, post_pad_data,p_param);
      write_dst_data_fp16(post_pad_data,p_param);
    }

    else if(wdma_form == 3){//bf16
     // vpp_out_top_bf16(crop_idx, post_pad_data,p_param);
      write_dst_data_bf16(post_pad_data,p_param);
    }

    else if(wdma_form == 4){//fp32
       float *vpp_tmp_float = new float[dst_crop_h * dst_crop_w * 3];
      // vpp_out_top_fp32(crop_idx, vpp_tmp_float, post_pad_data,p_param);
       write_dst_data_fp32(post_pad_data,p_param);
       delete [] vpp_tmp_float;
    }
    else{ //unknow data format
      VppErr("unknow data format: %d",wdma_form);
    }



    VppInfo("process vpp_out_top finish\n");

    delete [] post_pad_data;
    post_pad_data = NULL;
    delete []  tmp_data_c;
    tmp_data_c = NULL;
    delete []  tmp_data_i;
    tmp_data_i = NULL;
    delete []  down_sample_data;
    down_sample_data = NULL;
    delete [] vpp_tmp_uint8;
    vpp_tmp_uint8 = NULL;
    delete []vpp_tmp_int8;
    vpp_tmp_int8 = NULL;
  }

  delete [] vpp_in;
  vpp_in = NULL;

  VppInfo("\n----- End VPP-1686 Test -----\n");

  return VPP_OK;
}

  /*
  *color_space_in :
  *0: rgb
  *1: yuv
  */

  /*
  *color_space_out :
  *0: rgb
  *1: yuv
  *2: hsv
  */
int output_color_space_judgment(uint8 dst_fmt, int color_space_in) {
  uint8 color_space_out = 0;

  VppAssert((color_space_in == COLOR_SPACE_IN_RGB) || (color_space_in == COLOR_SPACE_IN_YUV));
  color_space_out = ((dst_fmt <= OUT_NV21) && (dst_fmt != 2)) ? COLOR_SPACE_OUT_YUV :
    ((dst_fmt == OUT_HSV180) || (dst_fmt == OUT_HSV256)) ? COLOR_SPACE_OUT_HSV : COLOR_SPACE_OUT_RGB;

  if ((color_space_in == COLOR_SPACE_IN_YUV) && (dst_fmt == OUT_RGBYP)) {
    color_space_out = COLOR_SPACE_OUT_RGB;  // OUT_RGBYP is considered to be rgb color space when input coloe space is yuv
  } else if ((color_space_in == COLOR_SPACE_IN_RGB) && (dst_fmt == OUT_RGBYP)) {
    color_space_out = COLOR_SPACE_OUT_YUV;  // OUT_RGBYP is considered to be yuv color space when input coloe space is rgb
  }
  return color_space_out;
}

/*
*color_space_in : 0: rgb, 1: yuv
*
*color_space_out : 0: rgb, 1: yuv, 2: hsv
*csc_path: 0: yuv to yuv, or rgb to rgb; 1: yuv to rgb; 2: rgb to yuv; 3: rgb to hsv;
*/
int csc_path_judgment(int color_space_in, int color_space_out) {
  int csc_path = 0;
  VppAssert((color_space_in == COLOR_SPACE_IN_RGB) || (color_space_in == COLOR_SPACE_IN_YUV));
  VppAssert((color_space_out == COLOR_SPACE_OUT_RGB) || (color_space_out == COLOR_SPACE_OUT_YUV) ||
    (color_space_out == COLOR_SPACE_OUT_HSV));


  switch (color_space_in) {
    case COLOR_SPACE_IN_RGB:
      csc_path = (color_space_out == COLOR_SPACE_OUT_YUV) ? CSC_PATH_RGB_YUV :
        (color_space_out == COLOR_SPACE_OUT_HSV) ? CSC_PATH_RGB_HSV : CSC_PATH_RGB_RGB;
      break;

    case COLOR_SPACE_IN_YUV:
      //VppAssert(color_space_out != COLOR_SPACE_OUT_HSV);
      csc_path = (color_space_out == COLOR_SPACE_OUT_YUV) ? CSC_PATH_YUV_YUV : (color_space_out == COLOR_SPACE_OUT_HSV) ? CSC_PATH_YUV_HSV : CSC_PATH_YUV_RGB;
      break;

    default:  // error input color space
      VppErr("csc_path_judgment err!\n")
      break;
  }
  return csc_path;
}

void malloc_mem_vpp_param(VPP1686_PARAM *vpp_param) {
  int crop_num = vpp_param->processor_param[0].CROP_NUM;

  vpp_param->processor_param[0].DES_HEAD = (struct DES_HEAD_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_CTRL = (struct SRC_CTRL_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_CROP_SIZE = (struct SRC_CROP_SIZE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_CROP_ST = (struct SRC_CROP_ST_S *)new uint32[crop_num];

  vpp_param->processor_param[0].DST_CTRL = (struct DST_CTRL_S *)new uint32[crop_num];
  vpp_param->processor_param[0].DST_CROP_ST = (struct DST_CROP_ST_S *)new uint32[crop_num];
  vpp_param->processor_param[0].DST_CROP_SIZE = (struct DST_CROP_SIZE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].DST_STRIDE_CH0 = (struct DST_STRIDE_CH0_S *)new uint32[crop_num];
  vpp_param->processor_param[0].DST_STRIDE_CH1 = (struct DST_STRIDE_CH1_S *)new uint32[crop_num];
  vpp_param->processor_param[0].DST_STRIDE_CH2 = (struct DST_STRIDE_CH2_S *)new uint32[crop_num];
  vpp_param->processor_param[0].DST_STRIDE_CH3 = (struct DST_STRIDE_CH3_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SCL_CTRL = (struct SCL_CTRL_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SCL_INIT = (struct SCL_INIT_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SCL_X = (struct SCL_X_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SCL_Y = (struct SCL_Y_S *)new uint32[crop_num];

  vpp_param->processor_param[0].PADDING_VALUE = (struct PADDING_VALUE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].PADDING_EXT = (struct PADDING_EXT_S *)new uint32[crop_num];

  vpp_param->processor_param[0].SRC_BORDER_VALUE = (struct SRC_BORDER_VALUE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_BORDER_SIZE = (struct SRC_BORDER_SIZE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_BORDER_ST = (struct SRC_BORDER_ST_S *)new uint32[crop_num];

  vpp_param->processor_param[0].SRC_FONT_PITCH = (struct SRC_FONT_PITCH_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_FONT_VALUE = (struct SRC_FONT_VALUE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_FONT_SIZE = (struct SRC_FONT_SIZE_S *)new uint32[crop_num];
  vpp_param->processor_param[0].SRC_FONT_ST = (struct SRC_FONT_ST_S *)new uint32[crop_num];
//  vpp_param->processor_param[0].SRC_FONT_ADDR = new uint64[crop_num];
//  vpp_param->processor_param[0].CSC_MATRIX = (struct CSC_MATRIX_S *)new uint32[crop_num * 12];

  vpp_param->processor_param[0].CON_BRI_A_0 = (struct CON_BRI_A_0_S *)new float[crop_num];
  vpp_param->processor_param[0].CON_BRI_A_1 = (struct CON_BRI_A_1_S *)new float[crop_num];
  vpp_param->processor_param[0].CON_BRI_A_2 = (struct CON_BRI_A_2_S *)new float[crop_num];

  vpp_param->processor_param[0].CON_BRI_CTRL = (struct CON_BRI_CTRL_S *)new uint32[crop_num];

  vpp_param->processor_param[0].CON_BRI_B_0 = (struct CON_BRI_B_0_S *)new float[crop_num];
  vpp_param->processor_param[0].CON_BRI_B_1 = (struct CON_BRI_B_1_S *)new float[crop_num];
  vpp_param->processor_param[0].CON_BRI_B_2 = (struct CON_BRI_B_2_S *)new float[crop_num];
  //for post padding value is float for each channel
  vpp_param->processor_param[0].POST_PADDING_VALUE = (struct POST_PADDING_VALUE_S *) new uint32 [crop_num * 4];
  vpp_param->processor_param[0].POST_PADDING_EXT   = (struct POST_PADDING_EXT_S *) new uint32 [crop_num];

}

void free_mem_vpp_param(VPP1686_PARAM *vpp_param) {
  delete [] vpp_param->processor_param[0].DES_HEAD;
  delete [] vpp_param->processor_param[0].SRC_CTRL;
  delete [] vpp_param->processor_param[0].SRC_CROP_SIZE;
  delete [] vpp_param->processor_param[0].SRC_CROP_ST;

  delete [] vpp_param->processor_param[0].DST_CROP_ST;
  delete [] vpp_param->processor_param[0].DST_CROP_SIZE;
  delete [] vpp_param->processor_param[0].DST_STRIDE_CH0;
  delete [] vpp_param->processor_param[0].DST_STRIDE_CH1;
  delete [] vpp_param->processor_param[0].DST_STRIDE_CH2;
  delete [] vpp_param->processor_param[0].DST_STRIDE_CH3;
  delete [] vpp_param->processor_param[0].SCL_CTRL;
  delete [] vpp_param->processor_param[0].SCL_INIT;
  delete [] vpp_param->processor_param[0].SCL_X;
  delete [] vpp_param->processor_param[0].SCL_Y;

  delete [] vpp_param->processor_param[0].PADDING_VALUE;
  delete [] vpp_param->processor_param[0].PADDING_EXT;

  delete [] vpp_param->processor_param[0].SRC_BORDER_VALUE;
  delete [] vpp_param->processor_param[0].SRC_BORDER_SIZE;
  delete [] vpp_param->processor_param[0].SRC_BORDER_ST;

  delete [] vpp_param->processor_param[0].SRC_FONT_PITCH;
  delete [] vpp_param->processor_param[0].SRC_FONT_VALUE;
  delete [] vpp_param->processor_param[0].SRC_FONT_SIZE;
  delete [] vpp_param->processor_param[0].SRC_FONT_ST;
//  delete [] vpp_param->processor_param[0].SRC_FONT_ADDR;
  delete [] vpp_param->processor_param[0].DST_CTRL;
//  delete [] vpp_param->processor_param[0].CSC_MATRIX;

  delete [] vpp_param->processor_param[0].CON_BRI_A_0;
  delete [] vpp_param->processor_param[0].CON_BRI_A_1;
  delete [] vpp_param->processor_param[0].CON_BRI_A_2;

  delete [] vpp_param->processor_param[0].CON_BRI_CTRL;

  delete [] vpp_param->processor_param[0].CON_BRI_B_0;
  delete [] vpp_param->processor_param[0].CON_BRI_B_1;
  delete [] vpp_param->processor_param[0].CON_BRI_B_2;

  delete [] vpp_param->processor_param[0].POST_PADDING_VALUE;
  delete [] vpp_param->processor_param[0].POST_PADDING_EXT;
}

/*
*color_space_in : 0: rgb, 1: yuv
*color_space_out : 0: rgb, 1: yuv, 2: hsv
*csc_path: 1: yuv to rgb; 2: rgb to yuv; 0: yuv to yuv, or rgb to rgb, or rgb to hsv
*/
int bm1684x_vpp_cmodel(struct vpp_batch_n *batch,
  bm1684x_vpp_mat   *vpp_input,
  bm1684x_vpp_mat   *vpp_output,
  bm1684x_vpp_param *vpp_param)
{
  VPP1686_PARAM p_param;
  int csc_path, idx = 0;
  int color_space_in;
  int color_space_out;
  descriptor *pdes = NULL;

  memset(&p_param, 0, sizeof(VPP1686_PARAM));

  malloc_mem_vpp_param(&p_param);

  for (idx = 0; idx < batch->num; idx++) {

    pdes = (batch->cmd + idx);

    color_space_in = input_color_space_judgment(pdes->src_ctrl.input_format);
    color_space_out = output_color_space_judgment(pdes->dst_ctrl.output_format, color_space_in);
    csc_path = csc_path_judgment(color_space_in, color_space_out);
    if (pdes->dst_ctrl.output_format == OUT_RGBYP) {
      VppAssert((csc_path == CSC_PATH_RGB_YUV) || (csc_path == CSC_PATH_YUV_RGB));
    }

    p_param.USER_DEF.csc_path = csc_path & 0x0f;
    p_param.PROJECT = 1686;
    p_param.processor_param[0].CROP_NUM = 1;

    p_param.processor_param[0].SRC_SIZE = ((pdes->src_size.src_width & 0xffff) << 16) | (pdes->src_size.src_height & 0xffff);

    p_param.processor_param[0].DES_HEAD[0].crop_flag = pdes->des_head.crop_flag;
    p_param.processor_param[0].DES_HEAD[0].crop_id = pdes->des_head.crop_id;

    p_param.processor_param[0].SRC_CTRL[0].input_format = (pdes->src_ctrl.input_format & 0x1f);
    p_param.processor_param[0].SRC_CTRL[0].post_padding_enable = (pdes->src_ctrl.post_padding_enable & 0x1);
    p_param.processor_param[0].SRC_CTRL[0].padding_enable = (pdes->src_ctrl.padding_enable & 0x1);
    p_param.processor_param[0].SRC_CTRL[0].rect_border_enable = (pdes->src_ctrl.rect_border_enable & 0x1);
    p_param.processor_param[0].SRC_CTRL[0].font_enable = (pdes->src_ctrl.font_enable & 0x1);
    p_param.processor_param[0].SRC_CTRL[0].con_bri_enable = (pdes->src_ctrl.con_bri_enable & 0x1);
    p_param.processor_param[0].SRC_CTRL[0].csc_scale_order = (pdes->src_ctrl.csc_scale_order & 0x1);
    p_param.processor_param[0].SRC_CTRL[0].wdma_form = (pdes->src_ctrl.wdma_form & 0x7);
    p_param.processor_param[0].SRC_CTRL[0].fbc_multi_map = (pdes->src_ctrl.fbc_multi_map& 0x1);

    p_param.processor_param[0].SRC_CROP_ST[0].src_crop_st_x = pdes->src_crop_st.src_crop_st_x & 0x3fff;
    p_param.processor_param[0].SRC_CROP_ST[0].src_crop_st_y = pdes->src_crop_st.src_crop_st_y & 0x3fff;
    p_param.processor_param[0].SRC_CROP_SIZE[0].src_crop_width = pdes->src_crop_size.src_crop_width & 0x3fff;
    p_param.processor_param[0].SRC_CROP_SIZE[0].src_crop_height = pdes->src_crop_size.src_crop_height & 0x3fff;

    p_param.processor_param[0].SRC_BASE_CH0 = vpp_input[idx].addr0;
    p_param.processor_param[0].SRC_BASE_CH1 = vpp_input[idx].addr1;
    p_param.processor_param[0].SRC_BASE_CH2 = vpp_input[idx].addr2;

    p_param.processor_param[0].SRC_STRIDE_CH0.src_stride_ch0 = pdes->src_stride_ch0.src_stride_ch0;
    p_param.processor_param[0].SRC_STRIDE_CH1.src_stride_ch1 = pdes->src_stride_ch1.src_stride_ch1;
    p_param.processor_param[0].SRC_STRIDE_CH2.src_stride_ch2 = pdes->src_stride_ch2.src_stride_ch2;

    /*padding*/
    p_param.processor_param[0].PADDING_EXT[0].padding_ext_up = pdes->padding_ext.padding_ext_up;
    p_param.processor_param[0].PADDING_EXT[0].padding_ext_bot = pdes->padding_ext.padding_ext_bot;
    p_param.processor_param[0].PADDING_EXT[0].padding_ext_left = pdes->padding_ext.padding_ext_left;
    p_param.processor_param[0].PADDING_EXT[0].padding_ext_right = pdes->padding_ext.padding_ext_right;
    p_param.processor_param[0].PADDING_VALUE[0].padding_value_ch0 = pdes->padding_value.padding_value_ch0;
    p_param.processor_param[0].PADDING_VALUE[0].padding_value_ch1 = pdes->padding_value.padding_value_ch1;
    p_param.processor_param[0].PADDING_VALUE[0].padding_value_ch2 = pdes->padding_value.padding_value_ch2;

    /*rect border*/
    p_param.processor_param[0].SRC_BORDER_VALUE[0].src_border_value_r = pdes->src_border_value.src_border_value_r;
    p_param.processor_param[0].SRC_BORDER_VALUE[0].src_border_value_g = pdes->src_border_value.src_border_value_g;
    p_param.processor_param[0].SRC_BORDER_VALUE[0].src_border_value_b = pdes->src_border_value.src_border_value_b;
    p_param.processor_param[0].SRC_BORDER_VALUE[0].src_border_thickness = pdes->src_border_value.src_border_thickness;
    p_param.processor_param[0].SRC_BORDER_SIZE[0].src_border_height = pdes->src_border_size.src_border_height;
    p_param.processor_param[0].SRC_BORDER_SIZE[0].src_border_width = pdes->src_border_size.src_border_width;
    p_param.processor_param[0].SRC_BORDER_ST[0].src_border_st_y = pdes->src_border_st.src_border_st_y;
    p_param.processor_param[0].SRC_BORDER_ST[0].src_border_st_x = pdes->src_border_st.src_border_st_x;

    /*font*/
    p_param.processor_param[0].SRC_FONT_PITCH[0].src_font_pitch = pdes->src_font_pitch.src_font_pitch;
    p_param.processor_param[0].SRC_FONT_PITCH[0].src_font_type = pdes->src_font_pitch.src_font_type;
    p_param.processor_param[0].SRC_FONT_PITCH[0].src_font_nf_range = pdes->src_font_pitch.src_font_nf_range;
    p_param.processor_param[0].SRC_FONT_PITCH[0].src_font_dot_en = pdes->src_font_pitch.src_font_dot_en;
    p_param.processor_param[0].SRC_FONT_VALUE[0].src_font_value_r = pdes->src_font_value.src_font_value_r;
    p_param.processor_param[0].SRC_FONT_VALUE[0].src_font_value_g = pdes->src_font_value.src_font_value_g;
    p_param.processor_param[0].SRC_FONT_VALUE[0].src_font_value_b = pdes->src_font_value.src_font_value_b;
    p_param.processor_param[0].SRC_FONT_VALUE[0].src_font_ext = pdes->src_font_value.src_font_ext;
    p_param.processor_param[0].SRC_FONT_SIZE[0].src_font_height = pdes->src_font_size.src_font_height;
    p_param.processor_param[0].SRC_FONT_SIZE[0].src_font_width = pdes->src_font_size.src_font_width;
    p_param.processor_param[0].SRC_FONT_ST[0].src_font_st_y = pdes->src_font_st.src_font_st_y;
    p_param.processor_param[0].SRC_FONT_ST[0].src_font_st_x = pdes->src_font_st.src_font_st_x;
    p_param.processor_param[0].SRC_FONT_ADDR = vpp_param[idx].font_param.font_addr;

    p_param.processor_param[0].DST_CTRL[0].output_format = pdes->dst_ctrl.output_format & 0x1f;
    p_param.processor_param[0].DST_CTRL[0].cb_coeff_sel_tl = pdes->dst_ctrl.cb_coeff_sel_tl;
    p_param.processor_param[0].DST_CTRL[0].cb_coeff_sel_tr = pdes->dst_ctrl.cb_coeff_sel_tr;
    p_param.processor_param[0].DST_CTRL[0].cb_coeff_sel_bl = pdes->dst_ctrl.cb_coeff_sel_bl;
    p_param.processor_param[0].DST_CTRL[0].cb_coeff_sel_br = pdes->dst_ctrl.cb_coeff_sel_br;
    p_param.processor_param[0].DST_CTRL[0].cr_coeff_sel_tl = pdes->dst_ctrl.cr_coeff_sel_tl;
    p_param.processor_param[0].DST_CTRL[0].cr_coeff_sel_tr = pdes->dst_ctrl.cr_coeff_sel_tr;
    p_param.processor_param[0].DST_CTRL[0].cr_coeff_sel_bl = pdes->dst_ctrl.cr_coeff_sel_bl;
    p_param.processor_param[0].DST_CTRL[0].cr_coeff_sel_br = pdes->dst_ctrl.cr_coeff_sel_br;

    p_param.processor_param[0].DST_CROP_ST[0].dst_crop_st_x = pdes->dst_crop_st.dst_crop_st_x & 0x3fff;
    p_param.processor_param[0].DST_CROP_ST[0].dst_crop_st_y = pdes->dst_crop_st.dst_crop_st_y & 0x3fff;
    p_param.processor_param[0].DST_CROP_SIZE[0].dst_crop_width = pdes->dst_crop_size.dst_crop_width & 0x3fff;
    p_param.processor_param[0].DST_CROP_SIZE[0].dst_crop_height = pdes->dst_crop_size.dst_crop_height & 0x3fff;

    p_param.processor_param[0].DST_STRIDE_CH0[0].dst_stride_ch0 = pdes->dst_stride_ch0.dst_stride_ch0;
    p_param.processor_param[0].DST_STRIDE_CH1[0].dst_stride_ch1 = pdes->dst_stride_ch1.dst_stride_ch1;
    p_param.processor_param[0].DST_STRIDE_CH2[0].dst_stride_ch2 = pdes->dst_stride_ch2.dst_stride_ch2;
    p_param.processor_param[0].DST_STRIDE_CH3[0].dst_stride_ch3 = pdes->dst_stride_ch3.dst_stride_ch3;

    p_param.processor_param[0].DST_BASE_CH0 = vpp_output[idx].addr0;
    p_param.processor_param[0].DST_BASE_CH1 = vpp_output[idx].addr1;
    p_param.processor_param[0].DST_BASE_CH2 = vpp_output[idx].addr2;
    p_param.processor_param[0].DST_BASE_CH3 = vpp_output[idx].addr3;

    p_param.processor_param[0].SCL_CTRL[0].scl_ctrl = pdes->scl_ctrl.scl_ctrl & 0x3;
    p_param.processor_param[0].SCL_INIT[0].scl_init_x = pdes->scl_init.scl_init_x;
    p_param.processor_param[0].SCL_INIT[0].scl_init_y = pdes->scl_init.scl_init_y;
    memcpy(&(p_param.processor_param[0].SCL_X[0].scl_x), &pdes->scl_x, sizeof(float));
    memcpy(&(p_param.processor_param[0].SCL_Y[0].scl_y), &pdes->scl_y, sizeof(float));

    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE00,&pdes->csc_coe00,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE01,&pdes->csc_coe01,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE02,&pdes->csc_coe02,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_ADD0,&pdes->csc_add0,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE10,&pdes->csc_coe10,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE11,&pdes->csc_coe11,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE12,&pdes->csc_coe12,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_ADD1,&pdes->csc_add1,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE20,&pdes->csc_coe20,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE21,&pdes->csc_coe21,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_COE22,&pdes->csc_coe22,4);
    memcpy(&p_param.processor_param[0].CSC_MATRIX.CSC_ADD2,&pdes->csc_add2,4);

    p_param.processor_param[0].CON_BRI_A_0[0].con_bri_a_0 = pdes->con_bri_a_0;
    p_param.processor_param[0].CON_BRI_A_1[0].con_bri_a_1 = pdes->con_bri_a_1;
    p_param.processor_param[0].CON_BRI_A_2[0].con_bri_a_2 = pdes->con_bri_a_2;

    p_param.processor_param[0].CON_BRI_CTRL[0].con_bri_round = pdes->con_bri_ctrl.con_bri_round;
    p_param.processor_param[0].CON_BRI_CTRL[0].con_bri_full = pdes->con_bri_ctrl.con_bri_full;
    p_param.processor_param[0].CON_BRI_CTRL[0].hr_csc_f2i_round = pdes->con_bri_ctrl.hr_csc_f2i_round;
    p_param.processor_param[0].CON_BRI_CTRL[0].hr_csc_i2f_round = pdes->con_bri_ctrl.hr_csc_i2f_round;

    p_param.processor_param[0].CON_BRI_B_0[0].con_bri_b_0 = pdes->con_bri_b_0;
    p_param.processor_param[0].CON_BRI_B_1[0].con_bri_b_1 = pdes->con_bri_b_1;
    p_param.processor_param[0].CON_BRI_B_2[0].con_bri_b_2 = pdes->con_bri_b_2;

    p_param.processor_param[0].POST_PADDING_EXT[0].post_padding_ext_up = pdes->post_padding_ext.post_padding_ext_up;
    p_param.processor_param[0].POST_PADDING_EXT[0].post_padding_ext_bot = pdes->post_padding_ext.post_padding_ext_bot;
    p_param.processor_param[0].POST_PADDING_EXT[0].post_padding_ext_left = pdes->post_padding_ext.post_padding_ext_left;
    p_param.processor_param[0].POST_PADDING_EXT[0].post_padding_ext_right = pdes->post_padding_ext.post_padding_ext_right;

    memcpy(&(p_param.processor_param[0].POST_PADDING_VALUE[0].post_padding_value_ch0), &pdes->post_padding_value_ch0, sizeof(float));
    memcpy(&(p_param.processor_param[0].POST_PADDING_VALUE[0].post_padding_value_ch1), &pdes->post_padding_value_ch1, sizeof(float));
    memcpy(&(p_param.processor_param[0].POST_PADDING_VALUE[0].post_padding_value_ch2), &pdes->post_padding_value_ch2, sizeof(float));
    memcpy(&(p_param.processor_param[0].POST_PADDING_VALUE[0].post_padding_value_ch3), &pdes->post_padding_value_ch3, sizeof(float));

    test_vpp_1686(&p_param);
  }

  free_mem_vpp_param(&p_param);
  return 0;
}

#endif
