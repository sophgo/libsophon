//--=========================================================================--
//  This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2011  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//        This file should be modified by some customers according to their SOC configuration.
//--=========================================================================--

#ifndef _BM_VPU_OPT_H_
#define _BM_VPU_OPT_H_

#include "config.h"


#define BODA950_CODE                    0x9500
#define CODA960_CODE                    0x9600
#define CODA980_CODE                    0x9800
#define WAVE510_CODE                    0x5100
#define WAVE512_CODE                    0x5120
#define WAVE520_CODE                    0x5200
#define WAVE515_CODE                    0x5150
#define WAVE525_CODE                    0x5250

#define WAVE511_CODE                    0x5110
#define WAVE521_CODE                    0x5210
#define WAVE521C_CODE                   0x521c
#define WAVE521C_DUAL_CODE              0x521d  // wave521 dual core

#define PRODUCT_CODE_W_SERIES(x) (x == WAVE510_CODE || x == WAVE512_CODE || x == WAVE520_CODE || x == WAVE515_CODE || x == WAVE525_CODE || x == WAVE511_CODE || x     == WAVE521_CODE || x == WAVE521C_CODE)
#define PRODUCT_CODE_NOT_W_SERIES(x) (x == BODA950_CODE || x == CODA960_CODE || x == CODA980_CODE)


#define WAVE521ENC_WORKBUF_SIZE         (128*1024)      /* HEVC 128K, AVC 40K */


#define MAX_INST_HANDLE_SIZE            48              /* DO NOT CHANGE THIS VALUE */
#define MAX_NUM_INSTANCE                32
#ifdef PRO_VERSION
# define LIMITED_INSTANCE_NUM           24              /* pro version */
#else
# define LIMITED_INSTANCE_NUM           18              /* lite version */
#endif

#if defined(BM_PCIE_MODE)
# define MAX_NUM_SOPHON_SOC             128             /* at most 128 SoCs */
#else
# define MAX_NUM_SOPHON_SOC             1
#endif
#define MAX_NUM_VPU_CORE_CHIP           5
#define MAX_NUM_VPU_CORE                (MAX_NUM_VPU_CORE_CHIP*MAX_NUM_SOPHON_SOC)
#define MAX_NUM_ENC_CORE                (MAX_NUM_SOPHON_SOC)

#define MIN_ENC_PIC_WIDTH               256
#define MIN_ENC_PIC_HEIGHT              128
#define MAX_ENC_PIC_WIDTH               8192
#define MAX_ENC_PIC_HEIGHT              8192

#define HOST_ENDIAN                     VDI_128BIT_LITTLE_ENDIAN
#define VPU_FRAME_ENDIAN                HOST_ENDIAN
#define VPU_STREAM_ENDIAN               HOST_ENDIAN
#define VPU_SOURCE_ENDIAN               HOST_ENDIAN


#define USE_SRC_PRP_AXI                 0
#define USE_SRC_PRI_AXI                 1
#define DEFAULT_SRC_AXI                 USE_SRC_PRP_AXI

/* vpu common memory  */
#define COMMAND_QUEUE_DEPTH             4
#define ONE_TASKBUF_SIZE_FOR_CQ         (8*1024*1024)  /* upto 8Kx8K, need 8Mbyte per task. TODO */
#define SIZE_COMMON                     ((2*1024*1024)+(COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ))

/*  Application specific configuration */
#if defined(HAPS_SIM)
# define VPU_ENC_TIMEOUT                (5000*1000)
# define VPU_BUSY_CHECK_TIMEOUT         (5000*1000)
#elif defined(PALLADIUM_SIM)
# define VPU_ENC_TIMEOUT                (5000*4000)
# define VPU_BUSY_CHECK_TIMEOUT         (5000*4000)
#else
# define VPU_ENC_TIMEOUT                60000
# define VPU_BUSY_CHECK_TIMEOUT         50000
#endif

#endif  /* _BM_VPU_OPT_H_ */

