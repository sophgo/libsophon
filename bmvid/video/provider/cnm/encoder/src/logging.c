/*****************************************************************************
 *
 *    Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
 *
 *    bmvid is licensed under the 2-Clause BSD License except for the
 *    third-party components.
 *
 *****************************************************************************/
/* This library provides a high-level interface for controlling the BitMain
 * Sophon VPU en/decoder.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#define __USE_GNU

#ifdef __linux__
 #include <termios.h>
#elif _WIN32

#endif
#include "bmvpu.h"
#include "bmvpu_logging.h"

#include "vpuopt.h"
#include "vdi.h"
#include "wave5_regdefine.h"
#include "vpuapi.h"
#include "internal.h"


vpu_log_level_t vpu_log_level_threshold = VPU_LOG_ERROR;

void vpu_set_logging_threshold(vpu_log_level_t threshold)
{
    vpu_log_level_threshold = threshold;

    int level = ERR;

    switch (threshold)
    {
    case VPU_LOG_ERROR:
        level = ERR;
        break;
    case VPU_LOG_WARNING:
        level = WARN;
        break;
    case VPU_LOG_INFO:
        level = INFO;
        break;
    case VPU_LOG_DEBUG:
        level = DEBUG;
        break;
    case VPU_LOG_LOG:
    case VPU_LOG_TRACE:
        level = TRACE;
        break;
    default:
        level = ERR;
        break;
    }
    vpu_SetMaxLogLevel(level);
}

#define MAX_PRINT_LENGTH 512

static int log_colors[MAX_LOG_LEVEL] = {
    0,
    TERM_COLOR_R|TERM_COLOR_BRIGHT,    // ERR
    TERM_COLOR_R|TERM_COLOR_B|TERM_COLOR_BRIGHT,    // WARN
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B|TERM_COLOR_BRIGHT, // INFO
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B|TERM_COLOR_BRIGHT, // DEBUG
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B    //TRACE
};

static unsigned log_decor = (LOG_HAS_TIME | LOG_HAS_FILE | LOG_HAS_MICRO_SEC |
                             LOG_HAS_NEWLINE |
                             LOG_HAS_SPACE | LOG_HAS_COLOR);
static int max_log_level = ERR;
#ifdef __linux__
static __thread FILE *fpLog = NULL;
#elif _WIN32
static   __declspec(thread) FILE* fpLog = NULL;
#endif

#if 0
static void term_set_color(int color);
static void term_restore_color();
#endif

int vpu_InitLog()
{
    char* s_logfile = getenv("BMVE_LOGLFILE");
    int   b_logfile = 0;
    char  time_s[64] = {0};
    char  tmp[256] = {0};
    time_t ct;
    struct tm* plt;

    if (s_logfile)
        b_logfile = atoi(s_logfile);

    if (b_logfile <= 0)
        return 0;

    if (fpLog)
        return 0;

    ct = time(NULL);
    plt = localtime(&ct);;
    strftime(time_s, 63, "%m%d_%Hh%Mm%Ss", plt);
#ifdef __linux__
    snprintf(tmp, 255,"/tmp/bmve_%s_%0lx.log", time_s, pthread_self());
#elif _WIN32
    snprintf(tmp, 255, "D:\\tool\\bmve_%s_%0lx.log", time_s, GetCurrentThreadId());
#endif
    fpLog = fopen(tmp, "w");
    if (fpLog==NULL) {
        fprintf(stderr, "Errror! failed to open %s\n", tmp);
        return -1;
    }

    return 0;
}

void vpu_DeInitLog()
{
    if (fpLog) {
        fclose(fpLog);
        fpLog = NULL;
    }
}

void vpu_SetLogColor(int level, int color)
{
    log_colors[level] = color;
}

int vpu_GetLogColor(int level)
{
    return log_colors[level];
}

void vpu_SetLogDecor(int decor)
{
    log_decor = decor;
}

int vpu_GetLogDecor()
{
    return log_decor;
}

void vpu_SetMaxLogLevel(int level)
{
    max_log_level = level;
}
int vpu_GetMaxLogLevel()
{
    return max_log_level;
}

void vpu_LogMsg(int level, const char *format, ...)
{
    va_list ptr;
    char logBuf[MAX_PRINT_LENGTH] = {0};

    if (level > max_log_level)
        return;

    va_start(ptr, format);
    vsnprintf(logBuf, MAX_PRINT_LENGTH, format, ptr);
    va_end(ptr);

    if ((log_decor & LOG_HAS_FILE) && fpLog) {
        fwrite(logBuf, strlen(logBuf), 1,fpLog);
        fflush(fpLog);
    }

#if 0
    if (log_decor & LOG_HAS_COLOR)
        term_set_color(log_colors[level]);
#endif

    fputs(logBuf, stderr);

#if 0
    if (log_decor & LOG_HAS_COLOR)
        term_restore_color();
#endif
}

#if 0
static void term_set_color(int color)
{
    /* put bright prefix to ansi_color */
    char ansi_color[12] = "\033[01;3";

    if (color & TERM_COLOR_BRIGHT)
        color ^= TERM_COLOR_BRIGHT;
    else
        strcpy(ansi_color, "\033[00;3");

    switch (color) {
    case 0:
        /* black color */
        strcat(ansi_color, "0m");
        break;
    case TERM_COLOR_R:
        /* red color */
        strcat(ansi_color, "1m");
        break;
    case TERM_COLOR_G:
        /* green color */
        strcat(ansi_color, "2m");
        break;
    case TERM_COLOR_B:
        /* blue color */
        strcat(ansi_color, "4m");
        break;
    case TERM_COLOR_R | TERM_COLOR_G:
        /* yellow color */
        strcat(ansi_color, "3m");
        break;
    case TERM_COLOR_R | TERM_COLOR_B:
        /* magenta color */
        strcat(ansi_color, "5m");
        break;
    case TERM_COLOR_G | TERM_COLOR_B:
        /* cyan color */
        strcat(ansi_color, "6m");
        break;
    case TERM_COLOR_R | TERM_COLOR_G | TERM_COLOR_B:
        /* white color */
        strcat(ansi_color, "7m");
        break;
    default:
        /* default console color */
        strcpy(ansi_color, "\033[00m");
        break;
    }

    fputs(ansi_color, stdout);
}

static void term_restore_color(void)
{
    term_set_color(log_colors[5]);
}
#endif

void vpu_logging(char const *lvlstr, char const *file, int const line, char const *fn, const char *format, ...)
{
    va_list args;

    printf("%s:%d (%s)   %s: ", file, line, fn, lvlstr);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

static void make_log(u64 coreIdx, const char *str, int step)
{
    uint32_t val;

    val = VpuReadReg(coreIdx, W5_CMD_INSTANCE_INFO);
    val &= 0xffff;
    if (step == 1)
        VLOG(DEBUG, "\n**%s start(%d)\n", str, val);
    else if (step == 2) //
        VLOG(DEBUG, "\n**%s timeout(%d)\n", str, val);
    else
        VLOG(DEBUG, "\n**%s end(%d)\n", str, val);
}

#define VCORE_DBG_ADDR(__vCoreIdx)      0x8000+(0x1000*__vCoreIdx) + 0x300
#define VCORE_DBG_DATA(__vCoreIdx)      0x8000+(0x1000*__vCoreIdx) + 0x304
#define VCORE_DBG_READY(__vCoreIdx)     0x8000+(0x1000*__vCoreIdx) + 0x308
static uint32_t ReadRegVCE(uint32_t core_idx, uint32_t vce_core_idx, uint32_t vce_addr)
{
    int     vcpu_reg_addr;
    int     udata;
    int     vce_core_base = 0x8000 + 0x1000*vce_core_idx;

    VpuSetClockGate(core_idx, 1);
    bm_vdi_fio_write_register(core_idx, VCORE_DBG_READY(vce_core_idx), 0);

    vcpu_reg_addr = vce_addr >> 2;

    bm_vdi_fio_write_register(core_idx, VCORE_DBG_ADDR(vce_core_idx),vcpu_reg_addr + vce_core_base);

    if (bm_vdi_fio_read_register(0, VCORE_DBG_READY(vce_core_idx)) == 1)
        udata= bm_vdi_fio_read_register(0, VCORE_DBG_DATA(vce_core_idx));
    else
    {
        VLOG(ERR, "failed to read VCE register: %d, 0x%04x\n", vce_core_idx, vce_addr);
        udata = -2;//-1 can be a valid value
    }
    VpuSetClockGate(core_idx, 0);

    return udata;
}
static void wave5_vcore_status(u64 coreIdx)
{
    uint32_t i;

    VLOG(DEBUG,"[+] BPU REG Dump\n");
    for(i = 0x8000; i < 0x80FC; i += 16)
    {
        VLOG(DEBUG,"0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", (W5_REG_BASE + i),
             bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i)),
             bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i + 4 )),
             bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i + 8 )),
             bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i + 12)));
    }
    VLOG(DEBUG,"[-] BPU REG Dump\n");

    // --------- VCE register Dump
    VLOG(DEBUG,"[+] VCE REG Dump\n");
    for (i=0x000; i<0x1fc; i+=16)
    {
        VLOG(DEBUG,"0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
             ReadRegVCE(coreIdx, 0, (i+0x00)),
             ReadRegVCE(coreIdx, 0, (i+0x04)),
             ReadRegVCE(coreIdx, 0, (i+0x08)),
             ReadRegVCE(coreIdx, 0, (i+0x0c)));
    }
    VLOG(DEBUG,"[-] VCE REG Dump\n");
}

static void print_vpu_status(u64 coreIdx)
{
    uint32_t productCode;

    productCode = VpuReadReg(coreIdx, VPU_PRODUCT_CODE_REGISTER);

    if (productCode == WAVE521C_CODE)
    {
        uint32_t vcpu_reg[31]= {0,};
        uint32_t i;

        VLOG(DEBUG,"-------------------------------------------------------------------------------\n");
        VLOG(DEBUG,"------                            VCPU STATUS                             -----\n");
        VLOG(DEBUG,"-------------------------------------------------------------------------------\n");

        // --------- VCPU register Dump
        VLOG(DEBUG,"[+] VCPU REG Dump\n");
        for (i = 0; i < 25; i++)
        {
            VpuWriteReg (coreIdx, 0x14, (1<<9) | (i & 0xff));
            vcpu_reg[i] = VpuReadReg (coreIdx, 0x1c);

            if (i < 16)
            {
                VLOG(DEBUG,"0x%08x\t",  vcpu_reg[i]);
                if ((i % 4) == 3) VLOG(DEBUG,"\n");
            }
            else
            {
                switch (i)
                {
                case 16: VLOG(DEBUG,"CR0: 0x%08x\t", vcpu_reg[i]); break;
                case 17: VLOG(DEBUG,"CR1: 0x%08x\n", vcpu_reg[i]); break;
                case 18: VLOG(DEBUG,"ML:  0x%08x\t", vcpu_reg[i]); break;
                case 19: VLOG(DEBUG,"MH:  0x%08x\n", vcpu_reg[i]); break;
                case 21: VLOG(DEBUG,"LR:  0x%08x\n", vcpu_reg[i]); break;
                case 22: VLOG(DEBUG,"PC:  0x%08x\n", vcpu_reg[i]); break;
                case 23: VLOG(DEBUG,"SR:  0x%08x\n", vcpu_reg[i]); break;
                case 24: VLOG(DEBUG,"SSP: 0x%08x\n", vcpu_reg[i]); break;
                default: break;
                }
            }
        }
        VLOG(DEBUG,"[-] VCPU REG Dump\n");
        /// -- VCPU ENTROPY PERI DECODE Common

        VLOG(DEBUG,"[+] VCPU ENT DEC REG Dump\n");
        for(i = 0x6000; i < 0x6800; i += 16)
        {
            VLOG(DEBUG,"0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", (W5_REG_BASE + i),
                 bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i)),
                 bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i + 4 )),
                 bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i + 8 )),
                 bm_vdi_fio_read_register(coreIdx, (W5_REG_BASE + i + 12)));
        }
        VLOG(DEBUG,"[-] VCPU ENT DEC REG Dump\n");
        wave5_vcore_status(coreIdx);
        VLOG(DEBUG,"-------------------------------------------------------------------------------\n");
    }
    else
    {
        VLOG(ERR, "Unknown product id : %08x\n", productCode);
    }
}
void bm_vdi_log(u64 coreIdx, int cmd, int step)
{
    int i;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return ;

    switch(cmd)
    {
    case W5_INIT_VPU:
        make_log(coreIdx, "INIT_VPU", step);
        break;
    case W5_ENC_SET_PARAM:
        make_log(coreIdx, "ENC_SET_PARAM", step);
        break;
    case W5_INIT_SEQ:
        make_log(coreIdx, "ENC INIT_SEQ", step);
        break;
    case W5_DESTROY_INSTANCE:
        make_log(coreIdx, "DESTROY_INSTANCE", step);
        break;
    case W5_ENC_PIC:
        make_log(coreIdx, "ENC_PIC", step);
        break;
    case W5_SET_FB:
        make_log(coreIdx, "SET_FRAMEBUF", step);
        break;
    case W5_FLUSH_INSTANCE:
        make_log(coreIdx, "FLUSH INSTANCE", step);
        break;
    case W5_QUERY:
        make_log(coreIdx, "QUERY", step);
        break;
    case W5_SLEEP_VPU:
        make_log(coreIdx, "SLEEP_VPU", step);
        break;
    case W5_WAKEUP_VPU:
        make_log(coreIdx, "WAKEUP_VPU", step);
        break;
    case W5_UPDATE_BS:
        make_log(coreIdx, "UPDATE_BS", step);
        break;
    case W5_CREATE_INSTANCE:
        make_log(coreIdx, "CREATE_INSTANCE", step);
        break;
    default:
        make_log(coreIdx, "ANY_CMD", step);
        break;
    }

    for (i=0x100; i<0x200; i+=16) { // wave host IF register 0x100 ~ 0x200
        VLOG(DEBUG, "0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
             VpuReadReg(coreIdx, i), VpuReadReg(coreIdx, i+4),
             VpuReadReg(coreIdx, i+8), VpuReadReg(coreIdx, i+0xc));
    }

    if (cmd == W5_INIT_VPU || cmd == W5_RESET_VPU || cmd == W5_CREATE_INSTANCE)
        print_vpu_status(coreIdx);
}

