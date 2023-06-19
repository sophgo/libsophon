#ifndef __W5_DEC_TEST_H__
#define __W5_DEC_TEST_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef  MAX_FILE_PATH
#define MAX_FILE_PATH               256
#endif

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

NOTE: Products earlier than CODA9 can only set this linear map type.
@endverbatim
*/
    LINEAR_FRAME_MAP            = 0,  /**< Linear frame map type */
    TILED_FRAME_V_MAP           = 1,  /**< Tiled frame vertical map type (CODA9 only) */
    TILED_FRAME_H_MAP           = 2,  /**< Tiled frame horizontal map type (CODA9 only) */
    TILED_FIELD_V_MAP           = 3,  /**< Tiled field vertical map type (CODA9 only) */
    TILED_MIXED_V_MAP           = 4,  /**< Tiled mixed vertical map type (CODA9 only) */
    TILED_FRAME_MB_RASTER_MAP   = 5,  /**< Tiled frame MB raster map type (CODA9 only) */
    TILED_FIELD_MB_RASTER_MAP   = 6,  /**< Tiled field MB raster map type (CODA9 only) */
    TILED_FRAME_NO_BANK_MAP     = 7,  /**< Tiled frame no bank map. (CODA9 only) */ // coda980 only
    TILED_FIELD_NO_BANK_MAP     = 8,  /**< Tiled field no bank map. (CODA9 only) */ // coda980 only
    LINEAR_FIELD_MAP            = 9,  /**< Linear field map type. (CODA9 only) */ // coda980 only
    CODA_TILED_MAP_TYPE_MAX     = 10,
    COMPRESSED_FRAME_MAP        = 10, /**< Compressed frame map type (WAVE only) */ // WAVE4 only
    ARM_COMPRESSED_FRAME_MAP    = 11, /**< AFBC(ARM Frame Buffer Compression) compressed frame map type */ // AFBC enabled WAVE decoder
    TILED_MAP_TYPE_MAX
} TiledMapType;
#endif

/************************************************************************/
/* Bitstream Feeder                                                     */
/************************************************************************/
#ifndef FEEDINGMETHOD
#define FEEDINGMETHOD
typedef enum {
    FEEDING_METHOD_FIXED_SIZE,
    FEEDING_METHOD_FRAME_SIZE,
    FEEDING_METHOD_SIZE_PLUS_ES,
    FEEDING_METHOD_MAX
} FeedingMethod;
#endif

/**
* @brief    This is an enumeration type for specifying frame buffer types when tiled2linear or wtlEnable is used.
*/
#ifndef FRAMEFLAG
#define FRAMEFLAG
typedef enum {
    FF_NONE                 = 0, /**< Frame buffer type when tiled2linear or wtlEnable is disabled */
    FF_FRAME                = 1, /**< Frame buffer type to store one frame */
    FF_FIELD                = 2, /**< Frame buffer type to store top field or bottom field separately */
}FrameFlag;
#endif

/**
 * @brief   This is an enumeration type for representing chroma formats of the frame buffer and pixel formats in packed mode.
 */
#ifndef FRAMEBUFFERFORMAT
#define FRAMEBUFFERFORMAT
typedef enum {
    FORMAT_420             = 0  ,      /* 8bit */
    FORMAT_422               ,         /* 8bit */
    FORMAT_224               ,         /* 8bit */
    FORMAT_444               ,         /* 8bit */
    FORMAT_400               ,         /* 8bit */

                                       /* Little Endian Perspective     */
                                       /*     | addr 0  | addr 1  |     */
    FORMAT_420_P10_16BIT_MSB = 5 ,     /* lsb | 00xxxxx |xxxxxxxx | msb */
    FORMAT_420_P10_16BIT_LSB ,         /* lsb | xxxxxxx |xxxxxx00 | msb */
    FORMAT_420_P10_32BIT_MSB ,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
    FORMAT_420_P10_32BIT_LSB ,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

                                       /* 4:2:2 packed format */
                                       /* Little Endian Perspective     */
                                       /*     | addr 0  | addr 1  |     */
    FORMAT_422_P10_16BIT_MSB ,         /* lsb | 00xxxxx |xxxxxxxx | msb */
    FORMAT_422_P10_16BIT_LSB ,         /* lsb | xxxxxxx |xxxxxx00 | msb */
    FORMAT_422_P10_32BIT_MSB ,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
    FORMAT_422_P10_32BIT_LSB ,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

    FORMAT_YUYV              ,         /**< 8bit packed format : Y0U0Y1V0 Y2U1Y3V1 ... */
    FORMAT_YUYV_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
    FORMAT_YUYV_P10_16BIT_LSB,         /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
    FORMAT_YUYV_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */
    FORMAT_YUYV_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */

    FORMAT_YVYU              ,         /**< 8bit packed format : Y0V0Y1U0 Y2V1Y3U1 ... */
    FORMAT_YVYU_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
    FORMAT_YVYU_P10_16BIT_LSB,         /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
    FORMAT_YVYU_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */
    FORMAT_YVYU_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */

    FORMAT_UYVY              ,         /**< 8bit packed format : U0Y0V0Y1 U1Y2V1Y3 ... */
    FORMAT_UYVY_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
    FORMAT_UYVY_P10_16BIT_LSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
    FORMAT_UYVY_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */
    FORMAT_UYVY_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */

    FORMAT_VYUY              ,         /**< 8bit packed format : V0Y0U0Y1 V1Y2U1Y3 ... */
    FORMAT_VYUY_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
    FORMAT_VYUY_P10_16BIT_LSB,         /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
    FORMAT_VYUY_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */
    FORMAT_VYUY_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */

    FORMAT_MAX,
} FrameBufferFormat;
#endif

#ifndef ENDIANMODE
#define ENDIANMODE
typedef enum {
    VDI_LITTLE_ENDIAN = 0,      /* 64bit LE */
    VDI_BIG_ENDIAN,             /* 64bit BE */
    VDI_32BIT_LITTLE_ENDIAN,
    VDI_32BIT_BIG_ENDIAN,
    /* WAVE PRODUCTS */
    VDI_128BIT_LITTLE_ENDIAN    = 16,
    VDI_128BIT_LE_BYTE_SWAP,
    VDI_128BIT_LE_WORD_SWAP,
    VDI_128BIT_LE_WORD_BYTE_SWAP,
    VDI_128BIT_LE_DWORD_SWAP,
    VDI_128BIT_LE_DWORD_BYTE_SWAP,
    VDI_128BIT_LE_DWORD_WORD_SWAP,
    VDI_128BIT_LE_DWORD_WORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_WORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_WORD_SWAP,
    VDI_128BIT_BE_DWORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_SWAP,
    VDI_128BIT_BE_WORD_BYTE_SWAP,
    VDI_128BIT_BE_WORD_SWAP,
    VDI_128BIT_BE_BYTE_SWAP,
    VDI_128BIT_BIG_ENDIAN        = 31,
    VDI_ENDIAN_MAX
} EndianMode;
#endif

#ifndef RENDERDEVICETYPE
#define RENDERDEVICETYPE
typedef enum {
    RENDER_DEVICE_NULL,
    RENDER_DEVICE_FBDEV,
    RENDER_DEVICE_HDMI,
    RENDER_DEVICE_MAX
} RenderDeviceType;
#endif


/**
* @brief    The data structure to enable low delay decoding.
*/
#ifndef LOWDELAYINFO
#define LOWDELAYINFO
typedef struct {
/**
@verbatim
This enables low delay decoding. (CODA980 H.264/AVC decoder only)

If this flag is 1, VPU sends an interrupt to HOST application when numRows decoding is done.

* 0 : disable
* 1 : enable

When this field is enabled, reorderEnable, tiled2LinearEnable, and the post-rotator should be disabled.
@endverbatim
*/
    int lowDelayEn;
/**
@verbatim
This field indicates the number of mb rows (macroblock unit).

The value is from 1 to height/16 - 1.
If the value of this field is 0 or picture height/16, low delay decoding is disabled even though lowDelayEn is 1.
@endverbatim
*/
    int numRows;
} LowDelayInfo;
#endif

#ifndef TESTMACHINE
#define TESTMACHINE
typedef struct ObserverStruct {
    void*   ctx;
    void (*construct)(struct ObserverStruct*, void*);
    BOOL (*update)(struct ObserverStruct* ctx, void* data);
    void (*destruct)(struct ObserverStruct*);
} Listener;

#define MAX_OBSERVERS           100
typedef struct TestMachine_struct {
    uint32_t          coreIdx;
    uint32_t          testEnvOptions;             /*!<< See debug.h */
    BOOL            reset;
    Listener        observers[MAX_OBSERVERS];
    uint32_t          numObservers;
} TestMachine;
#endif

/************************************************************************/
/* Structure                                                            */
/************************************************************************/
#ifndef TESTDECCONFIG
#define TESTDECCONFIG
typedef struct TestDecConfig_struct {
    uint32_t              magicNumber;
    char                outputPath[MAX_FILE_PATH];
    char                inputPath[MAX_FILE_PATH];
    int32_t               forceOutNum;
    int32_t               bitFormat;
    int32_t               reorder;
    TiledMapType        mapType;
    int32_t               bitstreamMode;
    FeedingMethod       feedingMode;
    BOOL                enableWTL;
    FrameFlag           wtlMode;
    FrameBufferFormat   wtlFormat;
    int32_t               coreIdx;
    int32_t               instIdx;
    BOOL                enableCrop;                 //!<< option for saving yuv
    uint32_t              loopCount;
    BOOL                cbcrInterleave;             //!<< 0: None, 1: NV12, 2: NV21
    BOOL                nv21;                       //!<< FALSE: NV12, TRUE: NV21,
                                                    //!<< This variable is valid when cbcrInterleave is TRUE
    EndianMode          streamEndian;
    EndianMode          frameEndian;
    int32_t               secondaryAXI;
    int32_t               compareType;
    char                md5Path[MAX_FILE_PATH];
    char                fwPath[MAX_FILE_PATH];
    char                refYuvPath[MAX_FILE_PATH];
    RenderDeviceType    renderType;
    BOOL                thumbnailMode;
    int32_t               skipMode;
    size_t              bsSize;
    size_t              bsBlockSize;
    int32_t             scaleDownWidth;
    int32_t             scaleDownHeight;     
    struct {
        BOOL        enableMvc;                      //!<< H.264 MVC
        BOOL        enableTiled2Linear;
        FrameFlag   tiled2LinearMode;
        BOOL        enableBWB;
        uint32_t      rotate;                         //!<< 0, 90, 180, 270
        uint32_t      mirror;
        BOOL        enableDering;                   //!<< MPEG-2/4
        BOOL        enableDeblock;                  //!<< MPEG-2/4
        uint32_t      mp4class;                       //!<< MPEG_4
        uint32_t      frameCacheBypass;
        uint32_t      frameCacheBurst;
        uint32_t      frameCacheMerge;
        uint32_t      frameCacheWayShape;
        LowDelayInfo    lowDelay;                   //!<< H.264
    } coda9;
    struct {
        uint32_t    numVCores;                      //!<< This numVCores is valid on PRODUCT_ID_4102 multi-core version
        uint32_t    fbcMode;
        BOOL        bwOptimization;                 //!<< On/Off bandwidth optimization function
        BOOL        craAsBla;
    } wave;
    BOOL            enableUserData;
    BOOL            enableLog;
    uint32_t          testEnvOptions;             /*!<< See debug.h */
    TestMachine*    testMachine;
} TestDecConfig;
#endif


BOOL TestDecoder(
    TestDecConfig *param
    );


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif
