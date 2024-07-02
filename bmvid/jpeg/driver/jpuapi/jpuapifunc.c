#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/file.h>
#include <err.h>
#endif

#include <sys/types.h>
#include "jpuapifunc.h"
#include "regdefine.h"
#include "../include/jpulog.h"

/******************************************************************************
 * Codec Instance Slot Management
 ******************************************************************************/

const char lendian[4] = {0x49, 0x49, 0x2A, 0x00};
const char bendian[4] = {0x4D, 0x4D, 0x00, 0x2A};

const char *jfif = "JFIF";
const char *jfxx = "JFXX";
const char *exif = "Exif";
uint8_t sJpuCompInfoTable[5][24] = {
    { 00, 02, 02, 00, 00, 00, 01, 01, 01, 01, 01, 01, 02, 01, 01, 01, 01, 01, 03, 00, 00, 00, 00, 00 }, //420
    { 00, 02, 01, 00, 00, 00, 01, 01, 01, 01, 01, 01, 02, 01, 01, 01, 01, 01, 03, 00, 00, 00, 00, 00 }, //422H
    { 00, 01, 02, 00, 00, 00, 01, 01, 01, 01, 01, 01, 02, 01, 01, 01, 01, 01, 03, 00, 00, 00, 00, 00 }, //422V
    { 00, 01, 01, 00, 00, 00, 01, 01, 01, 01, 01, 01, 02, 01, 01, 01, 01, 01, 03, 00, 00, 00, 00, 00 }, //444
    { 00, 01, 01, 00, 00, 00, 01, 00, 00, 00, 00, 00, 02, 00, 00, 00, 00, 00, 03, 00, 00, 00, 00, 00 }, //400
};

#if defined(FLOCK_PROCESS)
static const char *flock_process_file = "/tmp/jpuproc";
static __thread int jpuproc_fd = 0;
#endif

/*
 * GetJpgInstance() obtains a instance.
 * It stores a pointer to the allocated instance in *ppInst
 * and returns JPG_RET_SUCCESS on success.
 * Failure results in 0(null pointer) in *ppInst and JPG_RET_FAILURE.
 */

JpgRet GetJpgInstance(JpgInst ** ppInst)
{
    printf(" jpuapifunc.c : GetJpgInstance \n ");
#if 0
    int core_idx;
    JpgInst * pJpgInst = 0;
    jpu_instance_pool_t *jip;

    jip = (jpu_instance_pool_t *)jdi_get_instance_pool();
    if (!jip)
    {
        printf("GetJpgInstance()  jdi_get_instance_pool() \n");
        return JPG_RET_INVALID_HANDLE;
    }

    core_idx = jdi_get_core_index(0, 0);
    if( core_idx < 0 )
    {
         printf("GetJpgInstance()  jdi_get_core_index() core_idx=%d\n",core_idx);
         return JPG_RET_FAILURE;
    }
    pJpgInst = (JpgInst *)jip->jpgInstPool[core_idx];
    pJpgInst->inUse = 1;
    pJpgInst->coreIndex = core_idx;
    *ppInst = pJpgInst;
#endif
    return JPG_RET_SUCCESS;
}

int GetJpgCoreIndex(JpgInst* pJpgInst, unsigned long long readAddr, unsigned long long writeAddr)
{
    int core_idx = jdi_get_core_index(pJpgInst->device_index, readAddr, writeAddr);
    if (core_idx < 0)
    {
        JLOG(ERR, "jdi_get_core_index() failed! device %d, jpu core %d.\n",
             pJpgInst->device_index, core_idx);
        return -1;
    }
    pJpgInst->inUse = 1;
    pJpgInst->coreIndex =core_idx;
    return core_idx;
}

int FreeJpgInstance(JpgInst * pJpgInst)
{
    int ret;
    pJpgInst->inUse = 0;
    ret = jdi_close_instance(pJpgInst->coreIndex);
    return ret;
}

JpgRet CheckJpgInstValidity(JpgInst * pci)
{
#ifdef LIBBMJPULITE
    return JPG_RET_SUCCESS;
#else

#endif
}

/******************************************************************************
 * API Subroutines
 ******************************************************************************/


JpgRet CheckJpgDecOpenParam(JpgDecOpenParam * pop)
{
    if (pop == 0) {
        return JPG_RET_INVALID_PARAM;
    }
    if (pop->bitstreamBuffer % 8) {
        JLOG(ERR, "bitstreamBuffer is NOT 8-byte aligned!\n");
        return JPG_RET_INVALID_PARAM;
    }
    if (pop->bitstreamBufferSize % 1024) {
        JLOG(ERR, "the size of bitstreamBuffer (%d) is NOT multiple of 1024!\n",
             pop->bitstreamBufferSize);
        return JPG_RET_INVALID_PARAM;
    }
    if (pop->bitstreamBufferSize < 1024) {
        JLOG(ERR, "the size of bitstreamBuffer (%d) is less than 1024!\n",
             pop->bitstreamBufferSize);
        return JPG_RET_INVALID_PARAM;
    }

    if (pop->chromaInterleave != CBCR_SEPARATED &&
        pop->chromaInterleave != CBCR_INTERLEAVE &&
        pop->chromaInterleave != CRCB_INTERLEAVE) {
        JLOG(ERR, "Invalid chromaInterleave: %d!\n", pop->chromaInterleave);
        return JPG_RET_INVALID_PARAM;
    }

    if (pop->packedFormat > PACKED_FORMAT_444) {
        JLOG(ERR, "Invalid packedFormat: %d!\n", pop->packedFormat);
        return JPG_RET_INVALID_PARAM;
    }

    if (pop->packedFormat != PACKED_FORMAT_NONE) {
        if (pop->chromaInterleave != CBCR_SEPARATED) {
            JLOG(ERR, "chroma should be separated when enable packedFormat!\n");
            return JPG_RET_INVALID_PARAM;
        }
    }

    return JPG_RET_SUCCESS;
}

// TODO
static inline uint32_t sign_extend_data1(uint32_t data)
{
    uint32_t v = (data>>15) & 0x1;
    if (v)
        return 0xFFFF0000 | data;

    return data;
}
static inline uint32_t sign_extend_data2(uint8_t data)
{
    int8_t v1 = (int8_t)data;
    int32_t v2 = (int32_t)v1;

    return (uint32_t)v2;
}
int JpgDecHuffTabSetUp(JpgDecInfo *jpg)
{
    int i, j;
    int HuffLength;
    uint32_t v;

    // MIN Tables
    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x003);

    //DC Luma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMin[0][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v); // 32-bit
    }

    //DC Chroma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMin[2][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v); // 32-bit
    }

    //AC Luma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMin[1][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v); // 32-bit
    }

    //AC Chroma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMin[3][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v); // 32-bit
    }
    // MAX Tables
    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x403);
    JpuWriteReg(MJPEG_HUFF_ADDR_REG, 0x440);

    //DC Luma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMax[0][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }
    //DC Chroma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMax[2][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }
    //AC Luma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMax[1][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }
    //AC Chroma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data1(jpg->huffMax[3][j]);
        JpuWriteReg (MJPEG_HUFF_DATA_REG, v);
    }

    // PTR Tables
    JpuWriteReg (MJPEG_HUFF_CTRL_REG, 0x803);
    JpuWriteReg (MJPEG_HUFF_ADDR_REG, 0x880);


    //DC Luma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data2(jpg->huffPtr[0][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }
    //DC Chroma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data2(jpg->huffPtr[2][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }
    //AC Luma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data2(jpg->huffPtr[1][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }
    //AC Chroma
    for(j=0; j<16; j++)
    {
        v = sign_extend_data2(jpg->huffPtr[3][j]);
        JpuWriteReg(MJPEG_HUFF_DATA_REG, v);
    }

    // VAL Tables
    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0xC03);

    // VAL DC Luma
    HuffLength = 0;
    for(i=0; i<12; i++)
        HuffLength += jpg->huffBits[0][i];

    for (i=0; i<HuffLength; i++) { // 8-bit, 12 row, 1 category (DC Luma)
        v = sign_extend_data2(jpg->huffVal[0][i]);
        JpuWriteReg (MJPEG_HUFF_DATA_REG, v);
    }

    for (i=0; i<12-HuffLength; i++) {
        JpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
    }

    // VAL DC Chroma
    HuffLength = 0;
    for(i=0; i<12; i++)
        HuffLength += jpg->huffBits[2][i];
    for (i=0; i<HuffLength; i++) { // 8-bit, 12 row, 1 category (DC Chroma)
        v = sign_extend_data2(jpg->huffVal[2][i]);
        JpuWriteReg (MJPEG_HUFF_DATA_REG, v);
    }
    for (i=0; i<12-HuffLength; i++) {
        JpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
    }

    // VAL AC Luma
    HuffLength = 0;
    for(i=0; i<162; i++)
        HuffLength += jpg->huffBits[1][i];
    for (i=0; i<HuffLength; i++) { // 8-bit, 162 row, 1 category (AC Luma)
        v = sign_extend_data2(jpg->huffVal[1][i]);
        JpuWriteReg (MJPEG_HUFF_DATA_REG, v);
    }
    for (i=0; i<162-HuffLength; i++) {
        JpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
    }

    // VAL AC Chroma
    HuffLength = 0;
    for(i=0; i<162; i++)
        HuffLength += jpg->huffBits[3][i];
    for (i=0; i<HuffLength; i++) { // 8-bit, 162 row, 1 category (AC Chroma)
        v = sign_extend_data2(jpg->huffVal[3][i]);
        JpuWriteReg (MJPEG_HUFF_DATA_REG, v);
    }

    for (i=0; i<162-HuffLength; i++) {
        JpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
    }

    // end SerPeriHuffTab
    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x000);

    return 1;
}

int JpgDecQMatTabSetUp(JpgDecInfo *jpg)
{
    int i;
    int table;
    int val;

    // SetPeriQMatTab
    // Comp 0
    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x03);
    table = jpg->cInfoTab[0][3];
    for (i=0; i<64; i++) {
        val = jpg->qMatTab[table][i];
        JpuWriteReg(MJPEG_QMAT_DATA_REG, val);
    }
    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x00);

    // Comp 1
    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x43);
    table = jpg->cInfoTab[1][3];
    for (i=0; i<64; i++) {
        val = jpg->qMatTab[table][i];
        JpuWriteReg(MJPEG_QMAT_DATA_REG, val);
    }
    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x00);

    // Comp 2
    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x83);
    table = jpg->cInfoTab[2][3];
    for (i=0; i<64; i++) {
        val = jpg->qMatTab[table][i];
        JpuWriteReg(MJPEG_QMAT_DATA_REG, val);
    }
    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x00);
    return 1;
}

#define OUT_RECORD 1
void JpgDecGramSetup(JpgDecInfo * jpg)
{
    int dExtBitBufCurPos;
    int dExtBitBufBaseAddr;
    int dMibStatus;
    int coutn = 0;
#ifdef  OUT_RECORD
    char buffer[256];
    char filename[128];
    static FILE *  fp = NULL;
    time_t timep;
    struct tm *p;
#endif
    dMibStatus          = 1;
    dExtBitBufCurPos    = jpg->pagePtr;
    dExtBitBufBaseAddr  = jpg->streamBufStartAddr;

    JpuWriteReg(MJPEG_BBC_CUR_POS_REG, dExtBitBufCurPos);
    JpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, dExtBitBufBaseAddr + (dExtBitBufCurPos << 8));
    JpuWriteReg(MJPEG_BBC_INT_ADDR_REG, (dExtBitBufCurPos & 1) << 6);
    JpuWriteReg(MJPEG_BBC_DATA_CNT_REG, 256 / 4); // 64 * 4 byte == 32 * 8 byte
    JpuWriteReg(MJPEG_BBC_COMMAND_REG, (jpg->streamEndian << 1) | 0);

    while (dMibStatus == 1) {
        dMibStatus = JpuReadReg(MJPEG_BBC_BUSY_REG);
        if( coutn++ >= 500)
        {
#ifdef  OUT_RECORD
          time (&timep);
          p=gmtime(&timep);
          sprintf( buffer,"%d:%d:%d  1111111  %d, count = %d\n",(8+p->tm_hour), p->tm_min, p->tm_sec, dMibStatus, coutn);
#ifdef _WIN32
          sprintf(filename, "/data/recode_%lu",GetCurrentThreadId());
#else
          sprintf(filename, "/data/recode_%lu",pthread_self());
#endif
          fp = fopen(filename, "a+");
          if(fp != NULL)
          {
             fwrite( buffer, strlen(buffer),1, fp);
             fclose(fp);
             fp = NULL;
          }
#endif
          coutn = 0;
          break;
        }
    }

    dMibStatus          = 1;
    dExtBitBufCurPos    = dExtBitBufCurPos + 1;

    JpuWriteReg(MJPEG_BBC_CUR_POS_REG, dExtBitBufCurPos);
    JpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, dExtBitBufBaseAddr + (dExtBitBufCurPos << 8));
    JpuWriteReg(MJPEG_BBC_INT_ADDR_REG, (dExtBitBufCurPos & 1) << 6);
    JpuWriteReg(MJPEG_BBC_DATA_CNT_REG, 256 / 4); // 64 * 4 byte == 32 * 8 byte
    JpuWriteReg(MJPEG_BBC_COMMAND_REG, (jpg->streamEndian << 1) | 0);

    while (dMibStatus == 1) {
        dMibStatus = JpuReadReg(MJPEG_BBC_BUSY_REG);
        if( coutn++ >= 500)
        {
#ifdef  OUT_RECORD
          time (&timep);
          p=gmtime(&timep);
          sprintf( buffer,"%d:%d:%d 222222  %d, count = %d\n",(8+p->tm_hour), p->tm_min, p->tm_sec, dMibStatus, coutn);
#ifdef _WIN32
          sprintf(filename, "/data/recode_%lu",GetCurrentThreadId());
#else
		  sprintf(filename, "/data/recode_%lu",pthread_self());
#endif
          fp = fopen(filename, "a+");
          if(fp != NULL)
          {
             fwrite( buffer, strlen(buffer),1, fp);
             fclose(fp);
             fp = NULL;
          }
          coutn = 0;
 #endif
          break;
        }
    }

    dMibStatus          = 1;
    dExtBitBufCurPos    = dExtBitBufCurPos + 1;

    JpuWriteReg(MJPEG_BBC_CUR_POS_REG, dExtBitBufCurPos); // next unit page pointer

    JpuWriteReg(MJPEG_BBC_CTRL_REG, (jpg->streamEndian<< 1) | 1);

    JpuWriteReg(MJPEG_GBU_WD_PTR_REG, jpg->wordPtr);

    JpuWriteReg(MJPEG_GBU_BBSR_REG, 0);
    JpuWriteReg(MJPEG_GBU_BBER_REG, ((256 / 4) * 2) - 1);

    if (jpg->pagePtr & 1) {
        JpuWriteReg(MJPEG_GBU_BBIR_REG, 0);
        JpuWriteReg(MJPEG_GBU_BBHR_REG, 0);
    }
    else {
        JpuWriteReg(MJPEG_GBU_BBIR_REG, 256 / 4);   // 64 * 4 byte == 32 * 8 byte
        JpuWriteReg(MJPEG_GBU_BBHR_REG, 256 / 4);   // 64 * 4 byte == 32 * 8 byte
    }

    JpuWriteReg(MJPEG_GBU_CTRL_REG, 4);
    JpuWriteReg(MJPEG_GBU_FF_RPTR_REG, jpg->bitPtr);
}


enum {
    SAMPLE_420 = 0xA,
    SAMPLE_H422 = 0x9,
    SAMPLE_V422 = 0x6,
    SAMPLE_444 = 0x5,
    SAMPLE_400 = 0x1
};

const BYTE cDefHuffBits[4][16] =
{
    {   // DC index 0 (Luminance DC)
        0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }
    ,
    {   // AC index 0 (Luminance AC)
        0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
        0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D
    }
    ,
    {   // DC index 1 (Chrominance DC)
        0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
    }
    ,
    {   // AC index 1 (Chrominance AC)
        0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
        0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
    }
};

const BYTE cDefHuffVal[4][162] =
{
    {   // DC index 0 (Luminance DC)
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B
    }
    ,
    {   // AC index 0 (Luminance AC)
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
        0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
        0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
        0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
        0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
        0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
        0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
        0xF9, 0xFA
    }
    ,
    {   // DC index 1 (Chrominance DC)
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B
    }
    ,
    {   // AC index 1 (Chrominance AC)
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
        0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
        0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
        0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
        0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
        0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
        0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
        0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
        0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
        0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
        0xF9, 0xFA
    }
};



enum {
    Marker          = 0xFF,
    FF_Marker       = 0x00,

    SOI_Marker      = 0xFFD8,           // Start of image
    EOI_Marker      = 0xFFD9,           // End of image

    JFIF_CODE       = 0xFFE0,           // Application
    EXIF_CODE       = 0xFFE1,

    DRI_Marker      = 0xFFDD,           // Define restart interval
    RST_Marker      = 0xD,              // 0xD0 ~0xD7

    DQT_Marker      = 0xFFDB,           // Define quantization table(s)
    DHT_Marker      = 0xFFC4,           // Define Huffman table(s)

    SOF0_Marker     = 0xFFC0,           // Start of frame : Baseline DCT
    SOF1_Marker     = 0xFFC1,           // Start of frame: extend sequential DCT
    SOS_Marker      = 0xFFDA,           // Start of scan
};

enum {
	SOI_FOUND       = 0x1,
	SOF_FOUND       = 0x2,
	SOS_FOUND       = 0x4
};


static int check_start_code(JpgDecInfo *jpg)
{
    if (show_bits(&jpg->gbc, 8) == 0xFF)
        return 1;
    else
        return 0;
}


static int find_start_code(JpgDecInfo *jpg)
{
    int word;

    for(;;)
    {
        if (get_bits_left(&jpg->gbc) <= 16)
        {
            //printf("hit end of stream\n");
            return 0;
        }

        word = show_bits(&jpg->gbc, 16);
        if ((word > 0xFF00) && (word < 0xFFFF))
            break;

        get_bits(&jpg->gbc, 8);
    }

    return word;
}

#if 0
static int find_start_code_fast(JpgDecInfo *jpg)
{
    int word = 0;
    jpu_getbit_context_t *ctx = &jpg->gbc;
    int left_byte = get_bits_left(ctx) >> 3;
    BYTE *p = ctx->buffer + ctx->index;
    int offset = 0;

    if (left_byte <= 2)
        return 0;

    for (offset = 0; offset < left_byte; offset++){
        word = (word & 0xFF) << 8;
        word |= *p++;

        if ((word > 0xFF00) && (word < 0xFFFF))
            break;
    }

    if (offset == left_byte){
        skip_bits(&jpg->gbc, (left_byte - 2)*8);   // keep compatible with original find_start_code()
        return 0;
    } else {
        skip_bits(&jpg->gbc, (offset - 1)*8);
        return word;
    }
}
#endif

#ifndef LIBBMJPULITE
static int find_start_soi_code(JpgDecInfo *jpg)
{
    int word;

    for(;;)
    {
        if (get_bits_left(&jpg->gbc) <= 16)
        {
            //printf("hit end of stream\n");
            return 0;
        }

        word = show_bits(&jpg->gbc, 16);
        if ((word > 0xFF00) && (word < 0xFFFF))
        {
            if (word != SOI_Marker)
                get_bits(&jpg->gbc, 8);
            break;
        }


        get_bits(&jpg->gbc, 8);
    }

    return word;
}
#endif

#ifdef MJPEG_ERROR_CONCEAL
static int find_restart_marker(JpgDecInfo *jpg)
{
    int word;

    for(;;)
    {
        if (get_bits_left(&jpg->gbc) <= 16)
        {
            //printf("hit end of stream\n");
            return -1;
        }

        word = show_bits(&jpg->gbc, 16);
        if ( ((word >= 0xFFD0) && (word <= 0xFFD7)) || (word == 0xFFD8)  || (word == 0xFFD9))
            break;

        get_bits(&jpg->gbc, 8);
    }

    return word;
}
#endif

static int decode_app_header(JpgDecInfo *jpg)
{
    int length;

    if (get_bits_left(&jpg->gbc) < 16)
        return 0;

    length = get_bits(&jpg->gbc, 16);
    length -= 2;

    if (length * 8 > get_bits_left(&jpg->gbc))
        length = get_bits_left(&jpg->gbc) >> 3;

    skip_bits(&jpg->gbc, length*8);

    return 1;
}

/* Define Restart Interval */
static int decode_dri_header(JpgDecInfo *jpg)
{
    if (get_bits_left(&jpg->gbc) < 16*2)
        return 0;

    //Length, Lr
    get_bits(&jpg->gbc, 16);

    jpg->rstIntval = get_bits(&jpg->gbc, 16);

    return 1;
}

/* Define Quantization Table */
static int decode_dqt_header(JpgDecInfo *jpg)
{
    int Pq;
    int Tq;
    int i;
    int tmp;
    if (get_bits_left(&jpg->gbc) < 16)
        return 0;

    // Lq, Length of DQT
    get_bits(&jpg->gbc, 16);

    do {
        if (get_bits_left(&jpg->gbc) < 4+4+8*64)
            return 0;

        // Pq, Quantization Precision
        tmp = get_bits(&jpg->gbc, 8);
        // Tq, Quantization table destination identifier
        Pq = (tmp>>4) & 0xf;
        Tq = tmp&0xf;
        if (Tq > 3) continue;

        for (i=0; i<64; i++)
            jpg->qMatTab[Tq][i] = (BYTE)get_bits(&jpg->gbc, 8);
    } while(!check_start_code(jpg));

    if (Pq != 0) // not 8-bit
    {
        //printf("pq is not set to zero\n");
        return 0;
    }
    return 1;
}

/* Define Huffman Table */
static int decode_dht_header(JpgDecInfo *jpg)
{
    int Tc;
    int Th;
    int ThTc;
    int bitCnt;
    int i;
    int tmp;
    if (get_bits_left(&jpg->gbc) < 16)
        return 0;

    // Length, Lh
    get_bits(&jpg->gbc, 16);

    do {
        if (get_bits_left(&jpg->gbc) < 8 + 8*16)
            return 0;

        // Table class - DC, AC
        tmp = get_bits(&jpg->gbc, 8);
        // Table destination identifier
        Tc = (tmp>>4) & 0xf;
        Th = tmp&0xf;

        // DC_ID0 (0x00) -> 0
        // AC_ID0 (0x10) -> 1
        // DC_ID1 (0x01) -> 2
        // AC_ID1 (0x11) -> 3
        ThTc = ((Th&1)<<1) | (Tc&1);

        // Get Huff Bits list
        bitCnt = 0;
        for (i=0; i<16; i++)
        {
            jpg->huffBits[ThTc][i] = (BYTE)get_bits(&jpg->gbc, 8);
            bitCnt += jpg->huffBits[ThTc][i];

            if (cDefHuffBits[ThTc][i] != jpg->huffBits[ThTc][i])
                jpg->userHuffTab = 1;
        }


        if (get_bits_left(&jpg->gbc) <  8*bitCnt)
            return 0;

        // Get Huff Val list
        for (i=0; i<bitCnt; i++)
        {
            jpg->huffVal[ThTc][i] = (BYTE)get_bits(&jpg->gbc, 8);
            if (cDefHuffVal[ThTc][i] != jpg->huffVal[ThTc][i])
                jpg->userHuffTab = 1;
        }
    } while(!check_start_code(jpg));

    return 1;
}

/* Start Of Frame */
static int decode_sof_header(JpgDecInfo *jpg)
{
    int samplePrecision;
    int sampleFactor;
    int i;
    int Tqi;
    BYTE compID;
    int hSampFact[3] = {0,};
    int vSampFact[3] = {0,};
    int picX, picY;
    int numComp;
    int tmp;

    if (get_bits_left(&jpg->gbc) < 16+8+16+16+8)
        return 0;

    // LF, Length of SOF
    get_bits(&jpg->gbc, 16);

    // Sample Precision: Baseline(8), P
    samplePrecision = get_bits(&jpg->gbc, 8);
    if (samplePrecision != 8)
    {
        //printf("Sample Precision is not 8\n");
        return 0;
    }

    picY = get_bits(&jpg->gbc, 16);
    if (picY > MAX_MJPG_PIC_WIDTH)
    {
        //printf("Picture Vertical Size limits Maximum size\n");
        return 0;
    }

    picX = get_bits(&jpg->gbc, 16);
    if (picX > MAX_MJPG_PIC_HEIGHT)
    {
        //printf("Picture Horizontal Size limits Maximum size\n");
        return 0;
    }

    //Number of Components in Frame: Nf
    numComp = get_bits(&jpg->gbc, 8);
    if (numComp > 3)
    {
        //printf("Picture Horizontal Size limits Maximum size\n");
    }

    if (get_bits_left(&jpg->gbc) < numComp*(8+4+4+8))
        return 0;
    for (i=0; i<numComp; i++)
    {
        // Component ID, Ci 0 ~ 255
        compID = (BYTE)get_bits(&jpg->gbc, 8);
        tmp = get_bits(&jpg->gbc, 8);
        // Horizontal Sampling Factor, Hi
        hSampFact[i] = (tmp>>4) & 0xf;
        // Vertical Sampling Factor, Vi
        vSampFact[i] = tmp&0xf;
        // Quantization Table Selector, Tqi
        Tqi = get_bits(&jpg->gbc, 8);

        jpg->cInfoTab[i][0] = compID;
        jpg->cInfoTab[i][1] = (BYTE)hSampFact[i];
        jpg->cInfoTab[i][2] = (BYTE)vSampFact[i];
        jpg->cInfoTab[i][3] = (BYTE)Tqi;
    }

    if (hSampFact[0]>2 || vSampFact[0]>2 ||
        (numComp == 3 &&
         (hSampFact[1]!=1 || hSampFact[2]!=1 ||
          vSampFact[1]!=1 || vSampFact[2]!=1)))
    {
        printf("Not Supported Sampling Factor!\n");
        return 0;
    }

    if (numComp == 1)
        sampleFactor = SAMPLE_400;
    else
        sampleFactor = ((hSampFact[0]&3)<<2) | (vSampFact[0]&3);

    switch(sampleFactor) {
    case SAMPLE_420:
        jpg->format = FORMAT_420;
        break;
    case SAMPLE_H422:
        jpg->format = FORMAT_422;
        break;
    case SAMPLE_V422:
        jpg->format = FORMAT_224;
        break;
    case SAMPLE_444:
        jpg->format = FORMAT_444;
        break;
    default: // 4:0:0
        jpg->format = FORMAT_400;
    }

    jpg->picWidth = picX;
    jpg->picHeight = picY;

    return 1;
}

/* Start Of Scan */
static int decode_sos_header(JpgDecInfo *jpg)
{
    int i, j;
    int len;
    int numComp;
    int compID;
    int ecsPtr;
    int ss, se, ah, al;
    int dcHufTblIdx[3] = {0,};
    int acHufTblIdx[3] = {0,};
    int tmp;

    if (get_bits_left(&jpg->gbc) < 8)
        return 0;

    // Length, Ls
    len = get_bits(&jpg->gbc, 16);

    jpg->ecsPtr = get_bits_count(&jpg->gbc)/8 + len - 2 ;

    ecsPtr = jpg->ecsPtr + jpg->frameOffset;

#if 0
    printf("ecsPtr=0x%x frameOffset=0x%x, ecsOffset=0x%x, wrPtr=0x%x, rdPtr0x%x\n",
           jpg->ecsPtr, jpg->frameOffset, ecsPtr, jpg->streamWrPtr, jpg->streamRdPtr);
#endif

    jpg->pagePtr = ecsPtr >> 8;                                  //page unit  ecsPtr/256;
    jpg->wordPtr = (ecsPtr & 0xF0) >> 2;                         // word unit ((ecsPtr % 256) & 0xF0) / 4;

    if (jpg->pagePtr & 1)
        jpg->wordPtr += 64;
    if (jpg->wordPtr & 1)
        jpg->wordPtr -= 1; // to make even.

    jpg->bitPtr = (ecsPtr & 0xF) << 3;                          // bit unit (ecsPtr & 0xF) * 8;

    if (get_bits_left(&jpg->gbc) < 8)
        return 0;

    //Number of Components in Scan: Ns
    numComp = get_bits(&jpg->gbc, 8);

    if (get_bits_left(&jpg->gbc) < numComp*(8+4+4))
        return 0;
    for (i=0; i<numComp; i++) {
        // Component ID, Csj 0 ~ 255
        compID = get_bits(&jpg->gbc, 8);
        tmp = get_bits(&jpg->gbc, 8);
        // dc entropy coding table selector, Tdj
        dcHufTblIdx[i] = (tmp>>4) & 0xf;
        // ac entropy coding table selector, Taj
        acHufTblIdx[i] = tmp&0xf;


        for (j=0; j<numComp; j++)
        {
            if (compID == jpg->cInfoTab[j][0])
            {
                jpg->cInfoTab[j][4] = (BYTE)dcHufTblIdx[i];
                jpg->cInfoTab[j][5] = (BYTE)acHufTblIdx[i];
            }
        }
    }

    if (get_bits_left(&jpg->gbc) < 8+8+4+4)
        return 0;
    // Ss 0
    ss = get_bits(&jpg->gbc, 8);
    // Se 3F
    se = get_bits(&jpg->gbc, 8);
    tmp = get_bits(&jpg->gbc, 8);
    // Ah 0
    ah = (i>>4) & 0xf;
    // Al 0
    al = tmp&0xf;

    if ((ss != 0) || (se != 0x3F) || (ah != 0) || (al != 0))
    {
        //printf("The Jpeg Image must be another profile\n");
        return 0;
    }

    return 1;
}

static void genDecHuffTab(JpgDecInfo *jpg, int tabNum)
{
    uint8_t *huffPtr, *huffBits;
    unsigned int *huffMax, *huffMin; // TODO uint16_t

    int ptrCnt =0;
    int huffCode = 0;
    int dataFlag = 0;
    int i;

    huffBits = jpg->huffBits[tabNum];
    huffPtr  = jpg->huffPtr[tabNum];
    huffMax  = jpg->huffMax[tabNum];
    huffMin  = jpg->huffMin[tabNum];

    for (i=0; i<16; i++)
    {
        if (huffBits[i]) // if there is bit cnt value
        {
            huffPtr[i] = (BYTE)ptrCnt;
            ptrCnt += huffBits[i];
            huffMin[i] = huffCode;
            huffMax[i] = huffCode + (huffBits[i] - 1);

            dataFlag = 1;
            huffCode += huffBits[i];
        }
        else
        {
            huffPtr[i] = 0xFF;
            huffMin[i] = 0xFFFF;
            huffMax[i] = 0xFFFF;
        }

        if (dataFlag == 1)
            huffCode <<= 1;
    }
}

int JpuGbuInit(jpu_getbit_context_t *ctx, BYTE *buffer, int size)
{
    ctx->buffer = buffer;
    ctx->index = 0;
    ctx->size = size/8;

    return 1;
}

int JpuGbuGetUsedBitCount(jpu_getbit_context_t *ctx)
{
    return ctx->index*8;
}

int JpuGbuGetLeftBitCount(jpu_getbit_context_t *ctx)
{
    return (ctx->size*8) - JpuGbuGetUsedBitCount(ctx);
}

unsigned int JpuGbuGetBit(jpu_getbit_context_t *ctx, int bit_num)
{
    BYTE *p;
    unsigned int b = 0x0;

    if (bit_num > JpuGbuGetLeftBitCount(ctx))
        return (unsigned int)-1;

    p = ctx->buffer + ctx->index;

    if (bit_num == 8)
    {
        b = *p;
        ctx->index++;
    }
    else if(bit_num == 16)
    {
        b = *p++<<8;
        b |= *p++;
        ctx->index += 2;
    }
    else if(bit_num == 32)
    {
        b = *p++<<24;
        b |= (*p++<<16);
        b |= (*p++<<8);
        b |= (*p++<<0);
        ctx->index += 4;
    }

    return b;
}

unsigned int JpuGguShowBit(jpu_getbit_context_t *ctx, int bit_num)
{
    BYTE *p;
    unsigned int b = 0x0;

    if (bit_num > JpuGbuGetLeftBitCount(ctx))
        return (unsigned int)-1;

    p = ctx->buffer + ctx->index;

    if (bit_num == 8)
    {
        b = *p;
    }
    else if(bit_num == 16)
    {
        b = *p++<<8;
        b |= *p++;
    }
    else if(bit_num == 32)
    {
        b = *p++<<24;
        b |= (*p++<<16);
        b |= (*p++<<8);
        b |= (*p++<<0);
    }

    return b;
}

void JpuGbuSkipBit(jpu_getbit_context_t *ctx, int bit_num) //skip byte-aligned bits
{
    if (bit_num > JpuGbuGetLeftBitCount(ctx))
        bit_num = JpuGbuGetLeftBitCount(ctx);
    ctx->index += (bit_num >> 3);

    return;
}

static int wraparound_bistream_data(JpgDecInfo *jpg, int return_type)
{
    BYTE *dst;
    BYTE *src;
    BYTE *data;
    int data_size;
    int src_size;
    int dst_size;

    data_size = jpg->streamWrPtr - jpg->streamBufStartAddr;
    data = (BYTE *)malloc(data_size);

    if (data_size && data)
        JpuReadMem(jpg->streamBufStartAddr, data, data_size, jpg->streamEndian);

    src_size = jpg->streamBufSize - jpg->frameOffset;
    src = (BYTE *)malloc(src_size);
    dst_size = ((src_size+(JPU_GBU_SIZE-1))&~(JPU_GBU_SIZE-1));
    dst = (BYTE *)malloc(dst_size);
    if (dst && src)
    {
        memset(dst, 0x00, dst_size);
        JpuReadMem(jpg->streamBufStartAddr+jpg->frameOffset, src, src_size, jpg->streamEndian);
        memcpy(dst+(dst_size-src_size), src, src_size);
        JpuWriteMem(jpg->streamBufStartAddr, dst, dst_size, jpg->streamEndian);
        if (data_size && data)
            JpuWriteMem(jpg->streamBufStartAddr+dst_size, data, data_size, jpg->streamEndian);
        free(src);
        free(dst);
    }

    if (data_size && data)
        free(data);

    if (return_type == -2) {// header wraparound
        jpg->streamWrPtr = jpg->streamBufStartAddr+dst_size+data_size;
        jpg->consumeByte = 0;
        return -2;
    }
    else if(return_type == -1){ // ecsPtr wraparound
        jpg->streamWrPtr = jpg->streamBufStartAddr+dst_size+data_size;
        jpg->frameOffset = 0;
        return -1;
    }

    return 0; // error
}

#ifdef MJPEG_ERROR_CONCEAL
int JpegDecodeConcealError(JpgDecInfo *jpg)
{
    unsigned int code;
    int ret, foced_stop = 0;
    BYTE *b;
    Uint32 ecsPtr = 0;
    Uint32 size, wrOffset;
    Uint32 nextMarkerPtr;
    Uint32 iRstIdx;
    int numMcu = 0, numMcuCol = 0;

    // "nextOffset" is distance between frameOffset and next_restart_index that exist.
    nextMarkerPtr = jpg->nextOffset + jpg->frameOffset;

    if (jpg->BitStreamByteSize)
    {
        size = jpg->BitStreamByteSize - nextMarkerPtr;
        wrOffset = jpg->BitStreamByteSize;
    }
    else
    {
        if (jpg->streamWrPtr == jpg->streamBufStartAddr)
        {
            size    = jpg->streamBufSize - nextMarkerPtr;
            wrOffset= jpg->streamBufSize;
        }
        else
        {
            size    = jpg->streamWrPtr - nextMarkerPtr;
            wrOffset= (jpg->streamWrPtr - jpg->streamBufStartAddr);
        }
    }

    //pointing to find next_restart_marker
    b = jpg->pBitStream + nextMarkerPtr;
    init_get_bits(&jpg->gbc, b, size*8);

    for(;;)
    {
        ret = find_restart_marker(jpg);
        if ( ret < 0)
        {
            break;
        }

        code = get_bits(&jpg->gbc, 16);
        if(code == 0xFFD8 || code == 0xFFD9)
        {
            // if next_start_maker meet EOI(end of picture) or next SOI(start of image) but decoding is not completed yet,
            // To prevent over picture boundary in ringbuffer mode,
            // finding previous maker and make decoding forced to stop.
            b = jpg->pBitStream + jpg->currOffset + jpg->frameOffset;
            init_get_bits(&jpg->gbc, b, size*8);

            nextMarkerPtr = jpg->currOffset + jpg->frameOffset;
            foced_stop = 1;
            continue;
        }

        iRstIdx = (code & 0xF);
        // you can find next restart marker which will be.
        if( iRstIdx >= 0 && iRstIdx <= 7)
        {
            int numMcuRow;

            if (get_bits_left(&jpg->gbc) < 8)
                return (-1);

            jpg->ecsPtr = get_bits_count(&jpg->gbc)/8;

            // set ecsPtr that restart marker is founded.
            ecsPtr = jpg->ecsPtr + nextMarkerPtr;

            jpg->pagePtr = ecsPtr >> 8;
            jpg->wordPtr = (ecsPtr  & 0xF0) >> 2;    // word unit
            if (jpg->pagePtr & 1)
                jpg->wordPtr += 64;
            if (jpg->wordPtr & 1)
                jpg->wordPtr -= 1;                   // to make even.

            jpg->bitPtr = (ecsPtr & 0xF) << 3;       // bit unit

            numMcuRow = (jpg->alignedWidth / jpg->mcuWidth);
            numMcuCol = (jpg->alignedHeight/ jpg->mcuHeight);


            // num of restart marker that is rounded can be calculated from error position.
            //numRstMakerRounding = ((jpg->errInfo.errPosY*numMcuRow + jpg->errInfo.errPosX) / (jpg->rstIntval*8));
            jpg->curRstIdx = iRstIdx;
            if(jpg->curRstIdx < jpg->nextRstIdx)
                jpg->numRstMakerRounding++;

            // find mcu position to restart.
            numMcu = (jpg->numRstMakerRounding*jpg->rstIntval*8) + (jpg->curRstIdx + 1)*jpg->rstIntval;
            //numMcu = jpg->rstIntval;
            jpg->setPosX = (numMcu % numMcuRow);
            jpg->setPosY = (numMcu / numMcuRow);
            jpg->gbuStartPtr = ((ecsPtr- jpg->frameOffset)/256)*256;

            // if setPosY is greater than Picture Height, set mcu position to last mcu of picture to finish decoding.
            if((jpg->setPosY > numMcuCol) || foced_stop)
            {
                jpg->setPosX = numMcuRow - 1;
                jpg->setPosY = numMcuCol - 1;
            }

            // update restart_index to find next.
            jpg->nextRstIdx = iRstIdx++;

            ret = 0;
            break;
        }
    }

    // prevent to find same position.
    jpg->currOffset = jpg->nextOffset;

    // if the distance between ecsPtr and streamBufEndAddr is less than 512byte, that rdPtr run over than streamBufEndAddr without interrupt.
    // bellow is workaround to avoid this case.
    if (jpg->streamBufSize - (jpg->frameOffset+ecsPtr) < JPU_GBU_SIZE)
    //if((jpg->streamBufEndAddr - ecsPtr < JPU_GBU_SIZE) )
    {
        ret = wraparound_bistream_data(jpg, -1);
    }

    //if (wrOffset - (jpg->frameOffset+jpg->ecsPtr)  < JPU_GBU_SIZE &&
    //    jpg->streamEndflag == 0) {
    //    return -1;
    //}

    return ret;
}
#endif

int JpegDecodeHeader(JpgDecInfo *jpg)
{
    unsigned int code;
    int ret = 1;
    int i;
    int v;
    int wrOffset;
    BYTE *b = jpg->pBitStream + jpg->frameOffset;
    int size;
    int flag = 0;

    if (jpg->BitStreamByteSize)
    {
        size = jpg->BitStreamByteSize - jpg->frameOffset;
        wrOffset = jpg->BitStreamByteSize;
    }
    else
    {
        if (jpg->streamWrPtr == jpg->streamBufStartAddr)
        {
            size = jpg->streamBufSize - jpg->frameOffset;
            wrOffset = jpg->streamBufSize;
        }
        else
        {
            if (jpg->frameOffset >= (int)(jpg->streamWrPtr - jpg->streamBufStartAddr))
                size = jpg->streamBufSize - jpg->frameOffset;
            else
                size = (jpg->streamWrPtr - jpg->streamBufStartAddr) - jpg->frameOffset;
            wrOffset = (jpg->streamWrPtr - jpg->streamBufStartAddr);
        }
    }

    if (!b || !size) {
        ret = -1;
        goto DONE_DEC_HEADER;
    }

    /* Change for decoding Motion JPEG */
#ifndef LIBBMJPULITE
    // find start code of next frame
    if (!jpg->ecsPtr)
    {
        int nextOffset = 0;
        int soiOffset = 0;

        if (jpg->consumeByte != 0)  // meaning is frameIdx > 0
        {
            nextOffset = jpg->consumeByte;
            if (nextOffset <= 0)
                nextOffset = 2; //in order to consume start code.
        }

        //consume to find the start code of next frame.
        b += nextOffset;
        size -= nextOffset;

        if (size < 0)
        {
            jpg->consumeByte -= (b - (jpg->pBitStream+jpg->streamBufSize));
            if (jpg->consumeByte < 0) {
                ret = 0;
                goto DONE_DEC_HEADER;
            }
            ret = -1;
            goto DONE_DEC_HEADER;
        }

        init_get_bits(&jpg->gbc, b, size*8);
        for (;;)
        {
            code = find_start_soi_code(jpg);
            if (code == 0)
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }

            if (code == SOI_Marker)
                break;
        }

        soiOffset = get_bits_count(&jpg->gbc)/8;

        b += soiOffset;
        size -= soiOffset;
        jpg->frameOffset += (soiOffset+ nextOffset);
    }
#endif

    if (jpg->headerSize > 0 && (jpg->headerSize > (jpg->streamBufSize - jpg->frameOffset))) { // if header size is smaller than room of stream end. copy the buffer to bistream start.
        return wraparound_bistream_data(jpg, -2);
    }

    init_get_bits(&jpg->gbc, b, size*8);

    // Initialize component information table
    for (i=0; i<4; i++)
    {
        jpg->cInfoTab[i][0] = 0;
        jpg->cInfoTab[i][1] = 0;
        jpg->cInfoTab[i][2] = 0;
        jpg->cInfoTab[i][3] = 0;
        jpg->cInfoTab[i][4] = 0;
        jpg->cInfoTab[i][5] = 0;
    }

    for (;;)
    {
        if (find_start_code(jpg) == 0)
        {
            ret = -1;
            goto DONE_DEC_HEADER;
        }

        code = get_bits(&jpg->gbc, 16); //getbit 2byte

        switch (code) {
        case SOI_Marker:
	        flag = flag | SOI_FOUND;
            break;
        case JFIF_CODE:
        case EXIF_CODE:
            if (!decode_app_header(jpg))
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }
            break;
        case DRI_Marker:
            if (!decode_dri_header(jpg))
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }
            break;
        case DQT_Marker:
            if (!decode_dqt_header(jpg))
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }
            break;
        case DHT_Marker:
            if (!decode_dht_header(jpg))
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }
            break;
        case SOF0_Marker:
        case SOF1_Marker:
            if (!decode_sof_header(jpg))
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }
            flag = flag | SOF_FOUND;
            break;
        case SOS_Marker:
            if (!decode_sos_header(jpg))
            {
                ret = -1;
                goto DONE_DEC_HEADER;
            }
            // TODO we assume header size of all frame is same for mjpeg case
            if (!jpg->headerSize)
                jpg->headerSize = jpg->ecsPtr;
            flag = flag | SOS_FOUND;
            goto DONE_DEC_HEADER;
            break;
        case EOI_Marker:
            goto DONE_DEC_HEADER;
        default:
            switch (code&0xFFF0)
            {
            case 0xFFE0:    // 0xFFEX
            case 0xFFF0:    // 0xFFFX
                if (get_bits_left(&jpg->gbc) <=0 ) {
                    {
                        ret = -1;
                        goto DONE_DEC_HEADER;
                    }
                }
                else
                {
                    if (!decode_app_header(jpg))
                    {
                        ret = -1;
                        goto DONE_DEC_HEADER;
                    }
                    break;
                }
            default:
                //in case,  restart marker is founded.
                if( (code&0xFFF0) >= 0xFFD0 && (code&0xFFF0) <= 0xFFD7)
                    break;

                //printf("code = [%x]\n", code);
                return  0;
            }
            break;
        }
    }

DONE_DEC_HEADER:
    if (ret == -1)
    {
        if (wrOffset < jpg->frameOffset)
            return -2;

        return -1;
    }

    if (!jpg->ecsPtr)
        return 0;

    if (!((flag & SOI_FOUND) && (flag & SOF_FOUND) && (flag & SOS_FOUND)))
        return -1;

    //if (wrOffset - (jpg->frameOffset+jpg->ecsPtr)  < JPU_GBU_SIZE &&
    //    jpg->streamEndflag == 0) {
    //    return -1;
    //}

    // this bellow is workaround to avoid the case that JPU is run over without interrupt.
    if (jpg->streamBufSize - (jpg->frameOffset+jpg->ecsPtr) < JPU_GBU_SIZE)
        return wraparound_bistream_data(jpg, -1);

    // Generate Huffman table information
    for(i=0; i<4; i++)
        genDecHuffTab(jpg, i);

    // Q Idx
    v =          jpg->cInfoTab[0][3];
    v = v << 1 | jpg->cInfoTab[1][3];
    v = v << 1 | jpg->cInfoTab[2][3];
    jpg->Qidx = v;


    // Huff Idx[DC, AC]
    v =          jpg->cInfoTab[0][4];
    v = v << 1 | jpg->cInfoTab[1][4];
    v = v << 1 | jpg->cInfoTab[2][4];
    jpg->huffDcIdx = v;

    v =          jpg->cInfoTab[0][5];
    v = v << 1 | jpg->cInfoTab[1][5];
    v = v << 1 | jpg->cInfoTab[2][5];
    jpg->huffAcIdx = v;


    switch(jpg->format)
    {
    case FORMAT_420:
        jpg->busReqNum = 2;
        jpg->mcuBlockNum = 6;
        jpg->compNum = 3;
        jpg->compInfo[0] = 10;
        jpg->compInfo[1] = 5;
        jpg->compInfo[2] = 5;
        jpg->alignedWidth = ((jpg->picWidth+15)&~15);
        jpg->alignedHeight = ((jpg->picHeight+15)&~15);
        jpg->mcuWidth  = 16;
        jpg->mcuHeight = 16;
        break;
    case FORMAT_422:
        jpg->busReqNum = 3;
        jpg->mcuBlockNum = 4;
        jpg->compNum = 3;
        jpg->compInfo[0] = 9;
        jpg->compInfo[1] = 5;
        jpg->compInfo[2] = 5;
        jpg->alignedWidth = ((jpg->picWidth+15)&~15);
        jpg->alignedHeight = ((jpg->picHeight+7)&~7);
        jpg->mcuWidth  = 16;
        jpg->mcuHeight = 8;
        break;
    case FORMAT_224:
        jpg->busReqNum = 3;
        jpg->mcuBlockNum = 4;
        jpg->compNum = 3;
        jpg->compInfo[0] = 6;
        jpg->compInfo[1] = 5;
        jpg->compInfo[2] = 5;
        jpg->alignedWidth = ((jpg->picWidth+7)&~7);
        jpg->alignedHeight = ((jpg->picHeight+15)&~15);
        jpg->mcuWidth  = 8;
        jpg->mcuHeight = 16;
        break;
    case FORMAT_444:
        jpg->busReqNum = 4;
        jpg->mcuBlockNum = 3;
        jpg->compNum = 3;
        jpg->compInfo[0] = 5;
        jpg->compInfo[1] = 5;
        jpg->compInfo[2] = 5;
        jpg->alignedWidth = ((jpg->picWidth+7)&~7);
        jpg->alignedHeight = ((jpg->picHeight+7)&~7);
        jpg->mcuWidth  = 8;
        jpg->mcuHeight = 8;
        break;
    case FORMAT_400:
        jpg->busReqNum = 4;
        jpg->mcuBlockNum = 1;
        jpg->compNum = 1;
        jpg->compInfo[0] = 5;
        jpg->compInfo[1] = 0;
        jpg->compInfo[2] = 0;
        jpg->alignedWidth = ((jpg->picWidth+7)&~7);
        jpg->alignedHeight = ((jpg->picHeight+7)&~7);
        jpg->mcuWidth  = 8;
        jpg->mcuHeight = 8;
        break;
    }

    return 1;
}

JpgRet CheckJpgEncOpenParam(JpgEncOpenParam * pop)
{
    int picWidth;
    int picHeight;

    if (pop == 0) {
        return JPG_RET_INVALID_PARAM;
    }

    picWidth = pop->picWidth;
    picHeight = pop->picHeight;

    if (pop->bitstreamBuffer % 8) {
        JLOG(ERR, "bitstreamBuffer is NOT 8-byte aligned!\n");
        return JPG_RET_INVALID_PARAM;
    }

    if (pop->bitstreamBufferSize % 1024) {
        JLOG(ERR, "the size of bitstreamBuffer (%d) is NOT multiple of 1024!\n",
             pop->bitstreamBufferSize);
        return JPG_RET_INVALID_PARAM;
    }
    if (pop->bitstreamBufferSize < 1024) {
        JLOG(ERR, "the size of bitstreamBuffer (%d) is less than 1024!\n",
             pop->bitstreamBufferSize);
        return JPG_RET_INVALID_PARAM;
    }
    // if (pop->bitstreamBufferSize > 16383 * 1024) {
    //     JLOG(ERR, "the size of bitstreamBuffer (%d) is greater than 16383*1024!\n",
    //          pop->bitstreamBufferSize);
    //     return JPG_RET_INVALID_PARAM;
    // }

    if (picWidth < 16 ) {
        JLOG(ERR, "picWidth is less than 16.\n");
        return JPG_RET_INVALID_PARAM;
    }
    if (picWidth > MAX_MJPG_PIC_WIDTH ) {
        JLOG(ERR, "picWidth is greater than %d.\n", MAX_MJPG_PIC_WIDTH);
        return JPG_RET_INVALID_PARAM;
    }
    if (picHeight < 16 ) {
        JLOG(ERR, "picHeight is less than 16.\n");
        return JPG_RET_INVALID_PARAM;
    }
    if (picHeight > MAX_MJPG_PIC_HEIGHT ) {
        JLOG(ERR, "picHeight is greater than %d.\n", MAX_MJPG_PIC_HEIGHT);
        return JPG_RET_INVALID_PARAM;
    }

    return JPG_RET_SUCCESS;
}
#if !defined(_WIN32)
int JpgGetDump(int typeCodec)
{
    int ret = 0;
    DumpInfo  dumpInfo = { 0 };
    int isDump = jdi_get_dump_info(&dumpInfo);
    if(isDump)
    {
        ret = dumpInfo.dump_frame_num;
        if( ret > 0)
        {
            jdi_clear_dump_info();
        }
            printf(" JpgGetDump  isDump = %d ,dump_frame_num = %d\n",isDump,ret);
    }
    return ret;
}
void JpgEncDumpIn(JpgEncHandle handle, JpgEncParam * param)
{
    JpgInst *pJpgInst;
    JpgEncInfo *pEncInfo;
    DumpInfo  dumpInfo = { 0 };
    FILE *fp = NULL;
    static __thread int dump_enc_frame_num = 0;
    int isDump = jdi_get_dump_info(&dumpInfo);
    if(isDump)
    {
        char filename[255];
        pJpgInst = handle;
        pEncInfo = &pJpgInst->JpgInfo.encInfo;
        int width = pEncInfo->openParam.picWidth;
        int height = pEncInfo->openParam.picHeight;
        int dump_total_num = dumpInfo.dump_frame_num;
        if(dumpInfo.jpufd == 0 || dumpInfo.jpufd == -1)
            return;
        if( dump_enc_frame_num < dump_total_num)
        {
            sprintf(filename, "/data/pic%d_%dx%d_yuv420_tid_%lu_enc.yuv", dump_enc_frame_num++, width, height, pthread_self());
            fp = fopen(filename, "wb+");
            if(fp != NULL) {
#ifdef LIBBMJPULITE
                int strideY = param->sourceFrame->strideY;
#else
                int strideY = param->sourceFrame->stride;
#endif

                PhysicalAddress addrY  = param->sourceFrame->bufY;
                PhysicalAddress addrCb = param->sourceFrame->bufCb;
                PhysicalAddress addrCr = param->sourceFrame->bufCr;

                unsigned long long page_size = sysconf(_SC_PAGE_SIZE);
                unsigned long long page_mask = (~(page_size-1));
                off_t pgoff = addrY & page_mask;
                unsigned long long virt_addrY = (unsigned long long)mmap(NULL, strideY*height*3/2, PROT_READ, MAP_SHARED, dumpInfo.jpufd, pgoff);
                if ((void *)virt_addrY == MAP_FAILED) {
                    goto release;
                }

                for(int i = 0; i < height; i++)
                {
                    fwrite((void*)( virt_addrY + i* strideY), sizeof(Uint8), width, fp);
                }

                unsigned long long virt_addrCb = virt_addrY + (addrCb - addrY);
                for(int i = 0; i < (height / 2); i++)
                {
                    fwrite((void*)( virt_addrCb + i* strideY/2), sizeof(Uint8), width / 2, fp);
                }

                unsigned long long virt_addrCr = virt_addrCb + (addrCr - addrCb);
                for(int i = 0; i < (height / 2); i++)
                {
                    fwrite((void*)( virt_addrCr + i* strideY/2), sizeof(Uint8), width / 2, fp);
                }

                munmap((void *)virt_addrY, strideY*height*3/2);
            }
            else
            {
                JLOG(ERR, "JpgEncDumpIn fopen failure\n");
            }
        }
        else{
            jdi_clear_dump_info();
            dump_enc_frame_num = 0;
        }
    }
release:
    if(fp != NULL)
        fclose(fp);
}
#endif

JpgRet CheckJpgEncParam(JpgEncHandle handle, JpgEncParam * param)
{
    JpgInst *pJpgInst;
    JpgEncInfo *pEncInfo;

    pJpgInst = handle;
    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    if (param == 0) {
        return JPG_RET_INVALID_PARAM;
    }

    if (pEncInfo->packedFormat == PACKED_FORMAT_444) {
#ifdef LIBBMJPULITE
        int stride = param->sourceFrame->strideY;
#else
        int stride = param->sourceFrame->stride;
#endif
        if (stride < pEncInfo->openParam.picWidth*2) {
            return JPG_RET_INVALID_PARAM;
        }
        if (stride < pEncInfo->openParam.picWidth*3) {
            return JPG_RET_INVALID_PARAM;
        }
    }

    return JPG_RET_SUCCESS;
}


int JpgEncGenHuffTab(JpgEncInfo * pEncInfo, int tabNum)
{
    int p, i, l, lastp, si, maxsymbol;

    int huffsize[256] = {0,};
    int huffcode[256] = {0,};
    int code;

    BYTE *bitleng, *huffval;
    unsigned int *ehufco, *ehufsi;

    bitleng = pEncInfo->pHuffBits[tabNum];
    huffval = pEncInfo->pHuffVal[tabNum];
    ehufco  = pEncInfo->huffCode[tabNum];
    ehufsi  = pEncInfo->huffSize[tabNum];

    maxsymbol = tabNum & 1 ? 256 : 16;

    /* Figure C.1: make table of Huffman code length for each symbol */

    p = 0;
    for (l=1; l<=16; l++) {
        i = bitleng[l-1];
        if (i < 0 || p + i > maxsymbol)
            return 0;
        while (i--)
            huffsize[p++] = l;
    }
    lastp = p;

    /* Figure C.2: generate the codes themselves */
    /* We also validate that the counts represent a legal Huffman code tree. */

    code = 0;
    si = huffsize[0];
    p = 0;
    while(huffsize[p] != 0){
        while (huffsize[p] == si) {
            huffcode[p++] = code;
            code++;
        }
        if (code >= (1 << si))
            return 0;
        code <<= 1;
        si++;
    }

    /* Figure C.3: generate encoding tables */
    /* These are code and size indexed by symbol value */

    for(i=0; i<256; i++)
        ehufsi[i] = 0x00;

    for(i=0; i<256; i++)
        ehufco[i] = 0x00;

    for (p=0; p<lastp; p++) {
        i = huffval[p];
        if (i < 0 || i >= maxsymbol || ehufsi[i])
            return 0;
        ehufco[i] = huffcode[p];
        ehufsi[i] = huffsize[p];
    }

    return 1;
}

int JpgEncLoadHuffTab(JpgEncInfo * pEncInfo)
{
    int i, j, t;
    int huffData;

    for (i=0; i<4; i++)
        JpgEncGenHuffTab(pEncInfo, i);

    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x3);

    for (j=0; j<4; j++)
    {

        t = (j==0) ? AC_TABLE_INDEX0 : (j==1) ? AC_TABLE_INDEX1 : (j==2) ? DC_TABLE_INDEX0 : DC_TABLE_INDEX1;

        for (i=0; i<256; i++)
        {
            if ((t==DC_TABLE_INDEX0 || t==DC_TABLE_INDEX1) && (i>15))   // DC
                break;

            if ((pEncInfo->huffSize[t][i] == 0) && (pEncInfo->huffCode[t][i] == 0))
                huffData = 0;
            else
            {
                huffData =                    (pEncInfo->huffSize[t][i] - 1);   // Code length (1 ~ 16), 4-bit
                huffData = (huffData << 16) | (pEncInfo->huffCode[t][i]    );   // Code word, 16-bit
            }
            JpuWriteReg(MJPEG_HUFF_DATA_REG, huffData);
        }
    }
    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x0);
    return 1;
}

int JpgEncLoadQMatTab(JpgEncInfo * pEncInfo)
{
#ifdef WIN32
    __int64 dividend = 0x80000;
    __int64 quotient;
#else
    long long int dividend = 0x80000;
    long long int quotient;
#endif
    int comp;
    int i, t;

    for (comp=0; comp<3; comp++) {
        int quantID = pEncInfo->pCInfoTab[comp][3];
        if (quantID >= 4)
            return 0;
        t = (comp==0)? Q_COMPONENT0 :
            (comp==1)? Q_COMPONENT1 : Q_COMPONENT2;
        JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x3 + t);
        for (i=0; i<64; i++) {
            int divisor = pEncInfo->pQMatTab[quantID][i];
            quotient = dividend / divisor;
            // enhace bit precision & rounding Q
            JpuWriteReg(MJPEG_QMAT_DATA_REG, (int)(divisor<<20)|(int)(quotient & 0xFFFFF));
        }
        JpuWriteReg(MJPEG_QMAT_CTRL_REG, t);
    }

    return 1;
}


int JpgEncEncodeHeader(JpgEncHandle handle, JpgEncParamSet * para)
{
    JpgInst *pJpgInst;
    JpgEncInfo *pEncInfo;
    int i;
#if !defined(LIBBMJPULITE)
    BYTE *pCInfoTab[4];
    int frameFormat;
#endif
    pJpgInst = handle;
    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    if (!para)
        return 0;

    // SOI Header
    JpuWriteReg( MJPEG_GBU_PBIT_16_REG, 0xFFD8);

    // DRI header
    if (pEncInfo->rstIntval) {
        JpuWriteReg( MJPEG_GBU_PBIT_32_REG, 0xFFDD0004);
        JpuWriteReg( MJPEG_GBU_PBIT_16_REG, pEncInfo->rstIntval);
    }

    // DQT Header
    JpuWriteReg( MJPEG_GBU_PBIT_16_REG, 0xFFDB);

    if (para->quantMode == JPG_TBL_NORMAL) {
        JpuWriteReg( MJPEG_GBU_PBIT_24_REG, 0x004300);

        for (i=0; i< 64; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pQMatTab[0][i    ]<<24 |
                pEncInfo->pQMatTab[0][i + 1]<<16 |
                pEncInfo->pQMatTab[0][i + 2]<<8 |
                pEncInfo->pQMatTab[0][i + 3]);
        }

        if (pEncInfo->format != FORMAT_400) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG, 0xFFDB0043);
            JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x01);

            for (i=0; i< 64; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pQMatTab[1][i    ]<<24 |
                    pEncInfo->pQMatTab[1][i + 1]<<16 |
                    pEncInfo->pQMatTab[1][i + 2]<<8 |
                    pEncInfo->pQMatTab[1][i + 3]);
            }
        }
    } else { // if (para->quantMode == JPG_TBL_MERGE)
        if (pEncInfo->format != FORMAT_400)
            JpuWriteReg( MJPEG_GBU_PBIT_24_REG, 0x008400);
        else
            JpuWriteReg( MJPEG_GBU_PBIT_24_REG, 0x004300);

        for (i=0; i< 64; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pQMatTab[0][i    ]<<24 |
                pEncInfo->pQMatTab[0][i + 1]<<16 |
                pEncInfo->pQMatTab[0][i + 2]<<8 |
                pEncInfo->pQMatTab[0][i + 3]);
        }

        if (pEncInfo->format != FORMAT_400) {
            JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x01);
            for (i=0; i< 64; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pQMatTab[1][i    ]<<24 |
                    pEncInfo->pQMatTab[1][i + 1]<<16 |
                    pEncInfo->pQMatTab[1][i + 2]<<8 |
                    pEncInfo->pQMatTab[1][i + 3]);
            }
        }
    }

    // DHT Header
    JpuWriteReg( MJPEG_GBU_PBIT_16_REG, 0xFFC4);

    if (para->huffMode == JPG_TBL_NORMAL) {
        JpuWriteReg( MJPEG_GBU_PBIT_24_REG, 0x001F00);

        for (i=0; i< 16; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffBits[0][i    ]<<24 |
                pEncInfo->pHuffBits[0][i + 1]<<16 |
                pEncInfo->pHuffBits[0][i + 2]<<8 |
                pEncInfo->pHuffBits[0][i + 3]);
        }

        for (i=0; i< 12; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffVal[0][i    ]<<24 |
                pEncInfo->pHuffVal[0][i + 1]<<16 |
                pEncInfo->pHuffVal[0][i + 2]<<8 |
                pEncInfo->pHuffVal[0][i + 3]);
        }

        JpuWriteReg( MJPEG_GBU_PBIT_32_REG, 0xFFC400B5);
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x10);

        for (i=0; i< 16; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffBits[1][i    ]<<24 |
                pEncInfo->pHuffBits[1][i + 1]<<16 |
                pEncInfo->pHuffBits[1][i + 2]<<8 |
                pEncInfo->pHuffBits[1][i + 3]);
        }


        for (i=0; i< 160; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffVal[1][i    ]<<24 |
                pEncInfo->pHuffVal[1][i + 1]<<16 |
                pEncInfo->pHuffVal[1][i + 2]<<8 |
                pEncInfo->pHuffVal[1][i + 3]);
        }

        JpuWriteReg( MJPEG_GBU_PBIT_16_REG,
            pEncInfo->pHuffVal[1][160]<<8 |
            pEncInfo->pHuffVal[1][161]);


        if (pEncInfo->format != FORMAT_400) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG, 0xFFC4001F);
            JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x01);

            for (i=0; i< 16; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffBits[2][i    ]<<24 |
                    pEncInfo->pHuffBits[2][i + 1]<<16 |
                    pEncInfo->pHuffBits[2][i + 2]<<8 |
                    pEncInfo->pHuffBits[2][i + 3]);
            }

            for (i=0; i< 12; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffVal[2][i    ]<<24 |
                    pEncInfo->pHuffVal[2][i + 1]<<16 |
                    pEncInfo->pHuffVal[2][i + 2]<<8 |
                    pEncInfo->pHuffVal[2][i + 3]);
            }


            JpuWriteReg( MJPEG_GBU_PBIT_32_REG, 0xFFC400B5);
            JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x11);

            for (i=0; i< 16; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffBits[3][i    ]<<24 |
                    pEncInfo->pHuffBits[3][i + 1]<<16 |
                    pEncInfo->pHuffBits[3][i + 2]<<8 |
                    pEncInfo->pHuffBits[3][i + 3]);
            }


            for (i=0; i< 160; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffVal[3][i    ]<<24 |
                    pEncInfo->pHuffVal[3][i + 1]<<16 |
                    pEncInfo->pHuffVal[3][i + 2]<<8 |
                    pEncInfo->pHuffVal[3][i + 3]);
            }

            JpuWriteReg( MJPEG_GBU_PBIT_16_REG,
                pEncInfo->pHuffVal[3][160]<<8 |
                pEncInfo->pHuffVal[3][161]);
        }
    } else { // if (para->huffMode == JPG_TBL_MERGE)
        if (pEncInfo->format != FORMAT_400)
            JpuWriteReg( MJPEG_GBU_PBIT_24_REG, 0x01A200);
        else
            JpuWriteReg( MJPEG_GBU_PBIT_24_REG, 0x00D400);

        for (i=0; i< 16; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffBits[0][i    ]<<24 |
                pEncInfo->pHuffBits[0][i + 1]<<16 |
                pEncInfo->pHuffBits[0][i + 2]<<8 |
                pEncInfo->pHuffBits[0][i + 3]);
        }

        for (i=0; i< 12; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffVal[0][i    ]<<24 |
                pEncInfo->pHuffVal[0][i + 1]<<16 |
                pEncInfo->pHuffVal[0][i + 2]<<8 |
                pEncInfo->pHuffVal[0][i + 3]);
        }

        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x10);

        for (i=0; i< 16; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffBits[1][i    ]<<24 |
                pEncInfo->pHuffBits[1][i + 1]<<16 |
                pEncInfo->pHuffBits[1][i + 2]<<8 |
                pEncInfo->pHuffBits[1][i + 3]);
        }


        for (i=0; i< 160; i+=4) {
            JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                pEncInfo->pHuffVal[1][i    ]<<24 |
                pEncInfo->pHuffVal[1][i + 1]<<16 |
                pEncInfo->pHuffVal[1][i + 2]<<8 |
                pEncInfo->pHuffVal[1][i + 3]);
        }

        JpuWriteReg( MJPEG_GBU_PBIT_16_REG,
            pEncInfo->pHuffVal[1][160]<<8 |
            pEncInfo->pHuffVal[1][161]);


        if (pEncInfo->format != FORMAT_400) {
            JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x01);

            for (i=0; i< 16; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffBits[2][i    ]<<24 |
                    pEncInfo->pHuffBits[2][i + 1]<<16 |
                    pEncInfo->pHuffBits[2][i + 2]<<8 |
                    pEncInfo->pHuffBits[2][i + 3]);
            }

            for (i=0; i< 12; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffVal[2][i    ]<<24 |
                    pEncInfo->pHuffVal[2][i + 1]<<16 |
                    pEncInfo->pHuffVal[2][i + 2]<<8 |
                    pEncInfo->pHuffVal[2][i + 3]);
            }

            JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x11);

            for (i=0; i< 16; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffBits[3][i    ]<<24 |
                    pEncInfo->pHuffBits[3][i + 1]<<16 |
                    pEncInfo->pHuffBits[3][i + 2]<<8 |
                    pEncInfo->pHuffBits[3][i + 3]);
            }


            for (i=0; i< 160; i+=4) {
                JpuWriteReg( MJPEG_GBU_PBIT_32_REG,
                    pEncInfo->pHuffVal[3][i    ]<<24 |
                    pEncInfo->pHuffVal[3][i + 1]<<16 |
                    pEncInfo->pHuffVal[3][i + 2]<<8 |
                    pEncInfo->pHuffVal[3][i + 3]);
            }

            JpuWriteReg( MJPEG_GBU_PBIT_16_REG,
                pEncInfo->pHuffVal[3][160]<<8 |
                pEncInfo->pHuffVal[3][161]);
        }
    }

    // SOF header
    JpuWriteReg( MJPEG_GBU_PBIT_16_REG, 0xFFC0);
    JpuWriteReg( MJPEG_GBU_PBIT_16_REG, (8+(pEncInfo->compNum*3)) );
    JpuWriteReg( MJPEG_GBU_PBIT_08_REG, 0x08);


    if(pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270)
        JpuWriteReg( MJPEG_GBU_PBIT_32_REG, pEncInfo->picWidth << 16 | pEncInfo->picHeight);
    else
        JpuWriteReg( MJPEG_GBU_PBIT_32_REG, pEncInfo->picHeight << 16 | pEncInfo->picWidth);

    JpuWriteReg( MJPEG_GBU_PBIT_08_REG, pEncInfo->compNum);

#if defined(LIBBMJPULITE)
    for (i=0; i<pEncInfo->compNum; i++) {
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, (i+1));
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, ((pEncInfo->pCInfoTab[i][1]<<4) & 0xF0) + (pEncInfo->pCInfoTab[i][2] & 0x0F));
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, pEncInfo->pCInfoTab[i][3]);
    }
#else
    frameFormat = pEncInfo->format;
    if (pEncInfo->rotationEnable && (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270)) {
        frameFormat = (frameFormat==FORMAT_422) ? FORMAT_224 : (frameFormat==FORMAT_224) ? FORMAT_422 : frameFormat;
    }

    pCInfoTab[0] = sJpuCompInfoTable[frameFormat];
    pCInfoTab[1] = pCInfoTab[0] + 6;
    pCInfoTab[2] = pCInfoTab[1] + 6;
    pCInfoTab[3] = pCInfoTab[2] + 6;

    for (i=0; i<pEncInfo->compNum; i++) {
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, (i+1));
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, ((pCInfoTab[i][1]<<4) & 0xF0) + (pCInfoTab[i][2] & 0x0F));
        JpuWriteReg( MJPEG_GBU_PBIT_08_REG, pCInfoTab[i][3]);
    }
#endif

    pEncInfo->frameIdx++;

    return 1;
}

JpgRet JpgEnterLock()
{
    jdi_lock();
    JpgSetClockGate(1);
    return JPG_RET_SUCCESS;
}
JpgRet JpgLeaveLock()
{
    JpgSetClockGate(0);
    jdi_unlock();
    return JPG_RET_SUCCESS;
}

JpgRet JpgSetClockGate(Uint32 on)
{
#if 0
    JpgInst *inst;
    jpu_instance_pool_t *jip;

    jip = (jpu_instance_pool_t *)jdi_get_instance_pool();
    if (!jip)
        return JPG_RET_FAILURE;


    inst = jip->pendingInst;
    if(inst && !on)
        return JPG_RET_SUCCESS;

    jdi_set_clock_gate(on);

    return JPG_RET_SUCCESS;
#else
    return JPG_RET_SUCCESS;
#endif
}

void SetJpgPendingInst(JpgInst *inst)
{
#if 0
    jpu_instance_pool_t *jip = jdi_get_instance_pool(inst->device_index);
    if (!jip)
        return;

    jip->pendingInst = inst;
#endif
}
void ClearJpgPendingInst()
{
#if 0
    jpu_instance_pool_t *jip = jdi_get_instance_pool();
    if (!jip)
        return;

    if (jip->pendingInst)
        jip->pendingInst = NULL;
#endif
}
JpgInst *GetJpgPendingInst()
{
#if 0
    jpu_instance_pool_t *jip = jdi_get_instance_pool(inst->device_index);
    if (!jip)
        return NULL;

    return jip->pendingInst;
#else
    return NULL;
#endif
}

#if defined(FLOCK_PROCESS)
int open_flock()
{
    if (jpuproc_fd)
        return JPG_RET_SUCCESS;

    /* create or open a share file for flock */
    jpuproc_fd = open(flock_process_file, O_RDWR|O_APPEND|O_CREAT,
                      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (0 == jpuproc_fd)
    {
        JLOG(ERR, "can't create flock file!");
        return JPG_RET_FAILURE;
    }

    return JPG_RET_SUCCESS;
}
int close_flock()
{
    /* release file handle and release flock */
    if (0 == jpuproc_fd)
        return JPG_RET_SUCCESS;

    close(jpuproc_fd);
    jpuproc_fd = 0;
    return JPG_RET_SUCCESS;
}

static JpgRet JpgEnterLock2()
{
    int ret;
    if(0 == jpuproc_fd)
    {
        ret = open_flock();
        if (ret != 0)
        {
            JLOG(ERR, "Invalid flock file handle!id=%lu", pthread_self());
            return JPG_RET_FAILURE;
        }
        JLOG(INFO, "open flock. id=%lu\n", pthread_self());
    }
    ret = flock(jpuproc_fd, LOCK_EX);
    if(0 != ret)
    {
        JLOG(ERR, "flock failed!! id=%lu\n", pthread_self());
        return JPG_RET_FAILURE;
    }
    return JPG_RET_SUCCESS;
}

static JpgRet JpgLeaveLock2()
{
    int ret;
    if(0 == jpuproc_fd)
    {
        ret = open_flock();
        if (ret != 0)
        {
            JLOG(ERR, "Invalid flock file handle! id=%lu", pthread_self());
            return JPG_RET_FAILURE;
        }
    }
    ret = flock(jpuproc_fd, LOCK_UN);
    if(0 != ret)
    {
        JLOG(ERR, "release flock fail! id=%lu", pthread_self());
        return JPG_RET_FAILURE;
    }
    return JPG_RET_SUCCESS;
}

void *JpuProcessEnterLock()
{
    int ret = JpgEnterLock2();
    if (ret != 0)
        return NULL;
    return (void *)((long long)jpuproc_fd);
}
void JpuProcessLeaveLock(void * c)
{
    JpgLeaveLock2();
}
#endif /* FLOCK_PROCESS */

