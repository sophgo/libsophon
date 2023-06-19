/*
 * Copyright (c) 2018, Chips&Media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VDI_H_
#define _VDI_H_

#include "bmvpu_types.h"
#include "vpuopt.h"

#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#define ATTRIBUTE 
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define ATTRIBUTE __attribute__((deprecated))
#define DECL_EXPORT
#define DECL_IMPORT
#endif
/************************************************************************/
/* COMMON REGISTERS                                                     */
/************************************************************************/
#define VPU_PRODUCT_NAME_REGISTER                 0x1040
#define VPU_PRODUCT_CODE_REGISTER                 0x1044

#define DDR_MAP_REGISTER                          (0x50010064)              // bm1684 special DDR Mapping
#define DDR_MAP_BIT_SHIFT                          0                        // bm1684 video core 0 and 1 shift
#define DDR_MAP_BIT_SHIFT_1                        16                       // bm1684 video core 2 and 3 shift

#define MAX_VPU_BUFFER_POOL (64*MAX_NUM_INSTANCE+12*3)//+12*3 => mvCol + YOfsTable + COfsTable

#ifdef _WIN32

DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon0,
    0x84703ec3,
    0x9b1b,
    0x49d7,
    0x9a,
    0xa6,
    0xc,
    0x42,
    0xc6,
    0x46,
    0x56,
    0x81);

typedef struct _HPI_LIST_ITEM
{
    ULONG                        Ibuffer;
    ULONG                        InSize;
    ULONG                        OutBufferL[2];
    ULONG                       OutSize;
} HPI_LIST_ITEM, * PHPI_LIST_ITEM;


HPI_LIST_ITEM                    ioItem;


typedef enum {
    BM_SUCCESS = 0,
    BM_ERR_DEVNOTREADY = 1, /* Device not ready yet */
    BM_ERR_FAILURE = 2,     /* General failure */
    BM_ERR_TIMEOUT = 3,     /* Timeout */
    BM_ERR_PARAM = 4,       /* Parameters invalid */
    BM_ERR_NOMEM = 5,       /* Not enough memory */
    BM_ERR_DATA = 6,        /* Data error */
    BM_ERR_BUSY = 7,        /* Busy */
    BM_ERR_NOFEATURE = 8,   /* Not supported yet */
    BM_NOT_SUPPORTED = 9
} bm_status_t;

#endif

typedef struct vpu_buffer_t {
    unsigned int  size;
    u64 phys_addr;
    u64 base;
    u64 virt_addr;

    unsigned int  reserve[2];
} vpu_buffer_t;

#ifndef ENDIANMODE
#define ENDIANMODE
typedef enum {
    VDI_LITTLE_ENDIAN = 0,      /* 64bit LE */
    VDI_BIG_ENDIAN,             /* 64bit BE */
    VDI_32BIT_LITTLE_ENDIAN,
    VDI_32BIT_BIG_ENDIAN,
    /* WAVE PRODUCTS */
    VDI_128BIT_LITTLE_ENDIAN    = 16,
    VDI_128BIT_LE_BYTE_SWAP,
    VDI_128BIT_LE_WORD_SWAP,
    VDI_128BIT_LE_WORD_BYTE_SWAP,
    VDI_128BIT_LE_DWORD_SWAP,
    VDI_128BIT_LE_DWORD_BYTE_SWAP,
    VDI_128BIT_LE_DWORD_WORD_SWAP,
    VDI_128BIT_LE_DWORD_WORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_WORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_WORD_SWAP,
    VDI_128BIT_BE_DWORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_SWAP,
    VDI_128BIT_BE_WORD_BYTE_SWAP,
    VDI_128BIT_BE_WORD_SWAP,
    VDI_128BIT_BE_BYTE_SWAP,
    VDI_128BIT_BIG_ENDIAN        = 31,
    VDI_ENDIAN_MAX
} EndianMode;
#endif

#define VDI_128BIT_ENDIAN_MASK      0xf

typedef unsigned long long  vmem_key_t;

typedef struct page_struct {
    int             pageno;
    u64             addr;
    int             used;
    int             alloc_pages;
    int             first_pageno;
} page_t;

typedef struct avl_node_struct {
    vmem_key_t   key;
    int     height;
    page_t* page;
    struct avl_node_struct* left;
    struct avl_node_struct* right;
} avl_node_t;

typedef struct _video_mm_struct {
    avl_node_t*     free_tree;
    avl_node_t*     alloc_tree;
    page_t*         page_list;
    int             num_pages;
    u64             base_addr;
    u64             mem_size;
    int             free_page_count;
    int             alloc_page_count;
} video_mm_t;

// TODO
typedef struct vpu_instance_pool_s {
    /* Since VDI don't know the size of CodecInst structure,
     * VDI should have the enough space not to overflow. */
    uint8_t      codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
    vpu_buffer_t vpu_common_buffer;
    int          vpu_instance_num;
    int          instance_pool_inited;
    void*        pendingInst;
    int          pendingInstIdxPlus1;

    video_mm_t   vmem;
} vpu_instance_pool_t;


#ifdef _WIN32
int winDeviceIoControl(HANDLE devHandle, u32 cmd, void* param);
#endif
DECL_EXPORT int bm_vdi_init(uint32_t core_idx);
DECL_EXPORT int bm_vdi_release(uint32_t core_idx);    //this function may be called only at system off.

vpu_instance_pool_t* bm_vdi_get_instance_pool(uint32_t core_idx);
int bm_vdi_get_common_memory(uint32_t core_idx, vpu_buffer_t *vb);

int bm_vdi_get_sram_memory(uint32_t core_idx, vpu_buffer_t *vb);

#ifdef SUPPORT_MULTI_INST_INTR
int bm_vdi_wait_interrupt(uint32_t core_idx, unsigned int instIdx, int timeout);
#else
int bm_vdi_wait_interrupt(uint32_t core_idx, int timeout);
#endif

int bm_vdi_set_clock_gate(uint32_t core_idx, int enable);
int bm_vdi_get_clock_gate(uint32_t core_idx);

int bm_vdi_get_instance_num(uint32_t core_idx);

/* system register write */
void VpuWriteReg(uint32_t core_idx, uint64_t addr, unsigned int data);
/* system register read */
unsigned int VpuReadReg(uint32_t core_idx, uint64_t addr);

void bm_vdi_fio_write_register(uint32_t core_idx, uint64_t addr, unsigned int data);
unsigned int bm_vdi_fio_read_register(uint32_t core_idx, uint64_t addr);
int bm_vdi_write_memory(uint32_t core_idx, uint64_t addr, uint8_t *data, int len);
#ifdef BM_PCIE_MODE
DECL_EXPORT int bm_vdi_read_memory(uint32_t core_idx, uint64_t addr, uint8_t *data, int len);
#endif
int vdienc_get_video_cap(int core_idx, int* video_cap, int* bin_type, int* max_core_num, int* chip_id);
int bm_vdi_lock(uint32_t core_idx);
int bm_vdi_unlock(uint32_t core_idx);
int bm_vdi_lock2(uint32_t core_idx);
int bm_vdi_unlock2(uint32_t core_idx);

int bm_vdi_open_instance(uint32_t core_idx, uint32_t inst_idx);
int bm_vdi_close_instance(uint32_t core_idx, uint32_t inst_idx);
int bm_vdi_set_bit_firmware_to_pm(uint32_t core_idx, const uint16_t *code);
int bm_vdi_convert_endian(unsigned int endian);

unsigned int bm_vdi_get_ddr_map(uint32_t core_idx);

int bm_vdi_get_firmware_status(uint32_t core_idx);

int bm_vdi_resume_kernel_reset(uint32_t core_idx);
int bm_vdi_disable_kernel_reset(uint32_t core_idx);

#endif //#ifndef _VDI_H_

