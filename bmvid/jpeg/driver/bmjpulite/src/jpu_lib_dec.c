
#include "jpu_types.h"
#include "jpu_lib.h"
#include "jpu_logging.h"

#include "../jpuapi/jpuapifunc.h"
#include "../jpuapi/jpuapi.h"

int jpu_DecOpen(DecHandle* pHandle, DecOpenParam* p_par)
{
    return JPU_DecOpen(pHandle, p_par);
}

int jpu_DecClose(DecHandle handle)
{
    return JPU_DecClose(handle);
}

int jpu_DecGetInitialInfo(DecHandle handle, DecInitialInfo* info)
{
    return JPU_DecGetInitialInfo(handle, info);
}

int jpu_DecRegisterFrameBuffer(DecHandle handle,
                               FrameBuffer* bufArray, int num, int stride,
                               void* p_par0)
{
    return JPU_DecRegisterFrameBuffer(handle, bufArray, num, stride);
}

int jpu_DecSetResolutionInfo(DecHandle handle, int width, int height)
{
    return JPU_DecSetResolutionInfo(handle, width, height);
}

int jpu_DecSetRdPtrEx(DecHandle handle, PhysicalAddress addr, BOOL updateWrPtr)
{
    return JPU_DecSetRdPtrEx(handle, addr, updateWrPtr);
}

// TODO size: uint32_t or int
int jpu_DecUpdateBitstreamBuffer(DecHandle handle, uint32_t size)
{
    return JPU_DecUpdateBitstreamBuffer(handle, size);
}

int jpu_DecStartOneFrame(DecHandle handle, DecParam* param)
{
    return JPU_DecStartOneFrame(handle, param);
}

int jpu_DecGetOutputInfo(DecHandle handle, DecOutputInfo* info)
{
    return JPU_DecGetOutputInfo(handle, info);
}

int jpu_DecGetRotateInfo(DecHandle handle, int *rotationEnable, int *rotationAngle, int *mirrorEnable, int *mirrorDirection)
{
    return JPU_DecGetRotateInfo(handle, rotationEnable, rotationAngle, mirrorEnable, mirrorDirection);
}
int jpu_DecGiveCommand(DecHandle handle, CodecCommand cmd, void* p_par)
{
    return JPU_DecGiveCommand(handle, cmd, p_par);
}

int jpu_DecWaitForInt(DecHandle handle, int timeout_in_ms, int timeout_counts)
{
    int timeout = 0;
    int cnt = 0;

    while (1) {
        /* Wait for decode interrupt */
        Uint32  coreIndex = JPU_GetCoreIndex(handle);
        int  deviceIndex = JPU_GetDeviceIndex(handle);
        uint32_t intr = JPU_WaitInterrupt(deviceIndex, coreIndex, timeout_in_ms);

        if (intr == (uint32_t)-1) {
            cnt++;
            printf("WARNING : Decoder timeout happened %d/%d\n", cnt, timeout_counts);
            JPUPrintReg();

            if (cnt < timeout_counts)
                continue;

            SetJpgPendingInst(0);
            JpgLeaveLock();
            timeout = 1;
            break;
        }

        JPU_DEBUG("intr=%x, busy=%d", intr, jpu_IsBusy());

        /* Must catch INT_JPU_DONE interrupt before catching EMPTY interrupt */
        if ((intr & (1<<INT_JPU_DONE)) || (intr & (1<<INT_JPU_ERROR))) {
            // Do no clear INT_JPU_DONE and INT_JPU_ERROR interrupt.
            // these will be cleared in JPU_DecGetOutputInfo.
            break;
        }

        if (intr & (1<<INT_JPU_BIT_BUF_EMPTY)) {
            JPU_WARNING("info: bitstream buffer is empty?!");
            JPU_ClrStatus((1<<INT_JPU_BIT_BUF_EMPTY));
        }

        if (intr & (1<<INT_JPU_BIT_BUF_STOP)) {
            int ret = JPU_DecCompleteStop(handle);
            if( ret != JPG_RET_SUCCESS ) {
                JPU_ERROR("JPU_DecCompleteStop failed Error code is 0x%x \n", ret );
                // TODO
            }

            JPU_ClrStatus((1<<INT_JPU_BIT_BUF_STOP));
            break;
        }

        if (intr & (1<<INT_JPU_PARIAL_OVERFLOW)) {
            JPU_DEBUG("Error: frame buffer parial_overflow?");
            JPU_ClrStatus((1<<INT_JPU_PARIAL_OVERFLOW));
        }
    }

    return timeout;
}
