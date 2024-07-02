
#ifndef JPUAPI_H_INCLUDED
#define JPUAPI_H_INCLUDED

#include "jpuconfig.h"
#include "../jdi/jdi.h"
#include "jpu_lib.h"


//------------------------------------------------------------------------------
// common struct and definition
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// decode struct and definition
//------------------------------------------------------------------------------
typedef void*    JpgDecHandle;

typedef DecOpenParam   JpgDecOpenParam;
typedef DecInitialInfo JpgDecInitialInfo;
typedef DecParam       JpgDecParam;
typedef DecOutputInfo  JpgDecOutputInfo;



//------------------------------------------------------------------------------
// encode struct and definition
//------------------------------------------------------------------------------
typedef void* JpgEncHandle;


typedef EncOpenParam   JpgEncOpenParam;
typedef EncInitialInfo JpgEncInitialInfo;
typedef EncParam       JpgEncParam;
typedef EncOutputInfo  JpgEncOutputInfo;
typedef EncParamSet    JpgEncParamSet;

#ifdef __cplusplus
extern "C" {
#endif
    int    JPU_IsBusy();
    Uint32 JPU_GetStatus();
    void   JPU_ClrStatus(Uint32 val);
    Uint32 JPU_IsInit(void);

    Uint32 JPU_WaitInterrupt(int device_index, Uint32 coreIdx,int timeout);

    JpgRet JPU_Init(Uint32 device_index);
    void   JPU_DeInit(Uint32 device_index);
    JpgRet JPU_GetVersionInfo(Uint32 *versionInfo);

    // function for decode
    JpgRet JPU_DecOpen(JpgDecHandle *, JpgDecOpenParam *);
    JpgRet JPU_DecClose(JpgDecHandle);
    JpgRet JPU_DecGetInitialInfo(JpgDecHandle handle,
                                 JpgDecInitialInfo * info);

    JpgRet JPU_DecSetResolutionInfo(DecHandle handle, int width, int height);
    JpgRet JPU_DecSetRdPtrEx(JpgDecHandle handle, PhysicalAddress addr, BOOL updateWrPtr);

    JpgRet JPU_DecSetRdPtr(JpgDecHandle handle,
                           PhysicalAddress addr,
                           int updateWrPtr);

    JpgRet JPU_DecRegisterFrameBuffer(JpgDecHandle handle,
                                      FrameBuffer * bufArray,
                                      int num,
                                      int stride);
#if !defined(LIBBMJPULITE)
    JpgRet JPU_DecGetBitstreamBuffer(JpgDecHandle handle,
                                     PhysicalAddress * prdPrt,
                                     PhysicalAddress * pwrPtr,
                                     int * size );
#endif
    JpgRet JPU_DecUpdateBitstreamBuffer(JpgDecHandle handle,
                                        int size);
    JpgRet JPU_HWReset();
    JpgRet JPU_SWReset();
    JpgRet JPU_DecStartOneFrame(JpgDecHandle handle,
                                JpgDecParam *param );
    JpgRet JPU_DecGetOutputInfo(JpgDecHandle handle,
                                JpgDecOutputInfo * info);
    JpgRet JPU_DecIssueStop(JpgDecHandle handle);
    JpgRet JPU_DecCompleteStop(JpgDecHandle handle);
    JpgRet JPU_DecGiveCommand(JpgDecHandle handle,
                              JpgCommand cmd,
                              void * parameter);

    // function for encode
    JpgRet JPU_EncOpen(JpgEncHandle *, JpgEncOpenParam *);
    JpgRet JPU_EncClose(JpgEncHandle);
    JpgRet JPU_EncGetInitialInfo(JpgEncHandle, JpgEncInitialInfo *);
#if !defined(LIBBMJPULITE)
    JpgRet JPU_EncGetBitstreamBuffer(JpgEncHandle handle,
                                     PhysicalAddress * prdPrt,
                                     PhysicalAddress * pwrPtr,
                                     int * size);
    JpgRet JPU_EncUpdateBitstreamBuffer(JpgEncHandle handle,
                                        int size);
#endif
    JpgRet JPU_EncStartOneFrame(JpgEncHandle handle,
                                JpgEncParam * param );
    JpgRet JPU_EncGetOutputInfo(JpgEncHandle handle,
                                JpgEncOutputInfo * info);
    JpgRet JPU_EncIssueStop(JpgDecHandle handle);
    JpgRet JPU_EncCompleteStop(JpgDecHandle handle);

    JpgRet JPU_DecGetRotateInfo(JpgDecHandle handle, 
                                int *rotationEnable, 
                                int *rotationAngle, 
                                int *mirrorEnable, 
                                int *mirrorDirection);

    JpgRet JPU_EncGiveCommand(JpgEncHandle handle,
                              JpgCommand cmd,
                              void * parameter);
    void JPU_EncSetHostParaAddr(PhysicalAddress baseAddr,
                                PhysicalAddress paraAddr);
    Uint32 JPU_GetCoreIndex(void * handle);
    int JPU_GetDeviceIndex(void * handle);
    int JPU_GetDump();

    JpgRet JPU_DecSetBsPtr(JpgDecHandle  pHandle, uint8_t *data, int data_size);

    JpgRet VPP_Init(Uint32 device_index);

    void JPUPrintReg(void);

#ifdef __cplusplus
}
#endif

#endif

