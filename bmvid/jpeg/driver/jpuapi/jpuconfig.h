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
#ifndef _JPU_CONFIG_H_
#define _JPU_CONFIG_H_

#include "../config.h"
#include "jputypes.h"


#define MAX_INST_HANDLE_SIZE            (12*1024)

#ifdef JPU_FPGA_PLATFORM
#define JPU_FRAME_ENDIAN            JDI_BIG_ENDIAN
#define JPU_STREAM_ENDIAN            JDI_BIG_ENDIAN
#else
#define JPU_FRAME_ENDIAN            JDI_LITTLE_ENDIAN
#define JPU_STREAM_ENDIAN           JDI_LITTLE_ENDIAN
#endif
#define JPU_CHROMA_INTERLEAVE       1       // 0 (chroma separate mode), 1 (cbcr interleave mode), 2 (crcb interleave mode)


#define JPU_INTERRUPT_TIMEOUT_MS    2000

#define JPU_STUFFING_BYTE_FF        0       // 0 : ON ("0xFF"), 1 : OFF ("0x00") for stuffing

#define JPU_PARTIAL_DECODE          1       // 0 : OFF, 1 : ON

#define MAX_MJPG_PIC_WIDTH   32768
#define MAX_MJPG_PIC_HEIGHT  32768

  
#define MAX_FRAME               (1*MAX_NUM_INSTANCE) // For AVC decoder, 16(reference) + 2(current) + 1(rotator)


#define STREAM_FILL_SIZE        0x10000
#define STREAM_END_SIZE         0

#define JPU_GBU_SIZE            512

#define STREAM_BUF_SIZE         0x500000


#define JPU_CHECK_WRITE_RESPONSE_BVALID_SIGNAL 0

#ifdef CHIP_BM1684
#define MAX_NUM_JPU_CORE                4
#define MAX_NUM_JPU_CORE_2              2
#define MAX_NUM_JPU_CORE_1              1
#else   // BM1682
#define MAX_NUM_JPU_CORE                1
#endif
#ifdef BM_PCIE_MODE
#define MAX_NUM_DEV           64
#else
#define MAX_NUM_DEV           1
#endif
#define MAX_NUM_INSTANCE               MAX_NUM_JPU_CORE
#endif  /* _JPU_CONFIG_H_ */
