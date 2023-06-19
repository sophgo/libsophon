#ifndef __1684VPP_CMODEL__
#define __1684VPP_CMODEL__
#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(bool)
#define	bool	int
#endif
#if !defined(true)
#define true	1
#endif
#if !defined(false)
#define	false	0
#endif

typedef unsigned int   uint32 ;
typedef int            int32  ;
typedef unsigned short uint16 ;
typedef short          int16  ;
typedef unsigned char  uint8  ;

#define CSC_WL  10
#define SHIFT   0
#define FRAC_WL 14 // fraction bits word length
#define FP_WL   14 // fixed point word length
#define Ratio   4

#define sign(in) ((in >= 0) ? 1 : -1)
#define iabs(in) ((in >= 0) ? in : -in)
#define clip(in, low, upp) ((in<low) ? low : (in>upp) ? upp : in)

typedef struct def_csc_matrix {
  int32 CSC_COE00 ;
  int32 CSC_COE01 ;
  int32 CSC_COE02 ;
  int32 CSC_ADD0  ;
  int32 CSC_COE10 ;
  int32 CSC_COE11 ;
  int32 CSC_COE12 ;
  int32 CSC_ADD1  ;
  int32 CSC_COE20 ;
  int32 CSC_COE21 ;
  int32 CSC_COE22 ;
  int32 CSC_ADD2  ;
} DEF_CSC_MATRIX ;

typedef struct vpp1684_param
{
  uint32          USER_DEF           ; //jy_add@20180601
  uint32          PROJECT            ; //hxx_add@20180521
  uint32          CROP_NUM           ;
  uint32          SCA_CTRL           ;

  DEF_CSC_MATRIX  CSC_MATRIX         ;

  uint32          PADDING_CTRL       ;
  uint32          SRC_CTRL           ;
  uint32          SRC_SIZE           ;
  uint32          *SRC_CROP_ST       ;
  uint32          *SRC_CROP_SIZE     ;
  uint32          SRC_RY_BASE        ;
  uint32          SRC_GU_BASE        ;
  uint32          SRC_BV_BASE        ;
  uint32          SRC_STRIDE         ;
  uint32          SRC_RY_BASE_EXT    ;
  uint32          SRC_GU_BASE_EXT    ;
  uint32          SRC_BV_BASE_EXT    ;

  uint32          DST_CTRL           ;
  uint32          DST_SIZE           ;
  uint32         *DST_CROP_ST        ;
  uint32         *DST_CROP_SIZE      ;
  uint32         *DST_RY_BASE        ;
  uint32         *DST_GU_BASE        ;
  uint32         *DST_BV_BASE        ;
  uint32         *DST_STRIDE         ;
  uint32         *DST_RY_BASE_EXT    ;
  uint32         *DST_GU_BASE_EXT    ;
  uint32         *DST_BV_BASE_EXT    ;
  uint32         *DST_PADDING_HEIGHT ;

  uint32         *SCL_CTRL           ;
  uint32         *SCL_INIT           ;
  uint32         *SCL_X              ;
  uint32         *SCL_Y              ;

  uint32         *SCL_X_COE          ;
  uint32         *SCL_Y_COE          ;
} VPP1684_PARAM;

extern int vpp1684_cmodel_test(struct vpp_batch_n *batch);

#if defined(__cplusplus)
}
#endif
#endif
