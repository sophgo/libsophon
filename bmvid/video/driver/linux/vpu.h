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

#include <linux/fs.h>
#include <linux/types.h>

//#define SUPPORT_TIMEOUT_RESOLUTION
#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

#define VDI_IOCTL_MAGIC  'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY       _IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY            _IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT                 _IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE                 _IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET                          _IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL              _IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY              _IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE                  _IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE                 _IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM               _IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO              _IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_GET_FREE_MEM_SIZE              _IO(VDI_IOCTL_MAGIC, 13)
#define VDI_IOCTL_GET_FIRMWARE_STATUS            _IO(VDI_IOCTL_MAGIC, 14)
#ifdef BM_PCIE_MODE
#define VDI_IOCTL_WRITE_VMEM                     _IO(VDI_IOCTL_MAGIC, 20)
#define VDI_IOCTL_READ_VMEM                      _IO(VDI_IOCTL_MAGIC, 21)
#endif
#if !defined(BM_PCIE_MODE) && !defined(BM_ION_MEM)
#define VDI_IOCTL_FLUSH_DCACHE                   _IO(VDI_IOCTL_MAGIC, 24)
#define VDI_IOCTL_INVALIDATE_DCACHE              _IO(VDI_IOCTL_MAGIC, 25)
#endif

#define VDI_IOCTL_SYSCXT_SET_STATUS              _IO(VDI_IOCTL_MAGIC, 26)
#define VDI_IOCTL_SYSCXT_GET_STATUS              _IO(VDI_IOCTL_MAGIC, 27)
#define VDI_IOCTL_SYSCXT_CHK_STATUS              _IO(VDI_IOCTL_MAGIC, 28)
#define VDI_IOCTL_SYSCXT_SET_EN                  _IO(VDI_IOCTL_MAGIC, 29)
#define VDI_IOCTL_GET_VIDEO_CAP                  _IO(VDI_IOCTL_MAGIC, 30)
#define VDI_IOCTL_GET_BIN_TYPE                   _IO(VDI_IOCTL_MAGIC, 31)
#define VDI_IOCTL_GET_CHIP_ID                    _IO(VDI_IOCTL_MAGIC, 32)
#define VDI_IOCTL_GET_MAX_CORE_NUM               _IO(VDI_IOCTL_MAGIC, 33)
#define VDI_IOCTL_CTRL_KERNEL_RESET              _IO(VDI_IOCTL_MAGIC, 34)

typedef struct vpudrv_syscxt_info_s {
    unsigned int core_idx;
    unsigned int inst_idx;
    unsigned int core_status;
    unsigned int pos;
    unsigned int fun_en;

    unsigned int is_all_block;
    int is_sleep;
} vpudrv_syscxt_info_t;

typedef struct vpudrv_buffer_t {
    unsigned int  size;
    unsigned long phys_addr;
    unsigned long base;                            /* kernel logical address in use kernel */
    unsigned long virt_addr;                /* virtual user space address */

    unsigned int  core_idx;

#if !defined(BM_PCIE_MODE)
    int    enable_cache;
#endif

#ifdef BM_ION_MEM
    int    ion_fd;
    struct dma_buf_attachment *attach;
    struct sg_table *table;
    struct dma_buf *dma_buf;
#endif
} vpudrv_buffer_t;

typedef struct vpu_bit_firmware_info_t {
    unsigned int size;                        /* size of this structure*/
    unsigned int core_idx;
    unsigned long reg_base_offset;
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
    unsigned long reg_base;
    unsigned int value[4];
    unsigned int mask[4];
} vpudrv_regrw_info_t;

typedef struct {
    int core_idx;
    pid_t reset_core_disable;
} vpudrv_reset_flag;

#ifdef BM_PCIE_MODE
typedef struct vpu_video_mem_op_t {
    unsigned int size;                        /* size of this structure*/
    unsigned long src;
    unsigned long dst;
} vpu_video_mem_op_t;
#endif

#define DUMP_FLAG_MEM_SIZE (MAX_NUM_VPU_CORE*MAX_NUM_INSTANCE*sizeof(unsigned int)*5 + MAX_NUM_VPU_CORE*2*sizeof(unsigned int))

#define VPU_SECURITY_REGISTER_SIZE 0x300
#define SECURITY_SETTINGS_REG 0x4
#define SECURITY_INTERRUPT_STATUS_REG 0x10
#define SECURITY_INTERRUPT_CLEAR_REG 0x14
#define SECURITY_SPACE3_START_REG 0x38
#define SECURITY_SPACE3_END_REG 0x3C
#define SECURITY_SPACE4_START_REG 0x40
#define SECURITY_SPACE4_END_REG 0x44

#endif

