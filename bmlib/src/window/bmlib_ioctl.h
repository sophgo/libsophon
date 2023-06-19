#ifndef BMLIB_SRC_BMLIB_IOCTL_H_
#define BMLIB_SRC_BMLIB_IOCTL_H_
#ifdef __linux__
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#endif

/*
 * IOCTL definations, sync with driver/bm_uapi.h
 */
#define BMDEV_MEMCPY                                                           \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_ALLOC_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_FREE_GMEM                                                        \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TOTAL_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_AVAIL_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_REQUEST_ARM_RESERVED                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x14, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_RELEASE_ARM_RESERVED                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x15, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_MAP_GMEM                                                         \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x16, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_INVALIDATE_GMEM                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x17, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_FLUSH_GMEM                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x18, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_ALLOC_GMEM_ION                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x19, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GMEM_ADDR                                                        \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SEND_API                                                         \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_THREAD_SYNC_API                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x21, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_DEVICE_SYNC_API                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x23, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_HANDLE_SYNC_API                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x27, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SEND_API_EXT                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x28, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_QUERY_API_RESULT                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x29, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_MISC_INFO                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_UPDATE_FW_A9                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_PROFILE                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x32, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_PROGRAM_A53                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x33, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOOT_INFO                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x34, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_UPDATE_BOOT_INFO                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x35, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SN                                                               \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x36, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_MAC0                                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x37, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_MAC1                                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x38, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_BOARD_TYPE                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x39, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_PROGRAM_MCU                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_CHECKSUM_MCU                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_REG                                                          \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_REG                                                          \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DEV_STAT                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3e, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_ENABLE                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x40, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_DISABLE                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x41, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACEITEM_NUMBER                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x42, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_DUMP                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x43, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRACE_DUMP_ALL                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x44, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_ENABLE_PERF_MONITOR                                              \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x45, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_DISABLE_PERF_MONITOR                                             \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x46, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DEVICE_TIME                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x47, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_SET_TPU_DIVIDER                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x50, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_MODULE_RESET                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x51, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_TPU_FREQ                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x52, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_FREQ                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x53, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRIGGER_VPP                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRIGGER_SPACC                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x61, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SECKEY_VALID                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x62, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_EFUSE_WRITE                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x66, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_EFUSE_READ                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x67, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_BASE64_PREPARE                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x70, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_BASE64_START                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x71, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_BASE64_CODEC                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x72, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_I2C_READ_SLAVE                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x73, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_I2C_WRITE_SLAVE                                                  \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x74, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_HEAP_STAT_BYTE                                               \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x75, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_HEAP_NUM                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x76, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_I2C_SMBUS_ACCESS                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x77, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_SET_BMCPU_STATUS                                                 \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x7F, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_TRIGGER_BMCPU                                                    \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define BMDEV_GET_TPUC                                                         \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x81, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_MAXP                                                         \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x82, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOARDP                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x83, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_FAN                                                          \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x84, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_CORRECTN                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x85, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_12V_ATX                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x86, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_SN                                                           \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x87, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_STATUS                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x88, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_MINCLK                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x89, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_MAXCLK                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DRIVER_VERSION                                               \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOARD_TYPE                                                   \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_BOARDT                                                       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8D, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_CHIPT                                                        \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8E, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_P                                                        \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8F, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_TPU_V                                                        \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x90, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_CARD_ID                                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x91, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_GET_DYNFREQ_STATUS                                               \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x92, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMDEV_CHANGE_DYNFREQ_STATUS                                            \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x93, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_DEV_CNT                                                      \
    CTL_CODE(FILE_DEVICE_BEEP, 0x0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_SMI_ATTR                                                     \
    CTL_CODE(FILE_DEVICE_BEEP, 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_SET_LED                                                          \
    CTL_CODE(FILE_DEVICE_BEEP, 0x02, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_SET_ECC                                                          \
    CTL_CODE(FILE_DEVICE_BEEP, 0x03, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_PROC_GMEM                                                    \
    CTL_CODE(FILE_DEVICE_BEEP, 0x04, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_DEV_RECOVERY                                                     \
    CTL_CODE(FILE_DEVICE_BEEP, 0x05, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_DRIVER_VERSION                                               \
    CTL_CODE(FILE_DEVICE_BEEP, 0x06, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_CARD_INFO                                                    \
    CTL_CODE(FILE_DEVICE_BEEP, 0x07, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BMCTL_GET_CARD_NUM                                                    \
    CTL_CODE(FILE_DEVICE_BEEP, 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef SOC_MODE
struct ce_base {
    unsigned int alg;
    unsigned int enc;
    unsigned long long src;
    unsigned long long dst;
    unsigned long long len;
    unsigned long long dstlen;
};

#define CE_BASE_IOC_TYPE                ('C')
#define CE_BASE_IOC_IOR(nr, type)       _IOR(CE_BASE_IOC_TYPE, nr, type)
/* do one shot operation on physical address */
#define CE_BASE_IOC_OP_PHY      \
        CE_BASE_IOC_IOR(0, struct ce_base)
enum {
        CE_BASE_IOC_ALG_BASE64,
        CE_BASE_IOC_ALG_MAX,
};
#endif
#endif  // BMLIB_SRC_BMLIB_IOCTL_H_

