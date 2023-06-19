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
//--=========================================================================-

#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/kfifo.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/proc_fs.h>
#include <linux/uuid.h>
#include <linux/kthread.h>

#if defined(BM_ION_MEM)
#include <linux/dma-buf.h>
/* since currently ion is in staging stage
 * that is why has to create below such
 * long include path.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
#include <../drivers/soc/bitmain/ion/bitmain/bitmain_ion_alloc.h>
#else
#include <../drivers/staging/android/ion/bitmain/bitmain_ion_alloc.h>
#endif
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
#include <linux/sched/signal.h>
#endif

/* if the driver want to use interrupt service from kernel ISR */
#define VPU_SUPPORT_ISR

#include "vpuconfig.h"
#include "vpuerror.h"
#include "wave5_regdefine.h"
#include "coda9_regdefine.h"
#include "vpu.h"

// #define ENABLE_DEBUG_MSG
#ifdef ENABLE_DEBUG_MSG
#define DPRINTK(args...)        pr_info(args);//printk(KERN_INFO args);
#else
#define DPRINTK(args...)
#endif
extern void efuse_ft_get_video_cap(int *cap);
extern void efuse_ft_get_bin_type(int *bin_type);
extern uint32_t sophon_get_chip_id(void);

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
/* #define VPU_SUPPORT_CLOCK_CONTROL */

#ifdef VPU_SUPPORT_ISR
/* if the driver want to disable and enable IRQ whenever interrupt asserted. */
//#define VPU_IRQ_CONTROL
#endif

/* if the platform driver knows the name of this driver */
/* VPU_PLATFORM_DEVICE_NAME */
#define VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
#ifndef BM_ION_MEM
/* if this driver knows the dedicated video memory address */
#define VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#endif
#define VPU_PLATFORM_DEVICE_NAME "vdec"
#define VPU_CLK_NAME    "vcodec"
#define VPU_CLASS_NAME  "vpu"
#define VPU_DEV_NAME    "vpu"

struct class *vpu_class;
#define MAX_DATA_SIZE (2048* get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE) // [size(url)+size(uuid)+size(misc)]*core*inst + size(head)
char *dat = NULL;

/* if the platform driver knows this driver */
/* the definition of VPU_REG_BASE_ADDR and VPU_REG_SIZE are not meaningful */
#ifdef CHIP_BM1880
#define VPU_REG_BASE_ADDR 0x50040000
static int s_vpu_reg_phy_base[MAX_NUM_VPU_CORE] = {0x50040000};
#elif defined(CHIP_BM1684)
#define VPU_REG_BASE_ADDR 0x50050000
#ifdef TEST_VPU_ONECORE_FPGA
static int s_vpu_reg_phy_base[MAX_NUM_VPU_CORE] = {0x50050000};
#else
static int s_vpu_reg_phy_base[MAX_NUM_VPU_CORE] = {0x50050000, 0x50060000, 0x500D0000, 0x500E0000, 0x50126000};
#endif
#else
#define VPU_REG_BASE_ADDR 0x50440000
static int s_vpu_reg_phy_base[MAX_NUM_VPU_CORE] = {0x50440000, 0x50450000, 0x50460000};
#endif
#define VPU_REG_SIZE 0x10000

/* this definition is only for chipsnmedia FPGA board env */
/* so for SOC env of customers can be ignored */
#define VPU_SUPPORT_CLOSE_COMMAND
#ifndef VM_RESERVED    /*for kernel up to 3.7.0 version*/
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static int s_vpu_sub_bin_flag[MAX_NUM_VPU_CORE] = {1,1,1,1,1};

enum {
  SYSCXT_STATUS_WORKDING      = 0,
  SYSCXT_STATUS_EXCEPTION     ,
};

typedef struct vpu_crst_context_s
{
    unsigned int instcall[MAX_NUM_INSTANCE];
    unsigned int status;
    unsigned int disable;

    unsigned int reset;
    unsigned int count;
} vpu_crst_context_t;

typedef struct vpu_drv_context_t {
    struct fasync_struct *async_queue;
#ifdef SUPPORT_MULTI_INST_INTR
    u32 interrupt_reason[MAX_NUM_INSTANCE*MAX_NUM_VPU_CORE];
#else
    u32 interrupt_reason[MAX_NUM_VPU_CORE];
#endif
#if defined(BM_ION_MEM)
    struct device *dev;
#endif
    u32 open_count;                     /*!<< device reference count. Not instance count */
    vpu_crst_context_t crst_cxt[MAX_NUM_VPU_CORE];
    u32 reset_vpu_core_disable[MAX_NUM_VPU_CORE];
} vpu_drv_context_t;

typedef struct vpu_drv_context_all_t
{
    vpu_drv_context_t *p_vpu_context;
    int core_idx;
} vpu_drv_context_all_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
    struct list_head list;
    struct vpudrv_buffer_t vb;
    struct file *filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
    struct list_head list;
    unsigned long inst_idx;
    unsigned long core_idx;
    struct file *filp;
} vpudrv_instanace_list_t;

/* Please keep consistent with that of vdi.h */
typedef struct vpu_buffer_t {
    unsigned int  size;
    unsigned long phys_addr;
    unsigned long base;                            /* kernel logical address in use kernel */
    unsigned long virt_addr;                /* virtual user space address */

    unsigned int  core_idx;
    int           enable_cache;
} vpu_buffer_t;

typedef struct vpudrv_instance_pool_t {
    unsigned char codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
    vpu_buffer_t  vpu_common_buffer;
    int           vpu_instance_num;
    int           instance_pool_inited;
    void*         pendingInst;
    int           pendingInstIdxPlus1;
    int             doSwResetInstIdxPlus1;
} vpudrv_instance_pool_t;

static struct proc_dir_entry *entry = NULL;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#ifdef ASIC_BOX
#    define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (224*1024*1024)
#ifdef CHIP_BM1684
#    define VPU_DRAM_PHYSICAL_BASE 0x4f0000000
#else
#    define VPU_DRAM_PHYSICAL_BASE 0x1f0000000
#endif
#else
#    define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (3*1024*1024*1024U)
#ifdef CHIP_BM1684
#    define VPU_DRAM_PHYSICAL_BASE 0x430000000
#else
#    define VPU_DRAM_PHYSICAL_BASE 0x1b0000000
#endif
#endif
#include "vmm.h"
static video_mm_t s_vmem;
static int s_video_memory_flag = 0;
#endif /*VPU_SUPPORT_RESERVED_VIDEO_MEMORY*/
static vpudrv_buffer_t s_video_memory = {0};
static int chip_id   = 0;
static int video_cap = 0;

static int vpu_hw_reset(int);
static void vpu_clk_disable(struct clk *clk);
static int vpu_clk_enable(struct clk *clk);
static struct clk *vpu_clk_get(struct device *dev);
static void vpu_clk_put(struct clk *clk);

/* end customer definition */
static vpudrv_buffer_t s_instance_pool[MAX_NUM_VPU_CORE] = {0};
static vpudrv_buffer_t s_common_memory[MAX_NUM_VPU_CORE] = {0};
static vpu_drv_context_t s_vpu_drv_context = {0};
static int s_vpu_major;
static struct cdev s_vpu_cdev;

static struct clk *s_vpu_clk;
//static struct reset_control *s_vpu_rst[MAX_NUM_VPU_CORE] = {NULL,};

typedef struct _vpu_reset_ctrl_ {
    struct reset_control *axi2_rst[MAX_NUM_VPU_CORE];
#ifdef CHIP_BM1684
    struct clk *apb_clk[MAX_NUM_VPU_CORE];
    struct clk *axi_clk[MAX_NUM_VPU_CORE];
    struct clk *axi_clk_enc;
#else
    struct reset_control *apb_video_rst;
    struct reset_control *video_axi_rst;
#endif
} vpu_reset_ctrl;
static vpu_reset_ctrl vpu_rst_ctrl = {0};

#define MAX_VPU_STAT_WIN_SIZE  100
typedef struct vpu_statistic_info {
    int vpu_open_ref_count[MAX_NUM_VPU_CORE];
    uint64_t vpu_working_time_in_ms[MAX_NUM_VPU_CORE];
    uint64_t vpu_total_time_in_ms[MAX_NUM_VPU_CORE];
    atomic_t vpu_busy_status[MAX_NUM_VPU_CORE];
    char vpu_status_array[MAX_NUM_VPU_CORE][MAX_VPU_STAT_WIN_SIZE];
    int vpu_status_index[MAX_NUM_VPU_CORE];
    int vpu_instant_usage[MAX_NUM_VPU_CORE];
    int vpu_instant_interval;

    int64_t vpu_mem_total_size;
    int64_t vpu_mem_free_size;
    int64_t vpu_mem_used_size;
}vpu_statistic_info_t;

static vpu_statistic_info_t s_vpu_usage_info;
static struct task_struct *s_vpu_monitor_task = NULL;

#ifdef VPU_SUPPORT_ISR
#ifdef CHIP_BM1880
static int s_vpu_irq[MAX_NUM_VPU_CORE] = {VPU_IRQ_NUM};
#elif defined(CHIP_BM1684)
#ifdef TEST_VPU_ONECORE_FPGA
static int s_vpu_irq[MAX_NUM_VPU_CORE] = {VPU_IRQ_NUM};
#else
static int s_vpu_irq[MAX_NUM_VPU_CORE] = {VPU_IRQ_NUM, VPU_IRQ_NUM_1, VPU_IRQ_NUM_2, VPU_IRQ_NUM_3, VPU_IRQ_NUM_4};
#endif
#if defined(ENABLE_MEMORY_SECURITY)
#define VPU_SUB_SYSTEM_NUM 2
static int s_vpu_security_irq[VPU_SUB_SYSTEM_NUM] = {109, 112};
static unsigned long s_vpu_security_reg_phys[VPU_SUB_SYSTEM_NUM] = {0x5000EC00, 0x5000F000};
static volatile int *p_security_reg_base[VPU_SUB_SYSTEM_NUM] = {NULL, NULL};
#else
#define VPU_SUB_SYSTEM_NUM 0
#endif
#else
#define VPU_SUB_SYSTEM_NUM 0
static int s_vpu_irq[MAX_NUM_VPU_CORE] = {VPU_IRQ_NUM, VPU_IRQ_NUM_1, VPU_IRQ_NUM_2};
#endif
#endif
static vpudrv_buffer_t s_vpu_register[MAX_NUM_VPU_CORE] = {0};

#ifdef SUPPORT_MULTI_INST_INTR
static int               s_interrupt_flag[MAX_NUM_INSTANCE*MAX_NUM_VPU_CORE] = {0};
static wait_queue_head_t s_interrupt_wait_q[MAX_NUM_INSTANCE*MAX_NUM_VPU_CORE];
static struct kfifo      s_interrupt_pending_q[MAX_NUM_INSTANCE*MAX_NUM_VPU_CORE];
#else
static int               s_interrupt_flag[MAX_NUM_VPU_CORE];
static wait_queue_head_t s_interrupt_wait_q[MAX_NUM_VPU_CORE];
#endif

static struct mutex s_vpu_lock;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(s_vpu_sem);
#else
static DEFINE_SEMAPHORE(s_vpu_sem);
#endif
static struct list_head s_vbp_head = LIST_HEAD_INIT(s_vbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(s_inst_list_head);

static vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE] = {0};
static u32 *s_vpu_dump_flag = NULL;
static u32 s_init_flag[MAX_NUM_VPU_CORE] = {0};
#ifdef CONFIG_PM
/* Product register */
#define VPU_PRODUCT_CODE_REGISTER       (BIT_BASE + 0x1044)


static u32    s_vpu_reg_store[MAX_NUM_VPU_CORE][64];
#endif

typedef struct vpu_inst_info
{
    unsigned char uuid[16];
    unsigned long flag;
    unsigned long time;
    unsigned long status;
    int pid;
    unsigned char *url;
} vpu_inst_info_t;

static vpu_inst_info_t s_vpu_inst_info[MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE] = {0};
static DEFINE_MUTEX(s_vpu_proc_lock);

int bm_vpu_monitor_thread(void *data);
static int vpu_init_get_flags_bm1686(int video_cap){
    int i =0;

    s_vpu_sub_bin_flag[1] = 0;
    s_vpu_sub_bin_flag[3] = 0;

    if(video_cap == 1){
        s_vpu_sub_bin_flag[0] = 0;
        s_vpu_sub_bin_flag[1] = 0;
        s_vpu_sub_bin_flag[3] = 0;
    }
    if(video_cap == 2){
        s_vpu_sub_bin_flag[1] = 0;
        s_vpu_sub_bin_flag[2] = 0;
        s_vpu_sub_bin_flag[3] = 0;
    }
    if(video_cap == 3){
        for(i=0;i<MAX_NUM_VPU_CORE-1;i++)
        s_vpu_sub_bin_flag[i] = 0;
    }
    return 0;
}

static int vpu_init_get_flags_bm1684(int video_cap){
    int i = 0;
    if(video_cap == 1){
        s_vpu_sub_bin_flag[0] = 0;
        s_vpu_sub_bin_flag[1] = 0;
    }
    else if(video_cap == 2){
        s_vpu_sub_bin_flag[2] = 0;
        s_vpu_sub_bin_flag[3] = 0;
    }
    if(video_cap == 3){
        for(i=0;i<MAX_NUM_VPU_CORE-1;i++)
        s_vpu_sub_bin_flag[i] = 0;
    }
    return 0;
}

static int vpu_init_set_source_bm1686(int video_cap){
    if(video_cap == 0){
        s_vpu_reg_phy_base[1] = s_vpu_reg_phy_base[2];
        s_vpu_reg_phy_base[2] = s_vpu_reg_phy_base[4];
        s_vpu_irq[1]          = s_vpu_irq[2];
        s_vpu_irq[2]          = s_vpu_irq[4];
        s_vpu_register[1]     = s_vpu_register[2];
        s_vpu_register[2]     = s_vpu_register[4];
    }
    if(video_cap == 1){
        s_vpu_reg_phy_base[0] = s_vpu_reg_phy_base[2];
        s_vpu_reg_phy_base[1] = s_vpu_reg_phy_base[4];
        s_vpu_irq[0]          = s_vpu_irq[2];
        s_vpu_irq[1]          = s_vpu_irq[4];
        s_vpu_register[0]     = s_vpu_register[2];
        s_vpu_register[1]     = s_vpu_register[4];
    }
    if(video_cap == 2){
        //s_vpu_reg_phy_base[0] = s_vpu_reg_phy_base[0];
        s_vpu_reg_phy_base[1] = s_vpu_reg_phy_base[4];
        s_vpu_irq[1]          = s_vpu_irq[4];
        s_vpu_register[1]     = s_vpu_register[4];
    }
    if(video_cap == 3){
        s_vpu_reg_phy_base[0] = s_vpu_reg_phy_base[4];
        s_vpu_irq[0]          = s_vpu_irq[4];
        s_vpu_register[0]     = s_vpu_register[4];
    }
    return 0;
}

static int vpu_init_set_source_bm1684(int video_cap){

    if(video_cap == 1){
        s_vpu_irq[0]          = s_vpu_irq[2];
        s_vpu_irq[1]          = s_vpu_irq[3];
        s_vpu_irq[2]          = s_vpu_irq[4];
        s_vpu_reg_phy_base[0] = s_vpu_reg_phy_base[2];
        s_vpu_reg_phy_base[1] = s_vpu_reg_phy_base[3];
        s_vpu_reg_phy_base[2] = s_vpu_reg_phy_base[4];
        s_vpu_register[0]     = s_vpu_register[2];
        s_vpu_register[1]     = s_vpu_register[3];
        s_vpu_register[2]     = s_vpu_register[4];
    }
    else if(video_cap == 2){
        s_vpu_reg_phy_base[2] = s_vpu_reg_phy_base[4];
        s_vpu_irq[2]          = s_vpu_irq[4];
        s_vpu_register[2]     = s_vpu_register[4];
    }
    if(video_cap == 3){
        s_vpu_reg_phy_base[0] = s_vpu_reg_phy_base[4];
        s_vpu_irq[0]          = s_vpu_irq[4];
        s_vpu_register[0]     = s_vpu_register[4];
    }
    return 0;
}

static int get_vpu_core_num(int chip_id, int video_cap){
    if(chip_id == 0x1684){
        if(video_cap == 1 || video_cap == 2)
            return MAX_NUM_VPU_CORE - 2;
        else if(video_cap == 3)
            return MAX_NUM_VPU_CORE - 4;
        else
            return MAX_NUM_VPU_CORE;
    }

#if defined(CHIP_BM1684)
    if(chip_id == 0x1686){
        if(video_cap == 1 || video_cap == 2)
           return MAX_NUM_VPU_CORE_1686 - 1;
        else if(video_cap == 3)
            return MAX_NUM_VPU_CORE_1686 - 2;
        else
            return MAX_NUM_VPU_CORE_1686;
    }
#endif
    return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
void do_gettimeofday(struct timeval *tv)
{
	struct timespec64 now;

	ktime_get_ts64(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_usec = now.tv_nsec/1000;
}
#endif

static unsigned long get_us_time(void)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

static void clean_exception_inst_info(void)
{
    int cur_tgid = current->tgid;
    int i;
    //pr_info("exception tgid : %d\n", cur_tgid);
    mutex_lock(&s_vpu_proc_lock);
    for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
    {
        if(s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 0 && cur_tgid == s_vpu_inst_info[i].pid)
        {
            s_vpu_inst_info[i].status = 1;
            s_vpu_inst_info[i].time = get_us_time();
        }
    }
    mutex_unlock(&s_vpu_proc_lock);
}

#define    ReadVpuRegister(addr)        *(volatile unsigned int *)(s_vpu_register[core].virt_addr + s_bit_firmware_info[core].reg_base_offset + addr)
#define    WriteVpuRegister(addr, val)  *(volatile unsigned int *)(s_vpu_register[core].virt_addr + s_bit_firmware_info[core].reg_base_offset + addr) = (unsigned int)val
#define    WriteVpu(addr, val)          *(volatile unsigned int *)(addr) = (unsigned int)val;
#ifdef VPU_SUPPORT_CLOSE_COMMAND
static int WaitBusyTimeout(u32 core, u32 addr)
{
    unsigned long timeout = jiffies + HZ; /* vpu wait timeout to 1sec */
    while (ReadVpuRegister(addr)) {
        if (time_after(jiffies, timeout)) {
            return 1;
        }
    }
    return 0;
}
static int CodaCloseInstanceCommand(int core, u32 instanceIndex)
{
    int ret = 0;

    WriteVpuRegister(BIT_BUSY_FLAG, 1);
    WriteVpuRegister(BIT_RUN_INDEX, instanceIndex);
#define DEC_SEQ_END 2
    WriteVpuRegister(BIT_RUN_COMMAND, DEC_SEQ_END);
    if(WaitBusyTimeout(core, BIT_BUSY_FLAG)) {
        pr_err("CodaCloseInstanceCommand after BUSY timeout");
        ret = 1;
    }

    DPRINTK("sent CodaCloseInstanceCommand 55 ret=%d", ret);
    return ret;
}

static void Wave5BitIssueCommandInst(u32 core, u32 instanceIndex, u32 cmd)
{
    WriteVpuRegister(W5_CMD_INSTANCE_INFO,  instanceIndex&0xffff);
    WriteVpuRegister(W5_VPU_BUSY_STATUS, 1);
    WriteVpuRegister(W5_COMMAND, cmd);
    WriteVpuRegister(W5_VPU_HOST_INT_REQ, 1);
}

static int SendQuery(u32 core, u32 instanceIndex, u32 queryOpt)
{
    // Send QUERY cmd
    WriteVpuRegister(W5_QUERY_OPTION, queryOpt);
    WriteVpuRegister(W5_VPU_BUSY_STATUS, 1);

    Wave5BitIssueCommandInst(core, instanceIndex, W5_QUERY);

    if(WaitBusyTimeout(core, W5_VPU_BUSY_STATUS)) {
        pr_err("SendQuery BUSY timeout");
        return 2;
    }

    if (ReadVpuRegister(W5_RET_SUCCESS) == 0)
        return 1;

    return 0;
}

static int Wave5DecClrDispFlag(u32 core, u32 instanceIndex, u32 index)
{
    int ret = 0;

    WriteVpuRegister(W5_CMD_DEC_CLR_DISP_IDC, (1<<index));
    WriteVpuRegister(W5_CMD_DEC_SET_DISP_IDC, 0);

    ret = SendQuery(core, instanceIndex, UPDATE_DISP_FLAG);

    if (ret != 0) {
        pr_err("Wave5DecClrDispFlag QUERY FAILURE\n");
        return 1;
    }

    return 0;
}

static int Wave5VpuDecSetBitstreamFlag(u32 core, u32 instanceIndex)
{
    WriteVpuRegister(W5_BS_OPTION, 2);

    Wave5BitIssueCommandInst(core, instanceIndex, W5_UPDATE_BS);
    if(WaitBusyTimeout(core, W5_VPU_BUSY_STATUS)) {
        pr_err("Wave5VpuDecSetBitstreamFlag BUSY timeout");
        return 2;
    }

    if (ReadVpuRegister(W5_RET_SUCCESS) == 0) {
        pr_err("Wave5VpuDecSetBitstreamFlag failed.\n");
        return 1;
    }

    return 0;
}

static int FlushDecResult(u32 core, u32 instanceIndex)
{
    int     ret = 0;
    u32     regVal;

    WriteVpuRegister(W5_CMD_DEC_ADDR_REPORT_BASE, 0);
    WriteVpuRegister(W5_CMD_DEC_REPORT_SIZE,      0);
    WriteVpuRegister(W5_CMD_DEC_REPORT_PARAM,     31);
    //WriteVpuRegister(W5_CMD_DEC_GET_RESULT_OPTION, 0x1); // gregory add 0x1 : commanding get_result without checking interrupt.
#define GET_RESULT 2
    // Send QUERY cmd
    ret = SendQuery(core, instanceIndex, GET_RESULT);
    if (ret != 0) {
        regVal = ReadVpuRegister(W5_RET_FAIL_REASON);
        pr_info("flush result reason: 0x%x", regVal);
        return 1;
    }

    return 0;
}

static int FlushEncResult(u32 core, u32 instanceIndex)
{
    int     ret = 0;
    u32     regVal;

   #define GET_RESULT 2
    // Send QUERY cmd
    ret = SendQuery(core, instanceIndex, GET_RESULT);
    if (ret != 0) {
        regVal = ReadVpuRegister(W5_RET_FAIL_REASON);
        pr_info("flush result reason: 0x%x", regVal);
        return 1;
    }
    return 0;
}

static int Wave5CloseInstanceCommand(int core, u32 instanceIndex)
{
    int ret = 0;

#define W5_DESTROY_INSTANCE 0x0020
    WriteVpuRegister(W5_CMD_INSTANCE_INFO,  (instanceIndex&0xffff));
    WriteVpuRegister(W5_VPU_BUSY_STATUS, 1);
    WriteVpuRegister(W5_COMMAND, W5_DESTROY_INSTANCE);

    WriteVpuRegister(W5_VPU_HOST_INT_REQ, 1);

    if(WaitBusyTimeout(core, W5_VPU_BUSY_STATUS)) {
        pr_info("Wave5CloseInstanceCommand after BUSY timeout\n");
        ret = 1;
        goto DONE_CMD;
    }

    if (ReadVpuRegister(W5_RET_SUCCESS) == 0) {
        pr_info("Wave5CloseInstanceCommand failed REASON=[0x%x]\n", ReadVpuRegister(W5_RET_FAIL_REASON));

        if (ReadVpuRegister(W5_RET_FAIL_REASON) == WAVE5_VPU_STILL_RUNNING)
            ret = 2;
        else
            ret = 1;
        goto DONE_CMD;
    }
    ret = 0;
DONE_CMD:
    DPRINTK("sent Wave5CloseInstanceCommandt 55 ret=%d", ret);
    return ret;
}
#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
static int get_vpu_create_inst_flag(int core_idx)
{
    int *addr = (int *)(s_instance_pool[core_idx].base + s_instance_pool[core_idx].size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    return __atomic_load_n((int *)(addr + 2), __ATOMIC_SEQ_CST);
}

static void release_vpu_create_inst_flag(int core_idx, int inst_idx)
{
    int *addr = (int *)(s_instance_pool[core_idx].base +  s_instance_pool[core_idx].size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    int vpu_create_inst_flag = get_vpu_create_inst_flag(core_idx);
    __atomic_store_n((int *)(addr + 2), (vpu_create_inst_flag & (~(1 << inst_idx))), __ATOMIC_SEQ_CST);
}

static int CloseInstanceCommand(int core, u32 instanceIndex)
{
    int product_code;
    product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);
    if (PRODUCT_CODE_W_SERIES(product_code)) {
        u32 i =0;
        u32 interrupt_flag_in_q = 0;
        int vpu_create_inst_flag = get_vpu_create_inst_flag(core);
        if ((vpu_create_inst_flag & (1 << instanceIndex)) != 0) {
            if(WAVE521C_CODE != product_code) {
                Wave5VpuDecSetBitstreamFlag(core, instanceIndex);

                interrupt_flag_in_q = kfifo_out(&s_interrupt_pending_q[core*MAX_NUM_INSTANCE+instanceIndex], &i, sizeof(u32));
                if (interrupt_flag_in_q > 0) {
                    //FlushDecResult(core, instanceIndex);
                    pr_info("interrupt flag : %d\n", interrupt_flag_in_q);
                }
                FlushDecResult(core, instanceIndex);
                for(i=0; i<32; i++) {
                    int ret = Wave5DecClrDispFlag(core, instanceIndex, i);
                    if(ret != 0)
                        break;
                }
            }
            if (WAVE521C_CODE == product_code) {
                FlushEncResult(core, instanceIndex);
            }
            return Wave5CloseInstanceCommand(core, instanceIndex);
        }
        else
            return 0;
    }
    else {
        return CodaCloseInstanceCommand(core, instanceIndex);
    }
}
#endif
int bm_vpu_register_cdev(struct platform_device *pdev);
#ifdef CONFIG_ARCH_BM1880
static int vpu_allocate_memory(struct platform_device *pdev);
#endif
static void bm_vpu_assert(vpu_reset_ctrl *pRstCtrl);
static void bm_vpu_deassert(vpu_reset_ctrl *pRstCtrl);
static void bm_vpu_unregister_cdev(void);

#if defined(CHIP_BM1880) || defined(CHIP_BM1684)
static void vpu_set_topaddr(int high_addr)
{
    // get vpu remap reg
    #define VPU_REMAP_REG_ADDR 0x50010064
    #define VPU_REMAP_REG_SIZE 0x04
    static vpudrv_buffer_t s_vpu_remap_register = {0};
    s_vpu_remap_register.phys_addr = VPU_REMAP_REG_ADDR;
    s_vpu_remap_register.virt_addr = (unsigned long)ioremap_nocache(s_vpu_remap_register.phys_addr, VPU_REMAP_REG_SIZE);
    s_vpu_remap_register.size = VPU_REMAP_REG_SIZE;
    pr_info("[VPUDRV] : vpu remap reg address get from defined base addr=0x%lx, virtualbase=0x%lx, size=%d\n",
        s_vpu_remap_register.phys_addr, s_vpu_remap_register.virt_addr, s_vpu_remap_register.size);
    if (s_vpu_remap_register.virt_addr) {
        /* config vidoe remap register */
        volatile unsigned int* remap_reg = (unsigned int *)s_vpu_remap_register.virt_addr;
        unsigned int remap_val = *remap_reg;

#ifdef CHIP_BM1880
        remap_val  = (remap_val & 0x00FFFFFF) | 0x01000000;
#else
        remap_val = (remap_val & 0xF800FF00) | (high_addr<<24) | (high_addr<<16) | high_addr;
#endif
        *remap_reg = remap_val;
        pr_info("[VPUDRV] : vpu addr remap top register=0x%08x\n", remap_val);

        iounmap((void *)s_vpu_remap_register.virt_addr);
        s_vpu_remap_register.virt_addr = 0x00;
    } else {
        pr_err("[VPUDRV] :  fail to remap video addr remap reg ==0x%08lx, size=%d\n", s_vpu_remap_register.phys_addr, (int)s_vpu_remap_register.size);
    }
}
#endif /* CHIP_BM1880 */

#if defined(BM_ION_MEM)
static void* vpu_dma_buffer_attach_sg(vpudrv_buffer_t *vb)
{
    void* ret = ERR_PTR(0);

    vb->dma_buf = dma_buf_get(vb->ion_fd);
    if (IS_ERR(vb->dma_buf)) {
        ret = vb->dma_buf;
        goto err0;
    }

    vb->attach = dma_buf_attach(vb->dma_buf, s_vpu_drv_context.dev);
    if (IS_ERR(vb->attach)) {
        ret = vb->attach;
        goto err1;
    }

    vb->table = dma_buf_map_attachment(vb->attach, DMA_FROM_DEVICE);
    if (IS_ERR(vb->table)) {
        ret = vb->table;
        goto err2;
    }
    if (vb->table->nents != 1) {
        pr_err("muti-sg is not prefer\n");
        ret = ERR_PTR(-EINVAL);
        goto err2;
    }

    vb->phys_addr = sg_dma_address(vb->table->sgl);

    DPRINTK("ion_fd = %d attach_sg result is pass\n", vb->ion_fd);

    return ret;

err2:
    dma_buf_detach(vb->dma_buf, vb->attach);
err1:
    dma_buf_put(vb->dma_buf);
err0:

    DPRINTK("ion_fd = %d attach_sg result is failed\n", vb->ion_fd);

    return ret;
}

static void vpu_dma_buffer_unattach_sg(vpudrv_buffer_t *vb)
{
    dma_buf_unmap_attachment(vb->attach, vb->table, DMA_FROM_DEVICE);
    dma_buf_detach(vb->dma_buf, vb->attach);
    dma_buf_put(vb->dma_buf);
}
#endif

static int vpu_alloc_dma_buffer(vpudrv_buffer_t *vb)
{
    if (!vb)
        return -1;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    vb->phys_addr = (unsigned long)vmem_alloc(&s_vmem, vb->size, 0);
    if ((unsigned long)vb->phys_addr  == (unsigned long)-1) {
        printk(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
        return -1;
    }

    vb->base = (unsigned long)(s_video_memory.base + (vb->phys_addr - s_video_memory.phys_addr));
#elif defined(BM_ION_MEM)
    /* get memory from cma heap */
    vb->ion_fd = bm_ion_alloc(ION_HEAP_TYPE_CARVEOUT, vb->size, 0);
    if (!vb->ion_fd) {
        printk(KERN_ERR "[VPUDRV] ion memory allocation error size=%d\n", vb->size);
        return -1;
    } else {
        if (IS_ERR(vpu_dma_buffer_attach_sg(vb))) {
            bm_ion_free(vb->ion_fd);
            return -1;
        }
    }
    bm_ion_free(vb->ion_fd);
    vb->base = 0xffffffff;
#else
    vb->base = (unsigned long)dma_alloc_coherent(NULL, PAGE_ALIGN(vb->size), (dma_addr_t *) (&vb->phys_addr), GFP_DMA | GFP_KERNEL);
    if ((void *)(vb->base) == NULL)    {
        printk(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
        return -1;
    }
#endif
    return 0;
}


static void vpu_free_dma_buffer(vpudrv_buffer_t *vb)
{
    if (!vb)
        return;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (vb->base)
        vmem_free(&s_vmem, vb->phys_addr, 0);
#elif defined(BM_ION_MEM)
    if (vb->ion_fd) {
        vpu_dma_buffer_unattach_sg(vb);
        //bm_ion_free(vb->ion_fd);
        vb->base = 0;
    }
#else
    if (vb->base)
        dma_free_coherent(0, PAGE_ALIGN(vb->size), (void *)vb->base, vb->phys_addr);
#endif
}
#if 1
int get_lock(int core_idx)
{
    int val = 0;
    int val1 = current->tgid; //in soc getpid() get the tgid.
    int val2 = current->pid;
    int ret = 1;
    int count = 0;
    volatile int *addr = (int *)(s_instance_pool[core_idx].base +  s_instance_pool[core_idx].size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    //while(__atomic_compare_exchange_n(addr, &val, val1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) == 0){
    while((val = __sync_val_compare_and_swap(addr, 0, val1)) != 0){
        if(val == val1 || val==val2) {
            ret = 1;
            break;
        }
        else {
            val = 0;
        }
        if(count >= 5000) {
            pr_info("can't get lock, org: %d, ker: %d", *addr, val1);
            ret = 0;
            break;
        }
        msleep(2);
        count += 1;
    }

    return ret;
}

void release_lock(int core_idx)
{
    volatile int *addr = (int *)(s_instance_pool[core_idx].base + s_instance_pool[core_idx].size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    __sync_lock_release(addr);
    //__atomic_store_n(addr, 0, __ATOMIC_SEQ_CST);
}
int get_disp_lock(int core_idx)
{
    int val = 0;
    int val1 = current->tgid; //in soc getpid() get the tgid.
    int val2 = current->pid;
    int ret = 1;
    int count = 0;
    int *addr = (int *)(s_instance_pool[core_idx].base + s_instance_pool[core_idx].size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    addr = addr + 1;
    while(__atomic_compare_exchange_n(addr, &val, val1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) == 0){
        if(val == val1 || val==val2) {
            ret = 1;
            break;
        }
        else {
            val = 0;
        }
        if(count >= 50) {
            pr_info("can't get disp lock, org: %d, ker: %d", *addr, val1);
            ret = 0;
            break;
        }
        msleep(2);
        count += 1;
    }

    return ret;
}

void release_disp_lock(int core_idx)
{
    int *addr = (int *)(s_instance_pool[core_idx].base + s_instance_pool[core_idx].size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    addr = addr + 1;

    __atomic_store_n(addr, 0, __ATOMIC_SEQ_CST);
}
#endif
static int vpu_free_instances(struct file *filp)
{
    vpudrv_instanace_list_t *vil, *n;
    vpudrv_instance_pool_t *vip;
    void *vip_base;
    int ret = 0;

    DPRINTK("[VPUDRV] vpu_free_instances\n");

    list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
    {
        if (vil->filp == filp) {
            vpu_drv_context_all_t *p_drv_context = (vpu_drv_context_all_t *)filp->private_data;
            struct vpu_drv_context_t *dev = p_drv_context->p_vpu_context;
            vip_base = (void *)(s_instance_pool[vil->core_idx].base);
            DPRINTK("[VPUDRV] vpu_free_instances detect instance crash instIdx=%d, coreIdx=%d, vip_base=%p, instance_pool_size_per_core=%d\n", (int)vil->inst_idx, (int)vil->core_idx, vip_base, (int)instance_pool_size_per_core);
            vip = (vpudrv_instance_pool_t *)vip_base;
            if (vip) {
                pr_info("clean core %d, inst %d, use flag addr: %p\n",(int)vil->core_idx, (int)vil->inst_idx, vip->codecInstPool[vil->inst_idx]);
                if(vip->pendingInstIdxPlus1 - 1 == vil->inst_idx)
                    vip->pendingInstIdxPlus1 = 0;
                memset(vip->codecInstPool[vil->inst_idx], 0x00, 4);    /* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
                if(ret == 0) {
                    ret = vil->core_idx + 1;
                }
                else if(ret != vil->core_idx + 1) {
                    pr_err("please check the release core index: %ld ....\n", vil->core_idx);
                }
                vip->vpu_instance_num -= 1;
                s_vpu_usage_info.vpu_open_ref_count[vil->core_idx]--;
                dev->crst_cxt[vil->core_idx].instcall[vil->inst_idx] = 0;
                list_del(&vil->list);
                kfree(vil);
            }
        }
    }

    return ret;
}

static int vpu_free_buffers(struct file *filp)
{
    vpudrv_buffer_pool_t *pool, *n;
    vpudrv_buffer_t vb;

    DPRINTK("[VPUDRV] vpu_free_buffers\n");

    list_for_each_entry_safe(pool, n, &s_vbp_head, list)
    {
        if (pool->filp == filp) {
            vb = pool->vb;
            if (vb.base) {
                vpu_free_dma_buffer(&vb);
                list_del(&pool->list);
                kfree(pool);
            }
        }
    }

    return 0;
}
#ifdef SUPPORT_MULTI_INST_INTR
static inline u32 get_inst_idx(u32 reg_val)
{
    u32 inst_idx;
    int i;
    for (i=0; i < MAX_NUM_INSTANCE; i++)
    {
        if(((reg_val >> i)&0x01) == 1)
            break;
    }
    inst_idx = i;
    return inst_idx;
}



static u32 get_vpu_inst_idx(vpu_drv_context_t *dev, u32 *reason, u32 empty_inst, u32 done_inst, u32 other_inst)
{
    u32 inst_idx;
    u32 reg_val;
    u32 int_reason;

    int_reason = *reason;
    DPRINTK("[VPUDRV][+]%s, int_reason=0x%x, empty_inst=0x%x, done_inst=0x%x\n", __func__, int_reason, empty_inst, done_inst);

    if (int_reason & (1 << INT_WAVE5_BSBUF_EMPTY))
    {
        reg_val = empty_inst; //(empty_inst & 0xffff);
        inst_idx = get_inst_idx(reg_val);
        *reason = (1 << INT_WAVE5_BSBUF_EMPTY);
        DPRINTK("[VPUDRV]    %s, W5_RET_BS_EMPTY_INST reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }

    if (int_reason & (1 << INT_WAVE5_INIT_SEQ))
    {
        reg_val = done_inst; //(done_inst & 0xffff);
        inst_idx = get_inst_idx(reg_val);
        *reason  = (1 << INT_WAVE5_INIT_SEQ);
        DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST INIT_SEQ reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }

    if (int_reason & (1 << INT_WAVE5_DEC_PIC))
    {
        reg_val = done_inst; //(done_inst & 0xffff);
        inst_idx = get_inst_idx(reg_val);
        *reason  = (1 << INT_WAVE5_DEC_PIC);
        DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);

        if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY))
        {
            u32 ll_inst_idx;
            reg_val = (done_inst >> 16);
            ll_inst_idx = get_inst_idx(reg_val);
            if (ll_inst_idx == inst_idx)
                *reason = ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY));
            DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC and ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d, ll_inst_idx=%d\n", __func__, reg_val, inst_idx, ll_inst_idx);
        }
        goto GET_VPU_INST_IDX_HANDLED;
    }

    if (int_reason & (1 << INT_WAVE5_ENC_SET_PARAM))
    {
        reg_val = done_inst; //(done_inst & 0xffff);
        inst_idx = get_inst_idx(reg_val);
        *reason  = (1 << INT_WAVE5_ENC_SET_PARAM);
        DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST ENC_SET_PARAM reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }
#if 0
    if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY))
    {
        reg_val = (done_inst >> 16);
        inst_idx = get_inst_idx(reg_val);
        *reason  = (1 << INT_WAVE5_ENC_LOW_LATENCY);
        DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }
#endif
    // if  interrupt is not for empty and dec_pic and init_seq.
    reg_val = other_inst; //(other_inst&0xFF);
    inst_idx = reg_val;
    *reason  = int_reason;
    DPRINTK("[VPUDRV]    %s, W5_RET_DONE_INSTANCE_INFO reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);

GET_VPU_INST_IDX_HANDLED:

    DPRINTK("[VPUDRV][-]%s, inst_idx=%d. *reason=0x%x\n", __func__, inst_idx, *reason);

    return inst_idx;
}
#endif
#if defined(CHIP_BM1684) && defined(ENABLE_MEMORY_SECURITY)
static void vpu_mem_security_setting(int sys_idx)
{
#if 1
    volatile int *p_base = ioremap_nocache(s_vpu_security_reg_phys[sys_idx], VPU_SECURITY_REGISTER_SIZE);
    p_security_reg_base[sys_idx] = p_base;

    if(p_base != NULL) {
        pr_info("base: %p\n", p_base);
        *(p_base+(SECURITY_SETTINGS_REG/4)) = 0x18;
        pr_info("addr: %p, val: 0x%x\n", (p_base+(SECURITY_SETTINGS_REG/4)), *(p_base+(SECURITY_SETTINGS_REG/4)));
        *(p_base+(SECURITY_SPACE3_START_REG/4)) = 0x380000;
        pr_info("addr: %p, val: 0x%x\n", (p_base+(SECURITY_SPACE3_START_REG/4)), *(p_base+(SECURITY_SPACE3_START_REG/4)));
        *(p_base+(SECURITY_SPACE3_END_REG/4)) = 0x3FFFFF;
        pr_info("addr: %p, val: 0x%x\n", (p_base+(SECURITY_SPACE3_END_REG/4)), *(p_base+(SECURITY_SPACE3_END_REG/4)));
        *(p_base+(SECURITY_SPACE4_START_REG/4)) = 0x480000;
        pr_info("addr: %p, val: 0x%x\n", (p_base+(SECURITY_SPACE4_START_REG/4)), *(p_base+(SECURITY_SPACE4_START_REG/4)));
        *(p_base+(SECURITY_SPACE4_END_REG/4)) = 0x4FFFFF;
        pr_info("addr: %p, val: 0x%x\n", (p_base+(SECURITY_SPACE4_END_REG/4)), *(p_base+(SECURITY_SPACE4_END_REG/4)));
        //iounmap(p_base);
        pr_info("set vpu memory protection..\n");
    }
#endif
}

static irqreturn_t vpu_security_irq_handler(int irq, void *dev_id)
{
    int sys_idx = 0;
    volatile int *p_base = NULL;

    for(sys_idx=0; sys_idx<2; sys_idx++) {
        if(s_vpu_security_irq[sys_idx] == irq)
            break;
    }

    if(sys_idx > 1) {
        pr_err("wrong irq in security irq %d\n", irq);
        return IRQ_HANDLED;
    }

    pr_info("VPU SECURITY IRQ : %d\n", sys_idx);
    p_base = p_security_reg_base[sys_idx];

    #define SECURITY_STATUS_INFO_START 0x200
    #define SECURITY_STATUS_INFO_END 0x300

    if(p_base != NULL) {
        int i;
        i = *(p_base+(SECURITY_INTERRUPT_STATUS_REG/4));

        if(i != 0) {
            pr_info("security interrupt status: 0x%x\n", i);
            pr_info("status info from 0x%x\n", SECURITY_STATUS_INFO_START);
            for(i=SECURITY_STATUS_INFO_START/4; i<SECURITY_STATUS_INFO_START/4 + 4; i++) {
                pr_info("0x%x, ", *(p_base + i));
            }
            pr_info("\n");
            i = *(p_base+(SECURITY_INTERRUPT_STATUS_REG/4));
            pr_info("...clear the interrupt status..\n");
            *(p_base+(SECURITY_INTERRUPT_CLEAR_REG/4)) = i;
            pr_info("interrupt status: 0x%x\n", *(p_base+(SECURITY_INTERRUPT_STATUS_REG/4)));
            *(p_base+(SECURITY_INTERRUPT_CLEAR_REG/4)) = 0;
        }
    }
    return IRQ_HANDLED;
}
#endif
static irqreturn_t vpu_irq_handler(int irq, void *dev_id)
{
    vpu_drv_context_t *dev = (vpu_drv_context_t *)dev_id;

    /* this can be removed. it also work in VPU_WaitInterrupt of API function */
    int core;
    int product_code;
    u32 intr_reason;
#ifdef SUPPORT_MULTI_INST_INTR
    u32 intr_inst_index;
#endif

    DPRINTK("[VPUDRV][+]%s, irq: %d\n", __func__, irq);

#ifdef VPU_IRQ_CONTROL
    disable_irq_nosync(irq);
#endif

    intr_reason = 0;
#ifdef SUPPORT_MULTI_INST_INTR
    intr_inst_index = 0;
#endif
#ifdef VPU_SUPPORT_ISR
    for(core=0; core< get_vpu_core_num(chip_id, video_cap); core++) {
        if(s_vpu_irq[core]==irq)
            break;
    }
#endif
    if(core>= get_vpu_core_num(chip_id, video_cap)) {
        printk(KERN_ERR "[VPUDRV] :  can't find the core index in irq_process\n");
#ifdef VPU_IRQ_CONTROL
        enable_irq(irq);
#endif
        return IRQ_HANDLED;
    }

    if (s_bit_firmware_info[core].size == 0) {/* it means that we didn't get an information the current core from API layer. No core activated.*/
        printk(KERN_ERR "[VPUDRV] :  s_bit_firmware_info[core].size is zero\n");
#ifdef VPU_IRQ_CONTROL
        enable_irq(irq);
#endif
        return IRQ_HANDLED;
    }
    product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);

    if (PRODUCT_CODE_W_SERIES(product_code)) {
        if (ReadVpuRegister(W5_VPU_VPU_INT_STS)) {
#ifdef SUPPORT_MULTI_INST_INTR
            u32 empty_inst;
            u32 done_inst;
            u32 other_inst;

            intr_reason = ReadVpuRegister(W5_VPU_VINT_REASON);
            empty_inst = ReadVpuRegister(W5_RET_BS_EMPTY_INST);
            done_inst = ReadVpuRegister(W5_RET_QUEUE_CMD_DONE_INST);
            other_inst = ReadVpuRegister(W5_RET_DONE_INSTANCE_INFO);

            DPRINTK("[VPUDRV] vpu_irq_handler reason=0x%x, empty_inst=0x%x, done_inst=0x%x, other_inst=0x%x \n", intr_reason, empty_inst, done_inst, other_inst);

            intr_inst_index = get_vpu_inst_idx(dev, &intr_reason, empty_inst, done_inst, other_inst);
            if (intr_inst_index >= 0 && intr_inst_index < MAX_NUM_INSTANCE) {
                if (intr_reason == (1 << INT_WAVE5_BSBUF_EMPTY)) {
                    empty_inst = empty_inst & ~(1 << intr_inst_index);
                    WriteVpuRegister(W5_RET_BS_EMPTY_INST, empty_inst);
                    DPRINTK("[VPUDRV]    %s, W5_RET_BS_EMPTY_INST Clear empty_inst=0x%x, intr_inst_index=%d\n", __func__, empty_inst, intr_inst_index);
                }
                if ((intr_reason == (1 << INT_WAVE5_DEC_PIC)) || (intr_reason == (1 << INT_WAVE5_INIT_SEQ)) || (intr_reason == (1 << INT_WAVE5_ENC_SET_PARAM)))
                {
                    done_inst = done_inst & ~(1 << intr_inst_index);
                    WriteVpuRegister(W5_RET_QUEUE_CMD_DONE_INST, done_inst);
                    DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST Clear done_inst=0x%x, intr_inst_index=%d\n", __func__, done_inst, intr_inst_index);
                }
#if 0
                if ((intr_reason == (1 << INT_WAVE5_ENC_LOW_LATENCY)))
                {
                    done_inst = (done_inst >> 16);
                    done_inst = done_inst & ~(1 << intr_inst_index);
                    done_inst = (done_inst << 16);
                    WriteVpuRegister(W5_RET_QUEUE_CMD_DONE_INST, done_inst);
                    DPRINTK("[VPUDRV]    %s, W5_RET_QUEUE_CMD_DONE_INST INT_WAVE5_ENC_LOW_LATENCY Clear done_inst=0x%x, intr_inst_index=%d\n", __func__, done_inst, intr_inst_index);
                }
#endif
                if (!kfifo_is_full(&s_interrupt_pending_q[core*MAX_NUM_INSTANCE+intr_inst_index])) {
                    if (intr_reason == ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY))) {
                        u32 ll_intr_reason = (1 << INT_WAVE5_DEC_PIC);
                        kfifo_in(&s_interrupt_pending_q[core*MAX_NUM_INSTANCE+intr_inst_index], &ll_intr_reason, sizeof(u32));
                    }
                    else
                        kfifo_in(&s_interrupt_pending_q[core*MAX_NUM_INSTANCE+intr_inst_index], &intr_reason, sizeof(u32));
                }
                else {
                    printk(KERN_ERR "[VPUDRV] :  kfifo_is_full kfifo_count=%d \n", kfifo_len(&s_interrupt_pending_q[core*MAX_NUM_INSTANCE+intr_inst_index]));
                }
            }
            else {
                printk(KERN_ERR "[VPUDRV] :  intr_inst_index is wrong intr_inst_index=%d \n", intr_inst_index);
            }

            WriteVpuRegister(W5_VPU_VINT_REASON_CLR, intr_reason);
#else
            dev->interrupt_reason[core] = ReadVpuRegister(W5_VPU_VINT_REASON);
            WriteVpuRegister(W5_VPU_VINT_REASON_CLR, dev->interrupt_reason[core]);
#endif

            WriteVpuRegister(W5_VPU_VINT_CLEAR, 0x1);
        }
    }
    else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {
        if (ReadVpuRegister(BIT_INT_STS)) {
#ifdef SUPPORT_MULTI_INST_INTR
            intr_reason = ReadVpuRegister(BIT_INT_REASON);
            intr_inst_index = ReadVpuRegister(BIT_RET_RUN_INDEX);
            if(intr_inst_index >= MAX_NUM_INSTANCE)
                pr_err(">>>> core: %d, intr_inst_index: %d, %08x, %08x\n", core, intr_inst_index, intr_reason, BIT_RET_RUN_INDEX);
            else
                kfifo_in(&s_interrupt_pending_q[core*MAX_NUM_INSTANCE+intr_inst_index], &intr_reason, sizeof(u32));
#else
            dev->interrupt_reason[core] = ReadVpuRegister(BIT_INT_REASON);
#endif
            WriteVpuRegister(BIT_INT_CLEAR, 0x1);
        }
    }
    else {
        DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
#ifdef VPU_IRQ_CONTROL
        enable_irq(irq);
#endif
        return IRQ_HANDLED;
    }
#ifdef SUPPORT_MULTI_INST_INTR
    DPRINTK("[VPUDRV] product: 0x%08x\n", product_code);
#else
    DPRINTK("[VPUDRV] product: 0x%08x intr_reason: 0x%08x\n", product_code, dev->interrupt_reason[core]);
#endif

#if 0
    if (dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);    /* notify the interrupt to user space */
#endif

#ifdef SUPPORT_MULTI_INST_INTR
    if (intr_inst_index >= 0 && intr_inst_index < MAX_NUM_INSTANCE) {
        s_interrupt_flag[core*MAX_NUM_INSTANCE+intr_inst_index]= 1;
        wake_up_interruptible(&s_interrupt_wait_q[core*MAX_NUM_INSTANCE+intr_inst_index]);
    }
#else
    s_interrupt_flag[core] = 1;
    wake_up_interruptible(&s_interrupt_wait_q[core]);
#endif

#ifdef VPU_IRQ_CONTROL
    enable_irq(irq);
#endif
    DPRINTK("[VPUDRV][-]%s\n", __func__);

    return IRQ_HANDLED;
}

static int vpu_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    DPRINTK("[VPUDRV][+] %s\n", __func__);

    if ((ret =  mutex_lock_interruptible(&s_vpu_lock)) == 0) {
        vpu_drv_context_all_t *p_drv_context = NULL;

    p_drv_context = vmalloc(sizeof(vpu_drv_context_all_t));//(void *)(&s_vpu_drv_context);
    filp->private_data = p_drv_context;
    if(filp->private_data == NULL) {
        pr_err("can not allocate memory for private data.\n");
        return -1;
    }

    p_drv_context->core_idx = -1;
    p_drv_context->p_vpu_context = &s_vpu_drv_context;

    if (!s_vpu_drv_context.open_count)
        bm_vpu_deassert(&vpu_rst_ctrl);

    s_vpu_drv_context.open_count++;

    mutex_unlock(&s_vpu_lock);
    }

    DPRINTK("[VPUDRV][-] %s\n", __func__);

    return ret;
}
#ifndef BM_ION_MEM
/**
 * invalidate data cache in the range
 */
static void invalidate_dcache_range(unsigned long start, unsigned long size)
{
    unsigned long end = start + size;
    asm volatile(
                 "mrs   x3, ctr_el0      \n"
                 "ubfm  x3, x3, #16, #19 \n"
                 "mov   x2, #4           \n"
                 "lsl   x2, x2, x3       \n" /* cache line size */

                 /* x2 <- minimal cache line size in cache system */
                 "sub   x3, x2, #1       \n"
                 "bic   %0, %0, x3       \n"
              "1: dc ivac, %0            \n" /* invalidate data cache */
                 "add   %0, %0, x2       \n"
                 "cmp   %0, %1           \n"
                 "b.lo  1b               \n"
                 "dsb   sy               \n"
                 :
                 :
                 "r"(start), // %0
                 "r"(end)    // %1
                 : "memory"
                 );
}
/**
 * clean & invalidate data cache in the range
 */
static void flush_dcache_range(unsigned long start, unsigned long size)
{
    unsigned long end = start + size;
    asm volatile(
                 "mrs   x3, ctr_el0   \n" /* read Cache Type Register */
                 "lsr   x3, x3, #16   \n"
                 "and   x3, x3, #0xf  \n"
                 "mov   x2, #4        \n"
                 "lsl   x2, x2, x3    \n" /* cache line size */

                 /* x2 <- minimal cache line size in cache system */
                 "sub   x3, x2, #1    \n"
                 "bic   %0, %0, x3    \n"
              "1: dc civac, %0        \n" /* clean & invalidate data cache */
                 "add   %0, %0, x2    \n"
                 "cmp   %0, %1        \n"
                 "b.lo  1b            \n"
                 "dsb   sy            \n"
                 :
                 :
                 "r"(start), // %0
                 "r"(end)    // %1
                 : "memory"
                );
}
#endif

static int _vpu_syscxt_chkinst(vpu_crst_context_t *p_crst_cxt) {

    int i = 0;
    for (; i < MAX_NUM_INSTANCE; i++) {
        int pos = p_crst_cxt->count % 2;
        pos = (p_crst_cxt->reset ? pos : 1 - pos) * 8;
        if (((p_crst_cxt->instcall[i] >> pos) & 3) != 0)
            return -1;
    }

    return 1;
}

static int _vpu_syscxt_printinst(int core_idx, vpu_crst_context_t *p_crst_cxt) {

    int i = 0;
    DPRINTK("+*********************+\n");
    for (; i < MAX_NUM_INSTANCE; i++) {
        DPRINTK("core %d, inst %d, map:0x%x.\n",
                        core_idx, i, p_crst_cxt->instcall[i]);
    }
    DPRINTK("-*********************-\n\n");
    return 1;
}

static int _vpu_syscxt_chkready(int core_idx, vpu_crst_context_t *p_crst_cxt) {

    int num = 0, i = 0;
    for(; i < MAX_NUM_INSTANCE; i++) {
        int pos = (p_crst_cxt->count % 2) * 8;
        if (((p_crst_cxt->instcall[i] >> pos) & 3) != 0)
            num += 1;
    }

    return num;
}

/*static int vpu_ioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg) // for kernel 2.6.9 of C&M*/
static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
    int ret = 0;
    vpu_drv_context_all_t *p_drv_context = (vpu_drv_context_all_t *)filp->private_data;
    struct vpu_drv_context_t *dev = p_drv_context->p_vpu_context;

    switch (cmd) {
    case VDI_IOCTL_SYSCXT_SET_EN:
    {
        vpudrv_syscxt_info_t syscxt_info;
        int i;
        vpu_crst_context_t *p_crst_cxt;
        ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
        if (ret) {
            return -EFAULT;
        }

        mutex_lock(&s_vpu_lock);
        for (i = 0; i < get_vpu_core_num(chip_id, video_cap); i++) {
            p_crst_cxt = &dev->crst_cxt[i];
            p_crst_cxt->disable = !syscxt_info.fun_en;
        }
        mutex_unlock(&s_vpu_lock);
        pr_info("[VPUDRV] crst set enable:%d.\n", syscxt_info.fun_en);
        break;
    }
    case VDI_IOCTL_SYSCXT_SET_STATUS:
    {
        vpudrv_syscxt_info_t syscxt_info;
        int core_idx;
        vpu_crst_context_t *p_crst_cxt;
        ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
        if (ret) {
            return -EFAULT;
        }

        mutex_lock(&s_vpu_lock);
        core_idx = syscxt_info.core_idx;
        p_crst_cxt = &dev->crst_cxt[core_idx];
        if ((_vpu_syscxt_chkready(core_idx, p_crst_cxt) != s_vpu_usage_info.vpu_open_ref_count[core_idx]) && (syscxt_info.core_status)) {
            pr_info("[VPUDRV] core %d not ready! status: %d\n", core_idx, p_crst_cxt->status);
            mutex_unlock(&s_vpu_lock);
            break;
        }
        p_crst_cxt->status = p_crst_cxt->disable ? SYSCXT_STATUS_WORKDING : syscxt_info.core_status;
        mutex_unlock(&s_vpu_lock);

        if (p_crst_cxt->status) {
            pr_info("[VPUDRV] core %d set status:%d. |********************|\n",
                                                core_idx, p_crst_cxt->status);

        } else {
            pr_info("[VPUDRV] core %d set status:%d.\n",
                                                core_idx, p_crst_cxt->status);
        }

        break;
    }
    case VDI_IOCTL_SYSCXT_GET_STATUS:
    {
        vpudrv_syscxt_info_t syscxt_info;
        int core_idx;
        vpu_crst_context_t *p_crst_cxt;
        ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
        if (ret) {
            return -EFAULT;
        }

        mutex_lock(&s_vpu_lock);
        core_idx = syscxt_info.core_idx;
        p_crst_cxt = &dev->crst_cxt[core_idx];
        syscxt_info.core_status = p_crst_cxt->status;
        mutex_unlock(&s_vpu_lock);

        ret = copy_to_user((void __user *)arg, &syscxt_info, sizeof(vpudrv_syscxt_info_t));
        if (ret != 0) {
            return -EFAULT;
        }
        break;
    }
    case VDI_IOCTL_SYSCXT_CHK_STATUS:
    {
        vpudrv_syscxt_info_t syscxt_info;
        int core_idx;
        int instid, shift, mask;
        vpu_crst_context_t *p_crst_cxt;
        ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
        if (ret) {
            return -EFAULT;
        }

        mutex_lock(&s_vpu_lock);
        core_idx = syscxt_info.core_idx;
        instid = syscxt_info.inst_idx;
        p_crst_cxt = &dev->crst_cxt[core_idx];
        shift = (p_crst_cxt->count % 2) * 8;
        mask = (1u << syscxt_info.pos) << shift;
        if ((SYSCXT_STATUS_WORKDING == p_crst_cxt->status) && (syscxt_info.is_sleep < 0)) {

            int __num = _vpu_syscxt_chkready(core_idx, p_crst_cxt);
            if ((p_crst_cxt->instcall[instid] & mask) == 0) {
                pr_info("[VPUDRV] core %d instant %d mask:0x%x. pos %d, reset: %d, inst num: %d, chk num:%d\n",
                    core_idx, instid, mask, p_crst_cxt->count%2, p_crst_cxt->reset, s_vpu_usage_info.vpu_open_ref_count[core_idx], __num);
            }

            p_crst_cxt->instcall[instid] |= mask;
            syscxt_info.is_all_block = 0;
            syscxt_info.is_sleep = 0;
            p_crst_cxt->reset = __num == s_vpu_usage_info.vpu_open_ref_count[core_idx] ? 1 : p_crst_cxt->reset;
        } else {
            p_crst_cxt->instcall[instid] &= ~mask;
            syscxt_info.is_sleep = 1;
            syscxt_info.is_all_block = 0;
            if (_vpu_syscxt_chkinst(p_crst_cxt) == 1) {
                syscxt_info.is_all_block = 1;

                if (p_crst_cxt->reset == 1) {
                    vpu_hw_reset(core_idx);
                    p_crst_cxt->reset = 0;
                    p_crst_cxt->count += 1;
                }

                pr_info("[VPUDRV] core %d instant %d will re-start. pos %d\n", core_idx, instid, 1-(p_crst_cxt->count%2));
                _vpu_syscxt_printinst(core_idx, p_crst_cxt);
            }

            DPRINTK("[VPUDRV] core: %d, inst: %d, pos:%d, instcall %d, status: %d, sleep:%d, wakeup:%d.\n",
                                    core_idx, instid, syscxt_info.pos, p_crst_cxt->instcall[instid],
                                    p_crst_cxt->status, syscxt_info.is_sleep, syscxt_info.is_all_block);
        }
        mutex_unlock(&s_vpu_lock);

        ret = copy_to_user((void __user *)arg, &syscxt_info, sizeof(vpudrv_syscxt_info_t));
        if (ret != 0) {
            return -EFAULT;
        }
        break;
    }
    case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
        {
            vpudrv_buffer_pool_t *vbp;

            DPRINTK("[VPUDRV][+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

            if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
                vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
                if (!vbp) {
                    up(&s_vpu_sem);
                    return -ENOMEM;
                }

                ret = copy_from_user(&(vbp->vb), (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
                if (ret) {
                    kfree(vbp);
                    up(&s_vpu_sem);
                    return -EFAULT;
                }

                ret = vpu_alloc_dma_buffer(&(vbp->vb));
                if (ret == -1) {
                    ret = -ENOMEM;
                    kfree(vbp);
                    up(&s_vpu_sem);
                    break;
                }
                ret = copy_to_user((void __user *)arg, &(vbp->vb), sizeof(vpudrv_buffer_t));
                if (ret) {
                    kfree(vbp);
                    ret = -EFAULT;
                    up(&s_vpu_sem);
                    break;
                }

                vbp->filp = filp;
                list_add(&vbp->list, &s_vbp_head);

                up(&s_vpu_sem);
            }
            else {
                return -ERESTARTSYS;
            }
            DPRINTK("[VPUDRV][-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
        }
        break;
    case VDI_IOCTL_FREE_PHYSICALMEMORY:
        {
            vpudrv_buffer_pool_t *vbp, *n;
            vpudrv_buffer_t vb;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

            if ((ret = down_interruptible(&s_vpu_sem)) == 0) {

                ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
                if (ret) {
                    up(&s_vpu_sem);
                    return -EACCES;
                }

                if (vb.base)
                    vpu_free_dma_buffer(&vb);

                    list_for_each_entry_safe(vbp, n, &s_vbp_head, list)
                    {
                        if (vbp->vb.base == vb.base) {
                            list_del(&vbp->list);
                            kfree(vbp);
                            break;
                        }
                    }
                up(&s_vpu_sem);
            }
            else {
                return -ERESTARTSYS;
            }
            DPRINTK("[VPUDRV][-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

        }
        break;
    case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
        {
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
            if (s_video_memory.base != 0) {
                ret = copy_to_user((void __user *)arg, &s_video_memory, sizeof(vpudrv_buffer_t));
                if (ret != 0)
                    ret = -EFAULT;
            } else {
                ret = -EFAULT;
            }
            DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
#endif
        }
        break;

    case VDI_IOCTL_GET_FREE_MEM_SIZE:
        {
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
            vmem_info_t vinfo;
            unsigned long size;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_FREE_MEM_SIZE\n");
            vmem_get_info(&s_vmem, &vinfo);

            size = vinfo.free_pages * vinfo.page_size;
            ret = copy_to_user((void __user *)arg, &size, sizeof(unsigned long));
            if (ret != 0)
                ret = -EFAULT;
            DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_FREE_MEM_SIZE\n");
#endif
        }
        break;

    case VDI_IOCTL_WAIT_INTERRUPT:
        {
#ifdef SUPPORT_TIMEOUT_RESOLUTION
            ktime_t kt;
#endif
            vpudrv_intr_info_t info;
#ifdef SUPPORT_MULTI_INST_INTR
            u32 intr_inst_index;
            u32 intr_reason_in_q;
            u32 interrupt_flag_in_q;
#endif
            u32 core_idx;

            DPRINTK("[VPUDRV][+]VDI_IOCTL_WAIT_INTERRUPT\n");
            ret = copy_from_user(&info, (vpudrv_intr_info_t *)arg, sizeof(vpudrv_intr_info_t));
            if (ret != 0) {
                printk(KERN_ERR "[VPUDRV] copy_from_user failed\n");
                return -EFAULT;
            }

            core_idx = info.core_idx;
            DPRINTK("[VPUDRV]--core index: %d\n", core_idx);

            atomic_inc(&s_vpu_usage_info.vpu_busy_status[core_idx]);
#ifdef SUPPORT_MULTI_INST_INTR
            intr_inst_index = info.intr_inst_index;
            //fixbug SC7-8:VPU code interrupt is disordered. See details https://wiki.sophgo.com/pages/viewpage.action?pageId=52766832(Question 28)
            //intr_reason_in_q = 0;
            //interrupt_flag_in_q = kfifo_out(&s_interrupt_pending_q[core_idx*MAX_NUM_INSTANCE+intr_inst_index], &intr_reason_in_q, sizeof(u32));
            //if (interrupt_flag_in_q > 0) {
            //    dev->interrupt_reason[core_idx*MAX_NUM_INSTANCE+intr_inst_index] = intr_reason_in_q;
            //    DPRINTK("[VPUDRV] Interrupt Remain : intr_inst_index=%d, intr_reason_in_q=0x%x, interrupt_flag_in_q=%d\n", intr_inst_index, intr_reason_in_q, interrupt_flag_in_q);
            //    goto INTERRUPT_REMAIN_IN_QUEUE;
            //}
#endif

#ifdef SUPPORT_MULTI_INST_INTR
#ifdef SUPPORT_TIMEOUT_RESOLUTION
            kt =  ktime_set(0, info.timeout*1000*1000);
            ret = wait_event_interruptible_hrtimeout(s_interrupt_wait_q[core_idx*MAX_NUM_INSTANCE+intr_inst_index], s_interrupt_flag[core_idx*MAX_NUM_INSTANCE+intr_inst_index] != 0, kt);
#else
            ret = wait_event_interruptible_timeout(s_interrupt_wait_q[core_idx*MAX_NUM_INSTANCE+intr_inst_index], s_interrupt_flag[core_idx*MAX_NUM_INSTANCE+intr_inst_index] != 0, msecs_to_jiffies(info.timeout));
#endif
#else
            ret = wait_event_interruptible_timeout(s_interrupt_wait_q[core_idx], s_interrupt_flag[core_idx] != 0, msecs_to_jiffies(info.timeout));
#endif
#ifdef SUPPORT_TIMEOUT_RESOLUTION
            if (ret == -ETIME) {
                //DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d \n", info.timeout);
                atomic_dec(&s_vpu_usage_info.vpu_busy_status[core_idx]);
                break;
            }
#else
            if (!ret) {
                ret = -ETIME;
                atomic_dec(&s_vpu_usage_info.vpu_busy_status[core_idx]);
                //DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d \n", info.timeout);
                break;
            }
#endif
            if (signal_pending(current)) {
                printk(KERN_ERR "[VPUDRV] signal_pending failed\n");
                ret = -ERESTARTSYS;
                atomic_dec(&s_vpu_usage_info.vpu_busy_status[core_idx]);
                break;
            }

#ifdef SUPPORT_MULTI_INST_INTR
            intr_reason_in_q = 0;
            interrupt_flag_in_q = kfifo_out(&s_interrupt_pending_q[core_idx*MAX_NUM_INSTANCE+intr_inst_index], &intr_reason_in_q, sizeof(u32));
            if (interrupt_flag_in_q > 0) {
                dev->interrupt_reason[core_idx*MAX_NUM_INSTANCE+intr_inst_index] = intr_reason_in_q;
            }
            else {
                dev->interrupt_reason[core_idx*MAX_NUM_INSTANCE+intr_inst_index] = 0;
            }
#endif

#ifdef SUPPORT_MULTI_INST_INTR
            DPRINTK("[VPUDRV] inst_index(%d), s_interrupt_flag(%d), reason(0x%08lx)\n", intr_inst_index, s_interrupt_flag[core_idx*MAX_NUM_INSTANCE+intr_inst_index], dev->interrupt_reason[core_idx*MAX_NUM_INSTANCE+intr_inst_index]);
#else
            DPRINTK("[VPUDRV]    s_interrupt_flag(%d), reason(0x%08lx)\n", s_interrupt_flag[core_idx], dev->interrupt_reason[core_idx]);
#endif

//INTERRUPT_REMAIN_IN_QUEUE:

#ifdef SUPPORT_MULTI_INST_INTR
            info.intr_reason = dev->interrupt_reason[core_idx*MAX_NUM_INSTANCE+intr_inst_index];
            s_interrupt_flag[core_idx*MAX_NUM_INSTANCE+intr_inst_index] = 0;
            dev->interrupt_reason[core_idx*MAX_NUM_INSTANCE+intr_inst_index] = 0;
#else
            info.intr_reason = dev->interrupt_reason[core_idx];
            s_interrupt_flag[core_idx] = 0;
            dev->interrupt_reason[core_idx] = 0;
#endif
            atomic_dec(&s_vpu_usage_info.vpu_busy_status[core_idx]);

            ret = copy_to_user((void __user *)arg, &info, sizeof(vpudrv_intr_info_t));
            DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT\n");
            if (ret != 0) {
                printk(KERN_ERR "[VPUDRV] copy_to_user failed\n");
                return -EFAULT;
            }
        }
        break;
    case VDI_IOCTL_SET_CLOCK_GATE:
        {
            u32 clkgate;

            DPRINTK("[VPUDRV][+]VDI_IOCTL_SET_CLOCK_GATE\n");
            if (get_user(clkgate, (u32 __user *) arg))
                return -EFAULT;
#ifdef VPU_SUPPORT_CLOCK_CONTROL
            if (clkgate)
                vpu_clk_enable(s_vpu_clk);
            else
                vpu_clk_disable(s_vpu_clk);
#endif
            DPRINTK("[VPUDRV][-]VDI_IOCTL_SET_CLOCK_GATE\n");

        }
        break;
    case VDI_IOCTL_GET_INSTANCE_POOL:
        {
            vpudrv_buffer_t info;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_INSTANCE_POOL\n");
            ret = copy_from_user(&info, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
            if (ret != 0)
                return -EFAULT;
            p_drv_context->core_idx = info.core_idx;//info.base;
            if(info.core_idx >=  get_vpu_core_num(chip_id, video_cap))
                return -EFAULT;

            if ((ret =  mutex_lock_interruptible(&s_vpu_lock)) == 0) {
                if (s_instance_pool[info.core_idx].base != 0) {
                    ret = copy_to_user((void __user *)arg, &s_instance_pool[info.core_idx], sizeof(vpudrv_buffer_t));
                    if (ret != 0)
                        ret = -EFAULT;
                } else {
                    ret = copy_from_user(&s_instance_pool[info.core_idx], (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
                    if (ret == 0) {
    #ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
                    //s_instance_pool.size = PAGE_ALIGN(s_instance_pool.size);
                    s_instance_pool[info.core_idx].base = (unsigned long)vmalloc(s_instance_pool[info.core_idx].size);
                    s_instance_pool[info.core_idx].phys_addr = (unsigned long)vmalloc_to_pfn((void *)s_instance_pool[info.core_idx].base) << PAGE_SHIFT;

                    if (s_instance_pool[info.core_idx].base != 0)
    #else

                        if (vpu_alloc_dma_buffer(&s_instance_pool[info.core_idx]) != -1)
    #endif

                        {
                            memset((void *)s_instance_pool[info.core_idx].base, 0x0, s_instance_pool[info.core_idx].size); /*clearing memory*/
                            ret = copy_to_user((void __user *)arg, &s_instance_pool[info.core_idx], sizeof(vpudrv_buffer_t));
                            if (ret == 0) {
                                /* success to get memory for instance pool */
                                DPRINTK("[VPUDRV][-] VDI_IOCTL_GET_INSTANCE_POOL size: %d\n", s_instance_pool[info.core_idx].size);
                                mutex_unlock(&s_vpu_lock);
                                break;
                            }
                        }

                    }
                    ret = -EFAULT;
                }

                mutex_unlock(&s_vpu_lock);
            }
            else {
                return -ERESTARTSYS;
            }

            DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_INSTANCE_POOL\n");
        }
        break;
    case VDI_IOCTL_GET_COMMON_MEMORY:
        {
            vpudrv_buffer_t vdb = {0};
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_COMMON_MEMORY\n");
            ret = copy_from_user(&vdb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
            if (ret != 0) {
                ret = -EFAULT;
                pr_err("[%s,%d] copy_from_user failed!\n", __func__, __LINE__);
                break;
            }
            if (vdb.core_idx >=  get_vpu_core_num(chip_id, video_cap)) {
                ret = -EFAULT;
                pr_err("[%s,%d] core_idx incorrect!\n", __func__, __LINE__);
                break;
            }

            if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
                if (s_common_memory[vdb.core_idx].base == 0) {
                    memcpy(&s_common_memory[vdb.core_idx], &vdb, sizeof(vpudrv_buffer_t));
                    ret = vpu_alloc_dma_buffer(&s_common_memory[vdb.core_idx]);
                }
                if(ret == 0) {
                    ret = copy_to_user((void __user *)arg, &s_common_memory[vdb.core_idx], sizeof(vpudrv_buffer_t));
                }
                else {
                    ret = -EFAULT;
                }
                up(&s_vpu_sem);
            }
            DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_COMMON_MEMORY\n");
        }
        break;
    case VDI_IOCTL_OPEN_INSTANCE:
        {
            vpudrv_inst_info_t inst_info;
            vpudrv_instanace_list_t *vil;

            vil = kzalloc(sizeof(*vil), GFP_KERNEL);
            if (!vil)
                return -ENOMEM;

            if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t))) {
                kfree(vil);
                return -EFAULT;
            }

            vil->inst_idx = inst_info.inst_idx;
            vil->core_idx = inst_info.core_idx;
            vil->filp = filp;

            if ((ret =  mutex_lock_interruptible(&s_vpu_lock)) == 0) {
                list_add(&vil->list, &s_inst_list_head);

                s_vpu_usage_info.vpu_open_ref_count[vil->core_idx]++; /* flag just for that vpu is in opened or closed */
                inst_info.inst_open_count = s_vpu_usage_info.vpu_open_ref_count[vil->core_idx];
#ifdef SUPPORT_MULTI_INST_INTR
                kfifo_reset(&s_interrupt_pending_q[inst_info.core_idx*MAX_NUM_INSTANCE+inst_info.inst_idx]);
                s_interrupt_flag[inst_info.core_idx*MAX_NUM_INSTANCE+inst_info.inst_idx] = 0;
                release_vpu_create_inst_flag(inst_info.core_idx, inst_info.inst_idx);
#else
                s_interrupt_flag[inst_info.core_idx] = 0;
#endif
                mutex_unlock(&s_vpu_lock);
            }
            else {
                kfree(vil);
            }
            if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t))) {
                                kfree(vil);
                return -EFAULT;
                        }

            DPRINTK("[VPUDRV] VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d, vpu_open_ref_count=%d, inst_open_count=%d\n", \
                    (int)inst_info.core_idx, (int)inst_info.inst_idx, s_vpu_usage_info.vpu_open_ref_count[inst_info.core_idx], inst_info.inst_open_count);
        }
        break;
    case VDI_IOCTL_CLOSE_INSTANCE:
        {
            vpudrv_inst_info_t inst_info;
            vpudrv_instanace_list_t *vil, *n;

            DPRINTK("[VPUDRV][+]VDI_IOCTL_CLOSE_INSTANCE\n");

            if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t)))
                return -EFAULT;

            if ((ret =  mutex_lock_interruptible(&s_vpu_lock)) == 0) {
                list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
                {
                    if (vil->inst_idx == inst_info.inst_idx && vil->core_idx == inst_info.core_idx) {
                        s_vpu_usage_info.vpu_open_ref_count[vil->core_idx]--; /* flag just for that vpu is in opened or closed */
                        inst_info.inst_open_count = s_vpu_usage_info.vpu_open_ref_count[vil->core_idx]; /* counting the current open instance number */
                        list_del(&vil->list);
                        kfree(vil);
                        dev->crst_cxt[inst_info.core_idx].instcall[inst_info.inst_idx] = 0;
                        break;
                    }
                }
                mutex_unlock(&s_vpu_lock);
            }
            if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t)))
                return -EFAULT;

            DPRINTK("[VPUDRV][-] VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d, vpu_open_ref_count=%d, inst_open_count=%d\n", \
                    (int)inst_info.core_idx, (int)inst_info.inst_idx, s_vpu_usage_info.vpu_open_ref_count[inst_info.core_idx], inst_info.inst_open_count);
        }
        break;
    case VDI_IOCTL_GET_INSTANCE_NUM:
        {
            vpudrv_inst_info_t inst_info;
            vpudrv_instanace_list_t *vil, *n;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_INSTANCE_NUM\n");

            ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t));
            if (ret != 0)
                break;

            if ((ret =  mutex_lock_interruptible(&s_vpu_lock)) == 0) {
                inst_info.inst_open_count = 0;
                list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
                {
                    if (vil->core_idx == inst_info.core_idx)
                        inst_info.inst_open_count++;
                }
                mutex_unlock(&s_vpu_lock);
            }

            ret = copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t));

            DPRINTK("[VPUDRV] VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n", (int)inst_info.core_idx, (int)inst_info.inst_idx, inst_info.inst_open_count);

        }
        break;
    case VDI_IOCTL_RESET:
        {
            int core_idx;
            ret = copy_from_user(&core_idx, (int *)arg, sizeof(int));
            if (ret != 0)
                break;

            vpu_hw_reset(core_idx);
        }
        break;
    case VDI_IOCTL_GET_REGISTER_INFO:
        {
            u32 core_idx;
            vpudrv_buffer_t info;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_REGISTER_INFO\n");
            ret = copy_from_user(&info, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
            if (ret != 0)
                return -EFAULT;
            core_idx = info.size;
            DPRINTK("[VPUDRV]--core index: %d\n", core_idx);
            ret = copy_to_user((void __user *)arg, &s_vpu_register[core_idx], sizeof(vpudrv_buffer_t));
            if (ret != 0)
                ret = -EFAULT;
            DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_REGISTER_INFO s_vpu_register.phys_addr==0x%lx, s_vpu_register.virt_addr=0x%lx, s_vpu_register.size=%d\n", s_vpu_register[core_idx].phys_addr , s_vpu_register[core_idx].virt_addr, s_vpu_register[core_idx].size);
        }
        break;
    case VDI_IOCTL_GET_FIRMWARE_STATUS:
        {
            u32 core_idx;

            DPRINTK("[VPUDRV][+]VDI_IOCTL_SET_CLOCK_GATE\n");
            if (get_user(core_idx, (u32 __user *) arg))
                return -EFAULT;

            if (core_idx >=  get_vpu_core_num(chip_id, video_cap))
                return -EFAULT;
            if(s_init_flag[core_idx] == 0)
                ret = 100;
        }
        break;
#ifndef BM_ION_MEM
    case VDI_IOCTL_FLUSH_DCACHE:
        {
            vpudrv_buffer_t info;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_FLUSH_DCACHE\n");
            ret = copy_from_user(&info, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
            if (ret != 0)
                return -EFAULT;
            if((info.phys_addr >= s_video_memory.phys_addr) && (info.phys_addr + info.size <= s_video_memory.size + s_video_memory.phys_addr))
            {
                info.base = s_video_memory.base + (info.phys_addr - s_video_memory.phys_addr);
                flush_dcache_range(info.base, info.size);
            }
            else {
                pr_info("the phys error maybe : 0x%lx\n", info.phys_addr);
            }
            DPRINTK("[VPUDRV][-]VDI_IOCTL_FLUSH_DCACHE phys_addr==0x%lx, ker_addr=0x%lx, size=%d\n", info.phys_addr , info.base, info.size);
        }
        break;
    case VDI_IOCTL_INVALIDATE_DCACHE:
        {
            vpudrv_buffer_t info;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_FLUSH_DCACHE\n");
            ret = copy_from_user(&info, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
            if (ret != 0)
                return -EFAULT;
            if((info.phys_addr >= s_video_memory.phys_addr) && (info.phys_addr + info.size <= s_video_memory.size + s_video_memory.phys_addr))
            {
                info.base = s_video_memory.base + (info.phys_addr - s_video_memory.phys_addr);
                invalidate_dcache_range(info.base, info.size);
            }
            else {
                pr_info("the phys error maybe : 0x%lx\n", info.phys_addr);
            }
            DPRINTK("[VPUDRV][-]VDI_IOCTL_FLUSH_DCACHE phys_addr==0x%lx, ker_addr=0x%lx, size=%d\n", info.phys_addr , info.base, info.size);
        }
        break;
#endif
    case VDI_IOCTL_GET_VIDEO_CAP:
        {
            ret = copy_to_user((void __user *)arg, &video_cap, sizeof(int));
            if (ret != 0)
                return -EFAULT;
        }
        break;

    case VDI_IOCTL_GET_BIN_TYPE:
        {
            //vpudrv_buffer_t info;
            int bin_type = 0;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_BIN_TYPE\n");
            efuse_ft_get_bin_type(&bin_type);
            ret = copy_to_user((void __user *)arg, &bin_type, sizeof(int));
            if (ret != 0)
                return -EFAULT;
        }
        break;
    case VDI_IOCTL_GET_CHIP_ID:
        {
            if(chip_id <= 0)
                printk(KERN_ERR "[VPUDRV] VDI_IOCTL_GET_CHIP_ID, chip id invalid 0x%x\n", chip_id);
            ret = copy_to_user((void __user *)arg, &chip_id, sizeof(int));
            if (ret != 0)
                return -EFAULT;
        }
        break;
    case VDI_IOCTL_GET_MAX_CORE_NUM:
        {
            int max_core_num = get_vpu_core_num(chip_id,video_cap);
            ret = copy_to_user((void __user *)arg, &max_core_num, sizeof(int));
            if (ret != 0)
                return -EFAULT;
        }
        break;
    case VDI_IOCTL_CTRL_KERNEL_RESET:
        {
            vpudrv_reset_flag reset_flag;
            DPRINTK("[VPUDRV][+]VDI_IOCTL_CTRL_KERNEL_RESET\n");
            ret = copy_from_user(&reset_flag, (vpudrv_reset_flag *)arg, sizeof(vpudrv_reset_flag));
            if (ret != 0)
                return -EFAULT;

            if (reset_flag.core_idx < 0 || reset_flag.core_idx >= get_vpu_core_num(chip_id, video_cap))
                return -EFAULT;
            s_vpu_drv_context.reset_vpu_core_disable[reset_flag.core_idx] = reset_flag.reset_core_disable;
            DPRINTK("[VPUDRV][-]VDI_IOCTL_CTRL_KERNEL_RESET\n");
        }
        break;
    default:
        {
            printk(KERN_ERR "[VPUDRV] No such IOCTL, cmd is %d\n", cmd);
        }
        break;
    }
    return ret;
}

static ssize_t vpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    if(!buf) {
        printk(KERN_ERR "[VPUDRV] vpu_read buf = NULL error \n");
        return -EFAULT;
    }

    if (len == sizeof(vpudrv_regrw_info_t))
    {
        vpudrv_regrw_info_t *vpu_reg_info;
        void *p_reg_region = NULL;

        vpu_reg_info = kmalloc(sizeof(vpudrv_regrw_info_t), GFP_KERNEL);
        if (!vpu_reg_info)
        {
            printk(KERN_ERR "[VPUDRV] vpu_read  vpudrv_regrw_info allocation error \n");
            return -EFAULT;
        }
        if (copy_from_user(vpu_reg_info, buf, len))
        {
            printk(KERN_ERR "[VPUDRV] vpu_read copy_from_user error for vpudrv_regrw_info\n");
            kfree(vpu_reg_info);
            return -EFAULT;
        }
        if (vpu_reg_info->size > 0 && vpu_reg_info->size <= 16 && !(vpu_reg_info->size&0x3))        // maximum write 16 byte for one time
        {
            int i;
            p_reg_region = ioremap_nocache(vpu_reg_info->reg_base, vpu_reg_info->size);
            for (i = 0; i < vpu_reg_info->size; i+=sizeof(int))
            {
                unsigned int value;
                value = readl(p_reg_region + i);
                vpu_reg_info->value[i] = value & vpu_reg_info->mask[i];
            }
            if (copy_to_user(buf, vpu_reg_info, len))
            {
                printk(KERN_ERR "[VPUDRV] vpu_read copy_to_user error for vpudrv_regrw_info\n");
                if (p_reg_region) iounmap(p_reg_region);
                if (vpu_reg_info) kfree(vpu_reg_info);
                return -EFAULT;
            }

            if (p_reg_region) iounmap(p_reg_region);
            if (vpu_reg_info) kfree(vpu_reg_info);
            return len;
        }
        else
            printk(KERN_ERR "[VPUDRV] vpu_read invalid size %d\n", vpu_reg_info->size);

        if (vpu_reg_info) kfree(vpu_reg_info);
    }

    return -1;
}

static ssize_t vpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
    int ret;
    /* DPRINTK("[VPUDRV] vpu_write len=%d\n", (int)len); */
    if (!buf) {
        printk(KERN_ERR "[VPUDRV] vpu_write buf = NULL error \n");
        return -EFAULT;
    }
    if ((ret =  mutex_lock_interruptible(&s_vpu_lock)) == 0) {
    if (len == sizeof(vpu_bit_firmware_info_t))    {
        vpu_bit_firmware_info_t *bit_firmware_info;

        bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t), GFP_KERNEL);
        if (!bit_firmware_info) {
            mutex_unlock(&s_vpu_lock);
            printk(KERN_ERR "[VPUDRV] vpu_write  bit_firmware_info allocation error \n");
            return -EFAULT;
        }

        if (copy_from_user(bit_firmware_info, buf, len)) {
            mutex_unlock(&s_vpu_lock);
            printk(KERN_ERR "[VPUDRV] vpu_write copy_from_user error for bit_firmware_info\n");
            return -EFAULT;
        }

        if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
            DPRINTK("[VPUDRV] vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
            bit_firmware_info->core_idx, (int)bit_firmware_info->reg_base_offset, bit_firmware_info->size, bit_firmware_info->bit_code[0]);

            if (bit_firmware_info->core_idx >  get_vpu_core_num(chip_id, video_cap)) {
                mutex_unlock(&s_vpu_lock);
                printk(KERN_ERR "[VPUDRV] vpu_write coreIdx[%d] is exceeded than  MAX_NUM_VPU_CORE[%d]\n", bit_firmware_info->core_idx, get_vpu_core_num(chip_id, video_cap));
                return -ENODEV;
            }
            if(s_bit_firmware_info[bit_firmware_info->core_idx].size != bit_firmware_info->size)
                memcpy((void *)&s_bit_firmware_info[bit_firmware_info->core_idx], bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
            s_init_flag[bit_firmware_info->core_idx] = s_bit_firmware_info[bit_firmware_info->core_idx].size;
            kfree(bit_firmware_info);
            mutex_unlock(&s_vpu_lock);
            return len;
        }

        kfree(bit_firmware_info);
    }
    else if (len == sizeof(vpudrv_regrw_info_t))
    {
        vpudrv_regrw_info_t *vpu_reg_info;
        void *p_reg_region = NULL;

        vpu_reg_info = kmalloc(sizeof(vpudrv_regrw_info_t), GFP_KERNEL);
        if (!vpu_reg_info)
        {
            mutex_unlock(&s_vpu_lock);
            printk(KERN_ERR "[VPUDRV] vpu_write  vpudrv_regrw_info allocation error \n");
            return -EFAULT;
        }
        if (copy_from_user(vpu_reg_info, buf, len))
        {
            mutex_unlock(&s_vpu_lock);
            printk(KERN_ERR "[VPUDRV] vpu_write copy_from_user error for vpudrv_regrw_info\n");
            if(vpu_reg_info) kfree(vpu_reg_info);
            return -EFAULT;
        }
        if (vpu_reg_info->size > 0 && vpu_reg_info->size <= 16 && !(vpu_reg_info->size&0x3))    // maximum write 16 byte for one time
        {
            int i;
            p_reg_region = ioremap_nocache(vpu_reg_info->reg_base, vpu_reg_info->size);
            for (i = 0; i < vpu_reg_info->size; i+=sizeof(int))
            {
                unsigned int value;
                value = readl(p_reg_region + i);
                value = value & ~vpu_reg_info->mask[i];
                value = value | (vpu_reg_info->value[i] & vpu_reg_info->mask[i]);
                writel(value, p_reg_region + i);
            }
            if (p_reg_region)
                iounmap(p_reg_region);
            if (vpu_reg_info)
                kfree(vpu_reg_info);
            mutex_unlock(&s_vpu_lock);
            return len;
        }
        else
            printk(KERN_ERR "[VPUDRV] vpu_write invalid size %d\n", vpu_reg_info->size);

        if (vpu_reg_info)
            kfree(vpu_reg_info);
    }
    mutex_unlock(&s_vpu_lock);
    }
    return ret;
}

static int vpu_fasync(int fd, struct file *filp, int mode)
{
    struct vpu_drv_context_t *dev = ((struct vpu_drv_context_all_t *)filp->private_data)->p_vpu_context;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static long get_exception_instance_info(struct file *filp)
{
    vpudrv_instanace_list_t *vil, *n;
    long core_idx = -1;
    long ret = 0;

    list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
    {
        if (vil->filp == filp) {
            if(core_idx == -1) {
                core_idx = vil->core_idx;
            }
            else if(core_idx != vil->core_idx) {
                pr_err("please check the release core index: %ld ....\n", vil->core_idx);
            }
            if(vil->inst_idx>31)
                pr_err("please check the release core index too big. inst_idx : %ld\n", vil->inst_idx);
            ret = ret | (1UL<<vil->inst_idx);
        }
    }

    if(core_idx != -1) {
        ret = ret | (core_idx << 32);
    }

    return ret;
}

static void close_vpu_instance(long flags, struct file *filp)
{
    int core_idx, i;
    if(flags == 0)
        return;

    core_idx = (flags >> 32) & 0xff;
    if(core_idx >=  get_vpu_core_num(chip_id, video_cap)) {
        pr_err("[VPUDRV] please check the core_idx in close wave.(%d)\n", core_idx);
        return;
    }

    for(i=0; i<MAX_NUM_INSTANCE; i++) {
        if((flags &(1UL<<i)) != 0) {
            //close the vpu instance
            int count = 0;
            while(1)
            {
                int ret;
                get_lock(core_idx);
                ret = CloseInstanceCommand(core_idx, i);
                release_lock(core_idx);
                if (ret != 0)
                {
                    msleep(1);	// delay more to give idle time to OS;
                    count += 1;
                    if(count > 5) {
                        pr_info("can not stop instances core %d inst %d", (int)core_idx, i);
                        return; //do not close the core inst because the core exception.. maybe...
                    }
                    continue; // means there is command which should be flush.
                }
                break;
            }
        }
    }
}

static int vpu_release(struct inode *inode, struct file *filp)
{
    int ret = 0;
    vpu_drv_context_all_t *p_drv_context = (vpu_drv_context_all_t *)filp->private_data;
    int core_idx =  get_vpu_core_num(chip_id, video_cap);
    u32 open_count;
    unsigned long except_info = 0;
    int vpu_disable_reset_flag_sum = 0;

    DPRINTK("[VPUDRV] vpu_release\n");

    mutex_lock(&s_vpu_lock);
    except_info = get_exception_instance_info(filp);
    mutex_unlock(&s_vpu_lock);
    close_vpu_instance(except_info, filp);

    core_idx = p_drv_context->core_idx;
    if (core_idx <  get_vpu_core_num(chip_id, video_cap) && core_idx >= 0) {
        get_lock(core_idx);
    }

    down(&s_vpu_sem);
    /* found and free the not handled buffer by user applications */
    vpu_free_buffers(filp);
    up(&s_vpu_sem);

    mutex_lock(&s_vpu_lock);
    ret = vpu_free_instances(filp);
    mutex_unlock(&s_vpu_lock);

    if(ret > 0)
        clean_exception_inst_info();

    if (core_idx <  get_vpu_core_num(chip_id, video_cap) && core_idx >= 0) {
        release_lock(core_idx);
    }

    if(core_idx <  get_vpu_core_num(chip_id, video_cap) && core_idx >= 0 && get_disp_lock(core_idx) == 1) {
        release_disp_lock(core_idx);
    }

    mutex_lock(&s_vpu_lock);
    {
        s_vpu_drv_context.open_count--;
        open_count = s_vpu_drv_context.open_count;

        for (core_idx=0; core_idx <  get_vpu_core_num(chip_id, video_cap); core_idx++){
            if ((s_vpu_drv_context.reset_vpu_core_disable[core_idx]==current->tgid) || (s_vpu_drv_context.reset_vpu_core_disable[core_idx]==current->pid))
                s_vpu_drv_context.reset_vpu_core_disable[core_idx] = 0;

            if (s_vpu_drv_context.reset_vpu_core_disable[core_idx] != 0)
                vpu_disable_reset_flag_sum++;
        }

        if (open_count == 0 && vpu_disable_reset_flag_sum == 0) { //in pcie driver, using the sum of vpu_open_ref_count instead of open_count because pcie drv is not independent(ko).
            memset(&s_vpu_drv_context.crst_cxt[0], 0, sizeof(vpu_crst_context_t) *  get_vpu_core_num(chip_id, video_cap));
            bm_vpu_assert(&vpu_rst_ctrl);

            for(core_idx=0; core_idx <  get_vpu_core_num(chip_id, video_cap); core_idx++) {
                if (s_instance_pool[core_idx].base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
                    vfree((const void *)s_instance_pool[core_idx].base);
#else
                    vpu_free_dma_buffer(&s_instance_pool[core_idx]);
#endif
                    s_instance_pool[core_idx].base = 0;
                    s_instance_pool[core_idx].phys_addr = 0;
                }
                if (s_common_memory[core_idx].base) {
                    vpu_free_dma_buffer(&s_common_memory[core_idx]);
                    s_common_memory[core_idx].base = 0;
                }
            }
        }
#if 1
#if defined(CHIP_BM1682)
        if(ret > 0 && s_vpu_usage_info.vpu_open_ref_count[ret-1] == 0) {
            printk(KERN_INFO "exception will reset the vpu core: %d\n", ret - 1);
            s_init_flag[ret-1] = 0;
//            msleep(200); //waiting the decoder stoping maybe......
        }
#elif defined(CHIP_BM1684)
        if(ret > 0 && s_vpu_usage_info.vpu_open_ref_count[ret-1] == 0 && s_vpu_drv_context.reset_vpu_core_disable[ret-1]==0) {
            printk(KERN_INFO "exception will reset the vpu core: %d\n", ret - 1);
            s_init_flag[ret-1] = 0;
        }
#endif
#endif
        mutex_unlock(&s_vpu_lock);

    }

    vpu_fasync(-1, filp, 0);
    if(filp->private_data != NULL)
        vfree(filp->private_data);
    return 0;
}



static int vpu_map_to_register(struct file *fp, struct vm_area_struct *vm, int core_idx)
{
    unsigned long pfn;

    vm->vm_flags |= VM_IO | VM_RESERVED;
    vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
    pfn = s_vpu_register[core_idx].phys_addr >> PAGE_SHIFT;

    return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_physical_memory(struct file *fp, struct vm_area_struct *vm)
{
    int enable_cache = 0;

    vm->vm_flags |= VM_IO | VM_RESERVED;

#ifndef BM_ION_MEM
    if (down_interruptible(&s_vpu_sem) == 0) {
        vpudrv_buffer_pool_t *pool;
        list_for_each_entry(pool, &s_vbp_head, list)
        {
            if (vm->vm_pgoff == (pool->vb.phys_addr>>PAGE_SHIFT)) {
                enable_cache = pool->vb.enable_cache;
                break;
            }
        }
        up(&s_vpu_sem);
    }
    //pr_info("enable_cache : %d\n", enable_cache);
#endif
    if(enable_cache == 0)
        vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot);

    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_instance_pool_memory(struct file *fp, struct vm_area_struct *vm, int idx)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    int ret;
    long length = vm->vm_end - vm->vm_start;
    unsigned long start = vm->vm_start;
    char *vmalloc_area_ptr = (char *)s_instance_pool[idx].base;
    unsigned long pfn;

    vm->vm_flags |= VM_RESERVED;

    /* loop over all pages, map it page individually */
    while (length > 0)
    {
        pfn = vmalloc_to_pfn(vmalloc_area_ptr);
        if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED)) < 0) {
                return ret;
        }
        start += PAGE_SIZE;
        vmalloc_area_ptr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }


    return 0;
#else
    vm->vm_flags |= VM_RESERVED;
    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
#endif
}

static int vpu_map_vmalloc(struct file *fp, struct vm_area_struct *vm, char *vmalloc_area_ptr)
{
    int ret;
    long length = vm->vm_end - vm->vm_start;
    unsigned long start = vm->vm_start;
    unsigned long pfn;

    vm->vm_flags |= VM_RESERVED;
    /* loop over all pages, map it page individually */
    while (length > 0)
    {
        pfn = vmalloc_to_pfn(vmalloc_area_ptr);
        if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED)) < 0) {
                return ret;
        }
        start += PAGE_SIZE;
        vmalloc_area_ptr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }

    return 0;
}

/*!
 * @brief memory map interface for vpu file operation
 * @return  0 on success or negative error code on error
 */
static int vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
    int i = 0;
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    if (vm->vm_pgoff == 1)
        return vpu_map_vmalloc(fp, vm, (char *)s_vpu_dump_flag);
    for(i=0; i< get_vpu_core_num(chip_id,video_cap); i++)
    {

        if (vm->vm_pgoff == (s_vpu_register[i].phys_addr>>PAGE_SHIFT))
            return vpu_map_to_register(fp, vm, i);
        if (vm->vm_pgoff == (s_instance_pool[i].phys_addr >> PAGE_SHIFT))
            return vpu_map_to_instance_pool_memory(fp, vm, i);
    }

    return vpu_map_to_physical_memory(fp, vm);
#else
    if (vm->vm_pgoff) {
        if (vm->vm_pgoff == (s_instance_pool.phys_addr>>PAGE_SHIFT))
            return vpu_map_to_instance_pool_memory(fp, vm);

        return vpu_map_to_physical_memory(fp, vm);
    } else {
        for(i=0; i< get_vpu_core_num(chip_id,video_cap); i++)
        {

            if (vm->vm_pgoff == (s_vpu_register[i].phys_addr>>PAGE_SHIFT))
                return vpu_map_to_register(fp, vm, i);
        }
    }
#endif
}

static struct file_operations vpu_fops = {
    .owner = THIS_MODULE,
    .open = vpu_open,
    .read = vpu_read,
    .write = vpu_write,
    /*.ioctl = vpu_ioctl, // for kernel 2.6.9 of C&M*/
    .unlocked_ioctl = vpu_ioctl,
    .release = vpu_release,
    .fasync = vpu_fasync,
    .mmap = vpu_mmap,
};
#if defined(CHIP_BM1684)
static int vpu_register_clk(struct platform_device *pdev)
{
    int i,ret;
    const char *clk_name;

    if (!pdev)
        return -1;

    for(i = 0; i < MAX_NUM_VPU_CORE; i++)
    {
        if(s_vpu_sub_bin_flag[i] == 0)
            continue;

        of_property_read_string_index(pdev->dev.of_node, "clock-names", 2*i, &clk_name);

        vpu_rst_ctrl.apb_clk[i] = devm_clk_get(&pdev->dev, clk_name);
        if (IS_ERR(vpu_rst_ctrl.apb_clk[i])) {
            ret = PTR_ERR(vpu_rst_ctrl.apb_clk[i]);
            dev_err(&pdev->dev, "failed to retrieve vpu%d %s clock", i, clk_name);
            return ret;
        }

        of_property_read_string_index(pdev->dev.of_node, "clock-names", 2*i+1, &clk_name);

        vpu_rst_ctrl.axi_clk[i]  = devm_clk_get(&pdev->dev, clk_name);
        if (IS_ERR(vpu_rst_ctrl.axi_clk[i])) {
            ret = PTR_ERR(vpu_rst_ctrl.axi_clk[i]);
            dev_err(&pdev->dev, "failed to retrieve vpu%d %s clock", i, clk_name);
            return ret;
        }

        of_property_read_string_index(pdev->dev.of_node, "reset-names", i, &clk_name);

        vpu_rst_ctrl.axi2_rst[i]  = devm_reset_control_get(&pdev->dev, clk_name);
        if (IS_ERR(vpu_rst_ctrl.axi2_rst[i])) {
            ret = PTR_ERR(vpu_rst_ctrl.axi2_rst[i]);
            dev_err(&pdev->dev, "failed to retrieve vpu%d %s reset", i, clk_name);
            return ret;
        }
    }

    //just for encoder
    of_property_read_string_index(pdev->dev.of_node, "clock-names", 10, &clk_name);

    vpu_rst_ctrl.axi_clk_enc  = devm_clk_get(&pdev->dev, clk_name);
    if (IS_ERR(vpu_rst_ctrl.axi_clk_enc)) {
        ret = PTR_ERR(vpu_rst_ctrl.axi_clk_enc);
        dev_err(&pdev->dev, "failed to retrieve vpu 4 %s clock", clk_name);
        return ret;
    }

    return 0;
}
#endif
static int vpu_probe(struct platform_device *pdev)
{
    int err = 0;
    struct resource *res = NULL;
    int i = 0;

    //unsigned long *chip_id_addr;
    //unsigned int reg_val = 0;
    //chip_id_addr = (unsigned long *)ioremap((unsigned long)0x50010000,4);
    //reg_val = ioread32(chip_id_addr);
    //iounmap((void *)chip_id_addr);


#if defined(CHIP_BM1880) || defined(CHIP_BM1684)
    int high_addr = 3;

#endif
//#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    struct device_node *target = NULL;
    struct resource rmem;
//#endif
    DPRINTK("[VPUDRV] vpu_probe\n");

    mutex_init(&s_vpu_lock);
    for(i=0; i< MAX_NUM_VPU_CORE; i++)
    {
        if(s_vpu_sub_bin_flag[i] == 0)
            continue;

        if (pdev)
            res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (res) {/* if platform driver is implemented */
            s_vpu_register[i].phys_addr = res->start;
            s_vpu_register[i].virt_addr = (unsigned long)ioremap_nocache(res->start, res->end - res->start);
            s_vpu_register[i].size = res->end - res->start;
            pr_info("[VPUDRV] : vpu base address get from platform driver physical base addr==0x%lx, virtual base=0x%lx, core idx = %d\n", s_vpu_register[i].phys_addr , s_vpu_register[i].virt_addr, i);
        } else {
            s_vpu_register[i].phys_addr = s_vpu_reg_phy_base[i];
            s_vpu_register[i].virt_addr = (unsigned long)ioremap_nocache(s_vpu_register[i].phys_addr, VPU_REG_SIZE);
            s_vpu_register[i].size = VPU_REG_SIZE;
            pr_info("[VPUDRV] : vpu base address get from defined value physical base addr==0x%lx, virtual base=0x%lx, core idx = %d\n", s_vpu_register[i].phys_addr, s_vpu_register[i].virt_addr, i);
        }
    }


    err = bm_vpu_register_cdev(pdev);
    if (err < 0)
    {
        printk(KERN_ERR "bm_vpu_register_cdev\n");
        goto ERROR_PROVE_DEVICE;
    }

    if (pdev)
        s_vpu_clk = vpu_clk_get(&pdev->dev);
    else
        s_vpu_clk = vpu_clk_get(NULL);

    if (!s_vpu_clk)
        printk(KERN_ERR "[VPUDRV] : not support clock controller.\n");
    else
        DPRINTK("[VPUDRV] : get clock controller s_vpu_clk=%p\n", s_vpu_clk);

#if defined(CHIP_BM1682) || defined(CHIP_BM1880)
#ifdef CONFIG_ARCH_BM1880
        vpu_rst_ctrl.axi2_rst[0] = devm_reset_control_get_shared(&pdev->dev, "axi2");
        if (IS_ERR(vpu_rst_ctrl.axi2_rst[0])) {
            err = PTR_ERR(vpu_rst_ctrl.axi2_rst[0]);
            dev_err(&pdev->dev, "failed to retrieve axi2 reset");
            return err;
        }

        vpu_rst_ctrl.apb_video_rst = devm_reset_control_get(&pdev->dev, "apb_video");
        if (IS_ERR(vpu_rst_ctrl.apb_video_rst)) {
            err = PTR_ERR(vpu_rst_ctrl.apb_video_rst);
            dev_err(&pdev->dev, "failed to retrieve apb_video reset");
            return err;
        }

        vpu_rst_ctrl.video_axi_rst = devm_reset_control_get(&pdev->dev, "video_axi");
        if (IS_ERR(vpu_rst_ctrl.video_axi_rst)) {
            err = PTR_ERR(vpu_rst_ctrl.video_axi_rst);
            dev_err(&pdev->dev, "failed to retrieve video_axi reset");
            return err;
        }
#endif

#else //1684
    vpu_register_clk(pdev);
#endif

#ifdef VPU_SUPPORT_CLOCK_CONTROL
#else
    vpu_clk_enable(s_vpu_clk);
#endif
#if defined(ENABLE_MEMORY_SECURITY)
    for(i=0; i<VPU_SUB_SYSTEM_NUM; i++)
        vpu_mem_security_setting(i);
#endif
#ifdef VPU_SUPPORT_ISR
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    for(i=0; i< MAX_NUM_VPU_CORE + VPU_SUB_SYSTEM_NUM; i++)
    {
        if(s_vpu_sub_bin_flag[i] == 0 && i < MAX_NUM_VPU_CORE){
            continue;
        }
        if (pdev)
            res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
        if (res) {/* if platform driver is implemented */
            if(i< MAX_NUM_VPU_CORE)
                s_vpu_irq[i] = res->start;
#if defined(ENABLE_MEMORY_SECURITY)
            else
                s_vpu_security_irq[i- get_vpu_core_num(chip_id, video_cap)] = res->start;
#endif
            DPRINTK("[VPUDRV] : vpu irq number core %d get from platform driver irq=0x%x\n", i, res->start);
        } else {
            pr_err("[VPUDRV] : vpu irq number core %d get failed\n", i);
        }
    }
#else
    DPRINTK("[VPUDRV] : vpu irq number get from defined value irq=0x%x\n", s_vpu_irq[0]);
#endif
    for(i=0; i< MAX_NUM_VPU_CORE+VPU_SUB_SYSTEM_NUM; i++)
    {
        if(s_vpu_sub_bin_flag[i] == 0 && i < MAX_NUM_VPU_CORE){
            continue;
        }

        if(i< MAX_NUM_VPU_CORE)
        {
            err = request_irq(s_vpu_irq[i], vpu_irq_handler, 0, "VPU_CODEC_IRQ", (void *)(&s_vpu_drv_context));
        }
#if defined(ENABLE_MEMORY_SECURITY)
        else
            err = request_irq(s_vpu_security_irq[i- get_vpu_core_num(chip_id, video_cap)], vpu_security_irq_handler, 0, "VPU_CODEC_IRQ", (void *)(&s_vpu_drv_context));
#endif
        if (err) {
            printk(KERN_ERR "[VPUDRV] :  fail to register interrupt handler. core: %d\n", i);
            goto ERROR_PROVE_DEVICE;
        }
    }

#endif

    if(chip_id == 0x1684)
        vpu_init_set_source_bm1684(video_cap);
    if(chip_id == 0x1686)
        vpu_init_set_source_bm1686(video_cap);

#ifdef CONFIG_ARCH_BM1880
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (vpu_allocate_memory(pdev) < 0)
    {
        printk(KERN_ERR "[VPUDRV] :  fail to remap vpu memory\n");
        goto ERROR_PROVE_DEVICE;
    }
#endif
#else
    if (pdev) {
        target = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
    }
    if (target) {
        err = of_address_to_resource(target, 0, &rmem);
        if (err)
        {
            printk(KERN_INFO "[VPUDRV] No memory address assigned to the region\n");
            goto ERROR_PROVE_DEVICE;
        }

        s_video_memory.phys_addr = rmem.start;
        s_video_memory.size = resource_size(&rmem);
    }
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (target) {
        res = devm_request_mem_region(&pdev->dev, s_video_memory.phys_addr, s_video_memory.size, "vpu");
        if(!res)
        {
            printk(KERN_INFO "[VPUDRV] Request_mem_region failed!\n");
            goto ERROR_PROVE_DEVICE;
        }

        s_video_memory.base = (unsigned long)devm_ioremap_nocache(&pdev->dev, s_video_memory.phys_addr, s_video_memory.size);
        if(!s_video_memory.base)
        {
            printk("[VPUDRV] ioremap fail!\n");
            printk("!!!*s_video_memory.base = 0x%lx\n", s_video_memory.base);
            goto ERROR_PROVE_DEVICE;
        }
        s_video_memory_flag = 1;
        printk(KERN_INFO "[VPUDRV] success to probe vpu device with reserved memory phys_addr = 0x%lx, base = 0x%lx, size = 0x%x\n", s_video_memory.phys_addr, s_video_memory.base, s_video_memory.size);
    }
    else {
        s_video_memory.size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
        s_video_memory.phys_addr = VPU_DRAM_PHYSICAL_BASE;
        s_video_memory.base = (unsigned long)ioremap_nocache(s_video_memory.phys_addr, PAGE_ALIGN(s_video_memory.size));
    }
#endif
#endif
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (!s_video_memory.base) {
        printk(KERN_ERR "[VPUDRV] :  fail to remap video memory physical phys_addr==0x%lx, base==0x%lx, size=%d\n", s_video_memory.phys_addr, s_video_memory.base, (int)s_video_memory.size);
        goto ERROR_PROVE_DEVICE;
    }
#endif
#if defined(CHIP_BM1880) || defined(CHIP_BM1684)
    high_addr = (int)(s_video_memory.phys_addr>>32);
    vpu_set_topaddr(high_addr);
#endif /* CHIP_BM1880 */

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (vmem_init(&s_vmem, s_video_memory.phys_addr, s_video_memory.size) < 0) {
        printk(KERN_ERR "[VPUDRV] :  fail to init vmem system\n");
        goto ERROR_PROVE_DEVICE;
    }
    DPRINTK("[VPUDRV] success to probe vpu device with reserved video memory phys_addr==0x%lx, base = =0x%lx\n", s_video_memory.phys_addr, s_video_memory.base);
#elif defined(BM_ION_MEM)
    /* here we set up DMA mask */
    dma_set_coherent_mask(&(pdev->dev), 0x0);
    dma_set_mask(&(pdev->dev), 0xFFFFFFFFFFFFFFFF);
    /* save dev for ion dma_map operation */
    s_vpu_drv_context.dev = &pdev->dev;
#else
    DPRINTK("[VPUDRV] success to probe vpu device with non reserved video memory\n");
#endif

    /* launch vpu monitor thread */
    if (s_vpu_monitor_task == NULL){
        s_vpu_monitor_task = kthread_run(bm_vpu_monitor_thread, &s_vpu_usage_info, "vpu_monitor");
        if (s_vpu_monitor_task == NULL){
            pr_info("create vpu monitor thread failed\n");
            goto ERROR_PROVE_DEVICE;
        } else
            pr_info("create vpu monitor thread done\n");
    }


    return 0;

ERROR_PROVE_DEVICE:

    bm_vpu_unregister_cdev();

    for(i=0; i< get_vpu_core_num(chip_id,video_cap); i++)
    {
        if (s_vpu_register[i].virt_addr)
            iounmap((void *)s_vpu_register[i].virt_addr);

        if (s_vpu_irq[i])
            free_irq(s_vpu_irq[i], &s_vpu_drv_context);
    }

    return err;
}

int bm_vpu_register_cdev(struct platform_device *pdev)
{
    int err = 0;

    vpu_class = class_create(THIS_MODULE, VPU_CLASS_NAME);
    if (IS_ERR(vpu_class))
    {
        printk(KERN_ERR "create class failed\n");
        return PTR_ERR(vpu_class);
    }

    /* get the major number of the character device */
    if ((alloc_chrdev_region(&s_vpu_major, 0, 1, VPU_DEV_NAME)) < 0)
    {
        err = -EBUSY;
        printk(KERN_ERR "could not allocate major number\n");
        return err;
    }

    printk(KERN_INFO "SUCCESS alloc_chrdev_region\n");

    /* initialize the device structure and register the device with the kernel */
    cdev_init(&s_vpu_cdev, &vpu_fops);
    s_vpu_cdev.owner = THIS_MODULE;

    if ((cdev_add(&s_vpu_cdev, s_vpu_major, 1)) < 0)
    {
        err = -EBUSY;
        printk(KERN_ERR "could not allocate chrdev\n");
        return err;
    }

    device_create(vpu_class, &pdev->dev, s_vpu_major, NULL, "%s", VPU_DEV_NAME);

    return err;
}

#ifdef CONFIG_ARCH_BM1880
static int vpu_allocate_memory(struct platform_device *pdev)
{
    struct device_node *target = NULL;
    struct reserved_mem *prmem = NULL;

    if (pdev) {
        target = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
    }

    if (target) {
        prmem = of_reserved_mem_lookup(target);
        of_node_put(target);

        if (!prmem) {
            printk(KERN_ERR "[VPUDRV]: cannot acquire memory-region\n");
            return -1;
        }
    }
    else {
        printk(KERN_ERR "[VPUDRV]: cannot find the node, memory-region\n");
        return -1;
    }

    printk(KERN_ERR "[VPUDRV]: pool name = %s, size = 0x%x, base = 0x%lx\n",
           prmem->name,
           prmem->size,
           prmem->base);

    s_video_memory.phys_addr = (unsigned long) prmem->base;
    s_video_memory.size = (unsigned int) prmem->size;
    s_video_memory.base = (unsigned long) devm_ioremap_nocache(&pdev->dev,
                                                               s_video_memory.phys_addr,
                                                               s_video_memory.size);

    if(!s_video_memory.base)
    {
        printk(KERN_ERR "[VPUDRV] ioremap fail!\n");
        printk(KERN_ERR "s_video_memory.base = 0x%lx\n", s_video_memory.base);
        return -1;
    }

    s_video_memory_flag = 1;
    printk(KERN_INFO "[VPUDRV] success to probe vpu\n");
    printk(KERN_INFO "reserved memory phys_addr = 0x%lx, base = 0x%lx, size = 0x%x\n",
           s_video_memory.phys_addr,
           s_video_memory.base,
           s_video_memory.size);

    return 0;
}
#endif

static void bm_vpu_assert(vpu_reset_ctrl *pRstCtrl)
{
#ifdef CONFIG_ARCH_BM1880
    reset_control_assert(pRstCtrl->axi2_rst[0]);
    reset_control_assert(pRstCtrl->apb_video_rst);
    reset_control_assert(pRstCtrl->video_axi_rst);
#elif defined(CHIP_BM1684)
    {
        int i;
        pr_info("<<<<<<disable vpu clock...>>>>>>>>>>>>>>>\n");
        for(i=0; i< MAX_NUM_VPU_CORE; i++) {

            if(s_vpu_sub_bin_flag[i] == 0)
                continue;

            if(vpu_rst_ctrl.axi2_rst[i])
                reset_control_assert(vpu_rst_ctrl.axi2_rst[i]);

            if(vpu_rst_ctrl.apb_clk[i])
                clk_disable_unprepare(vpu_rst_ctrl.apb_clk[i]);

            if(vpu_rst_ctrl.axi_clk[i])
                clk_disable_unprepare(vpu_rst_ctrl.axi_clk[i]);
        }

        if(vpu_rst_ctrl.axi_clk_enc)
            clk_disable_unprepare(vpu_rst_ctrl.axi_clk_enc);
    }
#endif
}

static void bm_vpu_deassert(vpu_reset_ctrl *pRstCtrl)
{
#ifdef CONFIG_ARCH_BM1880
    reset_control_deassert(pRstCtrl->axi2_rst[0]);
    reset_control_deassert(pRstCtrl->apb_video_rst);
    reset_control_deassert(pRstCtrl->video_axi_rst);
#elif defined(CHIP_BM1684)
    {
        int i;
        pr_info("<<<<<<enable vpu clock...>>>>>>>>>>>>>>>\n");
        for(i=0; i< MAX_NUM_VPU_CORE; i++) {

            if(s_vpu_sub_bin_flag[i] == 0)
                continue;

            if(vpu_rst_ctrl.axi2_rst[i])
                reset_control_deassert(vpu_rst_ctrl.axi2_rst[i]);

            if(vpu_rst_ctrl.apb_clk[i])
                clk_prepare_enable(vpu_rst_ctrl.apb_clk[i]);

            if(vpu_rst_ctrl.axi_clk[i])
                clk_prepare_enable(vpu_rst_ctrl.axi_clk[i]);
        }
        if(vpu_rst_ctrl.axi_clk_enc)
            clk_prepare_enable(vpu_rst_ctrl.axi_clk_enc);
    }
#endif
}

static int vpu_remove(struct platform_device *pdev)
{
#ifdef VPU_SUPPORT_ISR
    int i;
#endif
    DPRINTK("[VPUDRV] vpu_remove\n");
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    for(i=0; i< get_vpu_core_num(chip_id, video_cap); i++) {
        if (s_instance_pool[i].base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
            vfree((const void *)s_instance_pool[i].base);
#else
            vpu_free_dma_buffer(&s_instance_pool[i]);
#endif
            s_instance_pool[i].base = 0;
        }
        if (s_common_memory[i].base) {
            vpu_free_dma_buffer(&s_common_memory[i]);
            s_common_memory[i].base = 0;
        }
    }

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base) {
        if(s_video_memory_flag)
            devm_iounmap(&pdev->dev, (void *)s_video_memory.base);
        else
            iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
        s_video_memory_flag = 0;
        vmem_exit(&s_vmem);
    }
#endif

    if (s_vpu_major > 0) {
        bm_vpu_unregister_cdev();
    }

#ifdef VPU_SUPPORT_ISR
    for(i=0; i< get_vpu_core_num(chip_id,video_cap) + VPU_SUB_SYSTEM_NUM; i++)
    {
        if (i< get_vpu_core_num(chip_id,video_cap)) {
            if( s_vpu_irq[i] )
                free_irq(s_vpu_irq[i], &s_vpu_drv_context);
        }
#if defined(ENABLE_MEMORY_SECURITY)
        else if(s_vpu_security_irq[i- get_vpu_core_num(chip_id, video_cap)])
            free_irq(s_vpu_security_irq[i- get_vpu_core_num(chip_id, video_cap)], &s_vpu_drv_context);
#endif
    }
#endif

    for(i=0; i< get_vpu_core_num(chip_id,video_cap); i++) {
        if (s_vpu_register[i].virt_addr)
            iounmap((void *)s_vpu_register[i].virt_addr);
    }


#ifdef SUPPORT_MULTI_INST_INTR
    for (i=0; i<MAX_NUM_INSTANCE* get_vpu_core_num(chip_id, video_cap); i++) {
        kfifo_free(&s_interrupt_pending_q[i]);
        s_interrupt_flag[i] = 0;
    }
#if defined(ENABLE_MEMORY_SECURITY)
    for(i=0; i<VPU_SUB_SYSTEM_NUM; i++) {
        if(p_security_reg_base[i] != NULL)
            iounmap(p_security_reg_base[i]);
    }
#endif

#else
    for (i=0; i< get_vpu_core_num(chip_id, video_cap); i++) {
        s_interrupt_flag[i] = 0;
    }
#endif

    vpu_clk_put(s_vpu_clk);

#if 0
    // device reset is called in platform_driver_unregister in vpu_exit, so remove it here.
    if(s_vpu_rst0)
        reset_control_put(s_vpu_rst0);
#ifdef CHIP_BM1682
    if(s_vpu_rst1)
        reset_control_put(s_vpu_rst1);
#endif

#endif
#endif /*VPU_SUPPORT_PLATFORM_DRIVER_REGISTER*/

    /* stop vpu monitor thread */
    if (s_vpu_monitor_task != NULL){
        kthread_stop(s_vpu_monitor_task);
        s_vpu_monitor_task = NULL;
        pr_info("vpu monitor thread released\n");
    }

    return 0;
}

static void bm_vpu_unregister_cdev(void)
{
    if(vpu_class != NULL && s_vpu_major > 0)
        device_destroy(vpu_class, s_vpu_major);

    if(vpu_class != NULL)
        class_destroy(vpu_class);

    cdev_del(&s_vpu_cdev);

    if(s_vpu_major > 0)
        unregister_chrdev_region(s_vpu_major, 1);

    s_vpu_major = 0;
}

#ifdef CONFIG_PM
#define W5_MAX_CODE_BUF_SIZE    (512*1024)
#define W5_CMD_INIT_VPU         (0x0001)
#define W5_CMD_SLEEP_VPU        (0x0004)
#define W5_CMD_WAKEUP_VPU       (0x0002)

static void Wave5BitIssueCommand(int core, u32 cmd)
{
    WriteVpuRegister(W5_VPU_BUSY_STATUS, 1);
    WriteVpuRegister(W5_COMMAND, cmd);
    WriteVpuRegister(W5_VPU_HOST_INT_REQ, 1);

    return;
}

static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
    int i;
    int core;
    unsigned long timeout = jiffies + HZ;    /* vpu wait timeout to 1sec */
    int product_code;

    DPRINTK("[VPUDRV] vpu_suspend\n");

    vpu_clk_enable(s_vpu_clk);

    for (core = 0; core <  get_vpu_core_num(chip_id, video_cap); core++) {
        if (s_vpu_usage_info.vpu_open_ref_count[core] > 0) {

            if (s_bit_firmware_info[core].size == 0)
                continue;
            product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);
            if (PRODUCT_CODE_W_SERIES(product_code)) {

                while (ReadVpuRegister(W5_VPU_BUSY_STATUS)) {
                    if (time_after(jiffies, timeout)) {
                        DPRINTK("SLEEP_VPU BUSY timeout");
                        goto DONE_SUSPEND;
                    }
                }

                Wave5BitIssueCommand(core, W5_CMD_SLEEP_VPU);

                while (ReadVpuRegister(W5_VPU_BUSY_STATUS)) {
                    if (time_after(jiffies, timeout)) {
                        DPRINTK("SLEEP_VPU BUSY timeout");
                        goto DONE_SUSPEND;
                    }
                }
                if (ReadVpuRegister(W5_RET_SUCCESS) == 0) {
                    DPRINTK("SLEEP_VPU failed [0x%x]", ReadVpuRegister(W5_RET_FAIL_REASON));
                    goto DONE_SUSPEND;
                }
            }
            else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {
                while (ReadVpuRegister(BIT_BUSY_FLAG)) {
                    if (time_after(jiffies, timeout))
                        goto DONE_SUSPEND;
                }

                for (i = 0; i < 64; i++)
                    s_vpu_reg_store[core][i] = ReadVpuRegister(BIT_BASE+(0x100+(i * 4)));
            }
            else {
                DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
                goto DONE_SUSPEND;
            }
        }
    }

    vpu_clk_disable(s_vpu_clk);
    return 0;

DONE_SUSPEND:

    vpu_clk_disable(s_vpu_clk);

    return -EAGAIN;

}
static int vpu_resume(struct platform_device *pdev)
{
    int i;
    int core;
    u32 val;
    unsigned long timeout = jiffies + HZ;    /* vpu wait timeout to 1sec */
    int product_code;

    unsigned long   code_base;
    u32              code_size;
    u32              remap_size;
    int             regVal;
    u32              hwOption  = 0;
    int             vpu_open_ref_count = 0;


    DPRINTK("[VPUDRV] vpu_resume\n");

    vpu_clk_enable(s_vpu_clk);

    for (core = 0; core <  get_vpu_core_num(chip_id, video_cap); core++) {

        if (s_bit_firmware_info[core].size == 0) {
            continue;
        }

        vpu_open_ref_count += s_vpu_usage_info.vpu_open_ref_count[core];

        product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);
        if (PRODUCT_CODE_W_SERIES(product_code)) {
            code_base = s_common_memory[core].phys_addr;
            /* ALIGN TO 4KB */
            code_size = (W5_MAX_CODE_BUF_SIZE&~0xfff);
            if (code_size < s_bit_firmware_info[core].size*2) {
                goto DONE_WAKEUP;
            }

            regVal = 0;
            WriteVpuRegister(W5_PO_CONF, regVal);

            /* Reset All blocks */
            regVal = 0x7ffffff;
            WriteVpuRegister(W5_VPU_RESET_REQ, regVal);    /*Reset All blocks*/

            /* Waiting reset done */
            while (ReadVpuRegister(W5_VPU_RESET_STATUS)) {
                if (time_after(jiffies, timeout))
                    goto DONE_WAKEUP;
            }

            WriteVpuRegister(W5_VPU_RESET_REQ, 0);

            /* remap page size */
            remap_size = (code_size >> 12) & 0x1ff;
            regVal = 0x80000000 | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remap_size;
            WriteVpuRegister(W5_VPU_REMAP_CTRL, regVal);
            WriteVpuRegister(W5_VPU_REMAP_VADDR,0x00000000);    /* DO NOT CHANGE! */
            WriteVpuRegister(W5_VPU_REMAP_PADDR,code_base);
            WriteVpuRegister(W5_ADDR_CODE_BASE, code_base);
            WriteVpuRegister(W5_CODE_SIZE,      code_size);
            WriteVpuRegister(W5_CODE_PARAM,        0);
            WriteVpuRegister(W5_TIMEOUT_CNT,   timeout);

            WriteVpuRegister(W5_HW_OPTION, hwOption);

            /* Interrupt */
            if (product_code == WAVE520_CODE) {
                regVal  = (1<<W5_INT_ENC_SET_PARAM);
                regVal |= (1<<W5_INT_ENC_PIC);
            }
            else if (product_code == WAVE525_CODE || product_code == WAVE521_CODE || product_code == WAVE521C_CODE ) {
                regVal  = (1<<W5_INT_ENC_SET_PARAM);
                regVal |= (1<<W5_INT_ENC_PIC);
                regVal |= (1<<W5_INT_INIT_SEQ);
                regVal |= (1<<W5_INT_DEC_PIC);
                regVal |= (1<<W5_INT_BSBUF_EMPTY);
            }
            else {
                // decoder
                regVal  = (1<<W5_INT_INIT_SEQ);
                regVal |= (1<<W5_INT_DEC_PIC);
                regVal |= (1<<W5_INT_BSBUF_EMPTY);
            }

            WriteVpuRegister(W5_VPU_VINT_ENABLE,  regVal);

            Wave5BitIssueCommand(core, W5_CMD_INIT_VPU);
            WriteVpuRegister(W5_VPU_REMAP_CORE_START, 1);

            while (ReadVpuRegister(W5_VPU_BUSY_STATUS)) {
                if (time_after(jiffies, timeout))
                    goto DONE_WAKEUP;
            }

            if (ReadVpuRegister(W5_RET_SUCCESS) == 0) {
                DPRINTK("WAKEUP_VPU failed [0x%x]", ReadVpuRegister(W5_RET_FAIL_REASON));
                goto DONE_WAKEUP;
            }
        }
        else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {

            WriteVpuRegister(BIT_CODE_RUN, 0);

            /*---- LOAD BOOT CODE*/
            for (i = 0; i < 512; i++) {
                val = s_bit_firmware_info[core].bit_code[i];
                WriteVpuRegister(BIT_CODE_DOWN, ((i << 16) | val));
            }

            for (i = 0 ; i < 64 ; i++)
                WriteVpuRegister(BIT_BASE+(0x100+(i * 4)), s_vpu_reg_store[core][i]);

            WriteVpuRegister(BIT_BUSY_FLAG, 1);
            WriteVpuRegister(BIT_CODE_RESET, 1);
            WriteVpuRegister(BIT_CODE_RUN, 1);

            while (ReadVpuRegister(BIT_BUSY_FLAG)) {
                if (time_after(jiffies, timeout))
                    goto DONE_WAKEUP;
            }
        }
        else {
            DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
            goto DONE_WAKEUP;
        }
    }

    if (vpu_open_ref_count == 0)
        vpu_clk_disable(s_vpu_clk);

DONE_WAKEUP:

    if (vpu_open_ref_count > 0)
        vpu_clk_enable(s_vpu_clk);

    return 0;
}
#else
#define    vpu_suspend    NULL
#define    vpu_resume    NULL
#endif                /* !CONFIG_PM */

#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
static const struct of_device_id bm_vpu_match_table[] = {
    {.compatible = "bitmain,bitmain-vdec"},
    {},
};

MODULE_DEVICE_TABLE(of, bm_vpu_match_table);

static struct platform_driver vpu_driver = {
    .driver = {
           .name = VPU_PLATFORM_DEVICE_NAME,
           .of_match_table = bm_vpu_match_table,
           },
    .probe = vpu_probe,
    .remove = vpu_remove,
    .suspend = vpu_suspend,
    .resume = vpu_resume,
};
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

static void set_stop_inst_info(unsigned long flag)
{
    int i;

    if(flag == 0) { //set all to stop
        for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
        {
            if(s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 0) {
                s_vpu_inst_info[i].time = get_us_time();
                s_vpu_inst_info[i].status = 1;
            }
        }
    }
    else {
        for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
        {
            if(s_vpu_inst_info[i].flag == flag && s_vpu_inst_info[i].status == 0) {
                s_vpu_inst_info[i].time = get_us_time();
                s_vpu_inst_info[i].status = 1;
                break;
            }
        }
    }
}

static void clean_timeout_inst_info(unsigned long timeout)
{
    unsigned long cur_time = get_us_time();
    int i;

    for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
    {
        if(s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 1 && cur_time - s_vpu_inst_info[i].time > timeout)
        {
            s_vpu_inst_info[i].flag = 0;
            s_vpu_inst_info[i].status = 0;
            s_vpu_inst_info[i].time = 0;
            vfree(s_vpu_inst_info[i].url);
            s_vpu_inst_info[i].url = NULL;
        }
    }

}

static int check_vpu_core_busy(vpu_statistic_info_t *vpu_usage_info, int coreIdx)
{
    int ret = 0;

#if defined(CHIP_BM1684)
    int core = coreIdx;
    if (s_bit_firmware_info[coreIdx].size){
        if (vpu_usage_info->vpu_open_ref_count[coreIdx] > 0){
            if (ReadVpuRegister(W5_VPU_BUSY_STATUS) == 1)
                ret = 1;
            else if (atomic_read(&vpu_usage_info->vpu_busy_status[coreIdx]) > 0)
                ret = 1;
        }
    }
#endif

    return ret;
}
/*
static int bm_vpu_set_interval(vpu_statistic_info_t *vpu_usage_info, int time_interval)
{
    mutex_lock(&s_vpu_proc_lock);
    vpu_usage_info->vpu_instant_interval = time_interval;
    mutex_unlock(&s_vpu_proc_lock);

    return 0;
}
*/
static int bm_vpu_check_usage_info(vpu_statistic_info_t *vpu_usage_info)
{
    int ret = 0, i;

    mutex_lock(&s_vpu_proc_lock);
    /* update memory status */
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base){
        vmem_info_t info;
        if (0 == vmem_get_info(&s_vmem, &info)){
            vpu_usage_info->vpu_mem_total_size = info.total_pages * info.page_size;
            vpu_usage_info->vpu_mem_free_size = info.free_pages * info.page_size;
            vpu_usage_info->vpu_mem_used_size = info.alloc_pages * info.page_size;
        }
    }
#endif

    /* update usage */
    for (i = 0; i <  get_vpu_core_num(chip_id, video_cap); i++){
        int busy = check_vpu_core_busy(vpu_usage_info, i);
        int vpu_instant_usage = 0;
        int j;
        vpu_usage_info->vpu_status_array[i][vpu_usage_info->vpu_status_index[i]] = busy;
        vpu_usage_info->vpu_status_index[i]++;
        vpu_usage_info->vpu_status_index[i] %= MAX_VPU_STAT_WIN_SIZE;

        if (busy == 1)
            vpu_usage_info->vpu_working_time_in_ms[i] += vpu_usage_info->vpu_instant_interval / MAX_VPU_STAT_WIN_SIZE;
        vpu_usage_info->vpu_total_time_in_ms[i] += vpu_usage_info->vpu_instant_interval / MAX_VPU_STAT_WIN_SIZE;

        for (j = 0; j < MAX_VPU_STAT_WIN_SIZE; j++)
            vpu_instant_usage += vpu_usage_info->vpu_status_array[i][j];

        vpu_usage_info->vpu_instant_usage[i] = vpu_instant_usage;
    }
    mutex_unlock(&s_vpu_proc_lock);

    return ret;
}

static int bm_vpu_usage_info_init(vpu_statistic_info_t *vpu_usage_info)
{
    memset(vpu_usage_info, 0, sizeof(vpu_statistic_info_t));

    vpu_usage_info->vpu_instant_interval = 500;

    return bm_vpu_check_usage_info(vpu_usage_info);
}

int bm_vpu_monitor_thread(void *data)
{
	int ret = 0;
    vpu_statistic_info_t *vpu_usage_info = (vpu_statistic_info_t *)data;

	set_current_state(TASK_INTERRUPTIBLE);
	ret = bm_vpu_usage_info_init(vpu_usage_info);
	if (ret)
		return ret;

	while (!kthread_should_stop()) {
        bm_vpu_check_usage_info(vpu_usage_info);
        msleep_interruptible(100);
	}

	return ret;
}

static int get_inst_info_idx(unsigned long flag)
{
    int i = -1;
    for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
    {
        if(s_vpu_inst_info[i].flag == flag)
            return i;
    }

    for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
    {
        if(s_vpu_inst_info[i].flag == 0)
            break;
    }

    return i;
}

static void printf_inst_info(void *mem, int size)
{
    char temp[1024]      = { 0 };
    int  single_list_len = 0;
    int  total_len       = 0;
    int  total_len_first = 0;
    int  i;
    sprintf(mem, "\"link\":[\n");
    total_len = strlen(mem);
    total_len_first = total_len;
    for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++)
    {
        if(s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].url != NULL) {
            sprintf(temp, "{\"id\" : \"%pU\", \"time\" : %lu, \"status\" : %lu, \"url\" : \"%s\"},\n", s_vpu_inst_info[i].uuid, s_vpu_inst_info[i].time, s_vpu_inst_info[i].status, s_vpu_inst_info[i].url);
            single_list_len = strlen(temp);
            if(single_list_len + total_len > size - 1 ){
                pr_err("maybe overflow the memory, please check..\n");
                break;
            }
            sprintf(mem + total_len, "%s",temp);
            total_len = strlen(mem);
        }
    }
    if(total_len > total_len_first)
        sprintf(mem + total_len - 2, "]}\n");
    else
    {
        sprintf(mem + total_len, "]}\n");
    }
}
static ssize_t info_read(struct file *file, char __user *buf, size_t size, loff_t *off)
{
    int len = 0;
    int err = 0;
    int i = 0;
    loff_t offset = *off;
    size_t bytes;
    if(offset == 0){
        memset(dat,0,MAX_DATA_SIZE);
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
        vmem_info_t vinfo;
        vmem_get_info(&s_vmem, &vinfo);

        size = vinfo.free_pages * (unsigned long)vinfo.page_size;
        sprintf(dat, "{\"total_mem_size\" : %lld, \"used_mem_size\" : %lld, \"free_mem_size\" : %lld,\n", \
                s_vpu_usage_info.vpu_mem_total_size, s_vpu_usage_info.vpu_mem_used_size, s_vpu_usage_info.vpu_mem_free_size);
#else
        sprintf(dat, "{");
#endif
        len = strlen(dat);
        sprintf(dat + len, "\"core\" : [\n");
        for(i=0; i< get_vpu_core_num(chip_id, video_cap)-1; i++) {
            len = strlen(dat);
            sprintf(dat + len, "{\"id\":%d, \"link_num\":%d, \"usage(instant|long)\":%d%%|%llu%%}, \n", i, s_vpu_usage_info.vpu_open_ref_count[i], \
                    s_vpu_usage_info.vpu_instant_usage[i], s_vpu_usage_info.vpu_working_time_in_ms[i]*100/s_vpu_usage_info.vpu_total_time_in_ms[i]);
        }
        len = strlen(dat);
        sprintf(dat + len, "{\"id\":%d, \"link_num\":%d, \"usage(instant|long)\":%d%%|%llu%%}],\n", i, s_vpu_usage_info.vpu_open_ref_count[i], \
                s_vpu_usage_info.vpu_instant_usage[i], s_vpu_usage_info.vpu_working_time_in_ms[i]*100/s_vpu_usage_info.vpu_total_time_in_ms[i]);

        len = strlen(dat);
        if ((err =  mutex_lock_interruptible(&s_vpu_proc_lock)) == 0) {
            printf_inst_info(dat+len, MAX_DATA_SIZE-len);
            mutex_unlock(&s_vpu_proc_lock);
        }
    }
    len = strlen(dat) + 1;
    if(offset < 0)
        return -EINVAL;

    if(offset >= len || size == 0)
        return 0;

    if(size > len - offset)
        size = len - offset;

    bytes = copy_to_user(buf, dat + offset, size);
    if(bytes == size)
    {
        printk("copy_to_user failed!\n");
        return -EFAULT;
    }
    size -= bytes;
    *off = offset + size;
    return size;
}
static ssize_t info_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
//for debug the vpu.
    char data[1024] = { 0 };
    int ret = 0;
    int err = 0;
    int core_idx = -1;
    int inst_idx = -1;
    int in_num = -1;
    int out_num = -1;
    int len;
    int i, j;
    unsigned long flag;
    data[1023] = '\0';
    len = size > 1023 ? 1023 : size;
    //if(size > 1024) { // not to use vmalloc here, to avoid overflow attack from application
    //    data[1023] = '\0';
    //    len = 1023;
    //} else
    //    len = size;
    err = copy_from_user(data, buf, len);
    if (err) return -EINVAL;

    sscanf(data, "%ld ", &flag);
    //pr_info("flag : %ld\n", flag);

    if(flag >  get_vpu_core_num(chip_id, video_cap)) {
        unsigned long __time;
        unsigned long __status;
        unsigned char *__url;
        __url = (unsigned char *)vmalloc(1024);
        __url[0]='\0'; //if in sscanf No URL scanned There may be no '\0' terminator
        if(__url == NULL){
            pr_info("vmalloc url memory failed\n");
            return -ENOMEM;
        }

        ret = sscanf(data, "%ld %ld %ld, %s", &flag, &__time, &__status, __url);
        if ((err =  mutex_lock_interruptible(&s_vpu_proc_lock)) == 0) {
            if(__status == 1) {
                set_stop_inst_info(flag);
            }
            else {
                clean_timeout_inst_info(60*1000);

                i = get_inst_info_idx(flag);
                if(i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE) {
                    if(s_vpu_inst_info[i].url != NULL)
                        vfree(s_vpu_inst_info[i].url);
                    j = strlen(__url);
                    s_vpu_inst_info[i].url = vmalloc(j+1);
                    memcpy(s_vpu_inst_info[i].url, __url, j);
                    s_vpu_inst_info[i].url[j] = 0;
                    s_vpu_inst_info[i].time = __time;
                    s_vpu_inst_info[i].status = __status;
                    s_vpu_inst_info[i].flag = flag;
                    s_vpu_inst_info[i].pid = current->tgid;
                    generate_random_uuid(s_vpu_inst_info[i].uuid);
                }
            }
            mutex_unlock(&s_vpu_proc_lock);
        }

        if (__url) vfree(__url);
        return size;
    }

    sscanf(data, "%d %d %d %d", &core_idx, &inst_idx, &in_num, &out_num);
    pr_info("%s", data);
    pr_info("core_idx: %d, inst: %d, in_num: %d, out_num: %d\n", core_idx, inst_idx, in_num, out_num);
    if(s_vpu_dump_flag) {
        if(core_idx >=0 && core_idx <  get_vpu_core_num(chip_id, video_cap) && inst_idx >= 0 && inst_idx < MAX_NUM_INSTANCE)
        {
            if(in_num >= 0)
                s_vpu_dump_flag[core_idx * MAX_NUM_INSTANCE * 2 + inst_idx * 2] = in_num;
            if(out_num >= 0)
                s_vpu_dump_flag[core_idx * MAX_NUM_INSTANCE * 2 + inst_idx * 2 + 1] = out_num;
        }
        else if(core_idx == -1) {
            for(i=0; i <  get_vpu_core_num(chip_id, video_cap); i++)
            {
                for(j=0; j < 2*MAX_NUM_INSTANCE; j=j+2)
                {
                    pr_info("[%d, %d]; ", s_vpu_dump_flag[i*MAX_NUM_INSTANCE*2+j], s_vpu_dump_flag[i*MAX_NUM_INSTANCE*2+j+1]);
                }
                pr_info("\n");
            }
        }
        else if(core_idx == -2)
        {
            memset(s_vpu_dump_flag, 0, sizeof(u32)* get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE*2);
        }
    }

    return size;
}
static const struct file_operations proc_info_operations = {
    .read   = info_read,
    .write  = info_write,
};

static int __init vpu_init(void)
{
    int res;
    int i;

    DPRINTK("[VPUDRV] begin vpu_init\n");

    chip_id = 0x1684;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
    unsigned int chip_id_tmp = sophon_get_chip_id();
    chip_id = chip_id_tmp >> 16;
#endif
    efuse_ft_get_video_cap(&video_cap);

    if(chip_id == 0x1684)
        vpu_init_get_flags_bm1684(video_cap);
    if(chip_id == 0x1686){
        video_cap = 0; // 1684x no sub mode
        vpu_init_get_flags_bm1686(video_cap);
    }

#ifdef SUPPORT_MULTI_INST_INTR
    for (i=0; i<MAX_NUM_INSTANCE* get_vpu_core_num(chip_id, video_cap); i++) {
        init_waitqueue_head(&s_interrupt_wait_q[i]);
    }

    for (i=0; i<MAX_NUM_INSTANCE* get_vpu_core_num(chip_id, video_cap); i++) {
#define MAX_INTERRUPT_QUEUE (16*MAX_NUM_INSTANCE)
        res = kfifo_alloc(&s_interrupt_pending_q[i], MAX_INTERRUPT_QUEUE*sizeof(u32), GFP_KERNEL);
        if (res) {
            DPRINTK("[VPUDRV] kfifo_alloc failed 0x%x\n", res);
        }
    }
#else
    for (i=0; i< get_vpu_core_num(chip_id, video_cap); i++)
        init_waitqueue_head(&s_interrupt_wait_q[i]);
#endif
    for(i=0; i< get_vpu_core_num(chip_id, video_cap); i++) {
        s_instance_pool[i].base = 0;
        s_common_memory[i].base = 0;
    }
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    res = platform_driver_register(&vpu_driver);
#else
    res = vpu_probe(NULL);
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

    dat = vmalloc(MAX_DATA_SIZE);

    s_vpu_dump_flag = (u32 *)vmalloc(DUMP_FLAG_MEM_SIZE);

    if(s_vpu_dump_flag != NULL)
        memset(s_vpu_dump_flag, 0, DUMP_FLAG_MEM_SIZE);

    entry = proc_create("vpuinfo", 0666, NULL, &proc_info_operations);

    DPRINTK("[VPUDRV] end vpu_init result=0x%x\n", res);
    return res;
}

static void __exit vpu_exit(void)
{
    int i;

#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    DPRINTK("[VPUDRV] vpu_exit\n");

    platform_driver_unregister(&vpu_driver);
    if(dat)
        vfree(dat);

#else /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

#ifdef VPU_SUPPORT_CLOCK_CONTROL
#else
    vpu_clk_disable(s_vpu_clk);
#endif
    vpu_clk_put(s_vpu_clk);
    for(i=0; i< get_vpu_core_num(chip_id, video_cap); i++) {
        if (s_instance_pool[i].base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
            vfree((const void *)s_instance_pool[i].base);
#else
            vpu_free_dma_buffer(&s_instance_pool[i]);
#endif
            s_instance_pool[i].base = 0;
        }
        if (s_common_memory[i].base) {
        vpu_free_dma_buffer(&s_common_memory[i]);
        s_common_memory[i].base = 0;
    }
    }

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base) {
        if(s_video_memory_flag!=0)
            iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
        s_video_memory_flag = 0;
        vmem_exit(&s_vmem);
    }
#endif

    if (s_vpu_major > 0) {
        bm_vpu_unregister_cdev();
    }

#ifdef VPU_SUPPORT_ISR
    for(i=0; i< get_vpu_core_num(chip_id,video_cap); i++) {
        if (s_vpu_irq[i])
            free_irq(s_vpu_irq[i], &s_vpu_drv_context);
    }
#endif

#ifdef SUPPORT_MULTI_INST_INTR
    for (i=0; i<MAX_NUM_INSTANCE* get_vpu_core_num(chip_id, video_cap); i++) {
        kfifo_free(&s_interrupt_pending_q[i]);
        s_interrupt_flag[i] = 0;
    }
#else
    for (i=0; i< get_vpu_core_num(chip_id, video_cap); i++) {
        s_interrupt_flag[i] = 0;
    }
#endif
    for(i=0; i< get_vpu_core_num(chip_id,video_cap); i++) {
        if (s_vpu_register[i].virt_addr) {
            iounmap((void *)s_vpu_register[i].virt_addr);
            s_vpu_register.virt_addr = 0x00;
        }
    }

#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

    if (entry) {
        proc_remove(entry);
        entry = NULL;
    }

    if(s_vpu_dump_flag != NULL)
        vfree(s_vpu_dump_flag);

    mutex_lock(&s_vpu_proc_lock);
    for(i=0; i< get_vpu_core_num(chip_id, video_cap)*MAX_NUM_INSTANCE; i++) {
        if(s_vpu_inst_info[i].url != NULL) {
            vfree(s_vpu_inst_info[i].url);
            s_vpu_inst_info[i].url = NULL;
        }
    }
    mutex_unlock(&s_vpu_proc_lock);
}

MODULE_AUTHOR("Bitmain");
MODULE_DESCRIPTION("VPU linux driver");
MODULE_LICENSE("GPL");

module_init(vpu_init);
module_exit(vpu_exit);

static int vpu_hw_reset(int core_idx)
{
    int ret = 0;

    pr_info("[VPUDRV] request vpu reset from application. \n");

    if(vpu_rst_ctrl.axi2_rst[core_idx]) {
        ret = reset_control_assert(vpu_rst_ctrl.axi2_rst[core_idx]);
        pr_info("reset core index : %d\n", core_idx);
    }

    if(vpu_rst_ctrl.axi2_rst[core_idx]) {
        ret = reset_control_deassert(vpu_rst_ctrl.axi2_rst[core_idx]);
    }

    return ret;
}

static struct clk *vpu_clk_get(struct device *dev)
{
    return clk_get(dev, VPU_CLK_NAME);
}
static void vpu_clk_put(struct clk *clk)
{
    if (!(clk == NULL || IS_ERR(clk)))
        clk_put(clk);
}
static int vpu_clk_enable(struct clk *clk)
{

    if (!(clk == NULL || IS_ERR(clk))) {
        /* the bellow is for C&M EVB.*/
#if 0
        struct clk *s_vpuext_clk = NULL;
        s_vpuext_clk = clk_get(NULL, "vcore");
        if (s_vpuext_clk)
        {
            DPRINTK("[VPUDRV] vcore clk=%p\n", s_vpuext_clk);
            clk_enable(s_vpuext_clk);
        }

        DPRINTK("[VPUDRV] vbus clk=%p\n", s_vpuext_clk);
        if (s_vpuext_clk)
        {
            s_vpuext_clk = clk_get(NULL, "vbus");
            clk_enable(s_vpuext_clk);
        }
#endif
        /* for C&M EVB. */

        DPRINTK("[VPUDRV] vpu_clk_enable\n");
        return clk_enable(clk);
    }

    return 0;
}

static void vpu_clk_disable(struct clk *clk)
{
    if (!(clk == NULL || IS_ERR(clk))) {
        DPRINTK("[VPUDRV] vpu_clk_disable\n");
        clk_disable(clk);
    }
}

