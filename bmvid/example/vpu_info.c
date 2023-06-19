//--=========================================================================--
//  This file is a part of VPUAPI
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2004 - 2014   CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#ifdef __linux__
#include <unistd.h>
#include <getopt.h>
#include <semaphore.h>
#elif _WIN32
#include <windows.h>
#include "windows/libusb-1.0.18/examples/getopt/getopt.h"
#endif

#include "bm_video_interface.h"
#include "bm_video_internal.h"
#include "vpuapifunc.h"
#include "main_helper.h"

extern u64 vdi_get_dma_memory_free_size(u64 coreIdx);

int main(int argc, char **argv)
{
    int coreIdx;
    Uint32   ver, rev, productId, instanceNum;
    u64 size;
    char *fwPath = NULL;
    RetCode ret = RETCODE_FAILURE;
    Uint16 *pusBitCode = NULL;
    Uint32 sizeInWord = 0;
    Uint32 core_num = MAX_NUM_VPU_CORE_CHIP;

    SetMaxLogLevel(4);

    VLOG(INFO, "Total VPU Core Num on Everyboard: %d\n", MAX_NUM_VPU_CORE_CHIP);
#ifdef CHIP_BM1684
    core_num -= 1;
    int bin_type  = 0;
    int video_cap = 0;
    int chip_id   = 0;
    int max_core_num = 0;

    if (vdi_get_video_cap(0, &video_cap, &bin_type, &max_core_num, &chip_id) < 0) {
        VLOG(ERR,"get chip info failed!!!\n");
        return -1;
    }
    core_num = max_core_num - 1;
#endif
    for(coreIdx = 0; coreIdx < core_num; coreIdx++)
    {
        ret = VPU_GetProductId(coreIdx);
        if (ret == -1)
        {
            VLOG(ERR, "Failed to get product ID\n");
            goto ERR_DEC_INIT;
        }
        else if(ret == -2)
        {
            goto ERR_DEC_INIT;
        }

        productId = ret;
        switch (productId) {
            case PRODUCT_ID_960:  fwPath = CORE_0_BIT_CODE_FILE_PATH; break;
            case PRODUCT_ID_512:  fwPath = CORE_2_BIT_CODE_FILE_PATH; break;
            case PRODUCT_ID_511:  fwPath = CORE_7_BIT_CODE_FILE_PATH; break;
            case PRODUCT_ID_521:  fwPath = CORE_6_BIT_CODE_FILE_PATH; break;
            default:
                VLOG(ERR, "Unknown product id: %d\n", productId);
            	goto ERR_DEC_INIT;
        }

        if (LoadFirmware(productId, (Uint8 **)&pusBitCode, &sizeInWord, fwPath) < 0)
        {
            VLOG(ERR, "%s:%d Failed to load firmware: %s\n", __FUNCTION__, __LINE__, fwPath);
            goto ERR_DEC_INIT;
        }

        ret = VPU_InitWithBitcode(coreIdx, (const Uint16 *)pusBitCode, sizeInWord);
        if (ret != RETCODE_CALLED_BEFORE && ret != RETCODE_SUCCESS)
        {
            VLOG(ERR, "Failed to boot up VPU(coreIdx: %d, productId: %d)\n", coreIdx, productId);
            goto ERR_DEC_INIT;
        }
        VLOG(INFO, "load firmware success!\n");

        VPU_GetVersionInfo(coreIdx, &ver, &rev, &productId);
        VLOG(INFO, "Core %d: VERSION=%d, REVISION=%d, PRODUCT_ID=%d\n", coreIdx, ver, rev, productId);

        instanceNum = VPU_GetOpenInstanceNum(coreIdx);
        VLOG(INFO, "Core %d: current instance number, %d\n", coreIdx, instanceNum);

ERR_DEC_INIT:
        VPU_DeInit(coreIdx);
    }

    size = vdi_get_dma_memory_free_size(0);
    VLOG(INFO, "Free VPU memory size: 0x%x\n", size);

    return 0;
}


