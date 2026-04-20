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

#ifndef _VDI_DEBUG_H_
#define _VDI_DEBUG_H_

#include "vputypes.h"
#include "vpuconfig.h"

void vdi_log(unsigned long coreIdx, unsigned long instIdx, int cmd, int step);

void vdi_stat_fps(int coreIdx, int instIndex, int picType);
void vdi_stat_hwcycles(int coreIdx, int hwCycles);
void vdi_update_resolution(int coreIdx, int instance, int width, int height);
typedef enum encode_frame_status {
	WAIT_TO_ENCODE   = 0,
	ENCODE_FAIL      = 1,
	SUCCESS_NOT_GET  = 2,
	NOT_FREE         = 3,
	OUT_FRAME        = 4,
	IN_FRAME         = 5,
} FrameStatus;
void vdi_update_channel_frames(int coreIdx, int instance, FrameStatus status, int frame_num);
#endif //#ifndef _VDI_DEBUG_H_

