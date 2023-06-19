//--=========================================================================--
//  This file is linux device driver for VPU.
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2015  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#ifdef __linux__
#include <linux/fs.h>
#include <linux/types.h>
#endif
#include "windatatype.h"

//#define SUPPORT_TIMEOUT_RESOLUTION
#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY


#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x300, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_FREE_PHYSICALMEMORY            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x301, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WAIT_INTERRUPT                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x302, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SET_CLOCK_GATE                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x303, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_RESET                          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x304, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_INSTANCE_POOL              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x305, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_COMMON_MEMORY              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x306, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x308, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_OPEN_INSTANCE                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x309, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_CLOSE_INSTANCE                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_INSTANCE_NUM               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_REGISTER_INFO              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_FREE_MEM_SIZE              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_FIRMWARE_STATUS            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30e, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_VMEM                     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30f, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_READ_VMEM                      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x310, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_SET_STATUS              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x311, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_GET_STATUS              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x312, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_CHK_STATUS              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x313, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_SET_EN                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x314, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*#define VDI_IOCTL_READ_HPI_REGISTER              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x315, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_FIRMWARE_REGISTER        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x316, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_REGRW_REGISTER           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x317, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_MAP_PHYSICAL_MEMORY            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x318, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_UNMAP_PHYSICALMEMORY           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x319, METHOD_BUFFERED, FILE_ANY_ACCESS)*/

#define VDI_IOCTL_READ_HPI_REGISTER              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x315, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_HPI_FIRMWARE_REGISTER    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x316, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_HPI_REGRW_REGISTER       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x317, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_READ_REGISTER                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x318, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRIET_REGISTER                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x319, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_MMAP_INSTANCEPOOL              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_MUNMAP_INSTANCEPOOL            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_CHIP_ID                    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_MAX_CORE_NUM               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_CTRL_KERNEL_RESET              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31e, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct vpudrv_syscxt_info_s {
    unsigned int core_idx;
    unsigned int inst_idx;
    unsigned int core_status;
    unsigned int pos;
    unsigned int fun_en;

    unsigned int is_all_block;
    int is_sleep;
} vpudrv_syscxt_info_t;

#if 0
typedef struct vpudrv_buffer_t {
    unsigned int  size;
    u64 phys_addr;
    u64 base;                            /* kernel logical address in use kernel */
    u64 virt_addr;                /* virtual user space address */

    unsigned int  core_idx;

#if !defined(BM_PCIE_MODE)
    int    enable_cache;
#endif

#ifdef BM_ION_MEM
    int    ion_fd;
    struct dma_buf_attachment *attach;
#ifdef linux__
    struct sg_table *table;
    struct dma_buf *dma_buf;
#endif
#endif
} vpudrv_buffer_t;

#endif

typedef struct vpudrv_buffer_t {
    u32           size;
    u64           phys_addr;
    u64           base;      /* kernel logical address in use kernel */
    u64           virt_addr; /* virtual user space address */
    u32          core_idx;
    u32 reserved;
} vpudrv_buffer_t;

typedef struct vpu_register_info_t {
    u32 core_idx;
    u32 address_offset;
    u32 data;
} vpu_register_info_t;

typedef struct vpu_bit_firmware_info_t {
    unsigned int size;                        /* size of this structure*/
    unsigned int core_idx;
    u64 reg_base_offset;
    unsigned short bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpudrv_inst_info_t {
    unsigned int core_idx;
    unsigned int inst_idx;
    int inst_open_count;    /* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
    unsigned int timeout;
    int            intr_reason;
#ifdef SUPPORT_MULTI_INST_INTR
    int            intr_inst_index;
#endif
    int         core_idx;
} vpudrv_intr_info_t;

typedef struct vpudrv_regrw_info_t {
    unsigned int size;
    u64 reg_base;
    unsigned int value[4];
    unsigned int mask[4];
} vpudrv_regrw_info_t;

typedef struct {
    int core_idx;
    u32 reset_core_disable;
} vpudrv_reset_flag;

#ifdef BM_PCIE_MODE
typedef struct vpu_video_mem_op_t {
    unsigned int size;                        /* size of this structure*/
    u64 src;
    u64 dst;
} vpu_video_mem_op_t;
#endif

#define DUMP_FLAG_MEM_SIZE (MAX_NUM_VPU_CORE*MAX_NUM_INSTANCE*sizeof(unsigned int)*5 + MAX_NUM_VPU_CORE*2*sizeof(unsigned int))
#endif

