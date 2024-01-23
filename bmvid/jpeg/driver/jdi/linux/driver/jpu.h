//-----------------------------------------------------------------------------
// COPYRIGHT (C) 2020   CHIPS&MEDIA INC. ALL RIGHTS RESERVED
//
// This file is distributed under BSD 3 clause and LGPL2.1 (dual license)
// SPDX License Identifier: BSD-3-Clause
// SPDX License Identifier: LGPL-2.1-only
//
// The entire notice above must be reproduced on all authorized copies.
//
// Description  :
//-----------------------------------------------------------------------------

#ifndef __JPU_DRV_H__
#define __JPU_DRV_H__
#if !defined  _WIN32
#include <linux/fs.h>
#include <linux/types.h>
#endif

//#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY    //close to avoid mmap/unmmap issues, use dma memory for instance pool

#ifdef  _WIN32
#define JDI_IOCTL_WAIT_INTERRUPT               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x402, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_RESET                        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_GET_REGISTER_INFO            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x410, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_GET_INSTANCE_CORE_INDEX      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x412, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x413, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_WRITE_VMEM                   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x416, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_READ_VMEM                    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x417, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_WRITE_REGISTER               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x418, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_READ_REGISTER                CTL_CODE(FILE_DEVICE_UNKNOWN, 0x419, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_RESET_ALL                    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x420, METHOD_BUFFERED, FILE_ANY_ACCESS)
#else
#define JDI_IOCTL_MAGIC  'J'
#define JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY       _IO(JDI_IOCTL_MAGIC, 0)
#define JDI_IOCTL_FREE_PHYSICAL_MEMORY           _IO(JDI_IOCTL_MAGIC, 1)
#define JDI_IOCTL_WAIT_INTERRUPT                 _IO(JDI_IOCTL_MAGIC, 2)
#define JDI_IOCTL_SET_CLOCK_GATE                 _IO(JDI_IOCTL_MAGIC, 3)
#define JDI_IOCTL_RESET                          _IO(JDI_IOCTL_MAGIC, 4)
#define JDI_IOCTL_GET_INSTANCE_POOL              _IO(JDI_IOCTL_MAGIC, 5)
#define JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(JDI_IOCTL_MAGIC, 6)
#define JDI_IOCTL_OPEN_INSTANCE                  _IO(JDI_IOCTL_MAGIC, 7)
#define JDI_IOCTL_CLOSE_INSTANCE                 _IO(JDI_IOCTL_MAGIC, 8)
#define JDI_IOCTL_GET_INSTANCE_NUM               _IO(JDI_IOCTL_MAGIC, 9)
#define JDI_IOCTL_GET_REGISTER_INFO              _IO(JDI_IOCTL_MAGIC, 10)
#define JDI_IOCTL_GET_CONTROL_REG                _IO(JDI_IOCTL_MAGIC, 11)
#define JDI_IOCTL_GET_INSTANCE_CORE_INDEX        _IO(JDI_IOCTL_MAGIC, 12)
#define JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX      _IO(JDI_IOCTL_MAGIC, 13)
#define JDI_IOCTL_INVAL_DCACHE_RANGE             _IO(JDI_IOCTL_MAGIC, 14)
#define JDI_IOCTL_FLUSH_DCACHE_RANGE             _IO(JDI_IOCTL_MAGIC, 15)

#ifdef BM_PCIE_MODE
#define JDI_IOCTL_WRITE_VMEM                     _IO(JDI_IOCTL_MAGIC, 16)
#define JDI_IOCTL_READ_VMEM                      _IO(JDI_IOCTL_MAGIC, 17)
#define JDI_IOCTL_READ_REG                       _IO(JDI_IOCTL_MAGIC, 18)
#endif
#define JDI_IOCTL_GET_MAX_NUM_JPU_CORE           _IO(JDI_IOCTL_MAGIC, 19)
#define JDI_IOCTL_RESET_ALL                      _IO(JDI_IOCTL_MAGIC, 20)
#endif




typedef struct jpudrv_buffer_t {
    unsigned long long phys_addr;
    unsigned long long base;                            /* kernel logical address in use kernel */
    unsigned long long virt_addr;                /* virtual user space address */
    unsigned int  size;

    /* page prot flag. 0: default; 1: writecombine; 2: noncache */
    unsigned int  flags;
} jpudrv_buffer_t;

typedef struct jpudrv_clkgate_info_t {
    unsigned int clkgate;
    int          core_idx;
} jpudrv_clkgate_info_t;

typedef struct jpudrv_intr_info_t {
    unsigned int timeout;
    unsigned int core_idx;
} jpudrv_intr_info_t;

typedef struct jpudrv_remap_info_t {
    unsigned long long read_addr;
    unsigned long long write_addr;
    int core_idx;
} jpudrv_remap_info_t;

typedef struct dcache_range_t {
    unsigned long long start;
    unsigned long long size;
} dcache_range_t;

#define DUMP_FLAG_MEM_SIZE   128     // (MAX_NUM_JPU_CORE*sizeof(unsigned int))
#endif
