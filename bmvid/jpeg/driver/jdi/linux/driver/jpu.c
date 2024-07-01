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
#include <linux/kernel.h>
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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/of_reserved_mem.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/kthread.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
#include <linux/sched/signal.h>
#endif

#include "../../../jpuapi/jpuconfig.h"
#include "../../../jpuapi/regdefine.h"
#include "../../../include/version.h"

#include "jpu.h"

// #define ENABLE_DEBUG_MSG
#ifdef ENABLE_DEBUG_MSG
#define DPRINTK(args...)        printk(KERN_INFO args);
#else
#define DPRINTK(args...)
#endif

// definitions to be changed as customer  configuration
//#define JPU_SUPPORT_CLOCK_CONTROL             /if you want to have clock gating scheme frame by frame
#define JPU_SUPPORT_ISR
#define JPU_SUPPORT_PLATFORM_DRIVER_REGISTER    //if the platform driver knows the name of this driver. JPU_PLATFORM_DEVICE_NAME

#ifndef USE_ION_MEMORY
#define JPU_SUPPORT_RESERVED_VIDEO_MEMORY       //if this driver knows the dedicated video memory address
#endif

#define JPU_PLATFORM_DEVICE_NAME "jpeg"
#define JPU_CLK_NAME    "jpege" // TODO
#define JPU_CLASS_NAME  "jpu"
#define JPU_DEV_NAME    "jpu"

struct class *jpu_class;

#if defined CONFIG_ARCH_BM1684
#define JPU_REMAP_REG_ADDR 0x50010060
#define JPU_REMAP_REG_SIZE 0x04
static int s_jpu_reg_phy_base[MAX_NUM_JPU_CORE] = {0x50030000, 0x50040000, 0x500B0000, 0x500C0000};
#elif (defined CONFIG_ARCH_BM1880)
static int s_jpu_reg_phy_base[MAX_NUM_JPU_CORE] = {0x50070000};
#elif (defined CONFIG_ARCH_BM1682)
static int s_jpu_reg_phy_base[MAX_NUM_JPU_CORE] = {0x50470000};
#else
#error "!!YOU MUST DEFINE CHIP TYPE FIRST!!"
#endif
#define JPU_REG_SIZE   0x300

#if defined CONFIG_ARCH_BM1684
#define JPU_CONTROL_REG_ADDR 0x50010060
#else
#define JPU_CONTROL_REG_ADDR 0x50008008
#endif
#define JPU_CONTROL_REG_SIZE 0x4

#ifdef JPU_SUPPORT_ISR
#if defined CONFIG_ARCH_BM1684
#define JPU_IRQ_NUM   (28+32)
#define JPU_IRQ_NUM_1 (29+32)
#define JPU_IRQ_NUM_2 (124+32)
#define JPU_IRQ_NUM_3 (125+32)

#elif (defined CONFIG_ARCH_BM1880)
#define JPU_IRQ_NUM   (75+32)
#elif (defined CONFIG_ARCH_BM1682)
#define JPU_IRQ_NUM   (88+32)
#else
    #error "!!YOU MUST DEFINE CHIP TYPE FIRST!!"
#endif
#endif

#ifndef VM_RESERVED // for kernel up to 3.7.0 version
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

extern uint32_t sophon_get_chip_id(void);
extern void efuse_ft_get_video_cap(int *cap);

typedef struct jpu_drv_context_t {
    struct fasync_struct *async_queue;
} jpu_drv_context_t;


/* To track the allocated memory buffer */
typedef struct jpudrv_buffer_pool_t {
    struct list_head list;
    jpudrv_buffer_t jb;
    struct file *filp;
} jpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct jpudrv_instance_list_t {
    struct list_head list;
    unsigned long inst_idx;
    int inuse;
    struct file *filp;
} jpudrv_instance_list_t;


typedef struct jpudrv_instance_pool_t {
    unsigned char jpgInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
} jpudrv_instance_pool_t;

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
#ifdef ASIC_BOX
#  define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (0x2000000)
#  define JPU_DRAM_PHYSICAL_BASE (0x1FE000000)
#else
#if defined CONFIG_ARCH_BM1684
#  define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (0x8000000)
#  define JPU_DRAM_PHYSICAL_BASE (0x400000000)
#elif (defined CONFIG_ARCH_BM1880)
#  define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (0x10000000)
#  define JPU_DRAM_PHYSICAL_BASE (0x150000000)
#elif (defined CONFIG_ARCH_BM1682)
#  define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (0x10000000)
#  define JPU_DRAM_PHYSICAL_BASE (0x150000000)
#else
#  error "!!YOU MUST DEFINE CHIP TYPE FIRST!!"
#endif
#endif
#include "jmm.h"
static jpu_mm_t s_jmem;
static jpudrv_buffer_t s_video_memory = {0};
#endif

#define CHIP_ID_1684 0x1684
#define CHIP_ID_1684x 0x1686

static int jpu_hw_reset(u32 core_idx);
static void jpu_clk_disable(struct clk *clk);
static int jpu_clk_enable(struct clk *clk);

#if 0
static struct clk *jpu_clk_get(struct device *dev);
static void jpu_clk_put(struct clk *clk);
#endif
// end customer definition

static jpudrv_buffer_t s_jpu_instance_pool = {0};
static jpu_drv_context_t s_jpu_drv_context;
static int s_jpu_major = 0;
static struct cdev s_jpu_cdev;
static int s_jpu_open_count = 0;
static u32 *s_jpu_dump_flag = NULL;
static struct proc_dir_entry *entry = NULL;

#ifdef JPU_SUPPORT_ISR
#if defined CONFIG_ARCH_BM1684
static int s_jpu_irq[MAX_NUM_JPU_CORE] = {JPU_IRQ_NUM, JPU_IRQ_NUM_1, JPU_IRQ_NUM_2, JPU_IRQ_NUM_3};
#else
static int s_jpu_irq[MAX_NUM_JPU_CORE] = {JPU_IRQ_NUM};
#endif
static int s_interrupt_flag[MAX_NUM_JPU_CORE] ={0};
#endif

static int s_chip_id = 0;
static int s_jpu_cap = 0;


static int s_jpu_core_enable[MAX_NUM_JPU_CORE] = {1};

#define MAX_JPU_STAT_WIN_SIZE  100
typedef struct jpu_statistic_info {
	int jpu_open_status[MAX_NUM_JPU_CORE];
	uint64_t jpu_working_time_in_ms[MAX_NUM_JPU_CORE];
	uint64_t jpu_total_time_in_ms[MAX_NUM_JPU_CORE];
	atomic_t jpu_busy_status[MAX_NUM_JPU_CORE];
	char jpu_status_array[MAX_NUM_JPU_CORE][MAX_JPU_STAT_WIN_SIZE];
	int jpu_status_index[MAX_NUM_JPU_CORE];
	int jpu_core_usage[MAX_NUM_JPU_CORE];
	int jpu_instant_interval;
}jpu_statistic_info_t;

static DEFINE_MUTEX(s_jpu_proc_lock);
static jpu_statistic_info_t s_jpu_usage_info = {0};
static struct task_struct *s_jpu_monitor_task = NULL;
static int bm_jpu_monitor_thread(void *data);



static jpudrv_buffer_t s_jpu_register[MAX_NUM_JPU_CORE] = {0};
static jpudrv_buffer_t s_jpu_control_register = {0};

typedef struct _jpu_power_ctrl_ {
    struct reset_control *jpu_rst[MAX_NUM_JPU_CORE];
    struct clk           *jpu_clk;
#ifdef CONFIG_ARCH_BM1880
    struct reset_control *axi2_rst;
    struct reset_control *apb_jpeg_rst;
    struct reset_control *jpeg_axi_rst;
#elif defined CONFIG_ARCH_BM1684
    struct clk *jpu_apb_clk[MAX_NUM_JPU_CORE];
    struct clk *jpu_axi_clk[MAX_NUM_JPU_CORE];
#endif
} jpu_power_ctrl;
static jpu_power_ctrl jpu_pwm_ctrl = {0};

#if defined CONFIG_ARCH_BM1684
static jpudrv_buffer_t s_jpu_remap_register = {0};
#endif

static int s_process_count = 0;
static int s_mem_diff      = 0;
static int s_mem_size      = 0;
static int s_core[4]       = {0};
static wait_queue_head_t s_interrupt_wait_q[MAX_NUM_JPU_CORE];

#define MUTEX_LOCK
#ifdef MUTEX_LOCK
static struct mutex  s_jpu_lock;
static struct mutex  s_buffer_lock;
#define LOCK( NAME )    mutex_lock(&NAME)
#define UNLOCK( NAME )   mutex_unlock(&NAME)
#else
static spinlock_t s_jpu_lock = __SPIN_LOCK_UNLOCKED(s_jpu_lock);
static spinlock_t s_buffer_lock = __SPIN_LOCK_UNLOCKED(s_buffer_lock);
#define LOCK( NAME )    spin_lock(&NAME)
#define UNLOCK( NAME )   spin_unlock(&NAME)
#endif

static DEFINE_SEMAPHORE(s_jpu_sem);
static struct list_head s_jbp_head = LIST_HEAD_INIT(s_jbp_head);
static struct list_head s_jpu_inst_list_head = LIST_HEAD_INIT(s_jpu_inst_list_head);
struct semaphore s_jpu_core;

/* implement to power management functions */
#define ReadJpuRegister(core_idx, addr)           *(volatile uint32_t *)(s_jpu_register[core_idx].virt_addr + addr)
#define WriteJpuRegister(core_idx, addr, val)     *(volatile uint32_t *)(s_jpu_register[core_idx].virt_addr + addr) = (uint32_t)val;

int bm_jpu_register_cdev(struct platform_device *pdev);

#ifdef CONFIG_ARCH_BM1880
static int jpu_allocate_memory(struct platform_device *pdev);
static void bm_jpu_assert(jpu_power_ctrl *pRstCtrl);
static void bm_jpu_deassert(jpu_power_ctrl *pRstCtrl);
#endif
static void bm_jpu_unregister_cdev(void);



static int get_max_num_jpu_core(void) {

    if (s_chip_id == CHIP_ID_1684) {
        if (s_jpu_cap == 1) {
            return MAX_NUM_JPU_CORE_2;
        } else if (s_jpu_cap == 2) {
            return MAX_NUM_JPU_CORE_2;
        } else if (s_jpu_cap == 3) {
            return 0;
        } else {
            return MAX_NUM_JPU_CORE;
        }

    } else if (s_chip_id == CHIP_ID_1684x) {
        if (s_jpu_cap == 1) {
            return MAX_NUM_JPU_CORE_1;
        } else if (s_jpu_cap == 2){
            return MAX_NUM_JPU_CORE_1;
        } else if (s_jpu_cap == 3){
            return 0;
        } else {
            return MAX_NUM_JPU_CORE_2;
        }
    }
    return 0;
}

static int jpu_init_bm1684x_bin(void)
{
    if (s_chip_id == CHIP_ID_1684) {
        if (s_jpu_cap == 1) {
            s_jpu_irq[0]                 = s_jpu_irq[2];
            s_jpu_irq[1]                 = s_jpu_irq[3];
            s_jpu_reg_phy_base[0]        = s_jpu_reg_phy_base[2];
            s_jpu_reg_phy_base[1]        = s_jpu_reg_phy_base[3];
            jpu_pwm_ctrl.jpu_rst[0]      = jpu_pwm_ctrl.jpu_rst[2];
            jpu_pwm_ctrl.jpu_rst[1]      = jpu_pwm_ctrl.jpu_rst[3];
            jpu_pwm_ctrl.jpu_apb_clk[0]  = jpu_pwm_ctrl.jpu_apb_clk[2];
            jpu_pwm_ctrl.jpu_apb_clk[1]  = jpu_pwm_ctrl.jpu_apb_clk[3];
            jpu_pwm_ctrl.jpu_axi_clk[0]  = jpu_pwm_ctrl.jpu_axi_clk[2];
            jpu_pwm_ctrl.jpu_axi_clk[1]  = jpu_pwm_ctrl.jpu_axi_clk[3];
            memcpy(&s_jpu_register[0], &s_jpu_register[2], sizeof(jpudrv_buffer_t));
            memcpy(&s_jpu_register[1], &s_jpu_register[3], sizeof(jpudrv_buffer_t));
        }
    } else if (s_chip_id == CHIP_ID_1684x) {
        if (s_jpu_cap == 0) {
            s_jpu_irq[1]                 = s_jpu_irq[2];
            s_jpu_reg_phy_base[1]        = s_jpu_reg_phy_base[2];
            jpu_pwm_ctrl.jpu_rst[1]      = jpu_pwm_ctrl.jpu_rst[2];
            jpu_pwm_ctrl.jpu_apb_clk[1]  = jpu_pwm_ctrl.jpu_apb_clk[2];
            jpu_pwm_ctrl.jpu_axi_clk[1]  = jpu_pwm_ctrl.jpu_axi_clk[2];
            memcpy(&s_jpu_register[1], &s_jpu_register[2], sizeof(jpudrv_buffer_t));
        } else if(s_jpu_cap == 1) {
            s_jpu_irq[0]                 = s_jpu_irq[2];
            s_jpu_reg_phy_base[0]        = s_jpu_reg_phy_base[2];
            jpu_pwm_ctrl.jpu_rst[0]      = jpu_pwm_ctrl.jpu_rst[2];
            jpu_pwm_ctrl.jpu_apb_clk[0]  = jpu_pwm_ctrl.jpu_apb_clk[2];
            jpu_pwm_ctrl.jpu_axi_clk[0]  = jpu_pwm_ctrl.jpu_axi_clk[2];
            memcpy(&s_jpu_register[0], &s_jpu_register[2], sizeof(jpudrv_buffer_t));
        }
    }

    return 0;
}



#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
static int jpu_alloc_dma_buffer(jpudrv_buffer_t *jb)
{
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    jb->phys_addr = (unsigned long)jmem_alloc(&s_jmem, jb->size, 0);
    if (jb->phys_addr  == (unsigned long)-1)
    {
        DPRINTK("[JPUDRV] Physical memory allocation error size=%d\n", jb->size);
        return -1;
    }

    jb->base = (unsigned long)(s_video_memory.base + (jb->phys_addr - s_video_memory.phys_addr));
#else
    jb->base = (unsigned long)dma_alloc_coherent(NULL, PAGE_ALIGN(jb->size), (dma_addr_t *) (&jb->phys_addr), GFP_DMA | GFP_KERNEL);
    if ((void *)(jb->base) == NULL)
    {
        DPRINTK("[JPUDRV] Physical memory allocation error size=%d\n", jb->size);
        return -1;
    }
#endif
    return 0;
}
#endif

static int jpu_free_dma_buffer(jpudrv_buffer_t *jb)
{
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (jb->base)
    {
        int ret = jmem_free(&s_jmem, jb->phys_addr, 0);
        if (ret < 0)
            return -1;
    }
#else
    if (jb->base)
    {
        dma_free_coherent(0, PAGE_ALIGN(jb->size), (void *)jb->base, jb->phys_addr);
    }
#endif
    return 0;
}

static int jpu_free_instances(struct file *filp)
{
    jpudrv_instance_list_t *jil, *n;
    jpudrv_instance_pool_t *jip;

    list_for_each_entry_safe(jil, n, &s_jpu_inst_list_head, list)
    {
        if (jil->filp == filp)
        {
            if(jil->inuse == 1)
            {
                Uint32 val;
                mdelay(100);           // 100ms time  for  JPU  decoding/encoding a  4k yuv444 image picture
                jil->inuse = 0;
                s_jpu_open_count--;
                jil->filp = NULL;
                DPRINTK(" s_mem_diff=%d, s_jpu_open_count=%d\n", s_mem_diff , s_jpu_open_count);
                val = ReadJpuRegister(jil->inst_idx, MJPEG_PIC_STATUS_REG);
                if( val != 0){
                    WriteJpuRegister(jil->inst_idx, MJPEG_PIC_STATUS_REG, val);
#if defined CONFIG_ARCH_BM1684
                    jpu_clk_disable(jpu_pwm_ctrl.jpu_axi_clk[jil->inst_idx]);
                    jpu_clk_disable(jpu_pwm_ctrl.jpu_apb_clk[jil->inst_idx]);
#endif
                }
                up(&s_jpu_core);
                printk(KERN_INFO  " val = %x ,  core_index = %ld\n", val, jil->inst_idx);
            }
            jil->filp = NULL;
            jip = (jpudrv_instance_pool_t *)s_jpu_instance_pool.base;
            if (jip)
                memset(&jip->jpgInstPool[jil->inst_idx], 0x00, 4);   // only first 4 byte is key point to free the corresponding instance.
            DPRINTK(" jpu_free_instances end,filp=%p\n" ,filp);
        }
    }
    return s_jpu_open_count;
}

static int jpu_free_buffers(struct file *filp)
{
    jpudrv_buffer_pool_t *pool, *n;
    jpudrv_buffer_t jb;
    int ret;
    list_for_each_entry_safe(pool, n, &s_jbp_head, list)
    {
        if (pool->filp == filp)
        {
            jb = pool->jb;

            if (jb.base)
            {
                s_mem_diff--;
                s_mem_size -= jb.size;
                DPRINTK(" jpu_free_buffers filp=%p,jb.size=%d,jb.base=%p,s_mem_diff=%d,s_mem_size=%d\n",filp,jb.size,jb.base,s_mem_diff,s_mem_size);
                ret = jpu_free_dma_buffer(&jb);
                if (ret < 0)
                {
                    printk(KERN_INFO " jpu_free_buffers error\n");
                }
                list_del(&pool->list);
                kfree(pool);
            }
            else{
                DPRINTK("jb.base null\n");
            }
        }
    }
    return 0;
}

#ifdef JPU_SUPPORT_ISR
static irqreturn_t jpu_irq_handler(int irq, void *dev_id)
{
    int core = 0;
    jpu_drv_context_t *dev = (jpu_drv_context_t *)dev_id;

    disable_irq_nosync( irq );


    for(core = 0; core < get_max_num_jpu_core(); core++)
    {
        if(s_jpu_irq[core] == irq)
            break;
    }

    if (dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN); // notify the interrupt to userspace

    s_interrupt_flag[core] = 1;

    wake_up_interruptible(&s_interrupt_wait_q[core]);
    return IRQ_HANDLED;
}
#endif

static int jpu_open(struct inode *inode, struct file *filp)
{
    jpudrv_instance_list_t *jil , *n;
    jpudrv_buffer_pool_t *jbp, *n2;

    if (!s_process_count)
    {
#ifdef CONFIG_ARCH_BM1880
        bm_jpu_deassert(&jpu_pwm_ctrl);
#endif

#if defined(CONFIG_ARCH_BM1682)
        /* Enable JPU clock */
        {
            volatile unsigned int* p = (unsigned int *)s_jpu_control_register.virt_addr;
            unsigned int val = *p;
            if ((val & 0x08000000) == 0) {
                DPRINTK("Enable JPU clock\n");
                *p = val | 0x08000000;
            }
        }
#endif

#if 0 //defined(CONFIG_ARCH_BM1684)
        {
            //config jpeg remap
            volatile unsigned int* remap_reg = (unsigned int *)s_jpu_remap_register.virt_addr;
            unsigned int remap_val = *remap_reg;
            remap_val = (remap_val & 0xFFFFFF00) | 0x04;
            *remap_reg = remap_val;
        }
#endif
        LOCK(s_jpu_lock);
        list_for_each_entry_safe(jil, n, &s_jpu_inst_list_head, list)
        {
            DPRINTK("s_jpu_inst_list_head inst_idx=%d, jil->inuse=%d, jil->filp=%p, filp=%p\n", (int)jil->inst_idx, jil->inuse , jil->filp,filp);
        }
        UNLOCK(s_jpu_lock);

        LOCK(s_buffer_lock);
        list_for_each_entry_safe(jbp, n2, &s_jbp_head, list)
        {
            DPRINTK("s_jbp_head  jbp->jb.base=%p, jbp->filp= %p, filp=%p\n",  jbp->jb.base , jbp->filp,filp);
            list_del(&jbp->list);
            kfree(jbp);
        }
        UNLOCK(s_buffer_lock);
    }
    s_process_count++;
    DPRINTK("jpu_open: s_process_count=%d,s_mem_diff=%d\n", s_process_count,s_mem_diff);

    LOCK(s_jpu_lock);
    filp->private_data = (void *)(&s_jpu_drv_context);
    UNLOCK(s_jpu_lock);
    return 0;
}

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
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

#if defined CONFIG_ARCH_BM1684
int jpu_set_remap(unsigned int coreIdx, unsigned long read_addr, unsigned long write_addr)
{
    unsigned int val,tmp, read_channel_id,write_channel_id, shift_n;
    read_channel_id = read_addr >> 32;
    write_channel_id = write_addr >> 32;

    if(read_channel_id > 4 || read_channel_id == 0)
    {
        printk(KERN_INFO "reading phys addr range is error! phys_addr = %lx, coreIdx = %d\n", read_addr, coreIdx);
        return -1;
    }

    if(write_channel_id > 4 || write_channel_id == 0)
    {
        printk(KERN_INFO "writing phys addr range is error! phys_addr = %lx, coreIdx = %d\n", write_addr, coreIdx);
        return -1;
    }

    if (s_chip_id == CHIP_ID_1684x) {
        shift_n = coreIdx << 4; // use jpeg1 subsys0 when coreIdx is 1.
    } else {
        shift_n = coreIdx << 3;
    }

    tmp = ((read_channel_id & 0x7) << (shift_n+4)) | ((write_channel_id & 0x7) << shift_n);

    val = *(volatile unsigned int *)s_jpu_control_register.virt_addr;

    *(volatile unsigned int *)s_jpu_control_register.virt_addr = (val & (~(0xff << shift_n))) | tmp;
    //printk(KERN_INFO "[JPUDRV]  remap register = 0x%x, read_addr = 0x%lx, write_addr = 0x%lx, coreIdx = %d, val = 0x%x\n",tmp, read_addr, write_addr, coreIdx, *(volatile unsigned int *)s_jpu_control_register.virt_addr );
    return 0;
}
#endif

static long jpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
    int ret = 0;

    switch (cmd)
    {
    case JDI_IOCTL_GET_MAX_NUM_JPU_CORE:
        {
            int max_nums_jpu_core;
            max_nums_jpu_core = get_max_num_jpu_core();
            ret = copy_to_user((void __user *)arg, &max_nums_jpu_core, sizeof(int));
            if (ret != 0)
                ret = -EFAULT;
        }
        break;
    case JDI_IOCTL_GET_INSTANCE_CORE_INDEX:
        {
            jpudrv_instance_list_t *jil , *n;
            jpudrv_remap_info_t  info;
            unsigned long  core_index = -1;
            int inuse = -1;

            ret = copy_from_user(&info, (jpudrv_remap_info_t *)arg, sizeof(jpudrv_remap_info_t));
            if(ret)
            {
                printk(KERN_INFO "error copy_from_user\n");
                ret = -EFAULT;
                break;
            }
            ret = down_interruptible(&s_jpu_core);
            if (ret == 0)
            {
                LOCK(s_jpu_lock);
                list_for_each_entry_safe(jil, n, &s_jpu_inst_list_head, list)
                {
                    if (jil->inuse == 0)
                    {
                        jil->inuse = 1;
                        jil->filp = filp;
                        s_jpu_open_count++;
                        inuse = 1;
                        core_index = jil->inst_idx;
                        s_jpu_usage_info.jpu_open_status[core_index] = 1;

                        s_core[core_index]++;
                        //printk(KERN_INFO "[JPUDRV] 11111111 inst_idx=%d, s_jpu_open_count=%d,filp=%p\n", (int)jil->inst_idx, s_jpu_open_count,filp);
                        break;
                    }
                    else{
                        //printk(KERN_INFO "error jil->inuse == 1,filp=%p\n",filp);
                    }
                }
#if defined CONFIG_ARCH_BM1684
                if(inuse == 1)
                {
                    jpu_set_remap(core_index, info.read_addr, info.write_addr);
                    list_del(&jil->list);
                    list_add_tail(&jil->list, &s_jpu_inst_list_head);
                }
#endif
                UNLOCK(s_jpu_lock);

                if(inuse == 1)
                {
                    info.core_idx = core_index;
#if defined CONFIG_ARCH_BM1684
                    jpu_clk_enable(jpu_pwm_ctrl.jpu_apb_clk[core_index]);
                    jpu_clk_enable(jpu_pwm_ctrl.jpu_axi_clk[core_index]);
                    jpu_hw_reset(core_index);
#endif
                    ret = copy_to_user((void __user *)arg, &info, sizeof(jpudrv_remap_info_t));
                    if (ret)
                    {
                        ret = -EFAULT;
                    }
                }
                if (signal_pending(current))
                {
                    if (core_index >= 0) {
                        DPRINTK("signal_pending down_interruptible ret=%d   core_index=%d \n",ret, core_index);
                        break;
                    } else {
                        DPRINTK("signal_pending down_interruptible->up ret=%d   core_index=%d \n",ret, core_index);
                        up(&s_jpu_core);
                        ret = -ERESTARTSYS;
                        break;
                    }
                }
            } else {
                if (signal_pending(current))
                {
                    DPRINTK("signal_pending down_interruptible failed. ret=%d core_index=%d \n",ret, core_index);
                    ret = -ERESTARTSYS;
                    break;
                }
            }
        }
        break;
    case JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX:
        {
            u32 core_index;
            jpudrv_instance_list_t *jil , *n;

            if (get_user(core_index, (u32 __user *) arg))
            {
                DPRINTK("[JPUDRV] JDI_FREE_FREE_INSTANCE_CORE_NO  get_user error\n");
                return -EFAULT;
            }

            LOCK(s_jpu_lock);

            list_for_each_entry_safe(jil, n, &s_jpu_inst_list_head, list)
            {
                if (jil->inst_idx == core_index && jil->filp == filp)
                {
                    s_jpu_open_count--;
                    jil->inuse = 0;
                    s_jpu_usage_info.jpu_open_status[core_index] = 0;

#if defined CONFIG_ARCH_BM1684
                    jpu_clk_disable(jpu_pwm_ctrl.jpu_axi_clk[core_index]);
                    jpu_clk_disable(jpu_pwm_ctrl.jpu_apb_clk[core_index]);
#endif
                    break;
                }
            }
            UNLOCK(s_jpu_lock);
            up(&s_jpu_core);
        }
        break;
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    case JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
        {
            jpudrv_buffer_pool_t *jbp;
            int size;

            if((ret = down_interruptible(&s_jpu_sem)) == 0)
            {
                jbp = kzalloc(sizeof(*jbp), GFP_KERNEL);
                if (!jbp)
                {
                    up(&s_jpu_sem);
                    return -ENOMEM;
                }

                ret = copy_from_user(&(jbp->jb), (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
                if (ret)
                {
                    kfree(jbp);
                    up(&s_jpu_sem);
                    return -EFAULT;
                }
                size = jbp->jb.size;
                ret = jpu_alloc_dma_buffer(&(jbp->jb));
                if (ret == -1)
                {
                    ret = -ENOMEM;
                    kfree(jbp);
                    up(&s_jpu_sem);
                    break;
                }

                ret = copy_to_user((void __user *)arg, &(jbp->jb), sizeof(jpudrv_buffer_t));
                if (ret)
                {
                    kfree(jbp);
                    ret = -EFAULT;
                    up(&s_jpu_sem);
                    break;
                }

                jbp->filp = filp;
                LOCK(s_buffer_lock);
                list_add(&jbp->list, &s_jbp_head);
                UNLOCK(s_buffer_lock);
                s_mem_diff++;
                s_mem_size += size;
                //printk(KERN_INFO "[JPUDRV] 333 s_mem_size=%d, s_mem_diff=%d,size=%d,filp=%p\n", s_mem_size, s_mem_diff,size,filp);

                up(&s_jpu_sem);
            }
        }
        break;

    case JDI_IOCTL_FREE_PHYSICAL_MEMORY:
        {
            jpudrv_buffer_pool_t *jbp, *n;
            jpudrv_buffer_t jb;
            int size = 0;
            if((ret = down_interruptible(&s_jpu_sem)) == 0)
            {
                ret = copy_from_user(&jb, (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
                if (ret)
                {
                    up(&s_jpu_sem);
                    return -EACCES;
                }

                if (jb.base)
                {
                    size = jb.size;
                    ret = jpu_free_dma_buffer(&jb);
                    if (ret < 0)
                    {
                        up(&s_jpu_sem);
                        return -EACCES;
                    }
                }

                LOCK(s_buffer_lock);
                list_for_each_entry_safe(jbp, n, &s_jbp_head, list)
                {
                    if (jbp->jb.base == jb.base)
                    {
                        list_del(&jbp->list);
                        kfree(jbp);
                        break;
                    }
                }
                UNLOCK(s_buffer_lock);

                s_mem_diff--;
                s_mem_size -= size;
                //printk(KERN_INFO "[JPUDRV] 444 s_mem_size=%d, s_mem_diff=%d,size=%d,filp=%p\n", s_mem_size, s_mem_diff,size,filp);

                up(&s_jpu_sem);
            }
        }
        break;

    case JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
        {
            if (s_video_memory.base == 0)
            {
                ret = -EFAULT;
                break;
            }

            ret = copy_to_user((void __user *)arg, &s_video_memory, sizeof(jpudrv_buffer_t));
            if (ret != 0)
                ret = -EFAULT;
        }
        break;
#endif

#ifdef JPU_SUPPORT_ISR
    case JDI_IOCTL_WAIT_INTERRUPT:
        {
            jpudrv_intr_info_t  info;
            ret = copy_from_user(&info, (jpudrv_intr_info_t *)arg, sizeof(jpudrv_intr_info_t));
            if(ret != 0)
                return -EFAULT;
            atomic_inc(&s_jpu_usage_info.jpu_busy_status[info.core_idx]);

            if (!wait_event_interruptible_timeout(s_interrupt_wait_q[info.core_idx], s_interrupt_flag[info.core_idx] != 0, msecs_to_jiffies(info.timeout)))
            {
                ret = -ETIME;
                atomic_dec(&s_jpu_usage_info.jpu_busy_status[info.core_idx]);
                break;
            }

            if (signal_pending(current))
            {
                ret = -ERESTARTSYS;
                atomic_dec(&s_jpu_usage_info.jpu_busy_status[info.core_idx]);
                break;
            }

            atomic_dec(&s_jpu_usage_info.jpu_busy_status[info.core_idx]);

            s_interrupt_flag[info.core_idx] = 0;

            enable_irq(s_jpu_irq[info.core_idx]);

        }
        break;
#endif  //JPU_SUPPORT_ISR
#if 0
    case JDI_IOCTL_SET_CLOCK_GATE:
        {
            jpudrv_clkgate_info_t  clkgate_info;
            ret = copy_from_user(&clkgate_info, (jpudrv_clkgate_info_t *)arg, sizeof(jpudrv_clkgate_info_t));
            if (ret != 0)
            {
                ret = -EFAULT;
                break;
            }
#ifdef JPU_SUPPORT_CLOCK_CONTROL
            if (clkgate)
                jpu_clk_enable(jpu_pwm_ctrl.jpu_clk);
            else
                jpu_clk_disable(jpu_pwm_ctrl.jpu_clk);
#endif
        }
        break;
    case JDI_IOCTL_GET_INSTANCE_POOL:
        {
            down(&s_jpu_sem);
            printk(KERN_INFO "JDI_IOCTL_GET_INSTANCE_POOL  \n");
            if (s_jpu_instance_pool.base == 0)
            {
                ret = copy_from_user(&s_jpu_instance_pool, (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
                if (ret != 0)
                {
                    ret = -EFAULT;
                    up(&s_jpu_sem);
                    break;
                }

#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
                s_jpu_instance_pool.size = PAGE_ALIGN(s_jpu_instance_pool.size);
                s_jpu_instance_pool.phys_addr =
                s_jpu_instance_pool.base = (unsigned long)vmalloc(s_jpu_instance_pool.size);
                if (s_jpu_instance_pool.base == 0)
                {
                    ret = -EFAULT;
                    up(&s_jpu_sem);
                    break;
                }
                memset((void *)s_jpu_instance_pool.base, 0x0, s_jpu_instance_pool.size);
#else
                if (jpu_alloc_dma_buffer(&s_jpu_instance_pool) == -1)
                {
                    ret = -EFAULT;
                    up(&s_jpu_sem);
                    break;
                }
                memset_io((void *)s_jpu_instance_pool.base, 0x0, s_jpu_instance_pool.size);
#endif
                DPRINTK("s_jpu_instance_pool base: 0x%lx, size: %d\n",
                        s_jpu_instance_pool.base, s_jpu_instance_pool.size);
            }

            ret = copy_to_user((void __user *)arg, &s_jpu_instance_pool, sizeof(jpudrv_buffer_t));
            if (ret != 0)
                ret = -EFAULT;

            up(&s_jpu_sem);

        }
        break;

    case JDI_IOCTL_OPEN_INSTANCE:
        {
            u32 inst_idx;
            jpudrv_instance_list_t *jil;

            jil = kzalloc(sizeof(*jil), GFP_KERNEL);
            if (!jil)
                return -ENOMEM;

            if (get_user(inst_idx, (u32 __user *) arg))
            {
                kfree(jil);
                return -EFAULT;
            }

            jil->inst_idx = inst_idx;
            jil->filp = filp;
            spin_lock(&s_jpu_lock);
            list_add(&jil->list, &s_jpu_inst_list_head);
            s_jpu_open_count++;
            printk(KERN_INFO "[JPUDRV] JDI_IOCTL_OPEN_INSTANCE inst_idx=%d, open_count=%d\n", (int)inst_idx, s_jpu_open_count);
            spin_unlock(&s_jpu_lock);
        }
        break;
    case JDI_IOCTL_CLOSE_INSTANCE:
        {
            u32 inst_idx;
            jpudrv_instance_list_t *jil, *n;

            if (get_user(inst_idx, (u32 __user *) arg))
                return -EFAULT;

            spin_lock(&s_jpu_lock);

            list_for_each_entry_safe(jil, n, &s_jpu_inst_list_head, list)
            {
                if (jil->inst_idx == inst_idx)
                {
                    s_jpu_open_count--;
                    jil->inuse = 0;
                    printk(KERN_INFO "[JPUDRV] JDI_IOCTL_CLOSE_INSTANCE inst_idx=%d, open_count=%d\n", (int)inst_idx, s_jpu_open_count);
                    break;
                }
            }
            spin_unlock(&s_jpu_lock);

        }
        break;

    case JDI_IOCTL_GET_INSTANCE_NUM:
        {
            down(&s_jpu_sem);
            ret = copy_to_user((void __user *)arg, &s_jpu_open_count, sizeof(int));
            if (ret != 0)
                ret = -EFAULT;

            DPRINTK("[JPUDRV] VDI_IOCTL_GET_INSTANCE_NUM open_count=%d\n", s_jpu_open_count);
            up(&s_jpu_sem);
        }
        break;
    case JDI_IOCTL_GET_CONTROL_REG:
        {
            DPRINTK("[JPUDRV][+]JDI_IOCTL_GET_CONTROL_REG\n");
            ret = copy_to_user((void __user *)arg, &s_jpu_control_register, sizeof(jpudrv_buffer_t));
            if (ret != 0)
                ret = -EFAULT;
            DPRINTK("[JPUDRV][-]JDI_IOCTL_GET_CONTROL_REG: phys_addr==0x%lx, virt_addr=0x%lx, size=%d\n",
                    s_jpu_control_register.phys_addr, s_jpu_control_register.virt_addr, s_jpu_control_register.size);
        }
        break;
#endif
    case JDI_IOCTL_RESET:
    #if 0
        {
            u32 core_idx;
            if (get_user(core_idx, (u32 __user *) arg))
            {
                return -EFAULT;
            }
            jpu_hw_reset(core_idx);
        }
        break;
    #endif
    case JDI_IOCTL_RESET_ALL:
        {
            u32 i, core_num;

            DPRINTK("[JPUDRV][+]JDI_IOCTL_RESET_ALL\n");
            if (get_user(core_num, (u32 __user *) arg))
            {
                return -EFAULT;
            }

            // get all cores
            i = core_num;
            while (i > 0) {
                if ((ret = down_interruptible(&s_jpu_sem)) == 0) {
                    i--;
                }
                udelay(1);
            }

            for (i = 0; i < core_num; i++) {
                jpu_hw_reset(i);
                up(&s_jpu_sem);
            }
            DPRINTK("[JPUDRV][-]JDI_IOCTL_RESET_ALL\n");
        }
        break;
    case JDI_IOCTL_GET_REGISTER_INFO:
        {
            DPRINTK("[JPUDRV][+]JDI_IOCTL_GET_REGISTER_INFO\n");
            ret = copy_to_user((void __user *)arg, &s_jpu_register, sizeof(jpudrv_buffer_t)*get_max_num_jpu_core());
            if (ret != 0)
                ret = -EFAULT;
            DPRINTK("[JPUDRV][-]JDI_IOCTL_GET_REGISTER_INFO: phys_addr==0x%lx, virt_addr=0x%lx, size=%d\n",
                    s_jpu_register[0].phys_addr, s_jpu_register[0].virt_addr, s_jpu_register[0].size);
        }
        break;
    case JDI_IOCTL_INVAL_DCACHE_RANGE:
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
        if((ret = down_interruptible(&s_jpu_sem)) == 0)
        {
            dcache_range_t dcr;
            unsigned long base;

            DPRINTK("[JPUDRV][+]JDI_IOCTL_INVAL_DCACHE_RANGE\n");
            ret = copy_from_user(&dcr, (dcache_range_t*)arg, sizeof(dcache_range_t));
            if (ret != 0) {
                ret = -EFAULT;
                up(&s_jpu_sem);
                break;
            }

            base = (unsigned long)(s_video_memory.base + (dcr.start - s_video_memory.phys_addr));
            invalidate_dcache_range(base, dcr.size);

            DPRINTK("[JPUDRV][-]JDI_IOCTL_INVAL_DCACHE_RANGE: virt_addr=0x%lx, size=%d\n",
                    dcr.start, dcr.size);
            up(&s_jpu_sem);
        }
#endif
        break;
    case JDI_IOCTL_FLUSH_DCACHE_RANGE:
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
        if((ret = down_interruptible(&s_jpu_sem)) == 0)
        {
            dcache_range_t dcr;
            unsigned long base;

            DPRINTK("[JPUDRV][+]JDI_IOCTL_FLUSH_DCACHE_RANGE\n");
            ret = copy_from_user(&dcr, (dcache_range_t*)arg, sizeof(dcache_range_t));
            if (ret != 0) {
                ret = -EFAULT;
                up(&s_jpu_sem);
                break;
            }

            base = (unsigned long)(s_video_memory.base + (dcr.start - s_video_memory.phys_addr));
            flush_dcache_range(base, dcr.size);

            DPRINTK("[JPUDRV][-]JDI_IOCTL_FLUSH_DCACHE_RANGE: virt_addr=0x%lx, size=%d\n",
                    dcr.start, dcr.size);
            up(&s_jpu_sem);
        }
#endif
        break;
    default:
        {
            printk(KERN_ERR "No such IOCTL, cmd is %d\n", cmd);
        }
        break;
    }
    return ret;
}

static ssize_t jpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    return -1;
}

static ssize_t jpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
    return -1;
}

static int jpu_release(struct inode *inode, struct file *filp)
{
    DPRINTK("jpu_release: s_process_count=%d\n", s_process_count);

    down(&s_jpu_sem);

    LOCK(s_jpu_lock);
    jpu_free_instances(filp);
    UNLOCK(s_jpu_lock);

    /* found and free the not handled buffer by user applications */
    LOCK(s_buffer_lock);
    jpu_free_buffers(filp);
    UNLOCK(s_buffer_lock);

    s_process_count--;
    if (s_process_count == 0)
    {
        /* found and free the instance not closed by user applications */
#if 0
        if (s_jpu_instance_pool.base)
        {
            DPRINTK("[JPUDRV] free instance pool\n");
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
            vfree((const void *)s_jpu_instance_pool.base);
#else
            jpu_free_dma_buffer(&s_jpu_instance_pool);
#endif
            s_jpu_instance_pool.base = 0;
        }
#endif
#ifdef CONFIG_ARCH_BM1880
        bm_jpu_assert(&jpu_pwm_ctrl);
#endif

        /* Disable JPU clock */
#if defined(CONFIG_ARCH_BM1682)
        {
            volatile unsigned int* p = (unsigned int *)s_jpu_control_register.virt_addr;
            unsigned int val = *p;
            if ((val & 0x08000000) == 1) {
                printk(KERN_INFO "Disable JPU clock\n");
                *p = val & (~0x08000000);
            }
        }
#endif
    }


    up(&s_jpu_sem);
    return 0;
}


static int jpu_fasync(int fd, struct file *filp, int mode)
{
    struct jpu_drv_context_t *dev = (struct jpu_drv_context_t *)filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}


static int jpu_map_to_register(struct file *filp, struct vm_area_struct *vm,int core_idx)
{
    unsigned long pfn;

    vm->vm_flags |= VM_IO | VM_RESERVED;
    vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
    pfn = s_jpu_register[core_idx].phys_addr >> PAGE_SHIFT;

    return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int jpu_map_to_physical_memory(struct file *filp, struct vm_area_struct *vm)
{
    jpudrv_buffer_pool_t *jbp, *n;
    int pgprot_flags = 0;

    vm->vm_flags |= VM_IO | VM_RESERVED;

    LOCK(s_buffer_lock);
    list_for_each_entry_safe(jbp, n, &s_jbp_head, list)
    {
        if (vm->vm_pgoff == (jbp->jb.phys_addr>>PAGE_SHIFT))
        {
            DPRINTK("[JPUDRV] vm->vm_pgoff = 0x%lx\n", jbp->jb.phys_addr);
            pgprot_flags = jbp->jb.flags;
            break;
        }
    }
    UNLOCK(s_buffer_lock);

    if (pgprot_flags == 1)
        vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot);
    else if (pgprot_flags == 2)
        vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
    else
        vm->vm_page_prot = vm_get_page_prot(vm->vm_flags);

    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

#if 0
static int jpu_map_to_instance_pool_memory(struct file *filp, struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    int ret;
    long length = vm->vm_end - vm->vm_start;
    unsigned long start = vm->vm_start;
    char *vmalloc_area_ptr = (char *)s_jpu_instance_pool.base;
    unsigned long pfn;

    vm->vm_flags |= VM_RESERVED;

    /* loop over all pages, map it page individually */
    while (length > 0)
    {
        pfn = vmalloc_to_pfn(vmalloc_area_ptr);
        ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED);
        if (ret < 0) {
            return ret;
        }
        start += PAGE_SIZE;
        vmalloc_area_ptr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }


    return 0;
#else

    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
#endif
}
#endif

static int jpu_map_vmalloc(struct file *fp, struct vm_area_struct *vm, char *vmalloc_area_ptr)
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
 * @brief memory map interface for jpu file operation
 * @return  0 on success or negative error code on error
 */
static int jpu_mmap(struct file *filp, struct vm_area_struct *vm)
{
    int i = 0;
#if 0
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    if (vm->vm_pgoff == 0)
        return jpu_map_to_instance_pool_memory(filp, vm);
#else
    if(vm->vm_pgoff == (s_jpu_instance_pool.phys_addr>>PAGE_SHIFT))
        return  jpu_map_to_instance_pool_memory(filp, vm);
#endif
#endif
    if (vm->vm_pgoff == 0)
        return jpu_map_vmalloc(filp, vm, (char *)s_jpu_dump_flag);

    for (i=0; i<get_max_num_jpu_core(); i++)
    {
        if (vm->vm_pgoff == (s_jpu_register[i].phys_addr>>PAGE_SHIFT))
        {
            DPRINTK("jpu core %d,vm->vm_pgoff = 0x%p\n",i,s_jpu_register[i].phys_addr);
            return jpu_map_to_register(filp, vm, i);
        }
    }

    return jpu_map_to_physical_memory(filp, vm);
}

struct file_operations jpu_fops = {
    .owner = THIS_MODULE,
    .open = jpu_open,
    .read = jpu_read,
    .write = jpu_write,
    .unlocked_ioctl = jpu_ioctl,
    .release = jpu_release,
    .fasync = jpu_fasync,
    .mmap = jpu_mmap,
};

static void set_topaddr(void)
{
#if defined CONFIG_ARCH_BM1684
    if (s_jpu_control_register.virt_addr) {
        *(volatile unsigned int *)(s_jpu_control_register.virt_addr) = (unsigned int) 0x44444444;
    } else {
        printk(KERN_ERR "[JPUDRV] : fail to remap the address of jpu remap reg. reg addr==0x%llx, size=%u\n",
               s_jpu_control_register.phys_addr, s_jpu_control_register.size);
    }
#elif defined(CONFIG_ARCH_BM1682)
    volatile unsigned int* p = (unsigned int *)s_jpu_control_register.virt_addr;
    unsigned int val = *p;
    printk(KERN_INFO "s: vdo_addr_remap=0x%0x\n", ((val>>16) & 0xFF));

#ifdef USE_ION_MEMORY
    /* high 4G */
    *p = (val&(~0x00FF0000)) | 0x00020000;
    printk(KERN_INFO "JPU use high 4G memory\n");
#else
    /* low 4G */
    *p = (val&(~0x00FF0000)) | 0x00010000;
    printk(KERN_INFO "JPU use low 4G memory\n");
#endif
    val = *p;
    printk(KERN_INFO "d: vdo_addr_remap=0x%0x\n", ((val>>16) & 0xFF));
#endif
}

static int jpu_register_reset(struct platform_device *pdev)
{
    const char *reset_name;
    int str_cnt, i;
    int err = 0;
    if (pdev)
    {
        if (of_find_property(pdev->dev.of_node, "reset-names", NULL))
        {
#if defined CONFIG_ARCH_BM1684
            str_cnt = of_property_count_strings(pdev->dev.of_node, "reset-names");
#else
            str_cnt = 1;
#endif
            for(i = 0; i < str_cnt; i++)
            {
                if (s_jpu_core_enable[i] == 0) {
                    continue;
                }
                of_property_read_string_index(pdev->dev.of_node, "reset-names", i, &reset_name);

                jpu_pwm_ctrl.jpu_rst[i] = devm_reset_control_get(&pdev->dev, reset_name);

                if (IS_ERR(jpu_pwm_ctrl.jpu_rst[i])) {
                    err = PTR_ERR(jpu_pwm_ctrl.jpu_rst[i]);
                    dev_err(&pdev->dev, "%s register reset failed\n", reset_name);
                    return err;
                }
              printk(KERN_ERR "retrive %s reset succeed.\n", reset_name);
            }
        }
    }
    else
    {
       return -1;
    }
    return 0;
}

static int jpu_register_clk(struct platform_device *pdev)
{
    if (pdev) {
#ifdef CONFIG_ARCH_BM1880
        int err;
        jpu_pwm_ctrl.axi2_rst = devm_reset_control_get_shared(&pdev->dev, "axi2");
        if (IS_ERR(jpu_pwm_ctrl.axi2_rst)) {
            err = PTR_ERR(jpu_pwm_ctrl.axi2_rst);
            dev_err(&pdev->dev, "failed to retrieve axi2 reset");
            goto err;
        }

        jpu_pwm_ctrl.apb_jpeg_rst = devm_reset_control_get(&pdev->dev, "apb_jpeg");
        if (IS_ERR(jpu_pwm_ctrl.apb_jpeg_rst)) {
            err = PTR_ERR(jpu_pwm_ctrl.apb_jpeg_rst);
            dev_err(&pdev->dev, "failed to retrieve apb_jpeg reset");
            goto err;
        }

        jpu_pwm_ctrl.jpeg_axi_rst = devm_reset_control_get(&pdev->dev, "jpeg_axi");
        if (IS_ERR(jpu_pwm_ctrl.jpeg_axi_rst)) {
            err = PTR_ERR(jpu_pwm_ctrl.jpeg_axi_rst);
            dev_err(&pdev->dev, "failed to retrieve jpeg_axi reset");
            goto err;
        }
#elif defined CONFIG_ARCH_BM1684
        int i,ret;
        const char *clk_name;

        for(i = 0; i < MAX_NUM_JPU_CORE; i++)
        {
            if (s_jpu_core_enable[i] == 0) {
                continue;
            }
            of_property_read_string_index(pdev->dev.of_node, "clock-names", 2*i, &clk_name);

            jpu_pwm_ctrl.jpu_apb_clk[i] = devm_clk_get(&pdev->dev, clk_name);
            if (IS_ERR(jpu_pwm_ctrl.jpu_apb_clk[i])) {
                ret = PTR_ERR(jpu_pwm_ctrl.jpu_apb_clk[i]);
                dev_err(&pdev->dev, "failed to retrieve jpu%d %s clock", i, clk_name);
                return ret;
            }
            printk(KERN_ERR "retrive %s clk succeed.\n", clk_name);
            of_property_read_string_index(pdev->dev.of_node, "clock-names", 2*i+1, &clk_name);

            jpu_pwm_ctrl.jpu_axi_clk[i]  = devm_clk_get(&pdev->dev, clk_name);
            if (IS_ERR(jpu_pwm_ctrl.jpu_axi_clk[i])) {
                ret = PTR_ERR(jpu_pwm_ctrl.jpu_axi_clk[i]);
                dev_err(&pdev->dev, "failed to retrieve jpu%d %s clock", i, clk_name);
                return ret;
            }
            printk(KERN_ERR "retrive %s clk succeed.\n", clk_name);
        }

#endif
    }
    else
    {
        return -1;
    }
    return 0;
}
static int check_jpu_core_busy(jpu_statistic_info_t *jpu_usage_info, int coreIdx)
{
    int ret = 0;

#if defined(CHIP_BM1684)
    if (jpu_usage_info->jpu_open_status[coreIdx] == 1){
        if (atomic_read(&jpu_usage_info->jpu_busy_status[coreIdx]) > 0)
            ret = 1;
    }

#endif
    return ret;
}


static int bm_jpu_check_usage_info(jpu_statistic_info_t *jpu_usage_info)
{
    int ret = 0, i;

    mutex_lock(&s_jpu_proc_lock);
    /* update usage */
    for (i = 0; i < get_max_num_jpu_core(); i++) {
        int busy = check_jpu_core_busy(jpu_usage_info, i);
        int jpu_core_usage = 0;
        int j;
        jpu_usage_info->jpu_status_array[i][jpu_usage_info->jpu_status_index[i]] = busy;
        jpu_usage_info->jpu_status_index[i]++;
        jpu_usage_info->jpu_status_index[i] %= MAX_JPU_STAT_WIN_SIZE;

        if (busy == 1)
            jpu_usage_info->jpu_working_time_in_ms[i] += jpu_usage_info->jpu_instant_interval / MAX_JPU_STAT_WIN_SIZE;
        jpu_usage_info->jpu_total_time_in_ms[i] += jpu_usage_info->jpu_instant_interval / MAX_JPU_STAT_WIN_SIZE;

        for (j = 0; j < MAX_JPU_STAT_WIN_SIZE; j++)
            jpu_core_usage += jpu_usage_info->jpu_status_array[i][j];

        jpu_usage_info->jpu_core_usage[i] = jpu_core_usage;
    }
    mutex_unlock(&s_jpu_proc_lock);

    return ret;
}

int bm_jpu_set_interval(jpu_statistic_info_t *jpu_usage_info, int time_interval)
{
    mutex_lock(&s_jpu_proc_lock);
    jpu_usage_info->jpu_instant_interval = time_interval;
    mutex_unlock(&s_jpu_proc_lock);

    return 0;
}

static int bm_jpu_usage_info_init(jpu_statistic_info_t *jpu_usage_info)
{
    memset(jpu_usage_info, 0, sizeof(jpu_statistic_info_t));

    jpu_usage_info->jpu_instant_interval = 500;

    return bm_jpu_check_usage_info(jpu_usage_info);
}

int bm_jpu_monitor_thread(void *data)
{
    int ret = 0;
    jpu_statistic_info_t *jpu_usage_info = (jpu_statistic_info_t *)data;

    set_current_state(TASK_INTERRUPTIBLE);
    ret = bm_jpu_usage_info_init(jpu_usage_info);
    if (ret)
        return ret;

    while (!kthread_should_stop()) {
        bm_jpu_check_usage_info(jpu_usage_info);
        msleep_interruptible(100);
    }

    return ret;
}


static int jpu_probe(struct platform_device *pdev)
{
    int err = 0, i;
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    struct device_node *target = NULL;
#endif
    struct resource *res = NULL;

    printk(KERN_INFO "jpu_probe\n");
    for(i = 0; i < MAX_NUM_JPU_CORE; i++)
    {
        if (s_jpu_core_enable[i] == 0) {
            continue;
        }

        if( pdev )
            res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (res)    // if platform driver is implemented
        {
            unsigned long size = resource_size(res);
            s_jpu_register[i].phys_addr = res->start;
            s_jpu_register[i].virt_addr = (unsigned long)ioremap_nocache(res->start, size);
            s_jpu_register[i].size = size;
            printk(KERN_INFO "[JPUDRV] : jpu base address get from platform driver base addr=0x%llx, virtualbase=0x%llx, size=%u\n",
                   s_jpu_register[i].phys_addr, s_jpu_register[i].virt_addr, s_jpu_register[i].size);
        }
        else
        {
            s_jpu_register[i].phys_addr = s_jpu_reg_phy_base[i];
            s_jpu_register[i].virt_addr = (unsigned long)ioremap_nocache(s_jpu_register[i].phys_addr, JPU_REG_SIZE);
            s_jpu_register[i].size = JPU_REG_SIZE;
            printk(KERN_INFO "[JPUDRV] : jpu base address get from defined value base addr=0x%llx, virtualbase=0x%llx, size=%u\n",
                   s_jpu_register[i].phys_addr, s_jpu_register[i].virt_addr, s_jpu_register[i].size);
        }
    }

    // get jpu control reg
    if( pdev )
#if defined CONFIG_ARCH_BM1684
        res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
#else
        res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
#endif
    if (res)    // if platform driver is implemented
    {
        unsigned long size = resource_size(res);
        s_jpu_control_register.phys_addr = res->start;
        s_jpu_control_register.virt_addr = (unsigned long)ioremap_nocache(res->start, size);
        s_jpu_control_register.size = size;
        printk(KERN_INFO "[JPUDRV] : jpu control reg address get from platform driver base addr=0x%llx, virtualbase=0x%llx, size=%u\n",
               s_jpu_control_register.phys_addr, s_jpu_control_register.virt_addr, s_jpu_control_register.size);
    }
    else
    {
        //TODO get res from kernel
        s_jpu_control_register.phys_addr = JPU_CONTROL_REG_ADDR;
        s_jpu_control_register.virt_addr = (unsigned long)ioremap_nocache(s_jpu_control_register.phys_addr, JPU_CONTROL_REG_SIZE);
        s_jpu_control_register.size = JPU_CONTROL_REG_SIZE;
        printk(KERN_INFO "[JPUDRV] : jpu control reg address get from defined base addr=0x%llx, virtualbase=0x%llx, size=%u\n",
               s_jpu_control_register.phys_addr, s_jpu_control_register.virt_addr, s_jpu_control_register.size);
    }
    set_topaddr();

    err = bm_jpu_register_cdev(pdev);
    if (err < 0)
    {
        printk(KERN_ERR "jpu_register_cdev\n");
        goto ERROR_PROVE_DEVICE;
    }

#ifdef JPU_SUPPORT_ISR
    for(i = 0; i < MAX_NUM_JPU_CORE; i++)
    {
        if (s_jpu_core_enable[i] == 0) {
            continue;
        }

        if(pdev)
            res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
        if (res)    // if platform driver is implemented
        {
            s_jpu_irq[i] = res->start;
            printk(KERN_INFO "[JPUDRV] : jpu irq number core %d get from platform driver irq=0x%x\n",i, s_jpu_irq[i] );
        }
        else
        {
            printk(KERN_INFO "[JPUDRV] : jpu irq number core %d get from defined value irq=0x%x\n",i, s_jpu_irq[i] );
        }
    }

    for(i = 0; i < MAX_NUM_JPU_CORE; i++)
    {
        if (s_jpu_core_enable[i] == 0) {
            continue;
        }

        err = request_irq(s_jpu_irq[i], jpu_irq_handler, 0, "JPU_CODEC_IRQ", (void *)(&s_jpu_drv_context));
        if (err)
        {
            printk(KERN_ERR "[JPUDRV] :  fail to register interrupt handler. core:%d\n",i);
            goto ERROR_PROVE_DEVICE;
        }
    }
#endif


#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
#ifdef CONFIG_ARCH_BM1880
    if (jpu_allocate_memory(pdev) < 0)
    {
        printk(KERN_ERR "[JPUDRV] :  fail to remap jpu memory\n");
        goto ERROR_PROVE_DEVICE;
    }
#else
    /* Get reserved memory region from Device-tree */
    target = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
    if (!target)
    {
        s_video_memory.size = JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
        s_video_memory.phys_addr = JPU_DRAM_PHYSICAL_BASE;
        s_video_memory.base = (unsigned long)devm_ioremap_nocache(&pdev->dev, s_video_memory.phys_addr, PAGE_ALIGN(s_video_memory.size));
        if (!s_video_memory.base)
        {
            printk(KERN_ERR "[JPUDRV] :  fail to remap video memory physical phys_addr=0x%lx, base=0x%lx, size=%d\n",
                   s_video_memory.phys_addr, s_video_memory.base, (int)s_video_memory.size);
            goto ERROR_PROVE_DEVICE;
        }
    }
    else
    {
        struct resource rmem;
        err = of_address_to_resource(target, 0, &rmem);
        if (err)
        {
            printk(KERN_INFO "[JPUDRV] No memory address assigned to the region\n");
            goto ERROR_PROVE_DEVICE;
        }

        s_video_memory.phys_addr = rmem.start;
        printk(" s_video_memory addr = %lx \n", s_video_memory.phys_addr);
        s_video_memory.size = resource_size(&rmem);
#ifdef ASIC_BOX
        if (s_video_memory.size > 32*1024*1024)
            s_video_memory.size = 32*1024*1024;
#endif
        res = devm_request_mem_region(&pdev->dev, s_video_memory.phys_addr, s_video_memory.size, "jpu");
        if(!res)
        {
            printk(KERN_INFO "[JPUDRV] Request_mem_region failed!\n");
            goto ERROR_PROVE_DEVICE;
        }

        s_video_memory.base = (unsigned long)devm_ioremap_nocache(&pdev->dev, s_video_memory.phys_addr, s_video_memory.size);
        // s_video_memory.base = (unsigned long)memremap(rmem.start, resource_size(&rmem), MEMREMAP_WC);
        // s_video_memory.base = (unsigned long)ioremap_nocache(rmem.start, resource_size(&rmem));
        // s_video_memory.base = devm_ioremap_wc(&pdev->dev, s_video_memory.phys_addr, s_video_memory.size);
        if(!s_video_memory.base)
        {
            printk("[JPUDRV] ioremap fail!\n");
            goto ERROR_PROVE_DEVICE;
        }
    }
#endif

    if (jmem_init(&s_jmem, s_video_memory.phys_addr, s_video_memory.size) < 0)
    {
        printk(KERN_ERR "[JPUDRV] :  fail to init vmem system\n");
        goto ERROR_PROVE_DEVICE;
    }

    printk(KERN_INFO "[JPUDRV] jpu reserved video memory phys_addr=0x%lx, base = 0x%lx, size = 0x%x\n",
           s_video_memory.phys_addr, s_video_memory.base, s_video_memory.size);
#else
    printk(KERN_INFO "[JPUDRV] success to probe jpu device with non reserved video memory\n");
#endif

    if(jpu_register_reset(pdev) < 0)
    {
        goto ERROR_PROVE_DEVICE;
    }
    if(jpu_register_clk(pdev) < 0)
    {
        goto ERROR_PROVE_DEVICE;
    }

    jpu_init_bm1684x_bin();

#if defined CONFIG_ARCH_BM1684
    for(i = 0; i < get_max_num_jpu_core(); i++)
    {
        jpu_clk_enable(jpu_pwm_ctrl.jpu_axi_clk[i]);
        jpu_clk_enable(jpu_pwm_ctrl.jpu_apb_clk[i]);
        jpu_clk_disable(jpu_pwm_ctrl.jpu_axi_clk[i]);
        jpu_clk_disable(jpu_pwm_ctrl.jpu_apb_clk[i]);
    }
#endif

    /* launch jpu monitor thread */
    if (s_jpu_monitor_task == NULL){
        s_jpu_monitor_task = kthread_run(bm_jpu_monitor_thread, &s_jpu_usage_info, "jpu_monitor");
        if (s_jpu_monitor_task == NULL){
            pr_info("create jpu monitor thread failed\n");
            goto ERROR_PROVE_DEVICE;
        } else
            pr_info("create jpu monitor thread done\n");
    }


    return 0;

ERROR_PROVE_DEVICE:

    if (s_jpu_major)
        unregister_chrdev_region(s_jpu_major, 1);

    for(i=0; i< get_max_num_jpu_core(); i++)
    {
        if (s_jpu_register[i].virt_addr) {
            iounmap((void *)s_jpu_register[i].virt_addr);
            s_jpu_register[i].virt_addr = 0x00;
        }
    }

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base)
    {
        jmem_exit(&s_jmem);
    }
#endif

#if defined CONFIG_ARCH_BM1684
    if (s_jpu_remap_register.virt_addr) {
        iounmap((void *)s_jpu_remap_register.virt_addr);
        s_jpu_remap_register.virt_addr = 0x00;
    }
#endif

    return err;
}

int bm_jpu_register_cdev(struct platform_device *pdev)
{
    int err = 0;

    jpu_class = class_create(THIS_MODULE, JPU_CLASS_NAME);
    if (IS_ERR(jpu_class))
    {
        printk(KERN_ERR "create class failed\n");
        return PTR_ERR(jpu_class);
    }

    /* get the major number of the character device */
    if ((alloc_chrdev_region(&s_jpu_major, 0, 1, JPU_DEV_NAME)) < 0)
    {
        err = -EBUSY;
        printk(KERN_ERR "could not allocate major number\n");
        return err;
    }

    /* initialize the device structure and register the device with the kernel */
    cdev_init(&s_jpu_cdev, &jpu_fops);
    s_jpu_cdev.owner = THIS_MODULE;

    if ((cdev_add(&s_jpu_cdev, s_jpu_major, 1)) < 0)
    {
        err = -EBUSY;
        printk(KERN_ERR "could not allocate chrdev\n");
        return err;
    }

    device_create(jpu_class, &pdev->dev, s_jpu_major, NULL, "%s", JPU_DEV_NAME);

    return err;
}

#ifdef CONFIG_ARCH_BM1880
static int jpu_allocate_memory(struct platform_device *pdev)
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
            printk(KERN_ERR "[JPUDRV]: cannot acquire memory-region\n");
            return -1;
        }
    }
    else {
        printk(KERN_ERR "[JPUDRV]: cannot find the node, memory-region\n");
        return -1;
    }

    printk(KERN_ERR "[JPUDRV]: pool name = %s, size = 0x%x, base = 0x%lx\n",
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
        printk(KERN_ERR "[JPUDRV] ioremap fail!\n");
        printk(KERN_ERR "s_video_memory.base = 0x%lx\n", s_video_memory.base);
        return -1;
    }

    printk(KERN_INFO "[JPUDRV] success to probe vpu\n");
    printk(KERN_INFO "reserved memory phys_addr = 0x%lx, base = 0x%lx, size = 0x%x\n",
           s_video_memory.phys_addr,
           s_video_memory.base,
           s_video_memory.size);

    return 0;
}

static void bm_jpu_assert(jpu_power_ctrl *pRstCtrl)
{
    reset_control_assert(pRstCtrl->axi2_rst);
    reset_control_assert(pRstCtrl->apb_jpeg_rst);
    reset_control_assert(pRstCtrl->jpeg_axi_rst);
}

static void bm_jpu_deassert(jpu_power_ctrl *pRstCtrl)
{
    reset_control_deassert(pRstCtrl->axi2_rst);
    reset_control_deassert(pRstCtrl->apb_jpeg_rst);
    reset_control_deassert(pRstCtrl->jpeg_axi_rst);
}
#endif

static int jpu_remove(struct platform_device *pdev)
{
    int i;
#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    printk(KERN_INFO "jpu_remove\n");

    if (s_jpu_instance_pool.base)
    {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
        vfree((const void *)s_jpu_instance_pool.base);
#else
        jpu_free_dma_buffer(&s_jpu_instance_pool);
#endif
        s_jpu_instance_pool.base = 0;
    }
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base)
    {
        // devm_unioremap((void *)s_video_memory.base);
        // s_video_memory.base = 0;
        jmem_exit(&s_jmem);
    }
#endif

    bm_jpu_unregister_cdev();

#ifdef JPU_SUPPORT_ISR
    for(i = 0; i < get_max_num_jpu_core(); i++)
    {
        if (s_jpu_irq[i])
            free_irq(s_jpu_irq[i], &s_jpu_drv_context);
    }
#endif
    for(i=0; i< get_max_num_jpu_core(); i++)
    {
        if (s_jpu_register[i].virt_addr) {
            iounmap((void *)s_jpu_register[i].virt_addr);
            s_jpu_register[i].virt_addr = 0x00;
        }
    }

    if (s_jpu_control_register.virt_addr) {
        iounmap((void *)s_jpu_control_register.virt_addr);
        s_jpu_control_register.virt_addr = 0x00;
    }

    /* stop jpu monitor thread */
    if (s_jpu_monitor_task != NULL){
        kthread_stop(s_jpu_monitor_task);
        s_jpu_monitor_task = NULL;
        pr_info("jpu monitor thread released\n");
    }


//    jpu_clk_put(jpu_pwm_ctrl.jpu_clk);

#endif //JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    return 0;
}

static void bm_jpu_unregister_cdev(void)
{
    cdev_del(&s_jpu_cdev);
    if (s_jpu_major > 0)
    {
        device_destroy(jpu_class, s_jpu_major);
        class_destroy(jpu_class);

        unregister_chrdev_region(s_jpu_major, 1);
        s_jpu_major = 0;
    }
}

#ifdef CONFIG_PM
static int jpu_suspend(struct platform_device *pdev, pm_message_t state)
{
    jpu_clk_disable(jpu_pwm_ctrl.jpu_clk);
    return 0;

}

static int jpu_resume(struct platform_device *pdev)
{
    jpu_clk_enable(jpu_pwm_ctrl.jpu_clk);
    return 0;
}
#else
#define jpu_suspend NULL
#define jpu_resume  NULL
#endif              /* !CONFIG_PM */

#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
static const struct of_device_id bm_jpu_match_table[] = {
    {.compatible = "bitmain,jpeg"},
    {},
};
MODULE_DEVICE_TABLE(of, bm_jpu_match_table);

#endif /* JPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

static struct platform_driver jpu_driver = {
    .driver   = {
        .name = JPU_PLATFORM_DEVICE_NAME,
#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
        .of_match_table = bm_jpu_match_table,
#endif /* JPU_SUPPORT_PLATFORM_DRIVER_REGISTER */
    },
    .probe    = jpu_probe,
    .remove   = jpu_remove,
    .suspend  = jpu_suspend,
    .resume   = jpu_resume,
};

static ssize_t info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    char dat[512] = { 0 };
    int len = 0, i = 0;
    int err = 0;
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    jmem_info_t jinfo;
    jmem_get_info(&s_jmem, &jinfo);

    size = jinfo.free_pages * (unsigned long)jinfo.page_size;
    sprintf(dat, "\n\"total_mem_size\" : %lu, \"used_mem_size\" : %lu, \"free_mem_size\" : %lu,\n",
            jinfo.total_pages * (unsigned long)jinfo.page_size, jinfo.alloc_pages * (unsigned long)jinfo.page_size, size);
#endif

#ifdef CONFIG_ARCH_BM1684
    if (s_chip_id == CHIP_ID_1684) {
        sprintf(dat+ strlen(dat), "JPU loadbalance:\nJPU0 = %d\nJPU1 = %d\nJPU2 = %d\nJPU3 = %d\n",s_core[0],s_core[1],s_core[2],s_core[3]);
    } else if (s_chip_id == CHIP_ID_1684x) {
        sprintf(dat+ strlen(dat), "JPU loadbalance:\nJPU0 = %d\nJPU1 = %d\n",s_core[0],s_core[1]);
    }
#else
    sprintf(dat+ strlen(dat), "JPU loadbalance:\nJPU0 = %d\n\n",s_core[0]);
#endif

    len = strlen(dat);
    sprintf(dat + len, "\"core\" : [\n");
    for (i = 0; i < get_max_num_jpu_core() - 1; i++) {
        len = strlen(dat);
        sprintf(dat + len, "{\"id\":%d, \"open_status\":%d, \"usage(short|long)\":%d%%|%llu%%}, ", i, s_jpu_usage_info.jpu_open_status[i], \
                s_jpu_usage_info.jpu_core_usage[i], s_jpu_usage_info.jpu_working_time_in_ms[i]*100/s_jpu_usage_info.jpu_total_time_in_ms[i]);
    }
    len = strlen(dat);
    sprintf(dat + len, "{\"id\":%d, \"open_status\":%d, \"usage(short|long)\":%d%%|%llu%%}]\n", i, s_jpu_usage_info.jpu_open_status[i], \
            s_jpu_usage_info.jpu_core_usage[i], s_jpu_usage_info.jpu_working_time_in_ms[i]*100/s_jpu_usage_info.jpu_total_time_in_ms[i]);

    len = strlen(dat);
    sprintf(dat + len, "\n");
    len = strlen(dat) + 1;
    if (size < len) {
        printk("read buf too small\n");
        return -EIO;
    }
    memset(s_core, 0, sizeof(s_core));
    if (*ppos >= len) return 0;

    err = copy_to_user(buf, dat, len);
    if (err) return 0;

    *ppos = len;

    return len;
}
static ssize_t info_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char data[256] = { 0 };
    int err = 0;
    int frame_num = -1;

    if (size > DUMP_FLAG_MEM_SIZE)
        return 0;

    err = copy_from_user(data, buf, size);
    if (err)
        return 0;
    sscanf(data, "%d", &frame_num);
    if (s_jpu_dump_flag) {
        if(frame_num <=0  || frame_num > 10)
        {
            memset(s_jpu_dump_flag, 0, DUMP_FLAG_MEM_SIZE );
            pr_info("       example: echo > dump_frame_num  /proc/jpuinfo\n");
            pr_info("dump_frame_num: 1 <= dump_frame_num <= 10\n");
            pr_info("       example: echo >  5  /proc/jpuinfo\n");
        }
        else
        {
            pr_info("dump_frame_num: %d \n", frame_num);
            s_jpu_dump_flag[0] = frame_num;
        }
    }
    return size;
}
static const struct file_operations proc_info_operations = {
    .read   = info_read,
    .write  = info_write,
};

static int jpu_init_get_flags(void) {

    if (s_chip_id == CHIP_ID_1684) {

        if (s_jpu_cap == 0) {
            s_jpu_core_enable[0] = 1;
            s_jpu_core_enable[1] = 1;
            s_jpu_core_enable[2] = 1;
            s_jpu_core_enable[3] = 1;
            sema_init(&s_jpu_core, MAX_NUM_JPU_CORE);
        } else if (s_jpu_cap == 1) {
            s_jpu_core_enable[0] = 0;
            s_jpu_core_enable[1] = 0;
            s_jpu_core_enable[2] = 1;
            s_jpu_core_enable[3] = 1;
            sema_init(&s_jpu_core, MAX_NUM_JPU_CORE_2);
        } else if (s_jpu_cap == 2) {
            s_jpu_core_enable[0] = 1;
            s_jpu_core_enable[1] = 1;
            s_jpu_core_enable[2] = 0;
            s_jpu_core_enable[3] = 0;
            sema_init(&s_jpu_core, MAX_NUM_JPU_CORE_2);
        } else if (s_jpu_cap == 3) {
            s_jpu_core_enable[0] = 0;
            s_jpu_core_enable[1] = 0;
            s_jpu_core_enable[2] = 0;
            s_jpu_core_enable[3] = 0;
            sema_init(&s_jpu_core, 0);
        }

    } else if (s_chip_id == CHIP_ID_1684x) {
        if(s_jpu_cap == 0) {
            s_jpu_core_enable[0] = 1;
            s_jpu_core_enable[1] = 0;
            s_jpu_core_enable[2] = 1;
            s_jpu_core_enable[3] = 0;
            sema_init(&s_jpu_core, MAX_NUM_JPU_CORE_2);
        }else if(s_jpu_cap == 1) {
            s_jpu_core_enable[0] = 0;
            s_jpu_core_enable[1] = 0;
            s_jpu_core_enable[2] = 1;
            s_jpu_core_enable[3] = 0;
            sema_init(&s_jpu_core, MAX_NUM_JPU_CORE_1);
        }else if(s_jpu_cap == 2){
            s_jpu_core_enable[0] = 1;
            s_jpu_core_enable[1] = 0;
            s_jpu_core_enable[2] = 0;
            s_jpu_core_enable[3] = 0;
            sema_init(&s_jpu_core, MAX_NUM_JPU_CORE_1);
        }else if(s_jpu_cap == 3){
            s_jpu_core_enable[0] = 0;
            s_jpu_core_enable[1] = 0;
            s_jpu_core_enable[2] = 0;
            s_jpu_core_enable[3] = 0;
            sema_init(&s_jpu_core, 0);
        }
    }
    return 0;
}

static int __init jpu_init(void)
{
    int res,i;
    res = 0;
    printk(KERN_INFO "jpu_init\n");

#ifdef CONFIG_ARCH_BM1684
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
    uint32_t chip_id = sophon_get_chip_id();
    s_chip_id = (chip_id & 0xffff0000) >> 16;
#else
    s_chip_id = CHIP_ID_1684;
#endif
#else
    s_chip_id = 0x1682;
#endif

    if (s_chip_id == CHIP_ID_1684) {
        efuse_ft_get_video_cap(&s_jpu_cap);
    } else {
        s_jpu_cap = 0;
    }
    printk(KERN_INFO "jpu_init efuse_ft_get_video_cap  s_jpu_cap=%d\n", s_jpu_cap);
    jpu_init_get_flags();

#ifdef MUTEX_LOCK
    mutex_init(&s_jpu_lock);
    mutex_init(&s_buffer_lock);
#endif

    for(i = 0 ; i < get_max_num_jpu_core(); i++)
        init_waitqueue_head(&s_interrupt_wait_q[i]);

    s_jpu_instance_pool.base = 0;


    for(i = 0 ; i < get_max_num_jpu_core(); i++)
    {
        jpudrv_instance_list_t *jil;

        jil = kzalloc(sizeof(*jil), GFP_KERNEL);
        if (!jil)
            return -ENOMEM;

        jil->inst_idx = i;
        jil->inuse = 0;
        jil->filp = NULL;

        LOCK(s_jpu_lock);
        list_add_tail(&jil->list, &s_jpu_inst_list_head);
        UNLOCK(s_jpu_lock);

        printk(KERN_INFO "jpu_init [JPUDRV]  i = %d\n",i);
    }

#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    res = platform_driver_register(&jpu_driver);
#else
    res = platform_driver_register(&jpu_driver);
    res = jpu_probe(NULL);
#endif

    s_jpu_dump_flag = (u32 *)vmalloc(DUMP_FLAG_MEM_SIZE);

    if(s_jpu_dump_flag != NULL)
        memset(s_jpu_dump_flag, 0, DUMP_FLAG_MEM_SIZE);

    entry = proc_create("jpuinfo", 0, NULL, &proc_info_operations);

    printk(KERN_INFO "end jpu_init result=0x%x\n", res);
    return res;
}

static void __exit jpu_exit(void)
{
#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    jpudrv_instance_list_t *jil, *n;
    printk(KERN_INFO "jpu_exit\n");
    list_for_each_entry_safe(jil, n, &s_jpu_inst_list_head, list)
    {
        printk(KERN_INFO "jpu_exit [JPUDRV] jil->inst_idx=%d ,jil->inuse=%d, jil-> filp=%p \n",
               (int)jil->inst_idx, (int)jil->inuse ,(void*) jil-> filp);
        list_del(&jil->list);
        kfree(jil);
    }
#else
    if (s_jpu_instance_pool.base)
    {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
        vfree((const void *)s_jpu_instance_pool.base);
#else
        jpu_free_dma_buffer(&s_jpu_instance_pool);
#endif
        s_jpu_instance_pool.base = 0;
    }

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base)
    {
        iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
    }
#endif

    bm_jpu_unregister_cdev();

#ifdef JPU_SUPPORT_ISR
    for(i = 0; i < get_max_num_jpu_core(); i++)
    {
        if (s_jpu_irq[i])
            free_irq(s_jpu_irq[i], &s_jpu_drv_context);
    }
#endif

#if 0
    for(i = 0 ; i < get_max_num_jpu_core(); i++)
    {
        jpu_clk_put(jpu_pwm_ctrl.jpu_apb_clk[i]);
        jpu_clk_put(jpu_pwm_ctrl.jpu_axi_clk[i]);
    }
#endif

    if (s_jpu_register[0].virt_addr) {
        iounmap((void *)s_jpu_register[0].virt_addr);
        s_jpu_register[0].virt_addr = 0x00;
    }

    if (s_jpu_control_register.virt_addr) {
        iounmap((void *)s_jpu_control_register.virt_addr);
        s_jpu_control_register.virt_addr = 0x00;
    }

    if (s_jpu_remap_register.virt_addr) {
        iounmap((void *)s_jpu_remap_register.virt_addr);
        s_jpu_remap_register.virt_addr = 0x00;
    }
#endif //JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    platform_driver_unregister(&jpu_driver);

    if (entry) {
        proc_remove(entry);
        entry = NULL;
    }

    if(s_jpu_dump_flag != NULL)
        vfree(s_jpu_dump_flag);

    return;
}

MODULE_AUTHOR("Bitmain");
MODULE_DESCRIPTION("JPU linux driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);

module_init(jpu_init);
module_exit(jpu_exit);

int jpu_hw_reset(u32 core_idx)
{
    if( core_idx >= get_max_num_jpu_core() || core_idx < 0)
    {
        printk(KERN_ERR "[JPUDRV] jpu core_idx(%d) range is not normal.\n",core_idx);
        return -1;
    }

    DPRINTK("[JPUDRV] request jpu reset from application. \n");

    if(!jpu_pwm_ctrl.jpu_rst[core_idx]) {
        printk(KERN_ERR "[JPUDRV] jpu reset is NULL.\n");
        return -1;
    }

    reset_control_assert(jpu_pwm_ctrl.jpu_rst[core_idx]);
    udelay(1);   // 1 microsecond delay for reset
    reset_control_deassert(jpu_pwm_ctrl.jpu_rst[core_idx]);
    return 0;
}

#if 0
struct clk *jpu_clk_get(struct device *dev)
{
    return clk_get(dev, JPU_CLK_NAME);
}

void jpu_clk_put(struct device *dev，　struct clk *clk)
{
#ifdef CONFIG_ARCH_BM1684
        if (!(clk == NULL || IS_ERR(clk)))
        {
            devm_clk_put(dev, clk);  //clk_put(clk);
        }
#else
#endif
}
#endif

int jpu_clk_enable(struct clk *clk)
{
    if (!(clk == NULL || IS_ERR(clk))) {
        //printk(KERN_INFO "jpu_clk_enable\n");
        return clk_prepare_enable(clk);
    }
    return 0;
}
void jpu_clk_disable(struct clk *clk)
{
    if (!(clk == NULL || IS_ERR(clk))) {
        //printk(KERN_INFO "jpu_clk_disable\n");
        clk_disable_unprepare(clk);
    }
}

