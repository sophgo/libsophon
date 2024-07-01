
#include "jpu_types.h"
#include "jpu_lib.h"
#include "jpu_logging.h"

#include "../jpuapi/jpuapifunc.h"
#include "../jpuapi/jpuapi.h"

int jpu_EncOpen(EncHandle* pHandle, EncOpenParam* p_par)
{
    return JPU_EncOpen(pHandle, p_par);
}

int jpu_EncClose(EncHandle handle)
{
    return JPU_EncClose(handle);
}

int jpu_EncGetInitialInfo(EncHandle handle, EncInitialInfo* p_info)
{
    return JPU_EncGetInitialInfo(handle, p_info);
}

int jpu_EncStartOneFrame(EncHandle handle, EncParam* param)
{
    return JPU_EncStartOneFrame(handle, param);
}

int jpu_EncGetOutputInfo(EncHandle handle, EncOutputInfo* info)
{
    return JPU_EncGetOutputInfo(handle, info);
}

int jpu_EncGiveCommand(EncHandle handle, CodecCommand cmd, void* p_par)
{
    return JPU_EncGiveCommand(handle, cmd, p_par);
}

int jpu_EncEncodeHeader(EncHandle handle, EncParamSet* p_par)
{
    return JpgEncEncodeHeader(handle, p_par);
}

int jpu_EncCompleteStop(EncHandle handle)
{
    return JPU_EncCompleteStop(handle);
}

/**
 * return value:
 *  1: timeout happened
 *  0: exit normally
 */
int jpu_EncWaitForInt(EncHandle handle, int timeout_in_ms, int timeout_counts)
{
    int timeout = 0;
    int cnt = 0;
    while (1) {
        /* Wait for encode interrupt */
        Uint32  coreIndex = JPU_GetCoreIndex(handle);
        int  deviceIndex = JPU_GetDeviceIndex(handle);
        uint32_t intr = JPU_WaitInterrupt(deviceIndex, coreIndex, timeout_in_ms);

        if (intr == (uint32_t)-1) {

            cnt++;
            printf("WARNING: Encoder timeout happened: %d/%d\n", cnt, timeout_counts);
            JPUPrintReg();

            if (cnt < timeout_counts)
                continue;

            SetJpgPendingInst(0);
            JpgLeaveLock();
            timeout = 1;
            break;
        }

        JPU_DEBUG("intr=%x, busy=%d", intr, jpu_IsBusy());

        /* INT_JPU_DONE interrupt should be checked  before FULL interrupt */
        if (intr & (1<<INT_JPU_DONE)) {
            /* Do not clear INT_JPU_DONE.
             * these will be cleared in JPU_EncGetOutputInfo. */
            break;
        }

        if (intr & (1<<INT_JPU_BIT_BUF_FULL)) {
            JpuWriteReg(MJPEG_PIC_START_REG, 1<< JPG_START_STOP);
            JPU_ERROR("Error: bitstream buffer is full?!");
            JPU_ClrStatus((1<<INT_JPU_BIT_BUF_FULL));
         }

        /* Expect this interrupt after stop is enabled. */
        if (intr & (1<<INT_JPU_BIT_BUF_STOP)) {
            int ret = JPU_EncCompleteStop(handle);
            if (ret != JPG_RET_SUCCESS) {
                JPU_ERROR("Failed to call jpu_EncCompleteStop(). Error code: 0x%x.", ret);
                // TODO
            }

            JpuWriteReg(MJPEG_GBU_BBSR_REG, 0);
            JpuWriteReg(MJPEG_GBU_BBER_REG, 0);
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
