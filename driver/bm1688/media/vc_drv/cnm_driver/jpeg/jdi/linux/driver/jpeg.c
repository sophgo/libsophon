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
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/of_reserved_mem.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
#include <linux/sched/signal.h>
#endif


#include "../../../jpuapi/jpuconfig.h"
#include "jpu.h"
#include "jpulog.h"
#include "ion.h"

//#define ENABLE_DEBUG_MSG
#ifdef ENABLE_DEBUG_MSG
#define DPRINTK(args...)           printk(KERN_INFO args)
#else
#define DPRINTK(args...)
#endif

static struct file *g_filp;


/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
//#define JPU_SUPPORT_CLOCK_CONTROL
// #define JPU_SUPPORT_ISR
/* if the platform driver knows the name of this driver */
/* JPU_PLATFORM_DEVICE_NAME */
#define JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

/* if this driver knows the dedicated video memory address */
//#define JPU_SUPPORT_RESERVED_VIDEO_MEMORY        //if this driver knows the dedicated video memory address

#define JPU_PLATFORM_DEVICE_NAME    "sophgo,jpu"
#define JPU_CLK_NAME                "jpege"
#define JPU_DEV_NAME                "jpu"

#define JPU_REG_BASE_ADDR           0x75300000
#define JPU_REG_SIZE                0x10000


#define JPEG_TOP_REG                0x21000010
#define JPEG_TOP_RESET_REG          0x28103000


#ifdef JPU_SUPPORT_ISR
#define JPU_IRQ_NUM                 (15+32)
#endif
/* if the driver want to disable and enable IRQ whenever interrupt asserted. */
#define JPU_IRQ_CONTROL


#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define JPU_SUPPORT_ION_MEMORY

typedef struct jpu_drv_context_t {
    struct fasync_struct *async_queue;
    u32 open_count;                     /*!<< device reference count. Not instance count */
    u32 interrupt_reason[MAX_NUM_JPU_CORE][MAX_JPEG_NUM_INSTANCE];
} jpu_drv_context_t;


/* To track the allocated memory buffer */
typedef struct jpudrv_buffer_pool_t {
    struct list_head        list;
    struct jpudrv_buffer_t  jb;
    struct file*            filp;
} jpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct jpudrv_instance_list_t {
    struct list_head    list;
    unsigned long       inst_idx;
    struct file*        filp;
} jpudrv_instance_list_t;


typedef struct jpudrv_instance_pool_t {
    unsigned char codecInstPool[MAX_JPEG_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
} jpudrv_instance_pool_t;


//#define JPU_SUPPORT_RESERVED_VIDEO_MEMORY

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
//	#define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE  (16*1024*1024)
//	#define JPU_DRAM_PHYSICAL_BASE              (0x8AA00000)
#define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE  (4*1024*1024*1024)
#define JPU_DRAM_PHYSICAL_BASE              (0x0)
#include "jmm.h"
static jpu_mm_t         s_jmem;
static jpudrv_buffer_t  s_video_memory = {0};
#endif /* JPU_SUPPORT_RESERVED_VIDEO_MEMORY */




static int jpu_hw_reset(int idx);
#ifdef JPU_SUPPORT_CLOCK_CONTROL
struct clk *jpu_clk_get(struct device *dev);
void jpu_clk_disable(int core_idx);
void jpu_clk_enable(int core_idx);
#endif
// end customer definition

static jpudrv_buffer_t s_instance_pool = {0};
static jpu_drv_context_t s_jpu_drv_context;

static int s_jpu_open_ref_count;
#if 1//def JPU_SUPPORT_ISR
static int s_jpu_irq[MAX_NUM_JPU_CORE] = {46, 47, 48, 49};
int jpu_core_irq_count[MAX_NUM_JPU_CORE] = {0};
int irq_status[MAX_NUM_JPU_CORE] = {0};
#endif

struct class *jpu_class;
unsigned long virt_top_addr = 0;


static jpudrv_buffer_t s_jpu_register[MAX_NUM_JPU_CORE] = {0};

typedef struct _jpu_power_ctrl_ {
    struct clk *jpu_sys_clk[4];
    struct clk *jpu_apb_clk[4];
    struct clk *jpu_src;
} jpu_power_ctrl;

static const char *const jpu_clk_name[9] = {
            "jpeg_vesys0", "jpeg_vesys1", "jpeg_vesys2", "jpeg_vesys3",
            "jpeg_apb0", "jpeg_apb1", "jpeg_apb2", "jpeg_apb3",
            "jpeg_src",
};
static int s_jpu_reg_phy_base[MAX_NUM_JPU_CORE] = {0x21020000, 0x21030000, 0x21040000, 0x21050000};

static jpu_power_ctrl jpu_pwm_ctrl = {0};

static int s_interrupt_flag[MAX_NUM_JPU_CORE*MAX_JPEG_NUM_INSTANCE];
static wait_queue_head_t s_interrupt_wait_q[MAX_NUM_JPU_CORE*MAX_JPEG_NUM_INSTANCE];


// static spinlock_t s_jpu_lock = __SPIN_LOCK_UNLOCKED(s_jpu_lock);
static DEFINE_MUTEX(s_jpu_lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(s_jpu_sem);
#else
static DEFINE_SEMAPHORE(s_jpu_sem);
#endif

static DEFINE_MUTEX(jpu_mutex);
static struct list_head s_jbp_head = LIST_HEAD_INIT(s_jbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(s_inst_list_head);

#define NPT_BASE                                0x0000
#define NPT_REG_SIZE                            0x300
#define MJPEG_PIC_STATUS_REG(_inst_no)          (NPT_BASE + (_inst_no*NPT_REG_SIZE) + 0x004)

#define ReadJpuRegister(core,addr)           vc_read_reg(s_jpu_register[core].phys_addr + addr)
#define WriteJpuRegister(core,addr, val)     vc_write_reg((s_jpu_register[core].phys_addr + addr), val)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
#define IOREMAP(addr, size) ioremap(addr, size)
#else
#define IOREMAP(addr, size) ioremap_nocache(addr, size)
#endif

extern int jpu_core_init_resources(unsigned int core_num);
extern void jpu_core_cleanup_resources(void);
extern unsigned int vc_read_reg(unsigned int addr);
extern unsigned int vc_write_reg(unsigned int addr, unsigned int data);
extern void drv_vc_enable_irq(int irq_num);
extern void drv_vc_disable_irq(int irq_num);
int jpu_enable_irq(int coreidx);

uint32_t jpu_get_extension_address(int core_idx)
{
    uint32_t origin_value = 0;
    int shift = 0;

    shift = (core_idx + 1) * 4;
    origin_value = vc_read_reg(JPEG_TOP_REG);
    return ((origin_value >> shift) & 0xf);
}

void jpu_set_extension_address(int core_idx, uint32_t addr)
{
    uint32_t origin_value = 0;
    uint32_t bit_mask = 0;
    int shift = 0;

    DPRINTK("[JPUDRV] jpu_set_extension_address: core_idx=%d, addr=0x%lx\n", core_idx, addr);

    switch (core_idx) {
        case 0:  // jpu core 0
            bit_mask = 0xffffff0f;
            shift = 4;
            break;
        case 1:  // jpu core 1
            bit_mask = 0xfffff0ff;
            shift = 8;
            break;
        case 2:  // jpu core 2
            bit_mask = 0xffff0fff;
            shift = 12;
            break;
        case 3:  // jpu core 3
            bit_mask = 0xfff0ffff;
            shift = 16;
            break;
        default:
            JLOG(ERR, "[JPUDRV] jpu_set_extension_address failed, invalid core index: %d\n", core_idx);
            return;
    }

    origin_value = vc_read_reg(JPEG_TOP_REG);
    vc_write_reg(JPEG_TOP_REG, (origin_value & bit_mask) | ((addr & 0xf) << shift));
    return;
}

void jpu_sw_top_reset(int core_idx)
{
    uint32_t origin_value = 0;
    uint32_t bit_mask = 0;

    switch (core_idx) {
        case 0:  // jpu core 0
            bit_mask = 0xffffffff & (~(1 << 18));
            break;
        case 1:  // jpu core 1
            bit_mask = 0xffffffff & (~(1 << 19));
            break;
        case 2:  // jpu core 2
            bit_mask = 0xffffffff & (~(1 << 20));
            break;
        case 3:  // jpu core 3
            bit_mask = 0xffffffff & (~(1 << 21));
            break;
        default:
            JLOG(ERR, "[JPUDRV] jpu_top_reset failed, invalid core index: %d\n", core_idx);
            return;
    }
    DPRINTK("[JPUDRV] jpu_top_reset: bit_mask = 0x%lx\n", bit_mask);

    mutex_lock(&s_jpu_lock);
    origin_value = vc_read_reg(JPEG_TOP_RESET_REG);
    DPRINTK("[JPUDRV] jpu_top_reset: origin_value = 0x%lx\n", origin_value);
    vc_write_reg(JPEG_TOP_RESET_REG, origin_value & bit_mask);
    udelay(1);
    vc_write_reg(JPEG_TOP_RESET_REG, origin_value);
    mutex_unlock(&s_jpu_lock);

    return;
}

static int jpu_alloc_dma_buffer(jpudrv_buffer_t *jb)
{
    if (!jb)
        return -1;
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    jb->phys_addr = (unsigned long long)jmem_alloc(&s_jmem, jb->size, 0);
    if ((unsigned long)jb->phys_addr  == (unsigned long)-1) {
        JLOG(ERR, "[JPUDRV] Physical memory allocation error size=%lu\n", jb->size);
        return -1;
    }

    jb->base = (unsigned long)(s_video_memory.base + (jb->phys_addr - s_video_memory.phys_addr));
    jb->virt_addr = jb->base;
#elif defined(JPU_SUPPORT_ION_MEMORY)
    if (base_ion_alloc((uint64_t *)&jb->phys_addr, (void **)&jb->virt_addr, "jpeg_ion", jb->size, jb->is_cached) != 0) {
        JLOG(ERR, "[JPUDRV] Physical memory allocation error size=%lu\n", jb->size);
        return -1;
    }
    jb->base = jb->virt_addr;

#else
    jb->base = (unsigned long)dma_alloc_coherent(jpu_dev, PAGE_ALIGN(jb->size), (dma_addr_t *) (&jb->phys_addr), GFP_DMA | GFP_KERNEL);
    if ((void *)(jb->base) == NULL) {
        JLOG(ERR, "[JPUDRV] Physical memory allocation error size=%lu\n", jb->size);
        return -1;
    }
    jb->virt_addr = jb->base;

    // pr_info("mark jpu alloc phys:0x%lx, virt:0x%lx, size:0x%x \n", jb->phys_addr, jb->virt_addr, jb->size);
#endif /* JPU_SUPPORT_RESERVED_VIDEO_MEMORY */
    return 0;
}

static void jpu_free_dma_buffer(jpudrv_buffer_t *jb)
{
    if (!jb) {
        return;
    }
    if (jb->base)
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
        jmem_free(&s_jmem, jb->phys_addr, 0);
#elif defined(JPU_SUPPORT_ION_MEMORY)
        base_ion_free(jb->phys_addr);
#else
        dma_free_coherent(jpu_dev, PAGE_ALIGN(jb->size), (void *)jb->base, jb->phys_addr);
#endif /* JPUR_SUPPORT_RESERVED_VIDEO_MEMORY */
}

int jpu_invalidate_cache(jpudrv_buffer_t *jb)
{
#ifdef JPU_SUPPORT_ION_MEMORY
    if (base_ion_cache_invalidate((uint64_t)jb->phys_addr, (void *)jb->virt_addr, jb->size) != 0) {
        JLOG(ERR, "[JPUDRV] invalidate cache failed, paddr: %lx, vaddr: %lx, size=%lu\n", jb->phys_addr, jb->virt_addr, jb->size);
        return -1;
    }
#endif
    return 0;
}

int jpu_flush_cache(jpudrv_buffer_t *jb)
{
#ifdef JPU_SUPPORT_ION_MEMORY
    if (base_ion_cache_flush((uint64_t)jb->phys_addr, (void *)jb->virt_addr, jb->size) != 0) {
        JLOG(ERR, "[JPUDRV] flush cache failed, paddr: %lx, vaddr: %lx, size=%lu\n", jb->phys_addr, jb->virt_addr, jb->size);
        return -1;
    }
#endif
    return 0;
}

int get_max_num_jpu_core(void) {

    return MAX_NUM_JPU_CORE;
}

irqreturn_t jpu_irq_handler(int core, void *dev_id)
{
    jpu_drv_context_t*  dev = (jpu_drv_context_t *)dev_id;
    int i;
    u32 flag;

    DPRINTK("[JPUDRV][+]%s, irq:%d\n", __func__, irq);

#ifdef JPU_IRQ_CONTROL
    drv_vc_disable_irq(s_jpu_irq[core]);
    jpu_core_irq_count[core]++;
    irq_status[core] = 0;
#endif

    flag = 0;
    for (i=0; i< MAX_NUM_REGISTER_SET; i++) {
        flag = ReadJpuRegister(core, MJPEG_PIC_STATUS_REG(i));
        if (flag != 0) {
            break;
        }
    }

    if (i != 0) {
        WriteJpuRegister(core, MJPEG_PIC_STATUS_REG(i), flag);
        jpu_enable_irq(core);
        return IRQ_HANDLED;
    }

    s_jpu_drv_context.interrupt_reason[core][i] = flag;
    s_interrupt_flag[core*MAX_JPEG_NUM_INSTANCE + i] = 1;
    DPRINTK("[JPUDRV][%d] core:%d INTERRUPT FLAG: %08x, %08x\n", i, core, s_jpu_drv_context.interrupt_reason[core][i], MJPEG_PIC_STATUS_REG(i));

    if (dev && dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);    // notify the interrupt to userspace

    wake_up(&s_interrupt_wait_q[core * MAX_JPEG_NUM_INSTANCE + i]);

    DPRINTK("[JPUDRV][-]%s flag=0x%x\n", __func__, flag);
    return IRQ_HANDLED;
}

int jpu_get_register_info(int core_idx, jpudrv_buffer_t *arg)
{
    memcpy(arg, &s_jpu_register[core_idx], sizeof(jpudrv_buffer_t));
    return 0;
}

int jpu_reset(int core_idx)
{
    jpu_hw_reset(core_idx);
    return 0;
}

int jpu_enable_irq(int core_idx)
{
#ifdef JPU_IRQ_CONTROL
    if(jpu_core_irq_count[core_idx] <= 0)
    {
        jpu_core_irq_count[core_idx] = 0;
        return 0;
    }
    drv_vc_enable_irq(s_jpu_irq[core_idx]);
    jpu_core_irq_count[core_idx]--;
    irq_status[core_idx] = 1;
#endif
    return 0;
}

int jpu_wait_interrupt(jpudrv_intr_info_t *arg)
{
    jpudrv_intr_info_t *p_info = (jpudrv_intr_info_t *)arg;
    int ret;
    struct jpu_drv_context_t *dev = (struct jpu_drv_context_t *)g_filp->private_data;
    u32 instance_no;
    u32 core_idx;

    instance_no = p_info->inst_idx;
    core_idx = p_info->core_idx;
    DPRINTK("[JPUDRV] 2 INSTANCE NO: %u, core_idx:%u s_interrupt_flag:%d\n", instance_no, core_idx, s_interrupt_flag[core_idx* MAX_JPEG_NUM_INSTANCE + instance_no]);
    ret = wait_event_timeout(s_interrupt_wait_q[core_idx * MAX_JPEG_NUM_INSTANCE + instance_no], s_interrupt_flag[core_idx* MAX_JPEG_NUM_INSTANCE + instance_no] != 0, msecs_to_jiffies(p_info->timeout));
    if (!ret) {
        DPRINTK("[JPUDRV] INSTANCE NO: %d ETIME\n", instance_no);
        return -ETIME;
    }

    /*
    DPRINTK("[JPUDRV] waked up ok,INSTANCE NO: %u, core_idx:%u\n", instance_no, core_idx);
    if (signal_pending(current)) {
        ret = -ERESTARTSYS;
        JLOG(ERR, "[JPUDRV] CORE:%d INSTANCE NO: %u ERESTARTSYS\n", core_idx, instance_no);
        return ret;
    }
    */

    DPRINTK("[JPUDRV] INST(%u) s_interrupt_flag(%d), reason(0x%08x)\n", instance_no, s_interrupt_flag[core_idx* MAX_JPEG_NUM_INSTANCE + instance_no],
        s_jpu_drv_context.interrupt_reason[core_idx][instance_no]);
    p_info->intr_reason = s_jpu_drv_context.interrupt_reason[core_idx][instance_no];
    s_interrupt_flag[core_idx* MAX_JPEG_NUM_INSTANCE + instance_no] = 0;
    dev->interrupt_reason[core_idx][instance_no] = 0;
#if 0
#ifdef JPU_IRQ_CONTROL
    enable_irq(s_jpu_irq[core_idx]);
#endif
#endif
    return 0;
}

int jpu_free_memory(jpudrv_buffer_t *arg)
{
    jpudrv_buffer_pool_t *jbp, *n;
    jpudrv_buffer_t jb;

    DPRINTK("[JPUDRV][+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");
    down(&s_jpu_sem);
    memcpy(&jb, (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));

    if (jb.base)
        jpu_free_dma_buffer(&jb);

    mutex_lock(&s_jpu_lock);
    list_for_each_entry_safe(jbp, n, &s_jbp_head, list) {
        if (jbp->jb.base == jb.base) {
            list_del(&jbp->list);
            kfree(jbp);
            break;
        }
    }
    mutex_unlock(&s_jpu_lock);

    up(&s_jpu_sem);

    DPRINTK("[JPUDRV][-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

    return 0;
}

int jpu_alloc_memory(jpudrv_buffer_t *arg)
{
    jpudrv_buffer_pool_t *jbp;
    int ret;

    DPRINTK("[JPUDRV][+]JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
    down(&s_jpu_sem);
    jbp = kzalloc(sizeof(jpudrv_buffer_pool_t), GFP_KERNEL);
    if (!jbp) {
        up(&s_jpu_sem);
        return -ENOMEM;
    }

    memcpy(&(jbp->jb), (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));

    ret = jpu_alloc_dma_buffer(&(jbp->jb));
    if (ret == -1)
    {
        ret = -ENOMEM;
        kfree(jbp);
        up(&s_jpu_sem);
        return ret;
    }
    memcpy(arg, &(jbp->jb), sizeof(jpudrv_buffer_t));

    mutex_lock(&s_jpu_lock);
    list_add(&jbp->list, &s_jbp_head);
    mutex_unlock(&s_jpu_lock);

    up(&s_jpu_sem);

    DPRINTK("[JPUDRV][-]JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

    return 0;
}

int jpu_open_device(void)
{
    DPRINTK("[JPUDRV][+] %s\n", __func__);

    mutex_lock(&s_jpu_lock);

    s_jpu_drv_context.open_count++;

    if(!g_filp)
        g_filp = vmalloc(sizeof(struct file));
    g_filp->private_data = (void *)(&s_jpu_drv_context);
    mutex_unlock(&s_jpu_lock);

    DPRINTK("[JPUDRV][-] %s\n", __func__);

    return 1;
}

int jpu_get_instancepool(jpudrv_buffer_t* arg)
{
    DPRINTK("[JPUDRV][+]JDI_IOCTL_GET_INSTANCE_POOL\n");

    down(&s_jpu_sem);
    if (s_instance_pool.base != 0) {
        memcpy(arg, &s_instance_pool, sizeof(jpudrv_buffer_t));
    } else {
        memcpy(&s_instance_pool, (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
        s_instance_pool.size      = PAGE_ALIGN(s_instance_pool.size);
        s_instance_pool.base      = (unsigned long)vmalloc(s_instance_pool.size);
        s_instance_pool.phys_addr = s_instance_pool.base;

        if (s_instance_pool.base != 0) {
            memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size); /*clearing memory*/
            memcpy(arg, &s_instance_pool, sizeof(jpudrv_buffer_t));
        }else {
            JLOG(ERR, "allocate failed,no memory for jpeg instance pool.\n");
        }
    }
    up(&s_jpu_sem);

    JLOG(INFO, "[JPUDRV][-]JDI_IOCTL_GET_INSTANCE_POOL: %s base: %lx, size: %lu\n",
            (s_instance_pool.base ? "OK" : "NG"),
            s_instance_pool.base, s_instance_pool.size);

    return 0;
}

int jpu_open_instance(jpudrv_inst_info_t *inst_info)
{
    mutex_lock(&s_jpu_lock);
    s_jpu_open_ref_count++; /* flag just for that jpu is in opened or closed */
    inst_info->inst_open_count = s_jpu_open_ref_count;
    mutex_unlock(&s_jpu_lock);

    DPRINTK("[JPUDRV] JDI_IOCTL_OPEN_INSTANCE inst_idx=%d, s_jpu_open_ref_count=%d, inst_open_count=%d\n",
            (int)inst_info->inst_idx, s_jpu_open_ref_count, inst_info->inst_open_count);
    return 0;
}

int jpu_close_instance(jpudrv_inst_info_t *inst_info)
{
    DPRINTK("[JPUDRV][+]JDI_IOCTL_CLOSE_INSTANCE\n");

    mutex_lock(&s_jpu_lock);
    s_jpu_open_ref_count--; /* flag just for that jpu is in opened or closed */
    inst_info->inst_open_count = s_jpu_open_ref_count;
    mutex_unlock(&s_jpu_lock);

    //jpu_core_release_resource(inst_info.core_idx);

    DPRINTK("[JPUDRV] JDI_IOCTL_CLOSE_INSTANCE inst_idx=%d, s_jpu_open_ref_count=%d, inst_open_count=%d\n",
            (int)inst_info->inst_idx, s_jpu_open_ref_count, inst_info->inst_open_count);
    return 0;
}

int jpu_set_clock_gate(int core_idx, int *enable)
{
    u32 clkgate;

    clkgate = *enable;
#ifdef JPU_SUPPORT_CLOCK_CONTROL
    if (clkgate)
        jpu_clk_enable(core_idx);
    else
        jpu_clk_disable(core_idx);
#endif /* JPU_SUPPORT_CLOCK_CONTROL */

    return 0;
}

int jpu_register_clk(struct platform_device *pdev)
{
    int ret;
    int i;

    for (i = 0; i < 4; i++) {
        jpu_pwm_ctrl.jpu_sys_clk[i] = devm_clk_get(&pdev->dev, jpu_clk_name[i]);
        if (IS_ERR(jpu_pwm_ctrl.jpu_sys_clk[i])) {
            ret = PTR_ERR(jpu_pwm_ctrl.jpu_sys_clk[i]);
            dev_err(&pdev->dev, "failed to retrieve jpu %s clock",  jpu_clk_name[i]);
            return ret;
        }

        jpu_pwm_ctrl.jpu_apb_clk[i] = devm_clk_get(&pdev->dev, jpu_clk_name[i + 4]);
        if (IS_ERR(jpu_pwm_ctrl.jpu_apb_clk[i])) {
            ret = PTR_ERR(jpu_pwm_ctrl.jpu_apb_clk[i]);
            dev_err(&pdev->dev, "failed to retrieve jpu %s clock",  jpu_clk_name[i + 4]);
            return ret;
        }
    }

    jpu_pwm_ctrl.jpu_src = devm_clk_get(&pdev->dev, jpu_clk_name[8]);
    if (IS_ERR(jpu_pwm_ctrl.jpu_src)) {
        ret = PTR_ERR(jpu_pwm_ctrl.jpu_src);
        dev_err(&pdev->dev, "failed to retrieve jpu %s clock",  jpu_clk_name[8]);
        return ret;
    }

    return 0;
}

#ifdef CONFIG_PM
int jpeg_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef JPU_SUPPORT_CLOCK_CONTROL
    int i;

    mutex_lock(&s_jpu_lock);
    if (!s_jpu_open_ref_count) {
        mutex_unlock(&s_jpu_lock);
        return 0;
    }
    mutex_unlock(&s_jpu_lock);
    for (i=0; i<MAX_NUM_JPU_CORE; i++) {
        jpu_clk_disable(i);
    }
#endif
    return 0;

}
int jpeg_drv_resume(struct platform_device *pdev)
{
#ifdef JPU_SUPPORT_CLOCK_CONTROL
    int i;

    mutex_lock(&s_jpu_lock);
    if (!s_jpu_open_ref_count) {
        mutex_unlock(&s_jpu_lock);
        return 0;
    }
    mutex_unlock(&s_jpu_lock);
    for (i=0; i<MAX_NUM_JPU_CORE; i++)
        jpu_clk_enable(i);
#endif

    return 0;
}
#endif /* !CONFIG_PM */


int jpeg_platform_init(struct platform_device *pdev)
{
    u32 i;
#if !defined(JPU_SUPPORT_ION_MEMORY) && !defined(JPU_SUPPORT_RESERVED_VIDEO_MEMORY)
    int err = 0;
#endif
    struct resource *res = NULL;

    DPRINTK("[JPUDRV] begin jpeg_platform_init\n");

    for (i=0; i<MAX_NUM_JPU_CORE * MAX_JPEG_NUM_INSTANCE; i++) {
        init_waitqueue_head(&s_interrupt_wait_q[i]);
        s_interrupt_flag[i] = 0;
    }

    jpu_core_init_resources(MAX_NUM_JPU_CORE);
    s_instance_pool.base = 0;
    for(i = 0; i < MAX_NUM_JPU_CORE; i++) {

        if (pdev) {
            res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        }
        if (res) {/* if platform driver is implemented */
            s_jpu_register[i].phys_addr = res->start;
            s_jpu_register[i].size  = resource_size(res);
        } else {
            s_jpu_register[i].phys_addr = s_jpu_reg_phy_base[i];
            s_jpu_register[i].size      = JPU_REG_SIZE;
        }
        // s_jpu_register[i].virt_addr = (unsigned long)IOREMAP(s_jpu_register[i].phys_addr, s_jpu_register[i].size);
        DPRINTK("[JPUDRV] : jpu base address get from defined value physical base addr==0x%lx, virtual base=0x%lx\n", s_jpu_register[i].phys_addr, s_jpu_register[i].virt_addr);
    }

#ifdef JPU_SUPPORT_ISR
        for(i = 0; i < MAX_NUM_JPU_CORE; i++) {
            if(pdev)
                res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
            if (res) {/* if platform driver is implemented */
                s_jpu_irq[i] = res->start;
                DPRINTK("[JPUDRV] : jpu irq number get from platform driver irq=0x%x\n", s_jpu_irq[i]);
            } else {
                DPRINTK("[JPUDRV] : jpu irq number get from defined value irq=0x%x\n", s_jpu_irq[i] );
            }

            err = request_irq(s_jpu_irq[i], jpu_irq_handler, IRQF_TRIGGER_NONE, "JPU_CODEC_IRQ", (void *)(&s_jpu_drv_context));
            if (err) {
                printk(KERN_ERR "[JPUDRV] :  fail to register interrupt handler\n");
                goto ERROR_PROVE_DEVICE;
            }
        }
#endif

        // virt_top_addr = (unsigned long)ioremap(JPEG_TOP_REG,4);
#ifdef JPU_SUPPORT_CLOCK_CONTROL
        if (jpu_register_clk(pdev)) {
            DPRINTK("[JPUDRV] : jpeg clock init failed\n");
            goto ERROR_PROVE_DEVICE;
        }
#endif
        return 0;
#if defined(JPU_SUPPORT_ISR) || defined(JPU_SUPPORT_CLOCK_CONTROL)
ERROR_PROVE_DEVICE:
#endif

    for(i=0; i < MAX_NUM_JPU_CORE; i++) {
        if (s_jpu_register[i].virt_addr)
            iounmap((void *)s_jpu_register[i].virt_addr);
        s_jpu_register[i].virt_addr = 0;
    }


    DPRINTK("[JPUDRV] end jpeg_init result=0x%x\n", err);
    return 0;
}

void jpeg_platform_exit(void)
{
    int i;
    DPRINTK("[JPUDRV] [+]jpeg_exit\n");

    if (s_instance_pool.base) {
        vfree((const void *)s_instance_pool.base);
        s_instance_pool.base = 0;
    }
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base) {
        iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
        jmem_exit(&s_jmem);
    }
#endif

#ifdef JPU_SUPPORT_ISR
    for(i = 0; i < MAX_NUM_JPU_CORE; i++) {
        if (s_jpu_irq[i])
            free_irq(s_jpu_irq[i], &s_jpu_drv_context);
    }
#endif

    for(i = 0; i < MAX_NUM_JPU_CORE; i++) {
        if (s_jpu_register[i].virt_addr)
            iounmap((void*)s_jpu_register[i].virt_addr);
        s_jpu_register[i].virt_addr = 0;
    }

    if (virt_top_addr) {
        iounmap((void *)virt_top_addr);
        virt_top_addr = 0;
    }

    jpu_core_cleanup_resources();

    DPRINTK("[JPUDRV] [-]jpeg_exit\n");

    return;
}

static int jpu_hw_reset(int idx)
{
    DPRINTK("[JPUDRV] request jpu reset from application. \n");
    return 0;
}

#ifdef JPU_SUPPORT_CLOCK_CONTROL
struct clk *jpu_clk_get(struct device *dev)
{
    return devm_clk_get(dev, JPU_CLK_NAME);
}
void jpu_clk_put(struct clk *clk)
{
   // if (!(clk == NULL || IS_ERR(clk)))
    //    clk_put(clk);
}
void jpu_clk_enable(int core_idx)
{
    if (!__clk_is_enabled(jpu_pwm_ctrl.jpu_sys_clk[core_idx]))
        clk_prepare_enable(jpu_pwm_ctrl.jpu_sys_clk[core_idx]);

    clk_prepare_enable(jpu_pwm_ctrl.jpu_src);

    if (!__clk_is_enabled(jpu_pwm_ctrl.jpu_apb_clk[core_idx]))
        clk_prepare_enable(jpu_pwm_ctrl.jpu_apb_clk[core_idx]);
}

void  jpu_clk_disable(int core_idx)
{
    if (__clk_is_enabled(jpu_pwm_ctrl.jpu_sys_clk[core_idx]))
        clk_disable_unprepare(jpu_pwm_ctrl.jpu_sys_clk[core_idx]);

    if (__clk_is_enabled(jpu_pwm_ctrl.jpu_src))
        clk_disable_unprepare(jpu_pwm_ctrl.jpu_src);

    if (__clk_is_enabled(jpu_pwm_ctrl.jpu_apb_clk[core_idx]))
        clk_disable_unprepare(jpu_pwm_ctrl.jpu_apb_clk[core_idx]);
}
#endif

void jpu_lock(void)
{
    mutex_lock(&jpu_mutex);
}

void jpu_unlock(void)
{
    mutex_unlock(&jpu_mutex);
}

