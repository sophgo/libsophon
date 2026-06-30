#ifndef BMLIB_SRC_BMLIB_IOCTL_H_
#define BMLIB_SRC_BMLIB_IOCTL_H_
#include <sys/ioctl.h>
#include <asm/ioctl.h>

/*
 * IOCTL definations, sync with driver/bm_uapi.h
 */
#define BMDEV_MEMCPY                  _IOW('p', 0x00, unsigned long)
#define BMDEV_MEMCPY_P2P              _IOW('p', 0x01, unsigned long)

#define BMDEV_ALLOC_GMEM              _IOWR('p', 0x10, unsigned long)
#define BMDEV_FREE_GMEM               _IOW('p', 0x11, unsigned long)
#define BMDEV_TOTAL_GMEM              _IOWR('p', 0x12, unsigned long)
#define BMDEV_AVAIL_GMEM              _IOWR('p', 0x13, unsigned long)
#define BMDEV_REQUEST_ARM_RESERVED    _IOWR('p', 0x14, unsigned long)

#define BMDEV_MAP_GMEM                _IOWR('p', 0x16, unsigned long)  // not used, map in mmap
#define BMDEV_ALLOC_GMEM_ION          _IOW('p', 0x19, unsigned long)
#define BMDEV_GMEM_ADDR               _IOW('p', 0x1a, unsigned long)

#define BMDEV_SEND_API                _IOW('p', 0x20, unsigned long)
#define BMDEV_READ_API         _IOW('p', 0x21, unsigned long)
#define BMDEV_SEND_API_RESULT            _IOW('p', 0x28, unsigned long)
#define BMDEV_READ_API_RESULT        _IOW('p', 0x29, unsigned long)

#define BMDEV_GET_MISC_INFO           _IOWR('p', 0x30, unsigned long)
#define BMDEV_PROGRAM_A53             _IOWR('p', 0x33, unsigned long)
#define BMDEV_GET_BOOT_INFO           _IOWR('p', 0x34, unsigned long)
#define BMDEV_UPDATE_BOOT_INFO        _IOWR('p', 0x35, unsigned long)
#define BMDEV_SN                      _IOWR('p', 0x36, unsigned long)
#define BMDEV_MAC0                    _IOWR('p', 0x37, unsigned long)
#define BMDEV_MAC1                    _IOWR('p', 0x38, unsigned long)
#define BMDEV_BOARD_TYPE              _IOWR('p', 0x39, unsigned long)
#define BMDEV_PROGRAM_MCU             _IOWR('p', 0x3a, unsigned long)
#define BMDEV_CHECKSUM_MCU            _IOWR('p', 0x3b, unsigned long)
#define BMDEV_SET_REG                 _IOWR('p', 0x3c, unsigned long)
#define BMDEV_GET_REG                 _IOWR('p', 0x3d, unsigned long)
#define BMDEV_GET_DEV_STAT            _IOWR('p', 0x3e, unsigned long)

#define BMDEV_SET_TPU_DIVIDER         _IOWR('p', 0x50, unsigned long)
#define BMDEV_SET_MODULE_RESET        _IOWR('p', 0x51, unsigned long)

#define BMDEV_TRIGGER_VPP             _IOWR('p', 0x60, unsigned long)

#define BMDEV_BASE64_PREPARE          _IOWR('p', 0x70, unsigned long)
#define BMDEV_BASE64_START            _IOWR('p', 0x71, unsigned long)
#define BMDEV_BASE64_CODEC            _IOWR('p', 0x72, unsigned long)

#define BMDEV_I2C_READ_SLAVE          _IOWR('p', 0x73, unsigned long)
#define BMDEV_I2C_WRITE_SLAVE         _IOWR('p', 0x74, unsigned long)

#define BMDEV_GET_HEAP_STAT_BYTE      _IOWR('p', 0x75, unsigned long)
#define BMDEV_GET_HEAP_NUM            _IOWR('p', 0x76, unsigned long)

#define BMDEV_I2C_SMBUS_ACCESS        _IOWR('p', 0x77, unsigned long)

#define BMDEV_SETUP_VETH              _IOWR('p', 0xA0, unsigned long)
#define BMDEV_RMDRV_VETH              _IOWR('p', 0xA1, unsigned long)

#define BMDEV_GET_VETH_STATE          _IOWR('p', 0xA4, unsigned long)
#define BMDEV_COMM_READ               _IOWR('p', 0xA5, unsigned long)
#define BMDEV_COMM_WRITE              _IOWR('p', 0xA6, unsigned long)
#define BMDEV_COMM_READ_MSG           _IOWR('p', 0xA7, unsigned long)
#define BMDEV_COMM_WRITE_MSG          _IOWR('p', 0xA8, unsigned long)
#define BMDEV_COMM_CONNECT_STATE      _IOWR('p', 0xA9, unsigned long)
#define BMDEV_COMM_SET_CARDID         _IOWR('p', 0xAA, unsigned long)
#define BMDEV_FORCE_RESET_TPU         _IOWR('p', 0xAE, unsigned long)

#define BMDEV_GET_TPUC                _IOWR('p', 0x81, unsigned long)

#define BMDEV_GET_BOARDP              _IOWR('p', 0x83, unsigned long)
// #define BMDEV_GET_FAN                 _IOWR('p', 0x84, unsigned long)
#define BMDEV_GET_CORRECTN            _IOR('p', 0x85, unsigned long)
#define BMDEV_GET_12V_ATX             _IOR('p', 0x86, unsigned long)
#define BMDEV_GET_SN                  _IOR('p', 0x87, unsigned long)
#define BMDEV_GET_STATUS              _IOR('p', 0x88, unsigned long)
#define BMDEV_GET_TPU_MINCLK          _IOR('p', 0x89, unsigned long)
#define BMDEV_GET_TPU_MAXCLK          _IOR('p', 0x8A, unsigned long)
#define BMDEV_GET_DRIVER_VERSION      _IOR('p', 0x8B, unsigned long)
#define BMDEV_GET_BOARD_TYPE          _IOR('p', 0x8C, unsigned long)
#define BMDEV_GET_BOARDT              _IOR('p', 0x8D, unsigned long)
#define BMDEV_GET_CHIPT               _IOR('p', 0x8E, unsigned long)
#define BMDEV_GET_TPU_P               _IOR('p', 0x8F, unsigned long)
#define BMDEV_GET_TPU_V               _IOR('p', 0x90, unsigned long)
#define BMDEV_GET_CARD_ID             _IOR('p', 0x91, unsigned long)
#define BMDEV_GET_DYNFREQ_STATUS      _IOR('p', 0x92, unsigned long)
#define BMDEV_CHANGE_DYNFREQ_STATUS   _IOR('p', 0x93, unsigned long)
#define BMDEV_LOADED_LIB              _IOR('p', 0x95, unsigned long)
#define BMDEV_GET_SMI_ATTR            _IOR('p', 0x96, unsigned long)

#define BMDEV_SET_IOMAP_TPYE             _IOWR('p', 0x99, int32_t)
#define BMDEV_READL             _IOWR('p', 0x100, unsigned long)
#define BMDEV_STORE_BMCPU_APP_THREAD             _IOWR('p', 0x101, int32_t)
#define BMDEV_KILL_BMCPU_APP_THREAD             _IOWR('p', 0x102, long)
#define BMDEV_GET_BMCPU_APP_THREAD             _IOWR('p', 0x103, long)
#define BMDEV_MEMCPY_ASYNC            _IOW('p', 0x104, unsigned long)


#define BMCTL_GET_DEV_CNT             _IOR('q', 0x0, unsigned long)
#define BMCTL_GET_SMI_ATTR            _IOWR('q', 0x01, unsigned long)
#define BMDEV_GET_HEAP_INFO                   _IOWR('q', 0x02, unsigned long)
// #define BMCTL_SET_ECC                   _IOWR('q', 0x03, unsigned long)
#define BMCTL_GET_PROC_GMEM           _IOWR('q', 0x04, unsigned long)
#define BMCTL_DEV_RECOVERY            _IOWR('q', 0x05, unsigned long)
#define BMCTL_GET_DRIVER_VERSION      _IOR('q', 0x06, unsigned long)
#define BMCTL_GET_CARD_NUM            _IOR('q', 0x08, unsigned long)


#define CE_BASE_IOC_TYPE                ('C')
#define CE_BASE_IOC_IOR(nr, type)       _IOR(CE_BASE_IOC_TYPE, nr, type)

enum {
        CE_BASE_IOC_ALG_BASE64,
        CE_BASE_IOC_ALG_MAX,
};
#endif  // BMLIB_SRC_BMLIB_IOCTL_H_

