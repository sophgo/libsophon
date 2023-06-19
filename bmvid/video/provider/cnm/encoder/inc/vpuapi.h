//--=========================================================================--
//  This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2013  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--

#ifndef VPUAPI_H_INCLUDED
#define VPUAPI_H_INCLUDED

#include "vpuopt.h"
#include "bmvpu_types.h"
#include "vdi.h"

#ifdef _WIN32
#include <stdint.h>
#endif

/************************************************************************/
/* WAVE5 SYSTEM ERROR (FAIL_REASON)                                     */
/************************************************************************/
#define WAVE5_QUEUEING_FAIL                                             0x00000001
#define WAVE5_SYSERR_ACCESS_VIOLATION_HW                                0x00000040
#define WAVE5_RESULT_NOT_READY                                          0x00000800
#define WAVE5_VPU_STILL_RUNNING                                         0x00001000
#define WAVE5_INSTANCE_DESTROYED                                        0x00004000
#define WAVE5_SYSERR_DEC_VLC_BUF_FULL                                   0x00010000
#define WAVE5_SYSERR_WATCHDOG_TIMEOUT                                   0x00020000

/************************************************************************/
/* WAVE5 ERROR ON ENCODER (ERR_INFO)                                    */
/************************************************************************/
#define WAVE5_SYSERR_ENC_VLC_BUF_FULL                                   0x00000100


#define MAX_GDI_IDX                             31
#define MAX_REG_FRAME                           MAX_GDI_IDX*2 // 2 for WTL

#define WAVE5_ENC_FBC50_LUMA_TABLE_SIZE(_w, _h)     (VPU_ALIGN2048(VPU_ALIGN32(_w))*VPU_ALIGN4(_h)/64)
#define WAVE5_ENC_FBC50_CHROMA_TABLE_SIZE(_w, _h)   (VPU_ALIGN2048(VPU_ALIGN32(_w)/2)*VPU_ALIGN4(_h)/128)

#define WAVE5_ENC_FBC50_LOSSLESS_LUMA_10BIT_FRAME_SIZE(_w, _h)    ((VPU_ALIGN32(_w)+127)/128*VPU_ALIGN4(_h)*160)
#define WAVE5_ENC_FBC50_LOSSLESS_LUMA_8BIT_FRAME_SIZE(_w, _h)     ((VPU_ALIGN32(_w)+127)/128*VPU_ALIGN4(_h)*128)
#define WAVE5_ENC_FBC50_LOSSLESS_CHROMA_10BIT_FRAME_SIZE(_w, _h)  ((VPU_ALIGN32(_w)/2+127)/128*VPU_ALIGN4(_h)/2*160)
#define WAVE5_ENC_FBC50_LOSSLESS_CHROMA_8BIT_FRAME_SIZE(_w, _h)   ((VPU_ALIGN32(_w)/2+127)/128*VPU_ALIGN4(_h)/2*128)
#define WAVE5_ENC_FBC50_LOSSY_LUMA_FRAME_SIZE(_w, _h, _tx)        ((VPU_ALIGN32(_w)+127)/128*VPU_ALIGN4(_h)*VPU_ALIGN32(_tx))
#define WAVE5_ENC_FBC50_LOSSY_CHROMA_FRAME_SIZE(_w, _h, _tx)      ((VPU_ALIGN32(_w)/2+127)/128*VPU_ALIGN4(_h)/2*VPU_ALIGN32(_tx))

/* YUV422 cframe */
#define WAVE5_ENC_FBC50_422_CHROMA_TABLE_SIZE(_w, _h)   (VPU_ALIGN2048(VPU_ALIGN32(_w)/2)*VPU_ALIGN4(_h)/128*2)
#define WAVE5_ENC_FBC50_LOSSLESS_422_CHROMA_10BIT_FRAME_SIZE(_w, _h)  ((VPU_ALIGN32(_w)/2+127)/128*VPU_ALIGN4(_h)*160)
#define WAVE5_ENC_FBC50_LOSSLESS_422_CHROMA_8BIT_FRAME_SIZE(_w, _h)   ((VPU_ALIGN32(_w)/2+127)/128*VPU_ALIGN4(_h)*128)
#define WAVE5_ENC_FBC50_LOSSY_422_CHROMA_FRAME_SIZE(_w, _h, _tx)      ((VPU_ALIGN32(_w)/2+127)/128*VPU_ALIGN4(_h)*VPU_ALIGN32(_tx))

#define WAVE5_FBC_LUMA_TABLE_SIZE(_w, _h)           (VPU_ALIGN16(_h)*VPU_ALIGN256(_w)/32)
#define WAVE5_FBC_CHROMA_TABLE_SIZE(_w, _h)         (VPU_ALIGN16(_h)*VPU_ALIGN256(_w/2)/32)

#define WAVE5_ENC_HEVC_MVCOL_BUF_SIZE(_w, _h)       (VPU_ALIGN64(_w)/64*VPU_ALIGN64(_h)/64*128)
#define WAVE5_ENC_AVC_MVCOL_BUF_SIZE(_w, _h)        (VPU_ALIGN64(_w)*VPU_ALIGN64(_h)/32)

/**
* @brief
@verbatim
This is an enumeration for declaring SET_PARAM command options.
Depending on this, SET_PARAM command parameter registers have different settings.

NOTE: This is only for WAVE encoder IP.

@endverbatim
*/
typedef enum {
    OPT_COMMON          = 0, /**< SET_PARAM command option for encoding sequence */
    OPT_CUSTOM_GOP      = 1, /**< SET_PARAM command option for setting custom GOP */
    OPT_CUSTOM_HEADER   = 2, /**< SET_PARAM command option for setting custom VPS/SPS/PPS */
    OPT_VUI             = 3, /**< SET_PARAM command option for encoding VUI  */
    OPT_CHANGE_PARAM    = 16 /**< SET_PARAM command option for parameters change (WAVE520 only)  */
} SET_PARAM_OPTION;

/************************************************************************/
/* PROFILE & LEVEL                                                      */
/************************************************************************/
/* HEVC */
#define HEVC_PROFILE_MAIN                   1
#define HEVC_PROFILE_MAIN10                 2
#define HEVC_PROFILE_STILLPICTURE           3

/* Tier */
#define HEVC_TIER_MAIN                      0
#define HEVC_TIER_HIGH                      1
/* Level */
#define HEVC_LEVEL(_Major, _Minor)          (_Major*10+_Minor)

/* H.264 */
#define H264_PROFILE_BASELINE               66
#define H264_PROFILE_MAIN                   77
#define H264_PROFILE_EXTENDED               88
#define H264_PROFILE_HIGH                   100
#define H264_PROFILE_HIGH10                 110
#define H264_PROFILE_HIGH10_INTRA           120
#define H264_PROFILE_HIGH422                122
#define H264_PROFILE_HIGH444                244
#define H264_PROFILE_CAVLC_444_INTRA        44

/************************************************************************/
/* Utility macros                                                       */
/************************************************************************/
#define VPU_CEIL(_data, _align)     (((_data)+(_align-1))&~(_align-1))
#define VPU_ALIGN4(_x)              (((_x)+0x03)&~0x03)
#define VPU_ALIGN8(_x)              (((_x)+0x07)&~0x07)
#define VPU_ALIGN16(_x)             (((_x)+0x0f)&~0x0f)
#define VPU_ALIGN32(_x)             (((_x)+0x1f)&~0x1f)
#define VPU_ALIGN64(_x)             (((_x)+0x3f)&~0x3f)
#define VPU_ALIGN128(_x)            (((_x)+0x7f)&~0x7f)
#define VPU_ALIGN256(_x)            (((_x)+0xff)&~0xff)
#define VPU_ALIGN512(_x)            (((_x)+0x1ff)&~0x1ff)
#define VPU_ALIGN2048(_x)           (((_x)+0x7ff)&~0x7ff)
#define VPU_ALIGN4096(_x)           (((_x)+0xfff)&~0xfff)
#define VPU_ALIGN16384(_x)          (((_x)+0x3fff)&~0x3fff)

#define RECON_IDX_FLAG_ENC_END       -1
#define RECON_IDX_FLAG_ENC_DELAY     -2
#define RECON_IDX_FLAG_HEADER_ONLY   -3
#define RECON_IDX_FLAG_CHANGE_PARAM  -4

/**
* @brief    This is a special enumeration type for some configuration commands
* which can be issued to VPU by HOST application. Most of these commands can be called occasionally,
* not periodically for changing the configuration of decoder or encoder operation running on VPU.
*
*/
typedef enum {
    ENABLE_ROTATION,  /**< This command enables rotation. In this case, `parameter` is ignored. This command returns VPU_RETCODE_SUCCESS. */
    DISABLE_ROTATION, /**< This command disables rotation. In this case, `parameter` is ignored. This command returns VPU_RETCODE_SUCCESS. */
    ENABLE_MIRRORING, /**< This command enables mirroring. In this case, `parameter` is ignored. This command returns VPU_RETCODE_SUCCESS. */
    DISABLE_MIRRORING,/**< This command disables mirroring. In this case, `parameter` is ignored. This command returns VPU_RETCODE_SUCCESS. */
/**
@verbatim
This command sets mirror direction of the post-rotator, and `parameter` is
interpreted as a pointer to MirrorDirection. The `parameter` should be one of
MIRDIR_NONE, MIRDIR_VER, MIRDIR_HOR, and MIRDIR_HOR_VER.

@* MIRDIR_NONE: No mirroring
@* MIRDIR_VER: Vertical mirroring
@* MIRDIR_HOR: Horizontal mirroring
@* MIRDIR_HOR_VER: Both directions

This command has one of the following return codes.::

* VPU_RETCODE_SUCCESS:
Operation was done successfully, which means given mirroring direction is valid.
* RETCODE_INVALID_PARAM:
The given argument parameter, `parameter`, was invalid, which means given mirroring
direction is invalid.

@endverbatim
*/
    SET_MIRROR_DIRECTION,
/**
@verbatim
This command sets counter-clockwise angle for post-rotation, and `parameter` is
interpreted as a pointer to the integer which represents rotation angle in
degrees. Rotation angle should be one of 0, 90, 180, and 270.

This command has one of the following return codes.::

* VPU_RETCODE_SUCCESS:
Operation was done successfully, which means given rotation angle is valid.
* RETCODE_INVALID_PARAM:
The given argument parameter, `parameter`, was invalid, which means given rotation
angle is invalid.
@endverbatim
*/
    SET_ROTATION_ANGLE,
/**
@verbatim
This command sets the secondary channel of AXI for saving memory bandwidth to
dedicated memory. The argument `parameter` is interpreted as a pointer to <<vpuapi_h_SecAxiUse>> which
represents an enable flag and physical address which is related with the secondary
channel.

This command has one of the following return codes::

* VPU_RETCODE_SUCCESS:
Operation was done successfully, which means given value for setting secondary
AXI is valid.
* RETCODE_INVALID_PARAM:
The given argument parameter, `parameter`, was invalid, which means given value is
invalid.
@endverbatim
*/
    SET_SEC_AXI,

/**
@verbatim
This command inserts an MPEG4 header syntax or SPS or PPS to the HEVC/AVC bitstream to the bitstream
during encoding. It is valid for all types of encoders. The argument `parameter` is interpreted as a pointer to <<vpuapi_h_EncHeaderParam>> holding

* buf is a physical address pointing the generated stream location
* size is the size of generated stream in bytes
* headerType is a type of header that HOST application wants to generate and have values as
* <<vpuapi_h_WaveEncHeaderType>>.

This command has one of the following return codes.::
+
--
* VPU_RETCODE_SUCCESS:
Operation was done successfully, which means the requested header
syntax was successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument cmd was invalid
which means the given cmd was undefined, or not allowed in the current instance. In this
case, the current instance might not be an MPEG4 encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or headerType
was invalid, which means it has a null pointer, or given values for some member variables
are improper values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.
--
@endverbatim
*/
    ENC_PUT_VIDEO_HEADER,

/**
@verbatim
This command changes encoding parameter(s) during the encoding operation. (WAVE encoder only)
The argument `parameter` is interpreted as a pointer to <<vpuapi_h_EncChangeParam>> structure holding

* enable_option : Set an enum value that is associated with parameters to change (multiple option allowed).

For instance, if bitrate and framerate need to be changed in the middle of encoding, that requires some setting like below.

 EncChangeParam changeParam;
 changeParam.enable_option  = ENC_RC_TARGET_RATE_CHANGE | ENC_FRAME_RATE_CHANGE;

 changeParam.bitrate        = 14213000;
 changeParam.frameRate      = 15;
 vpu_EncGiveCommand(handle, ENC_SET_PARA_CHANGE, &changeParam);

This command has one of the following return codes.::
+
--
* VPU_RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument `cmd` was invalid which means the given `cmd` was
undefined, or not allowed in the current instance. In this case, the current
instance might not be an H.264/AVC encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or `headerType` was invalid, which means it
has a null pointer, or given values for some member variables are improper
values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not received any response from VPU and has timed out.
--
@endverbatim
*/
    ENC_SET_PARA_CHANGE,
    ENABLE_LOGGING, /**< HOST can activate message logging once VPU_DecOpen() or VPU_EncOpen() is called. */
    DISABLE_LOGGING, /**< HOST can deactivate message logging which is off as default. */
    ENC_GET_QUEUE_STATUS, /**< This command returns the number of queued commands for the current encode instance and the number of queued commands for the total encode instances.  */
    GET_BANDWIDTH_REPORT, /**< This command reports the amount of bytes which are transferred on AXI bus. */     /* WAVE52x products. */
    SET_CYCLE_PER_TICK,
    CMD_END
} CodecCommand;

/**
 * @brief   This is an enumeration type for representing the way of writing chroma data in planar format of frame buffer.
 */
typedef enum {
    CBCR_ORDER_NORMAL,  /**< Cb data are written in Cb buffer, and Cr data are written in Cr buffer. */
    CBCR_ORDER_REVERSED /**< Cr data are written in Cb buffer, and Cb data are written in Cr buffer. */
} CbCrOrder;

/**
 * @brief    This is an enumeration type for representing the mirroring direction.
 */
typedef enum {
    MIRDIR_NONE,   /**< No mirroring */
    MIRDIR_VER,    /**< Vertical mirroring */
    MIRDIR_HOR,    /**< Horizontal mirroring */
    MIRDIR_HOR_VER /**< Horizontal and vertical mirroring */
} MirrorDirection;

/**
 * @brief   This is an enumeration type for representing chroma formats of the frame buffer and pixel formats in packed mode.
 */
#ifndef FRAMEBUFFERFORMAT
#define FRAMEBUFFERFORMAT
 typedef enum {
    FORMAT_ERR           = -1,
    FORMAT_420           = 0 ,    /* 8bit */
    FORMAT_422               ,    /* 8bit */
    FORMAT_224               ,    /* 8bit */
    FORMAT_444               ,    /* 8bit */
    FORMAT_400               ,    /* 8bit */

                                  /* Little Endian Perspective     */
                                  /*     | addr 0  | addr 1  |     */
    FORMAT_420_P10_16BIT_MSB = 5, /* lsb | 00xxxxx |xxxxxxxx | msb */
    FORMAT_420_P10_16BIT_LSB ,    /* lsb | xxxxxxx |xxxxxx00 | msb */
    FORMAT_420_P10_32BIT_MSB ,    /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
    FORMAT_420_P10_32BIT_LSB ,    /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

                                  /* 4:2:2 packed format */
                                  /* Little Endian Perspective     */
                                  /*     | addr 0  | addr 1  |     */
    FORMAT_422_P10_16BIT_MSB ,    /* lsb | 00xxxxx |xxxxxxxx | msb */
    FORMAT_422_P10_16BIT_LSB ,    /* lsb | xxxxxxx |xxxxxx00 | msb */
    FORMAT_422_P10_32BIT_MSB ,    /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
    FORMAT_422_P10_32BIT_LSB ,    /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

    FORMAT_YUYV              ,    /**< 8bit packed format : Y0U0Y1V0 Y2U1Y3V1 ... */
    FORMAT_YUYV_P10_16BIT_MSB,    /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
    FORMAT_YUYV_P10_16BIT_LSB,    /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
    FORMAT_YUYV_P10_32BIT_MSB,    /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */
    FORMAT_YUYV_P10_32BIT_LSB,    /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */

    FORMAT_YVYU              ,    /**< 8bit packed format : Y0V0Y1U0 Y2V1Y3U1 ... */
    FORMAT_YVYU_P10_16BIT_MSB,    /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
    FORMAT_YVYU_P10_16BIT_LSB,    /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
    FORMAT_YVYU_P10_32BIT_MSB,    /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */
    FORMAT_YVYU_P10_32BIT_LSB,    /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */

    FORMAT_UYVY              ,    /**< 8bit packed format : U0Y0V0Y1 U1Y2V1Y3 ... */
    FORMAT_UYVY_P10_16BIT_MSB,    /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
    FORMAT_UYVY_P10_16BIT_LSB,    /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
    FORMAT_UYVY_P10_32BIT_MSB,    /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */
    FORMAT_UYVY_P10_32BIT_LSB,    /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */

    FORMAT_VYUY              ,    /**< 8bit packed format : V0Y0U0Y1 V1Y2U1Y3 ... */
    FORMAT_VYUY_P10_16BIT_MSB,    /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
    FORMAT_VYUY_P10_16BIT_LSB,    /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
    FORMAT_VYUY_P10_32BIT_MSB,    /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */
    FORMAT_VYUY_P10_32BIT_LSB,    /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */

    FORMAT_MAX,
} FrameBufferFormat;
#endif

/**
 * @brief   This is an enumeration type for representing YUV packed format.
 */
typedef enum {
    NOT_PACKED = 0,
    PACKED_YUYV,
    PACKED_YVYU,
    PACKED_UYVY,
    PACKED_VYUY,
} PackedFormatNum;

/**
 * @brief   This is an enumeration type for representing bitstream handling modes in decoder.
 */
typedef enum {
    BS_MODE_INTERRUPT,  /**< VPU returns an interrupt when bitstream buffer is empty while decoding. VPU waits for more bitstream to be filled. */
    BS_MODE_RESERVED,   /**< Reserved for the future */
    BS_MODE_PIC_END,    /**< VPU tries to decode with very small amount of bitstream (not a complete 512-byte chunk). If it is not successful, VPU performs error concealment for the rest of the frame. */
}BitStreamMode;

/**
 * @brief  This is an enumeration type for representing software reset options.
 */
typedef enum {
    SW_RESET_SAFETY,    /**< It resets VPU in safe way. It waits until pending bus transaction is completed and then perform reset. */
    SW_RESET_FORCE,     /**< It forces to reset VPU without waiting pending bus transaction to be completed. It is used for immediate termination such as system off. */
    SW_RESET_ON_BOOT    /**< This is the default reset mode that is executed since system booting.  This mode is actually executed in VPU_Init(), so does not have to be used independently. */
}SWResetMode;

/**
 * @brief  This is an enumeration type for representing product IDs.
 */
typedef enum {
    PRODUCT_ID_521 = 6, /**< video encoder in BM1684 */
    PRODUCT_ID_NONE,
}ProductId;

/**
* @brief This is an enumeration type for representing map types for frame buffer.

NOTE: Tiled maps are only for CODA9. Please find them in the CODA9 datasheet for detailed information.
*/
#ifndef TILEDMAPTYPE
#define TILEDMAPTYPE
typedef enum {
/**
@verbatim
Linear frame map type
@endverbatim
*/
    LINEAR_FRAME_MAP                            = 0,  /**< Linear frame map type */
    CODA_TILED_MAP_TYPE_MAX                     = 10,
    COMPRESSED_FRAME_MAP                        = 10, /**< Compressed frame map type (WAVE only) */
    ARM_COMPRESSED_FRAME_MAP                    = 11, /**< AFBC(ARM Frame Buffer Compression) compressed frame map type */ // AFBC enabled WAVE decoder
    COMPRESSED_FRAME_MAP_V50_LOSSLESS_8BIT      = 12, /**< CFRAME50(Chips&Media Frame Buffer Compression) compressed framebuffer type */
    COMPRESSED_FRAME_MAP_V50_LOSSLESS_10BIT     = 13, /**< CFRAME50(Chips&Media Frame Buffer Compression) compressed framebuffer type */
    COMPRESSED_FRAME_MAP_V50_LOSSY              = 14, /**< CFRAME50(Chips&Media Frame Buffer Compression) compressed framebuffer type */
    COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_8BIT  = 16, /**< CFRAME50(Chips&Media Frame Buffer Compression) compressed 4:2:2 framebuffer type */
    COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_10BIT = 17, /**< CFRAME50(Chips&Media Frame Buffer Compression) compressed 4:2:2 framebuffer type */
    COMPRESSED_FRAME_MAP_V50_LOSSY_422          = 18, /**< CFRAME50(Chips&Media Frame Buffer Compression) compressed 4:2:2 framebuffer type */
    TILED_MAP_TYPE_MAX
} TiledMapType;
#endif

/**
* @brief    This is a data structure of CFRAME50 configuration(WAVE5 only)
VPUAPI sets default values for this structure.
However, HOST application can configure if the default values are not associated with their DRAM
    or desirable to change.
*/
typedef struct {
    int rasBit;     /**< This value is used for width of RAS bit. (13 on the CNM FPGA platform) */
    int casBit;     /**< This value is used for width of CAS bit. (9 on the CNM FPGA platform) */
    int bankBit;    /**< This value is used for width of BANK bit. (2 on the CNM FPGA platform) */
    int busBit;     /**< This value is used for width of system BUS bit. (3 on CNM FPGA platform) */
    int tx16y;      /**< This value is used for CFRAME50(Chips&Media Frame Buffer Compression) (WAVE5 only) */
    int tx16c;      /**< This value is used for CFRAME50(Chips&Media Frame Buffer Compression) (WAVE5 only) */
} DRAMConfig;

/**
* @brief    This is a data structure for representing use of secondary AXI for each hardware block.
*/
typedef struct {
    /* for Encoder */
    int useEncImdEnable; /**< This enables AXI secondary channel for intra mode decision. */
    int useEncLfEnable;  /**< This enables AXI secondary channel for loopfilter. */
    int useEncRdoEnable; /**< This enables AXI secondary channel for RDO. */
} SecAxiUse;

struct EncInst;
typedef struct EncInst* EncHandle;

/**
 * @brief   This is a data structure of queue command information. It is used for parameter when host issues DEC_GET_QUEUE_STATUS of <<vpuapi_h_VPU_DecGiveCommand>>. (WAVE5 only)
 */
typedef struct {
    uint32_t  instanceQueueCount; /**< This variable indicates the number of queued commands of the instance.  */
    uint32_t  totalQueueCount;    /**< This variable indicates the number of queued commands of all instances.  */
} QueueStatusInfo;

#define MAX_NUM_TEMPORAL_LAYER          7
#define MAX_GOP_NUM                     8

/**
* @brief This is an enumeration for encoder parameter change. (WAVE5 encoder only)
*/
typedef enum {
    // COMMON parameters which can be changed frame by frame.
    ENC_SET_CHANGE_PARAM_PPS                 = (1<<0),
    ENC_SET_CHANGE_PARAM_INTRA_PARAM         = (1<<1),
    ENC_SET_CHANGE_PARAM_RC_TARGET_RATE      = (1<<8),
    ENC_SET_CHANGE_PARAM_RC                  = (1<<9),
    ENC_SET_CHANGE_PARAM_RC_MIN_MAX_QP       = (1<<10),
    ENC_SET_CHANGE_PARAM_RC_BIT_RATIO_LAYER  = (1<<11),
    ENC_SET_CHANGE_PARAM_INDEPEND_SLICE      = (1<<16),
    ENC_SET_CHANGE_PARAM_DEPEND_SLICE        = (1<<17),
    ENC_SET_CHANGE_PARAM_RDO                 = (1<<18),
    ENC_SET_CHANGE_PARAM_NR                  = (1<<19),
    ENC_SET_CHANGE_PARAM_BG                  = (1<<20),
    ENC_SET_CHANGE_PARAM_CUSTOM_MD           = (1<<21),
    ENC_SET_CHANGE_PARAM_CUSTOM_LAMBDA       = (1<<22),
} Wave5ChangeParam;

/**
* @brief This is a data structure for encoding parameters that have changed.
*/
typedef struct {
    int enable_option;

    // ENC_SET_CHANGE_PARAM_PPS (lossless, WPP can't be changed while encoding)
    int constIntraPredFlag;             /**< It enables constrained intra prediction. */
    int lfCrossSliceBoundaryEnable;     /**< It enables filtering across slice boundaries for in-loop deblocking. */
    int weightPredEnable;

    int disableDeblk;                   /**< It disables in-loop deblocking filtering. */
    int betaOffsetDiv2;                 /**< It sets BetaOffsetDiv2 for deblocking filter. */
    int tcOffsetDiv2;                   /**< It sets TcOffsetDiv3 for deblocking filter. */

    int chromaCbQpOffset;               /**< The value of chroma(Cb) QP offset */
    int chromaCrQpOffset;               /**< The value of chroma(Cr) QP offset */

    int transform8x8Enable;             /**< (for H.264 encoder) */
    int entropyCodingMode;              /**< (for H.264 encoder) */


    // ENC_SET_CHANGE_PARAM_INDEPEND_SLICE
    /**
    @verbatim
    A slice mode for independent slice

    @* 0 : no multi-slice
    @* 1 : slice in CTU number
    @endverbatim
    */
    int independSliceMode;
    int independSliceModeArg;           /**< The number of CTU for a slice when independSliceMode is set with 1  */

    // ENC_SET_CHANGE_PARAM_DEPEND_SLICE
    /**
    @verbatim
    A slice mode for dependent slice

    @* 0 : no multi-slice
    @* 1 : slice in CTU number
    @* 2 : slice in number of byte
    @endverbatim
    */
    int dependSliceMode;
    int dependSliceModeArg;             /**< The number of CTU or bytes for a slice when dependSliceMode is set with 1 or 2  */

/**
@verbatim
A slice mode for independent slice

@* 0 : no multi-slice
@* 1 : slice in MB number
@endverbatim
*/
    int avcSliceArg;
    int avcSliceMode;                   /**< The number of MB for a slice when avcSliceMode is set with 1  */

    // ENC_SET_CHANGE_PARAM_RDO (cuSizeMode, MonoChrom, and RecommendEncParam
	//  can't be changed while encoding)
    int coefClearDisable;               /**< It disables the transform coefficient clearing algorithm for P or B picture. If this is 1, all-zero coefficient block is not evaluated in RDO. */
    int intraNxNEnable;                 /**< It enables intra NxN PUs. */
    int maxNumMerge;                    /**< It specifies the number of merge candidates in RDO (1 or 2). 2 of maxNumMerge (default) offers better quality of encoded picture, while 1 of maxNumMerge improves encoding performance.  */
    int customLambdaEnable;             /**< It enables custom lambda table. */
    int customMDEnable;                 /**< It enables custom mode decision. */
    int rdoSkip;                        /**< It skips RDO(rate distortion optimization) in H.264 encoder. */
    int lambdaScalingEnable;

    // ENC_SET_CHANGE_PARAM_RC_TARGET_RATE
    int64_t bitRate;                        /**< A target bitrate when separateBitrateEnable is 0 */


    // ENC_SET_CHANGE_PARAM_RC
	// (rcEnable, cuLevelRc, bitAllocMode, RoiEnable, RcInitQp can't be changed while encoding)
    int hvsQPEnable;                    /**< It enables CU QP adjustment for subjective quality enhancement. */
    int hvsQpScale;                     /**< QP scaling factor for CU QP adjustment when hvcQpenable is 1. */
    int vbvBufferSize;                  /**< Specifies the size of the VBV buffer in msec (10 ~ 3000). For example, 3000 should be set for 3 seconds. This value is valid when rcEnable is 1. VBV buffer size in bits is EncBitrate * VbvBufferSize / 1000. */
    int mbLevelRcEnable;                /**< (for H.264 encoder) */

    // ENC_SET_CHANGE_PARAM_RC_MIN_MAX_QP
    int minQpI;                         /**< A minimum QP of I picture for rate control */
    int maxQpI;                         /**< A maximum QP of I picture for rate control */
    int maxDeltaQp;                     /**< A maximum delta QP for rate control */


    int minQpP;                         /**< A minimum QP of P picture for rate control */
    int minQpB;                         /**< A minimum QP of B picture for rate control */
    int maxQpP;                         /**< A maximum QP of P picture for rate control */
    int maxQpB;                         /**< A maximum QP of B picture for rate control */

    // ENC_SET_CHANGE_PARAM_RC_BIT_RATIO_LAYER
    /**
    @verbatim
    A fixed bit ratio (1 ~ 255) for each picture of GOP's bit
    allocation

    @* N = 0 ~ (MAX_GOP_SIZE - 1)
    @* MAX_GOP_SIZE = 8

    For instance when MAX_GOP_SIZE is 3, FixedBitRatio0, FixedBitRatio1, and FixedBitRatio2 can be set as 2, 1, and 1 repsectively for
    the fixed bit ratio 2:1:1. This is only valid when BitAllocMode is 2.
    @endverbatim
    */
    int fixedBitRatio[MAX_GOP_NUM];

    // ENC_SET_CHANGE_PARAM_BG (bgDetectEnable can't be changed while encoding)
    int      s2fmeDisable;
    uint32_t bgThrDiff;      /**< It specifies the threshold of max difference that is used in s2me block. It is valid when background detection is on. */
    uint32_t bgThrMeanDiff;  /**< It specifies the threshold of mean difference that is used in s2me block. It is valid  when background detection is on. */
    uint32_t bgLambdaQp;     /**< It specifies the minimum lambda QP value to be used in the background area. */
    int      bgDeltaQp;      /**< It specifies the difference between the lambda QP value of background and the lambda QP value of foreground. */

    // ENC_SET_CHANGE_PARAM_NR
    uint32_t  nrYEnable;     /**< It enables noise reduction algorithm to Y component.  */
    uint32_t  nrCbEnable;    /**< It enables noise reduction algorithm to Cb component. */
    uint32_t  nrCrEnable;    /**< It enables noise reduction algorithm to Cr component. */
    uint32_t  nrNoiseEstEnable;  /**<  It enables noise estimation for noise reduction. When this is disabled, host carries out noise estimation with nrNoiseSigmaY/Cb/Cr. */
    uint32_t  nrNoiseSigmaY;  /**< It specifies Y noise standard deviation when nrNoiseEstEnable is 0.  */
    uint32_t  nrNoiseSigmaCb; /**< It specifies Cb noise standard deviation when nrNoiseEstEnable is 0. */
    uint32_t  nrNoiseSigmaCr; /**< It specifies Cr noise standard deviation when nrNoiseEstEnable is 0. */

    uint32_t  nrIntraWeightY;  /**< A weight to Y noise level for intra picture (0 ~ 31). nrIntraWeight/4 is multiplied to the noise level that has been estimated. This weight is put for intra frame to be filtered more strongly or more weakly than just with the estimated noise level. */
    uint32_t  nrIntraWeightCb; /**< A weight to Cb noise level for intra picture (0 ~ 31) */
    uint32_t  nrIntraWeightCr; /**< A weight to Cr noise level for intra picture (0 ~ 31) */
    uint32_t  nrInterWeightY;  /**< A weight to Y noise level for inter picture (0 ~ 31). nrInterWeight/4 is multiplied to the noise level that has been estimated. This weight is put for inter frame to be filtered more strongly or more weakly than just with the estimated noise level. */
    uint32_t  nrInterWeightCb; /**< A weight to Cb noise level for inter picture (0 ~ 31) */
    uint32_t  nrInterWeightCr; /**< A weight to Cr noise level for inter picture (0 ~ 31) */


    // ENC_SET_CHANGE_PARAM_CUSTOM_MD
    int    pu04DeltaRate;       /**< A value which is added to the total cost of 4x4 blocks */
    int    pu08DeltaRate;       /**< A value which is added to the total cost of 8x8 blocks */
    int    pu16DeltaRate;       /**< A value which is added to the total cost of 16x16 blocks */
    int    pu32DeltaRate;       /**< A value which is added to the total cost of 32x32 blocks */
    int    pu04IntraPlanarDeltaRate; /**< A value which is added to rate when calculating cost(=distortion + rate) in 4x4 Planar intra prediction mode. */
    int    pu04IntraDcDeltaRate;     /**< A value which is added to rate when calculating cost (=distortion + rate) in 4x4 DC intra prediction mode. */
    int    pu04IntraAngleDeltaRate;  /**< A value which is added to rate when calculating cost (=distortion + rate) in 4x4 Angular intra prediction mode.  */
    int    pu08IntraPlanarDeltaRate; /**< A value which is added to rate when calculating cost(=distortion + rate) in 8x8 Planar intra prediction mode.*/
    int    pu08IntraDcDeltaRate;     /**< A value which is added to rate when calculating cost(=distortion + rate) in 8x8 DC intra prediction mode.*/
    int    pu08IntraAngleDeltaRate;  /**< A value which is added to  rate when calculating cost(=distortion + rate) in 8x8 Angular intra prediction mode. */
    int    pu16IntraPlanarDeltaRate; /**< A value which is added to rate when calculating cost(=distortion + rate) in 16x16 Planar intra prediction mode. */
    int    pu16IntraDcDeltaRate;     /**< A value which is added to rate when calculating cost(=distortion + rate) in 16x16 DC intra prediction mode */
    int    pu16IntraAngleDeltaRate;  /**< A value which is added to rate when calculating cost(=distortion + rate) in 16x16 Angular intra prediction mode */
    int    pu32IntraPlanarDeltaRate; /**< A value which is added to rate when calculating cost(=distortion + rate) in 32x32 Planar intra prediction mode */
    int    pu32IntraDcDeltaRate;     /**< A value which is added to rate when calculating cost(=distortion + rate) in 32x32 DC intra prediction mode */
    int    pu32IntraAngleDeltaRate;  /**< A value which is added to rate when calculating cost(=distortion + rate) in 32x32 Angular intra prediction mode */
    int    cu08IntraDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU8x8 */
    int    cu08InterDeltaRate;       /**< A value which is added to rate when calculating cost for inter CU8x8 */
    int    cu08MergeDeltaRate;       /**< A value which is added to rate when calculating cost for merge CU8x8 */
    int    cu16IntraDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU16x16 */
    int    cu16InterDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU16x16 */
    int    cu16MergeDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU16x16 */
    int    cu32IntraDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU32x32 */
    int    cu32InterDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU32x32 */
    int    cu32MergeDeltaRate;       /**< A value which is added to rate when calculating cost for intra CU32x32 */

    // ENC_SET_CHANGE_PARAM_CUSTOM_LAMBDA
    bm_pa_t customLambdaAddr; /**< It specifies the address of custom lambda map.  */

    // ENC_SET_CHANGE_PARAM_INTRA_PARAM
    int intraQP;                        /**< A quantization parameter of intra picture */
    int intraPeriod;                    /**< A period of intra picture in GOP size */
} EncChangeParam;

/**
 * @brief   This is a special enumeration type for explicit encoding headers such as VPS, SPS, PPS. (WAVE encoder only)
*/
typedef enum {
    CODEOPT_ENC_VPS             = (1 << 2), /**< A flag to encode VPS nal unit explicitly */
    CODEOPT_ENC_SPS             = (1 << 3), /**< A flag to encode SPS nal unit explicitly */
    CODEOPT_ENC_PPS             = (1 << 4), /**< A flag to encode PPS nal unit explicitly */
} WaveEncHeaderType;

/**
 * @brief   This is a special enumeration type for NAL unit coding options
*/
typedef enum {
    CODEOPT_ENC_HEADER_IMPLICIT = (1 << 0), /**< A flag to encode (a) headers (VPS, SPS, PPS) implicitly for generating bitstreams conforming to spec. */
    CODEOPT_ENC_VCL             = (1 << 1), /**< A flag to encode VCL nal unit explicitly */
} ENC_PIC_CODE_OPTION;

/* COD_STD for WAVE series */
enum {
    W_HEVC_ENC   = 0x01,
    W_AVC_ENC    = 0x03,
};

/* BIT_RUN command */
enum {
    ENC_SEQ_INIT  =  1,
    ENC_SEQ_END   =  2,
    PIC_RUN       =  3,
    SET_FRAME_BUF =  4,
    ENCODE_HEADER =  5,
    ENC_PARA_SET  =  6,
    VPU_SLEEP     = 10,
    VPU_WAKE      = 11,
    ENC_ROI_INIT  = 12,
    FIRMWARE_GET  = 0xf
};

#if 0
enum {
    SRC_BUFFER_EMPTY        = 0,    //!< source buffer doesn't allocated.
    SRC_BUFFER_ALLOCATED    = 1,    //!< source buffer has been allocated.
    SRC_BUFFER_SRC_LOADED   = 2,    //!< source buffer has been allocated.
    SRC_BUFFER_USE_ENCODE   = 3     //!< source buffer was sent to VPU. but it was not used for encoding.
};
#endif

#ifndef MAX
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

//#define API_DEBUG
#ifdef API_DEBUG
#ifdef _MSC_VER
#define APIDPRINT(_fmt, ...)            printf(_fmt, __VA_ARGS__)
#else
#define APIDPRINT(_fmt, ...)            printf(_fmt, ##__VA_ARGS__)
#endif
#else
#define APIDPRINT(_fmt, ...)
#endif

typedef struct {
    int     useEncImdEnable;
    int     useEncRdoEnable;
    int     useEncLfEnable;

    bm_pa_t bufImd;
    bm_pa_t bufRdo;
    bm_pa_t bufLf;

    int     bufSize;
    bm_pa_t bufBase;
} SecAxiInfo;

typedef struct {
    VpuEncOpenParam     openParam;
    VpuEncInitialInfo   initialInfo;

    bm_pa_t             streamRdPtr;
    bm_pa_t             streamWrPtr;

    bm_pa_t             streamRdPtrRegAddr;
    bm_pa_t             streamWrPtrRegAddr;
    bm_pa_t             streamBufStartAddr;
    bm_pa_t             streamBufEndAddr;

    bm_pa_t             currentPC;
    bm_pa_t             busyFlagAddr;

    int                 streamBufSize;
    TiledMapType        mapType;

    VpuFrameBuffer      frameBufPool[MAX_REG_FRAME];

    int                 numFrameBuffers;
    int                 stride;

    int                 rotationEnable;
    int                 mirrorEnable;
    MirrorDirection     mirrorDirection;
    int                 rotationAngle;

    int                 initialInfoObtained;

    SecAxiInfo          secAxiInfo;

    int                 lineBufIntEn;

    vpu_buffer_t        vbWork;

    vpu_buffer_t        vbMV;                   //!< colMV buffer (WAVE encoder)
    vpu_buffer_t        vbFbcYTbl;              //!< FBC Luma table buffer (WAVE encoder)
    vpu_buffer_t        vbFbcCTbl;              //!< FBC Chroma table buffer (WAVE encoder)
    vpu_buffer_t        vbSubSamBuf;            //!< Sub-sampled buffer for ME (WAVE encoder)

    DRAMConfig          dramCfg;


    int32_t           errorReasonCode;
    uint64_t          curPTS;             /**! Current timestamp in 90KHz */
    // TODO
#if 0
    uint64_t          ptsMap[32];         /**! PTS mapped with source frame index */
#endif
    uint32_t          instanceQueueCount;
    uint32_t          totalQueueCount;

    uint32_t          firstCycleCheck;
    uint32_t          PrevEncodeEndTick;
    uint32_t          cyclePerTick;
} EncInfo;


typedef struct EncInst {
    int32_t   inUse;
    int32_t   instIndex;
    int32_t   coreIdx;
    int32_t   codecMode;
    int32_t   codecModeAux;
    int32_t   productId;
    int32_t   loggingEnable;
    uint32_t  isDecoder;

    EncInfo*  encInfo;
} EncInst;

#endif

