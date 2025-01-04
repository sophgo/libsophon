/*****************************************************************************
 *
 *    Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
 *
 *    bmvid is licensed under the 2-Clause BSD License except for the
 *    third-party components.
 *
 *****************************************************************************/
/* This library provides a high-level interface for controlling the BitMain
 * Sophon VPU en/decoder.
 */
#include <sys/stat.h>
#ifdef __linux__
#include <sys/errno.h>
#include <unistd.h>      
#include <sys/stat.h>
#include <sys/time.h>
#elif _WIN32
#include <time.h>
#include <windows.h>
#include <io.h>
#include <shlwapi.h>
#endif
#include <stdlib.h>
#include <string.h>


#include "bmvpu_types.h"
#include "bmvpu.h"
#include "bmvpu_logging.h"

#include "vpuapi.h"
#include "internal.h"

#ifdef __linux__
#define __USE_GNU
#endif
#if __linux__
#include <link.h>
#include <dlfcn.h>
#endif
#ifdef _WIN32
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif
int find_firmware_path(char fw_path[512], const char* path)
{
    char* ptr;
    int dirname_len;
    int ret;
#ifdef __linux__
    const char* path1 = "/system/data/lib/vpu_firmware/";
    Dl_info dl_info;

    /* 1. test ./chagall.bin */
    ret = access(path, F_OK);
    if (ret == 0)
    {
        memset(fw_path, 0, 512);
        strcpy(fw_path, path);
        return 0;
    }

    VLOG(DEBUG, "fw path \"%s\" does not exist. [error=%s].\n",
         path, strerror(errno));

    /* 2. test /system/data/lib/vpu_firmware/chagall.bin */
    memset(fw_path, 0, 512);
    strcpy(fw_path, path1);
    strcat(fw_path, path);
    ret = access(fw_path, F_OK);
    if (ret == 0)
    {
        return 1;
    }

    VLOG(DEBUG, "fw path \"%s\" does not exist. [error=%s].\n",
         fw_path, strerror(errno));

    /* 3. test libbmvpu_so_path/vpu_firmware/chagall.bin */

    ret = dladdr((void*)find_firmware_path, &dl_info);
    if (ret == 0)
    {
        VLOG(ERR, "dladdr() failed: %s\n", dlerror());
        return -1;
    }
    if (dl_info.dli_fname == NULL)
    {
        VLOG(ERR, "%s is NOT a symbol\n", __FUNCTION__);
        return -1;
    }

    VLOG(DEBUG, "The path of libbmvpu.so: %s\n", dl_info.dli_fname);

    ptr = strrchr(dl_info.dli_fname, '/');
    if (!ptr)
    {
        VLOG(ERR, "Invalid absolute path name of libbmvpu.so\n");
        return -1;
    }

    dirname_len = ptr - dl_info.dli_fname + 1;
    if (dirname_len <= 0)
    {
        VLOG(ERR, "Invalid length of folder name\n");
        return -1;
    }

    VLOG(DEBUG, "libbmvpu.so: path=%s, dirname_len=%d\n", dl_info.dli_fname, dirname_len);

    memset(fw_path, 0, 512);
    strncpy(fw_path, dl_info.dli_fname, dirname_len);
    strcat(fw_path, "vpu_firmware/");
    strcat(fw_path, path);

    ret = access(fw_path, F_OK);
    if (ret == 0)
    {
        VLOG(INFO, "vpu firmware path: %s\n", fw_path);
        return 2;
    }

    VLOG(DEBUG, "fw path \"%s\" does not exist. [error=%s].\n",
         fw_path, strerror(errno));

    VLOG(ERR, "Does NOT find firmware \"%s\".\n", path);

    return -1;
#elif _WIN32
    strcpy(fw_path, ".\\chagall.bin");

    /* 1. test ./chagall.bin */
    ret = _access(fw_path, 0);
    if (ret == 0){
        return 1;
    }
    VLOG(DEBUG, "fw path \"%s\" does not exist. [error=%s].\n",
        path, strerror(errno));


    /* 2. test C:\\vpu_firmware\\chagall.bin */
    memset(fw_path, 0, 512);
    strcpy(fw_path, "C:\\vpu_firmware\\chagall.bin");
    ret = _access(fw_path, 0);
    if (ret == 0) {
        return 2;
    }
    VLOG(DEBUG, "fw path \"%s\" does not exist. [error=%s].\n",
        path, strerror(errno));


    /* 3. test libbmvpulite_dll_path\\vpu_firmware\\chagall.bin */
    LPTSTR  strDLLPath1 = (void *)malloc(512);
    GetModuleFileName((HINSTANCE)&__ImageBase, strDLLPath1, _MAX_PATH);
    //strcpy(fw_path, (void *)strDLLPath1);

    ptr = strrchr(strDLLPath1, '\\');
    
    if (!ptr)
    {
        VLOG(ERR, "Invalid absolute path name of libbmvpu.so\n");
        return -1;
    }

    dirname_len = strlen(strDLLPath1) - strlen(ptr) + 1;
    if (dirname_len <= 0)
    {
        VLOG(ERR, "Invalid length of folder name\n");
        return -1;
    }
    memset(fw_path, 0, 512);
    strncpy(fw_path, strDLLPath1, dirname_len);
    strcat(fw_path, "vpu_firmware\\");
    strcat(fw_path, path);

    free(strDLLPath1);
    return 2;

#endif
}

int load_firmware(uint8_t** fw, uint32_t* fwSizeInWord, const char* path)
{
    FILE*    fp = NULL;
    size_t fw_size_w, fw_size;
    size_t len;

    if (path == NULL)
    {
        VLOG(ERR,"The path of vpu firmware is NULL\n");
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp==NULL)
    {
        VLOG(ERR,"Wrong path of vpu firmware:%s\n", path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    fw_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    fw_size_w = (fw_size + 1) &(~1);

    *fw = (uint8_t*)calloc(fw_size_w, sizeof(uint8_t));
    if (*fw == NULL)
    {
        VLOG(ERR, "calloc failed\n");
        return -1;
    }

    len = fread(*fw, sizeof(uint8_t), fw_size, fp);
    if (len != fw_size)
    {
        VLOG(ERR, "fread: the bytes read (%zd) is not equal to the expected %zd bytes\n",
             len, fw_size);
        free(*fw);
        return -1;
    }

    *fwSizeInWord = fw_size_w/2;

    fclose(fp);

    return 0;
}

int vpu_ShowProductInfo(uint32_t coreIdx, ProductInfo *productInfo)
{
    BOOL verbose = FALSE;
    int ret = VPU_RET_SUCCESS;

    ret = vpu_GetProductInfo(coreIdx, productInfo);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    VLOG(DEBUG, "VPU core No.%d\n", coreIdx);
    VLOG(DEBUG, "Firmware : CustomerCode: %04x | version : rev.%d\n", productInfo->customerId, productInfo->fwVersion);
    VLOG(DEBUG, "Hardware : %04x\n", productInfo->productId);
    VLOG(DEBUG, "API      : %d.%d.%d\n\n", API_VERSION_MAJOR, API_VERSION_MINOR, API_VERSION_PATCH);

    VLOG(DEBUG, "productId       : %08x\n", productInfo->productId);
    VLOG(DEBUG, "fwVersion       : %08x(r%d)\n", productInfo->fwVersion, productInfo->fwVersion);
    VLOG(DEBUG, "productName     : %c%c%c%c%04x\n",
         productInfo->productName>>24, productInfo->productName>>16, productInfo->productName>>8, productInfo->productName,
         productInfo->productVersion);

    if (verbose == TRUE)
    {
        uint32_t stdDef0          = productInfo->stdDef0;
        uint32_t stdDef1          = productInfo->stdDef1;
        uint32_t confFeature      = productInfo->confFeature;
        BOOL supportDownScaler  = FALSE;
        BOOL supportAfbce       = FALSE;
        char ch_ox[2]           = {'X', 'O'};

        VLOG(DEBUG, "==========================\n");
        VLOG(DEBUG, "stdDef0           : %08x\n", stdDef0);
        /* checking ONE AXI BIT FILE */
        VLOG(DEBUG, "MAP CONVERTER REG : %d\n", (stdDef0>>31)&1);
        VLOG(DEBUG, "MAP CONVERTER SIG : %d\n", (stdDef0>>30)&1);
        VLOG(DEBUG, "BG_DETECT         : %d\n", (stdDef0>>20)&1);
        VLOG(DEBUG, "3D NR             : %d\n", (stdDef0>>19)&1);
        VLOG(DEBUG, "ONE-PORT AXI      : %d\n", (stdDef0>>18)&1);
        VLOG(DEBUG, "2nd AXI           : %d\n", (stdDef0>>17)&1);
        VLOG(DEBUG, "GDI               : %d\n", !((stdDef0>>16)&1));//no-gdi
        VLOG(DEBUG, "AFBC              : %d\n", (stdDef0>>15)&1);
        VLOG(DEBUG, "AFBC VERSION      : %d\n", (stdDef0>>12)&7);
        VLOG(DEBUG, "FBC               : %d\n", (stdDef0>>11)&1);
        VLOG(DEBUG, "FBC  VERSION      : %d\n", (stdDef0>>8)&7);
        VLOG(DEBUG, "SCALER            : %d\n", (stdDef0>>7)&1);
        VLOG(DEBUG, "SCALER VERSION    : %d\n", (stdDef0>>4)&7);
        VLOG(DEBUG, "BWB               : %d\n", (stdDef0>>3)&1);
        VLOG(DEBUG, "==========================\n");
        VLOG(DEBUG, "stdDef1           : %08x\n", stdDef1);
        VLOG(DEBUG, "CyclePerTick      : %d\n", (stdDef1>>27)&1); //0:32768, 1:256
        VLOG(DEBUG, "MULTI CORE EN     : %d\n", (stdDef1>>26)&1);
        VLOG(DEBUG, "GCU EN            : %d\n", (stdDef1>>25)&1);
        VLOG(DEBUG, "CU REPORT         : %d\n", (stdDef1>>24)&1);
        VLOG(DEBUG, "VCORE ID 3        : %d\n", (stdDef1>>19)&1);
        VLOG(DEBUG, "VCORE ID 2        : %d\n", (stdDef1>>18)&1);
        VLOG(DEBUG, "VCORE ID 1        : %d\n", (stdDef1>>17)&1);
        VLOG(DEBUG, "VCORE ID 0        : %d\n", (stdDef1>>16)&1);
        VLOG(DEBUG, "BW OPT            : %d\n", (stdDef1>>15)&1);
        VLOG(DEBUG, "==========================\n");
        VLOG(DEBUG, "confFeature       : %08x\n", confFeature);
        VLOG(DEBUG, "VP9  ENC Profile2 : %d\n", (confFeature>>7)&1);
        VLOG(DEBUG, "VP9  ENC Profile0 : %d\n", (confFeature>>6)&1);
        VLOG(DEBUG, "VP9  DEC Profile2 : %d\n", (confFeature>>5)&1);
        VLOG(DEBUG, "VP9  DEC Profile0 : %d\n", (confFeature>>4)&1);
        VLOG(DEBUG, "HEVC ENC MAIN10   : %d\n", (confFeature>>3)&1);
        VLOG(DEBUG, "HEVC ENC MAIN     : %d\n", (confFeature>>2)&1);
        VLOG(DEBUG, "HEVC DEC MAIN10   : %d\n", (confFeature>>1)&1);
        VLOG(DEBUG, "HEVC DEC MAIN     : %d\n", (confFeature>>0)&1);
        VLOG(DEBUG, "==========================\n");
        VLOG(DEBUG, "configDate        : %d\n", productInfo->configDate);
        VLOG(DEBUG, "HW version        : r%d\n", productInfo->configRevision);

        supportDownScaler = (BOOL)((stdDef0>>7)&1);
        supportAfbce      = (BOOL)((stdDef0>>15)&1);

        VLOG(DEBUG, "------------------------------------\n");
        VLOG(DEBUG, "VPU CONF| SCALER | AFBCE  |\n");
        VLOG(DEBUG, "        |   %c    |    %c   |\n", ch_ox[supportDownScaler], ch_ox[supportAfbce]);
        VLOG(DEBUG, "------------------------------------\n");
    }
    else
    {
        VLOG(DEBUG, "==========================\n");
        VLOG(DEBUG, "stdDef0          : %08x\n", productInfo->stdDef0);
        VLOG(DEBUG, "stdDef1          : %08x\n", productInfo->stdDef1);
        VLOG(DEBUG, "confFeature      : %08x\n", productInfo->confFeature);
        VLOG(DEBUG, "configDate       : %08x\n", productInfo->configDate);
        VLOG(DEBUG, "configRevision   : %08x\n", productInfo->configRevision);
        VLOG(DEBUG, "configType       : %08x\n", productInfo->configType);
        VLOG(DEBUG, "==========================\n");
    }

    return ret;
}

uint64_t vpu_gettime(void)
{
#ifdef __linux__
    /*
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
*/

    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);

    return (tp.tv_sec*1000 + tp.tv_nsec/1000000);
#elif _WIN32
    SYSTEMTIME wtm;
    time_t clock;
    struct tm tm;
    wtm.wMilliseconds = 0;
    clock = mktime(&tm);
    GetLocalTime(&wtm);
    return  clock * 1000 + wtm.wMilliseconds;
#endif

}

