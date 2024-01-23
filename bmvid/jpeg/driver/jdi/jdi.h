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

#ifndef _JDI_HPI_H_
#define _JDI_HPI_H_

#include "../jpuapi/jpuconfig.h"
#include "../jpuapi/regdefine.h"
#include "mm.h"

#if !defined DECL_EXPORT
#ifdef _WIN32
    #define DECL_EXPORT __declspec(dllexport)
#else
    #define DECL_EXPORT
#endif
#endif


#define MAX_THREAD_NUM 32
#define MAX_JPU_BUFFER_POOL (2048 * MAX_THREAD_NUM)

#define JpuWriteReg( ADDR, DATA )    jdi_write_register( ADDR, DATA ) // system register write
#define JpuReadReg( ADDR )            jdi_read_register( ADDR )           // system register write
#define JpuWriteMem( ADDR, DATA, LEN, ENDIAN )    jdi_write_memory( ADDR, DATA, LEN, ENDIAN ) // system memory write
#define JpuReadMem( ADDR, DATA, LEN, ENDIAN )    jdi_read_memory( ADDR, DATA, LEN, ENDIAN ) // system memory write

#if defined(linux) || defined(__linux) || defined(ANDROID)
#define PGPROT_DEFAULT      0
#define PGPROT_WRITECOMBINE 1
#define PGPROT_NONCACHE     2
#endif

typedef struct jpu_buffer_t {
    unsigned long long phys_addr;
    unsigned long long base;
    unsigned long long virt_addr;
    unsigned int size;
#if defined(linux) || defined(__linux) || defined(ANDROID) || defined(_WIN32)
    /* page prot flags. 0: default; 1: writecombine; 2: noncache */
    unsigned int flags;
    unsigned int device_index;
#endif
} jpu_buffer_t;

typedef struct jpu_register_info_t {
    unsigned int core_idx;
    unsigned int address_offset;
    unsigned int data;
} jpu_register_info_t;

typedef struct jpu_instance_pool_t {
    unsigned char jpgInstPool[MAX_NUM_JPU_CORE][MAX_INST_HANDLE_SIZE];
#if defined(LIBBMJPULITE)
    long long jpu_mutex;
#else
    void* jpu_mutex;
#endif
    int jpu_instance_num;
    int instance_pool_inited;
    void *pendingInst;
    jpeg_mm_t vmem; // TODO
} jpu_instance_pool_t;

#ifdef SUPPORT_128BIT_BUS

typedef enum {
    JDI_128BIT_LITTLE_64BIT_LITTLE_ENDIAN = ((0<<2)+(0<<1)+(0<<0)),    //  128 bit little, 64 bit little
    JDI_128BIT_BIG_64BIT_LITTLE_ENDIAN = ((1<<2)+(0<<1)+(0<<0)),    //  128 bit big , 64 bit little
    JDI_128BIT_LITTLE_64BIT_BIG_ENDIAN = ((0<<2)+(0<<1)+(1<<0)),    //  128 bit little, 64 bit big
    JDI_128BIT_BIG_64BIT_BIG_ENDIAN = ((1<<2)+(0<<1)+(1<<0)),        //  128 bit big, 64 bit big
    JDI_128BIT_LITTLE_32BIT_LITTLE_ENDIAN = ((0<<2)+(1<<1)+(0<<0)),    //  128 bit little, 32 bit little
    JDI_128BIT_BIG_32BIT_LITTLE_ENDIAN = ((1<<2)+(1<<1)+(0<<0)),    //  128 bit big , 32 bit little
    JDI_128BIT_LITTLE_32BIT_BIG_ENDIAN = ((0<<2)+(1<<1)+(1<<0)),    //  128 bit little, 32 bit big
    JDI_128BIT_BIG_32BIT_BIG_ENDIAN = ((1<<2)+(1<<1)+(1<<0)),        //  128 bit big, 32 bit big
} EndianMode;
#define JDI_LITTLE_ENDIAN JDI_128BIT_LITTLE_64BIT_LITTLE_ENDIAN
#define JDI_BIG_ENDIAN JDI_128BIT_BIG_64BIT_BIG_ENDIAN
#define JDI_128BIT_ENDIAN_MASK (1<<2)
#define JDI_64BIT_ENDIAN_MASK  (1<<1)
#define JDI_ENDIAN_MASK  (1<<0)

#define JDI_32BIT_LITTLE_ENDIAN JDI_128BIT_LITTLE_32BIT_LITTLE_ENDIAN
#define JDI_32BIT_BIG_ENDIAN JDI_128BIT_LITTLE_32BIT_BIG_ENDIAN

#else

typedef enum {
    JDI_LITTLE_ENDIAN = 0,
    JDI_BIG_ENDIAN,
    JDI_32BIT_LITTLE_ENDIAN,
    JDI_32BIT_BIG_ENDIAN,
} EndianMode;
#endif

typedef enum {
    JDI_LOG_CMD_PICRUN  = 0,
    JDI_LOG_CMD_MAX
} jdi_log_cmd;


#if defined (__cplusplus)
extern "C" {
#endif
    int jdi_init(int device_index);
    int jdi_release(int device_index);    //this function may be called only at system off.

    int jdi_allocate_dma_memory(jpu_buffer_t *vb);
    void jdi_free_dma_memory(jpu_buffer_t *vb);

    int jdi_map_dma_memory(jpu_buffer_t* vb, int flags);
    int jdi_unmap_dma_memory(jpu_buffer_t* vb);


    int jdi_invalidate_region(jpu_buffer_t* vb, unsigned long long phys_addr, unsigned long long size);
    int jdi_flush_region(jpu_buffer_t* vb, unsigned long long phys_addr, unsigned long long size);

    int jdi_wait_interrupt(int device_index, Uint32 coreIdx,int timeout);

    int jdi_hw_reset();
    int jdi_hw_reset_all();

    int jdi_set_clock_gate(int enable);
    int jdi_get_clock_gate();

    int jdi_open_instance(unsigned long long instIdx);
    int jdi_close_instance(unsigned int instIdx);
    int jdi_get_instance_num();
    int jdi_get_core_index(int device_index,unsigned long long readAddr, unsigned long long writeAddr);

    int jdi_get_dump_info(void* dump_info);
    void jdi_clear_dump_info();

    void jdi_write_register(unsigned int addr, unsigned int data);
    unsigned int jdi_read_register(unsigned int addr);

    int jdi_write_memory(unsigned long long addr, unsigned char *data, int len, int endian);
    int jdi_read_memory(unsigned long long addr, unsigned char *data, int len, int endian);
#ifdef BM_PCIE_MODE
    int jdi_pcie_write_mem(int device_index, unsigned char *src, unsigned long long dst, size_t size);
    int jdi_pcie_read_mem(int device_index, unsigned long long src, unsigned char *dst, size_t size);
#endif
    DECL_EXPORT int  jdi_lock();
    DECL_EXPORT void jdi_unlock();

    void jdi_log(int cmd, int step);

    unsigned long long jdi_get_memory_addr_high(unsigned long long);
#if defined (__cplusplus)
}
#endif

#endif //#ifndef _JDI_HPI_H_
