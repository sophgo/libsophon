/*
 * Copyright (c) 2018, Chips&Media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include "cnm_app.h"
#include "encoder_listener.h"
#include "misc/bw_monitor.h"

#define ENC_PERFORMANCE_TEST

typedef struct CommandLineArgument{
    int argc;
    char **argv;
}CommandLineArgument;


static void Help(struct OptionExt *opt, const char *programName)
{
    int i;

    VLOG(INFO, "------------------------------------------------------------------------------\n");
    VLOG(INFO, "%s(API v%d.%d.%d)\n", GetBasename(programName), API_VERSION_MAJOR, API_VERSION_MINOR, API_VERSION_PATCH);
    //VLOG(INFO, "\tAll rights reserved by Chips&Media(C)\n");
    VLOG(INFO, "------------------------------------------------------------------------------\n");
    VLOG(INFO, "%s [option] --input bistream\n", GetBasename(programName));
    VLOG(INFO, "-h                          help\n");
    VLOG(INFO, "-n [num]                    output frame number, -1,0:unlimited encoding(depends on YUV size)\n");
    VLOG(INFO, "-v                          print version information\n");
    VLOG(INFO, "-c                          compare with golden bitstream\n");

    for (i = 0;i < MAX_GETOPT_OPTIONS;i++) {
        if (opt[i].name == NULL)
            break;
        VLOG(INFO, "%s", opt[i].help);
    }
}

static BOOL CheckTestConfig(TestEncConfig *testConfig)
{
    if ( (testConfig->compareType & (1<<MODE_SAVE_ENCODED)) && testConfig->bitstreamFileName[0] == 0) {
        testConfig->compareType &= ~(1<<MODE_SAVE_ENCODED);
        VLOG(ERR,"You want to Save bitstream data. Set the path\n");
        return FALSE;
    }

    if ( (testConfig->compareType & (1<<MODE_COMP_ENCODED)) && testConfig->ref_stream_path[0] == 0) {
        testConfig->compareType &= ~(1<<MODE_COMP_ENCODED);
        VLOG(ERR,"You want to Compare bitstream data. Set the path\n");
        return FALSE;
    }
    if ( testConfig->subFrameSyncEn == TRUE && testConfig->cframe50Enable == TRUE) {
        VLOG(ERR,"You can not use sub-frame-sync and cframe together at the same time\n");
        return FALSE;
    }

    return TRUE;
}
#if !defined(PLATFORM_NON_OS)
static BOOL ParseArgumentAndSetTestConfig(CommandLineArgument argument, TestEncConfig* testConfig) {
    Int32  opt  = 0, i = 0, idx = 0;
    int    argc = argument.argc;
    char** argv = argument.argv;
    char* optString = "rbhvn:";
    struct option options[MAX_GETOPT_OPTIONS];
    struct OptionExt options_help[MAX_GETOPT_OPTIONS] = {
        {"output",                1, NULL, 0, "--output                    An output bitstream file path\n"},
        {"input",                 1, NULL, 0, "--input                     YUV file path. The value of InputFile in a cfg is replaced to this value.\n"},
        {"codec",                 1, NULL, 0, "--codec                     codec index, 12:HEVC, 0:AVC\n"},
        {"cfgFileName",           1, NULL, 0, "--cfgFileName               cfg file path\n"},
        {"coreIdx",               1, NULL, 0, "--coreIdx                   core index: default 0\n"},
        {"picWidth",              1, NULL, 0, "--picWidth                  source width\n"},
        {"picHeight",             1, NULL, 0, "--picHeight                 source height\n"},
        {"kbps",                  1, NULL, 0, "--kbps                      RC bitrate in kbps. In case of without cfg file, if this option has value then RC will be enabled\n"},
        {"enable-lineBufInt",     0, NULL, 0, "--enable-lineBufInt         enable linebuffer interrupt\n"},
        {"subFrameSyncEn",        1, NULL, 0, "--subFrameSyncEn            subFrameSync 0: off 1: on, default off\n"},
        {"lowLatencyMode",        1, NULL, 0, "--lowLatencyMode            bit[1]: low latency interrupt enable, bit[0]: fast bitstream-packing enable\n"},
        {"loop-count",            1, NULL, 0, "--loop-count                integer number. loop test, default 0\n"},
        {"enable-cbcrInterleave", 0, NULL, 0, "--enable-cbcrInterleave     enable cbcr interleave\n"},
        {"nv21",                  1, NULL, 0, "--nv21                      enable NV21(must set enable-cbcrInterleave)\n"},
        {"packedFormat",          1, NULL, 0, "--packedFormat              1:YUYV, 2:YVYU, 3:UYVY, 4:VYUY\n"},
        {"rotAngle",              1, NULL, 0, "--rotAngle                  rotation angle(0,90,180,270), Not supported on WAVE420L, WAVE525\n"},
        {"mirDir",                1, NULL, 0, "--mirDir                    1:Vertical, 2: Horizontal, 3:Vert-Horz, Not supported on WAVE420L, WAVE525\n"}, /* 20 */
        {"secondary-axi",         1, NULL, 0, "--secondary-axi             0~3: bit mask values, Please refer programmer's guide or datasheet\n"
        "                            1: RDO, 2: LF\n"},
        {"frame-endian",          1, NULL, 0, "--frame-endian              16~31, default 31(LE) Please refer programmer's guide or datasheet\n"},
        {"stream-endian",         1, NULL, 0, "--stream-endian             16~31, default 31(LE) Please refer programmer's guide or datasheet\n"},
        {"source-endian",         1, NULL, 0, "--source-endian             16~31, default 31(LE) Please refer programmer's guide or datasheet\n"},
        {"ref_stream_path",       1, NULL, 0, "--ref_stream_path           golden data which is compared with encoded stream when -c option\n"},
        {"srcFormat3p4b",         1, NULL, 0, "--srcFormat3p4b             [WAVE420]This option MUST BE enabled when format of source yuv is 3pixel 4byte format\n"},
        {"cframe50Enable",        1, NULL, 0, "--cframe50Enable            enable Cframe input\n"},
        {"cframe50Lossless",      1, NULL, 0, "--cframe50Lossless          The compressed source is encoded with the lossless mode\n"},
        {"cframe50Tx16Y",         1, NULL, 0, "--cframe50Tx16Y             Compressed ratio of luma, which must be multiplied by 16 (min=32 max=160)\n"},
        {"cframe50Tx16C",         1, NULL, 0, "--cframe50Tx16C             Compressed ratio of chroma, which must be multiplied by 16 (min=32 max=160)\n"},
        {"cframe50_422",          1, NULL, 0, "--cframe50_422              yuv format(1:yuv422, 0:yuv420)\n"},
        {NULL,                    0, NULL, 0},
    };


    for (i = 0; i < MAX_GETOPT_OPTIONS;i++) {
        if (options_help[i].name == NULL)
            break;
        osal_memcpy(&options[i], &options_help[i], sizeof(struct option));
    }

    while ((opt=getopt_long(argc, argv, optString, options, (int *)&idx)) != -1) {
        switch (opt) {
        case 'n':
            testConfig->outNum = atoi(optarg);
            break;
        case 'c':
            testConfig->compareType |= (1 << MODE_COMP_ENCODED);
            VLOG(TRACE, "Stream compare Enable\n");
            break;
        case 'h':
            Help(options_help, argv[0]);
            exit(0);
        case 0:
            if (!strcmp(options[idx].name, "output")) {
                osal_memcpy(testConfig->bitstreamFileName, optarg, strlen(optarg));
                ChangePathStyle(testConfig->bitstreamFileName);
            } else if (!strcmp(options[idx].name, "input")) {
                strcpy(testConfig->optYuvPath, optarg);
                ChangePathStyle(testConfig->optYuvPath);
            } else if (!strcmp(options[idx].name, "codec")) {
                testConfig->stdMode = (CodStd)atoi(optarg);
            } else if (!strcmp(options[idx].name, "cfgFileName")) {
                osal_memcpy(testConfig->cfgFileName, optarg, strlen(optarg));
            } else if (!strcmp(options[idx].name, "coreIdx")) {
                testConfig->coreIdx = atoi(optarg);
            } else if (!strcmp(options[idx].name, "picWidth")) {
                testConfig->picWidth = atoi(optarg);
            } else if (!strcmp(options[idx].name, "picHeight")) {
                testConfig->picHeight = atoi(optarg);
            } else if (!strcmp(options[idx].name, "kbps")) {
                testConfig->kbps = atoi(optarg);
            } else if (!strcmp(options[idx].name, "enable-lineBufInt")) {
                testConfig->lineBufIntEn = TRUE;
            } else if (!strcmp(options[idx].name, "subFrameSyncEn")) {
                testConfig->subFrameSyncEn = atoi(optarg);
            } else if (!strcmp(options[idx].name, "lowLatencyMode")) {
                testConfig->lowLatencyMode = atoi(optarg);
            } else if (!strcmp(options[idx].name, "loop-count")) {
                testConfig->loopCount = atoi(optarg);
            } else if (!strcmp(options[idx].name, "enable-cbcrInterleave")) {
                testConfig->cbcrInterleave = 1;
            } else if (!strcmp(options[idx].name, "nv21")) {
                testConfig->nv21 = atoi(optarg);
            } else if (!strcmp(options[idx].name, "packedFormat")) {
                testConfig->packedFormat = atoi(optarg);
            } else if (!strcmp(options[idx].name, "rotAngle")) {
                testConfig->rotAngle = atoi(optarg);
            } else if (!strcmp(options[idx].name, "mirDir")) {
                testConfig->mirDir = atoi(optarg);
            } else if (!strcmp(options[idx].name, "secondary-axi")) {
                testConfig->secondaryAXI = atoi(optarg);
            } else if (!strcmp(options[idx].name, "frame-endian")) {
                testConfig->frame_endian = atoi(optarg);
            } else if (!strcmp(options[idx].name, "stream-endian")) {
                testConfig->stream_endian = (EndianMode)atoi(optarg);
            } else if (!strcmp(options[idx].name, "source-endian")) {
                testConfig->source_endian = atoi(optarg);
            } else if (!strcmp(options[idx].name, "ref_stream_path")) {
                osal_memcpy(testConfig->ref_stream_path, optarg, strlen(optarg));
                ChangePathStyle(testConfig->ref_stream_path);
            } else if (!strcmp(options[idx].name, "srcFormat3p4b")) {
                testConfig->srcFormat3p4b = atoi(optarg);
            } else if (!strcmp(options[idx].name, "cframe50Enable")) {
                testConfig->cframe50Enable = atoi(optarg);
            } else if (!strcmp(options[idx].name, "cframe50Lossless")) {
                testConfig->cframe50Lossless = atoi(optarg);
            } else if (!strcmp(options[idx].name, "cframe50Tx16Y")) {
                testConfig->cframe50Tx16Y = atoi(optarg);
            } else if (!strcmp(options[idx].name, "cframe50Tx16C")) {
                testConfig->cframe50Tx16C = atoi(optarg);
            } else if (!strcmp(options[idx].name, "cframe50_422")) {
                testConfig->cframe50_422 = atoi(optarg);
            } else {
                VLOG(ERR, "not exist param = %s\n", options[idx].name);
                Help(options_help, argv[0]);
                return FALSE;
            }
            break;
        default:
            VLOG(ERR, "%s\n", optarg);
            Help(options_help, argv[0]);
            return FALSE;
        }
    }
    VLOG(INFO, "\n");

    return TRUE;
}


int main(int argc, char **argv)
{
    char*               fwPath     = NULL;
    TestEncConfig       testConfig;
    CommandLineArgument argument;
    CNMComponentConfig  config;
    CNMTask             task;
    Component           yuvFeeder;
    Component           encoder;
    Component           reader;
    Uint32              sizeInWord;
    Uint16*             pusBitCode = NULL;
    BOOL                ret;
    BOOL                testResult;
    EncListenerContext  lsnCtx;
    char* loglevel_s = NULL;
    int   loglevel   = ERR;

    osal_memset(&argument, 0x00, sizeof(CommandLineArgument));
    osal_memset(&config,   0x00, sizeof(CNMComponentConfig));

    loglevel_s = getenv("BMVE_LOGLEVEL");
    if (loglevel_s)
        loglevel = atoi(loglevel_s);

    if (loglevel < ERR)
        loglevel = ERR;
    if (loglevel > TRACE)
        loglevel = TRACE;

    InitLog();
    SetMaxLogLevel(TRACE);

    SetDefaultEncTestConfig(&testConfig);

    argument.argc = argc;
    argument.argv = argv;
    if (ParseArgumentAndSetTestConfig(argument, &testConfig) == FALSE) {
        VLOG(ERR, "fail to ParseArgumentAndSetTestConfig()\n");
        return 1;
    }

    SetMaxLogLevel(loglevel);

    /* Need to Add */
    /* FW load & show version case*/
    testConfig.productId = (ProductId)VPU_GetProductId(testConfig.coreIdx);

    if (CheckTestConfig(&testConfig) == FALSE) {
        VLOG(ERR, "fail to CheckTestConfig()\n");
        return 1;
    }

    switch (testConfig.productId) {
    case PRODUCT_ID_520:    fwPath = CORE_4_BIT_CODE_FILE_PATH; break;
    case PRODUCT_ID_525:    fwPath = CORE_4_BIT_CODE_FILE_PATH; break;
    case PRODUCT_ID_521:    fwPath = CORE_6_BIT_CODE_FILE_PATH; break;
    default:
        VLOG(ERR, "Unknown product id: %d\n", testConfig.productId);
        return 1;
    }

    VLOG(INFO, "FW PATH = %s\n", fwPath);

    if (LoadFirmware(testConfig.productId, (Uint8**)&pusBitCode, &sizeInWord, fwPath) < 0) {
        VLOG(ERR, "%s:%d Failed to load firmware: %s\n", __FUNCTION__, __LINE__, fwPath);
        return 1;
    }


    config.testEncConfig = testConfig;
    config.bitcode       = (Uint8*)pusBitCode;
    config.sizeOfBitcode = sizeInWord;
	config.testEncConfig.subFrameSyncMode = REGISTER_BASE_SUB_FRAME_SYNC;
    config.testEncConfig.gopPresetIdx = PRESET_IDX_IBBBP;
	VLOG(INFO, "SetupEncoderOpenParam Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    if (SetupEncoderOpenParam(&config.encOpenParam, &config.testEncConfig) == FALSE)
        return 1;

    CNMAppInit();
VLOG(INFO, "CNMTaskCreate Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    task       = CNMTaskCreate();
VLOG(INFO, "CNMTaskCreate yuvFeeder Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    yuvFeeder  = ComponentCreate("yuvFeeder", &config);
VLOG(INFO, "CNMTaskCreate encoder Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    encoder    = ComponentCreate("encoder",   &config);
    reader     = ComponentCreate("reader",    &config);
VLOG(INFO, "CNMTaskAdd Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    CNMTaskAdd(task, yuvFeeder);
    CNMTaskAdd(task, encoder);
    CNMTaskAdd(task, reader);

    CNMAppAdd(task);


#ifdef ENC_PERFORMANCE_TEST
    double start_pts, end_pts;
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0)
        start_pts = tv.tv_sec + tv.tv_usec/1000000.0;
    else
        start_pts = 0;
#endif
    if ((ret=SetupEncListenerContext(&lsnCtx, &config)) == TRUE) {
        ComponentRegisterListener(encoder, COMPONENT_EVENT_ENC_ALL, EncoderListener, (void*)&lsnCtx);
        ret = CNMAppRun();
    }
    else {
        CNMAppStop();
    }

#ifdef ENC_PERFORMANCE_TEST
    if (gettimeofday(&tv, NULL) == 0)
        end_pts = tv.tv_sec + tv.tv_usec/1000000.0;
    else
        end_pts = 0;

    if (config.testEncConfig.outNum > 0) {
        double total_time = end_pts - start_pts;
        VLOG(ERR, "\ntotal frames : %d\n", config.testEncConfig.outNum);
        VLOG(ERR, "total time   : %0.3f s\n", total_time);
        VLOG(ERR, "avg frame rate : %0.1f fps\n", config.testEncConfig.outNum/total_time);
    }
#endif

    osal_free(pusBitCode);
    ClearEncListenerContext(&lsnCtx);

    testResult = (ret == TRUE && lsnCtx.match == TRUE && lsnCtx.matchOtherInfo == TRUE);
    if (testResult == TRUE) VLOG(INFO, "[RESULT] SUCCESS\n");
    else                    VLOG(ERR,  "[RESULT] FAILURE\n");

    if ( CNMErrorGet() != 0 )
        return CNMErrorGet();

    DeInitLog();

    return (testResult == TRUE) ? 0 : 1;
}
#else
#ifdef CHIP_BM1684
#include "chagall_521.h"
static const Uint16*       pusBitCode = wave521_bit_code;
static Uint32              sizeInWord = sizeof(wave521_bit_code) / sizeof(wave521_bit_code[0]);
//#else
//static const Uint16*       pusBitCode = wave512_bit_code;
//static Uint32              sizeInWord = sizeof(wave512_bit_code) / sizeof(wave512_bit_code[0]);
#endif

#define VIDEO_CVD_TEST
#ifdef VIDEO_CVD_TEST
#define PARAM_DDR_ADDR 0x4F0000004UL
__attribute__((__noinline__,__noclone__)) void test_video_enc_exit(void)
{
  while(1);
}
#endif

BOOL TestEncoder()
{
    char*               fwPath     = NULL;
    TestEncConfig       testConfig;
//    CommandLineArgument argument;
    CNMComponentConfig  config;
    CNMTask             task;
    Component           yuvFeeder;
    Component           encoder;
    Component           reader;
//    Uint32              sizeInWord;
//    Uint16*             pusBitCode;
    BOOL                ret;
    BOOL                testResult;
    EncListenerContext  lsnCtx;
    int                 loglevel;

//    osal_memset(&argument, 0x00, sizeof(CommandLineArgument));
    osal_memset(&config,   0x00, sizeof(CNMComponentConfig));

    InitLog();
    SetMaxLogLevel(ERR);

    //osal_memcpy(&testConfig, param, sizeof(testConfig));
    SetDefaultEncTestConfig(&testConfig);

#ifdef VIDEO_CVD_TEST
    int *param_addr = (int *)PARAM_DDR_ADDR;
    testConfig.coreIdx = *param_addr++;
    testConfig.stdMode = *param_addr++;
    testConfig.outNum = *param_addr++;
    testConfig.compareType = *param_addr++;
    testConfig.picWidth = *param_addr++;
    testConfig.picHeight = *param_addr++;
    testConfig.cbcrInterleave = *param_addr++;
    testConfig.fps = *param_addr++;
    testConfig.kbps = *param_addr++;
    testConfig.lowLatencyMode = *param_addr++;
    testConfig.bitdepth = *param_addr++;
    testConfig.secondaryAXI = *param_addr++;
    testConfig.rcIntraQp = *param_addr++;
    testConfig.picQpY = *param_addr++;
    testConfig.packedFormat = *param_addr++;
    testConfig.frame_endian = *param_addr++;
    testConfig.stream_endian = *param_addr++;
    testConfig.source_endian = *param_addr++;
    testConfig.bitstreamFileName[0] = *param_addr++; // for performance test '\0', for correct check '\a'
    testConfig.ref_stream_path[0] = *param_addr++;
    testConfig.gopPresetIdx = *param_addr++;
    loglevel = *param_addr++;

    SetMaxLogLevel(loglevel);

    osal_memset(testConfig.cfgFileName, 0, 16);

#endif

    /* Need to Add */
    /* FW load & show version case*/
    testConfig.productId = (ProductId)VPU_GetProductId(testConfig.coreIdx);

    if (CheckTestConfig(&testConfig) == FALSE) {
        VLOG(ERR, "fail to CheckTestConfig()\n");
        return 1;
    }

    switch (testConfig.productId) {
    case PRODUCT_ID_520:    fwPath = CORE_4_BIT_CODE_FILE_PATH; break;
    case PRODUCT_ID_525:    fwPath = CORE_4_BIT_CODE_FILE_PATH; break;
    case PRODUCT_ID_521:    fwPath = CORE_6_BIT_CODE_FILE_PATH; break;
    default:
        VLOG(ERR, "Unknown product id: %d\n", testConfig.productId);
        return 1;
    }

    VLOG(INFO, "FW PATH = %s\n", fwPath);
/*
    if (LoadFirmware(testConfig.productId, (Uint8**)&pusBitCode, &sizeInWord, fwPath) < 0) {
        VLOG(ERR, "%s:%d Failed to load firmware: %s\n", __FUNCTION__, __LINE__, fwPath);
        return 1;
    }

*/
    config.testEncConfig = testConfig;
    config.bitcode       = (Uint8*)pusBitCode;
    config.sizeOfBitcode = sizeInWord;
	config.testEncConfig.subFrameSyncMode = REGISTER_BASE_SUB_FRAME_SYNC;
	VLOG(INFO, "SetupEncoderOpenParam Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    if (SetupEncoderOpenParam(&config.encOpenParam, &config.testEncConfig) == FALSE)
        return 1;

    CNMAppInit();
VLOG(INFO, "CNMTaskCreate Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    task       = CNMTaskCreate();
VLOG(INFO, "CNMTaskCreate yuvFeeder Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    yuvFeeder  = ComponentCreate("yuvFeeder", &config);
VLOG(INFO, "CNMTaskCreate encoder Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    encoder    = ComponentCreate("encoder",   &config);
    reader     = ComponentCreate("reader",    &config);
VLOG(INFO, "CNMTaskAdd Enc coreIdx:%d\n", config.testEncConfig.coreIdx);
    CNMTaskAdd(task, yuvFeeder);
    CNMTaskAdd(task, encoder);
    CNMTaskAdd(task, reader);

    CNMAppAdd(task);

    if ((ret=SetupEncListenerContext(&lsnCtx, &config)) == TRUE) {
        ComponentRegisterListener(encoder, COMPONENT_EVENT_ENC_ALL, EncoderListener, (void*)&lsnCtx);
        ret = CNMAppRun();
    }
    else {
        CNMAppStop();
    }

    osal_free(pusBitCode);
    ClearEncListenerContext(&lsnCtx);

    testResult = (ret == TRUE && lsnCtx.match == TRUE && lsnCtx.matchOtherInfo == TRUE);
    if (testResult == TRUE) VLOG(INFO, "[RESULT] SUCCESS\n");
    else                    VLOG(ERR,  "[RESULT] FAILURE\n");

    if ( CNMErrorGet() != 0 )
        return CNMErrorGet();
#ifdef VIDEO_CVD_TEST
    int *pResult = (int *)(PARAM_DDR_ADDR + 0x100 - 4);
    if(testResult == TRUE) {
        *pResult = 1;
    }
    else {
        *pResult = 0;
    }
    test_video_enc_exit();
#endif
    return (testResult == TRUE) ? 0 : 1;
}

#endif
