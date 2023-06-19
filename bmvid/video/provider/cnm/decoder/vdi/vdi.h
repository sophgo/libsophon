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

#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#define ATTRIBUTE 
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define ATTRIBUTE __attribute__((deprecated))
#define DECL_EXPORT
#define DECL_IMPORT
#endif

#include "mm.h"
#include "vpuconfig.h"
#include "vputypes.h"
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#include<stdbool.h>
#endif
#include "bmlib_runtime.h"
#ifdef _WIN32
#include <windows.h>
#include "windatatype.h"
#endif

/************************************************************************/
/* COMMON REGISTERS                                                     */
/************************************************************************/
#define VPU_PRODUCT_NAME_REGISTER                 0x1040
#define VPU_PRODUCT_CODE_REGISTER                 0x1044

#ifdef CHIP_BM1880
#define DDR_MAP_REGISTER                          (0x50010064)              // bm1880 special DDR Mapping
#define DDR_MAP_BIT_SHIFT                          24
#elif defined(CHIP_BM1684)
#define DDR_MAP_REGISTER                          (0x50010064)              // bm1684 special DDR Mapping
#define DDR_MAP_BIT_SHIFT                          0                        // bm1684 video core 0 and 1 shift
#define DDR_MAP_BIT_SHIFT_1                        16                       // bm1684 video core 2 and 3 shift
#else
#define DDR_MAP_REGISTER                            (0x50008008)                // bm1682 special DDR MAPPING
#define DDR_MAP_BIT_SHIFT                          16
#endif

#define SUPPORT_MULTI_CORE_IN_ONE_DRIVER
#define MAX_VPU_CORE_NUM MAX_NUM_VPU_CORE

#define MAX_VPU_BUFFER_POOL (64*MAX_NUM_INSTANCE*2+12*3)//+12*3 => mvCol + YOfsTable + COfsTable

#define VpuWriteReg( CORE, ADDR, DATA )                 vdi_write_register( CORE, ADDR, DATA )                    // system register write
#define VpuReadReg( CORE, ADDR )                        vdi_read_register( CORE, ADDR )                            // system register read
#define VpuWriteMem( CORE, ADDR, DATA, LEN, ENDIAN )    vdi_write_memory( CORE, ADDR, DATA, LEN, ENDIAN )        // system memory write
#define VpuReadMem( CORE, ADDR, DATA, LEN, ENDIAN )     vdi_read_memory( CORE, ADDR, DATA, LEN, ENDIAN )        // system memory read

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

#endif //WIN32 endif

typedef struct vpu_buffer_t {
    unsigned int  size;
    u64 phys_addr;
    u64 base;
    u64 virt_addr;

    unsigned int  core_idx;
    int           enable_cache;
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

typedef struct vpu_pending_intr_t {
    int instance_id[COMMAND_QUEUE_DEPTH];
    int int_reason[COMMAND_QUEUE_DEPTH];
    int order_num[COMMAND_QUEUE_DEPTH];
    int in_use[COMMAND_QUEUE_DEPTH];
    int num_pending_intr;
    int count;
} vpu_pending_intr_t;

typedef enum {
    VDI_LINEAR_FRAME_MAP  = 0,
    VDI_TILED_FRAME_V_MAP = 1,
    VDI_TILED_FRAME_H_MAP = 2,
    VDI_TILED_FIELD_V_MAP = 3,
    VDI_TILED_MIXED_V_MAP = 4,
    VDI_TILED_FRAME_MB_RASTER_MAP = 5,
    VDI_TILED_FIELD_MB_RASTER_MAP = 6,
    VDI_TILED_FRAME_NO_BANK_MAP = 7,
    VDI_TILED_FIELD_NO_BANK_MAP = 8,
    VDI_LINEAR_FIELD_MAP  = 9,
    VDI_TILED_MAP_TYPE_MAX
} vdi_gdi_tiled_map;

typedef struct vpu_instance_pool_t {
    unsigned char   codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];  // Since VDI don't know the size of CodecInst structure, VDI should have the enough space not to overflow.
    vpu_buffer_t    vpu_common_buffer;
    int             vpu_instance_num;
    int             instance_pool_inited;
    void*           pendingInst;
    int             pendingInstIdxPlus1;
    video_mm_t      vmem;
//    vpu_pending_intr_t pending_intr_list;
    int          doSwResetInstIdxPlus1; // added forSetSWResetStatus and GetSWResetStatus function
} vpu_instance_pool_t;

#if defined (__cplusplus)
extern "C" {
#endif
#ifdef _WIN32
    int winDeviceIoControl(HANDLE devHandle, u32 cmd, void* param);
#endif
	int vdi_probe(u64 core_idx);
    int vdi_init(u64 core_idx);
    int vdi_release(u64 core_idx);    //this function may be called only at system off.

    vpu_instance_pool_t * vdi_get_instance_pool(u64 core_idx);
    int vdi_allocate_common_memory(u64 core_idx);
    int vdi_get_common_memory(u64 core_idx, vpu_buffer_t *vb);
    int vdi_allocate_dma_memory(u64 core_idx, vpu_buffer_t *vb);
    int vdi_attach_dma_memory(u64 core_idx, vpu_buffer_t *vb);
    void vdi_free_dma_memory(u64 core_idx, vpu_buffer_t *vb);
    int vdi_get_sram_memory(u64 core_idx, vpu_buffer_t *vb);
    int vdi_dettach_dma_memory(u64 core_idx, vpu_buffer_t *vb);

#ifdef SUPPORT_MULTI_INST_INTR
    int vdi_wait_interrupt(u64 coreIdx, unsigned int instIdx, int timeout);
#else
    int vdi_wait_interrupt(u64 core_idx, int timeout);
#endif
    int vdi_wait_vpu_busy(u64 core_idx, int timeout, unsigned int addr_bit_busy_flag);
    int vdi_wait_vcpu_bus_busy(u64 core_idx, int timeout, unsigned int addr_bit_busy_flag);
    int vdi_wait_bus_busy(u64 core_idx, int timeout, unsigned int gdi_busy_flag);
    int vdi_hw_reset(u64 core_idx);

    int vdi_crst_set_status(int core_idx, int status);
    int vdi_crst_set_enable(int core_idx, int en);
    int vdi_crst_get_status(int core_idx);
    int vdi_crst_chk_status(int core_idx, int inst_idx, int pos, int *p_is_sleep, int *p_is_block);

    int vdi_set_clock_gate(u64 core_idx, int enable);
    int vdi_get_clock_gate(u64 core_idx);
    /**
     * @brief       make clock stable before changing clock frequency
     * @detail      Before inoking vdi_set_clock_freg() caller MUST invoke vdi_ready_change_clock() function.
     *              after changing clock frequency caller also invoke vdi_done_change_clock() function.
     * @return  0   failure
     *          1   success
     */
    int vdi_ready_change_clock(u64 core_idx);
    int vdi_set_change_clock(u64 core_idx, u64 clock_mask);
    int vdi_done_change_clock(u64 core_idx);

    int  vdi_get_instance_num(u64 core_idx);

    void vdi_write_register(u64 core_idx, u64 addr, unsigned int data);
    unsigned int vdi_read_register(u64 core_idx, u64 addr);
    void vdi_fio_write_register(u64 core_idx, u64 addr, unsigned int data);
    unsigned int vdi_fio_read_register(u64 core_idx, u64 addr);
    DECL_EXPORT int vdi_clear_memory(u64 core_idx, u64 addr, int len, int endian);
    DECL_EXPORT int vdi_write_memory(u64 core_idx, u64 addr, unsigned char *data, int len, int endian);
    DECL_EXPORT int vdi_read_memory(u64 core_idx, u64 addr, unsigned char *data, int len, int endian);
    DECL_EXPORT int vdi_get_video_cap(int core_idx,int *video_cap,int *bin_type,int *max_core_num,int *chip_id);

    DECL_EXPORT int bm_vdi_memcpy_s2d(bm_handle_t bm_handle, int coreIdx,bm_device_mem_t dst, void *src,int endian);
    DECL_EXPORT int bm_vdi_memcpy_d2s(bm_handle_t bm_handle, int coreIdx, void *dst,bm_device_mem_t src,int endian);
    DECL_EXPORT void bm_free_mem(bm_handle_t dev_handle,bm_device_mem_t dev_mem,unsigned long long vaddr);
    DECL_EXPORT int  bm_vdi_mmap(bm_handle_t bm_handle, bm_device_mem_t *dmem,unsigned long long *vmem);
    int vdi_lock(u64 core_idx);
    int vdi_lock_check(u64 core_idx);
    void vdi_unlock(u64 core_idx);

    int vdi_disp_lock(u64 core_idx);
    void vdi_disp_unlock(u64 core_idx);
    void vdi_set_sdram(u64 core_idx, u64 addr, int len, unsigned char data, int endian);
    void vdi_log(u64 core_idx, int cmd, int step);
    int vdi_open_instance(u64 core_idx, u64 inst_idx);
    int vdi_close_instance(u64 core_idx, u64 inst_idx);
    int vdi_set_bit_firmware_to_pm(u64 core_idx, const unsigned short *code);
    void vdi_delay_ms(unsigned int delay_ms);
    int vdi_get_system_endian(u64 core_idx);
    int vdi_convert_endian(u64 core_idx, unsigned int endian);
    void vdi_print_vpu_status(u64 coreIdx);

    int vdi_resume_kernel_reset(u64 core_idx);
    int vdi_disable_kernel_reset(u64 core_idx);

#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    int vdi_get_task_num(u64 core_idx);
#endif
/*
 * this function is special for nonOS, because this part - interruption clear will be done
 * by Linux OS in ISR routine
 */
    int vdi_clear_interrupt(u64 coreIdx, unsigned int flags);
    unsigned int vdi_get_ddr_map(u64 coreIdx);
    unsigned int vdi_trigger(int step);

    u64 vdi_get_virt_addr(u64 core_idx, u64 addr);
    void vdi_invalidate_memory(u64 core_idx, vpu_buffer_t *vb);
    int vdi_get_firmware_status(unsigned int core_idx);
    int vdi_get_in_num(unsigned int core_idx, unsigned int inst_idx);
    int vdi_get_out_num(unsigned int core_idx, unsigned int inst_idx);
    int *vdi_get_exception_info(unsigned int core_idx);
    void vdi_destroy_lock(u64 core_idx);

//#define TRY_SEM_MUTEX
//#define TRY_FLOCK
#define TRY_ATOMIC_LOCK
#ifdef TRY_SEM_MUTEX
    #define CORE_SEM_NAME "/vpu_sem_core"
    #define CORE_DISP_SEM_NEME "/vpu_disp_sem_core"
#elif defined(TRY_FLOCK)
    #define CORE_FLOCK_NAME "/dev/shm/vpu_flock_core"
    #define CORE_DISP_FLOCK_NEME "/dev/shm/vpu_disp_flock_core"
    extern __thread int s_flock_fd[MAX_NUM_VPU_CORE];
    extern __thread int s_disp_flock_fd[MAX_NUM_VPU_CORE];
#endif
#if defined (__cplusplus)
}
#endif

#endif //#ifndef _VDI_H_

