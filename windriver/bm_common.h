//
// bm_common.h for bm sophon driver
//
#ifndef _PRECOMP_
#define _PRECOMP_
#undef __STDC_WANT_SECURE_LIB__
#define _NO_CRT_STDIO_INLINE
#define _CRT_SECURE_NO_WARNINGS
#define WIN9X_COMPAT_SPINLOCK
#include <ntddk.h>
#include <wdf.h>
#include <ntdef.h>
#include <stdio.h>

#include <initguid.h> // required for GUID definitions
#include <wdmsec.h> // for SDDLs
#include <wmistr.h>
#include <wmilib.h>
#include <ntintsafe.h>
#include <wdfobject.h>

//
// Disable warnings that prevent our driver from compiling with /W4 MSC_WARNING_LEVEL
//
// Disable warning C4214: nonstandard extension used : bit field types other than int
// Disable warning C4201: nonstandard extension used : nameless struct/union
// Disable warning C4115: named type definition in parentheses
//

#pragma warning(disable:4214)
#pragma warning(disable:4201)
#pragma warning(disable:4115)

#pragma warning(default:4214)
#pragma warning(default:4201)
#pragma warning(default:4115)

#include "trace.h"
#include "bm_def.h"
#include "public.h"

struct chip_info;
struct smbus_devinfo;
struct bm_chip_attr;

#include "bm_bgm.h"
#include "bm_msg.h"
#include "bm_io.h"
#include "bm_irq.h"
#include "bm_api.h"
#include "bm_cdma.h"
#include "bm_timer.h"
#include "pwm.h"
#include "uart.h"
#include "i2c.h"
#include "bm_wdt.h"
#include "spi.h"
#include "vpu/vpuconfig.h"
#include "vpu/vmm_type.h"
#include "vpu/vpu.h"

#include "bm_trace.h"

#include "bm_memcpy.h"
#include "bm_gmem.h"
#include "bm_attr.h"
#include "bm_uapi.h"
#include "bm_msgfifo.h"
#include "bm_debug.h"

#include "bm_thread.h"
#include "bm_card.h"

#include "bm_debug.h"

#include "bm_boot_info.h"
#include "bm_pcie.h"
#include "bm_fw.h"
#include "bm_drv.h"
#include "bm_ddr.h"
#include "bm_ctl.h"
#include "gpio.h"


#include "bm1682/bm1682_irq.h"
#include "bm1682/bm1682_card.h"
#include "bm1682/bm1682_pcie.h"
#include "bm1682/bm1682_msgfifo.h"
#include "bm1682/bm1682_gmem.h"

#include "bm1684/bm1684_card.h"
#include "bm1684/bm1684_flash.h"
#include "bm1684/bm1684_smbus.h"
#include "bm1684/bm1684_vpp.h"
#include "bm1684/bm1684_jpu.h"
#include "bm1684/bm1684_clkrst.h"
#include "bm1684/bm1684_irq.h"
#include "bm1684/bm1684_pcie.h"

#include "bm1684/bm1684_gmem.h"
#include "bm1684/bm1684_msgfifo.h"

#include "bm1684/bm1684_perf.h"
#include "bm1684/bm1684_base64.h"

#include "bm_common.h"
#include "bm_monitor.h"


#define BM_CHIP_VERSION 3
#define BM_MAJOR_VERSION 0
#define BM_MINOR_VERSION 0
#define BM_DRIVER_VERSION                                                      \
    ((BM_CHIP_VERSION << 16) | (BM_MAJOR_VERSION << 8) | BM_MINOR_VERSION)

#define MAX_CARD_NUMBER 64

#define BM_CLASS_NAME "bm-sophon"
#define BM_CDEV_NAME "bm-sophon"

#define BMDEV_CTL_NAME "bmdev-ctl"
#define BMPCI_POOL_TAG (ULONG)'EICP'


#define A53_RESET_STATUS_TRUE    1
#define A53_RESET_STATUS_FALSE   0

/*
 * memory policy
 * define it to use dma_xxx series APIs, which provide write-back
 * memory type, otherwise use kmalloc+set_memory_uc to get uncached-
 * minus memory type.
 */
#define USE_DMA_COHERENT

/* specify if platform is palladium */
#define PALLADIUM_CLK_RATIO 4000
#define DELAY_MS 20000
#define POLLING_MS 1

typedef enum { DEVICE, PALLADIUM, FPGA } PLATFORM;

enum {
    FIP_SRC_SPIF = 0X0, // SPI flash
    FIP_SRC_SRAM = 0X1, // SRAM upper half
    FIP_SRC_SDFT = 0X2, // SD card FAT32 partition
    FIP_SRC_INVALID = 0xFF,
};

enum {
    FIP_LOADED = (1 << 0), // fip.bin being loaded into RAM, set by PCIe host or JTAG
    SKIP_PCIEI = (1 << 1), // skip PCIe init during boot, set by warm reset routine
    PCIE_EP_LINKED = (1 << 2), // board has valid PCIe link to some PCIe host
    SOC_EP = (1 << 3), // Soc mode with PCIe EP linked
};

// #ifdef PCIE_MODE_ENABLE_CPU
// typedef enum {
//     BMCPU_IDLE,
//     BMCPU_RUNNING,
//     BMCPU_FAULT
// } BMCPU_STATUS;
// #endif

struct chip_info {
    const struct bm_card_reg *bm_reg;
    struct smbus_devinfo      dev_info;

    const char *chip_type;
    char        dev_name[10];

    int          share_mem_size;
    PLATFORM     platform;
    u32          delay_ms;
    u32          polling_ms;
    unsigned int chip_id;
    int          chip_index;

    int                a53_enable;
    unsigned long long heap2_size;
    int                irq_id;
    char               drv_irq_name[16];
    char               boot_loader_version[2][64];
    char               sn[18];
    u32                ddr_failmap;
    u32                ddr_ecc_correctN;
    // struct pci_dev *pcidev;
    bool has_msi;
    int  board_version; /*bit [8:15] board type, bit [0:7] hardware version*/
    bmdrv_submodule_irq_handler bmdrv_module_irq_handler[128];
    void (*bmdrv_map_bar)(struct bm_device_info *);
    void (*bmdrv_unmap_bar)(struct bm_bar_info *);
    void (*bmdrv_pcie_calculate_cdma_max_payload)(struct bm_device_info *);
    void (*bmdrv_enable_irq)(struct bm_device_info *bmdi,
                             int                    irq_num,
                             bool                   irq_enable);
    void (*bmdrv_get_irq_status)(struct bm_device_info *bmdi, u32 *status);
    void (*bmdrv_unmaskall_intc_irq)(struct bm_device_info *bmdi);

    int (*bmdrv_setup_bar_dev_layout)(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type);

    void (*bmdrv_start_arm9)(struct bm_device_info *);
    void (*bmdrv_stop_arm9)(struct bm_device_info *);

    void (*bmdrv_clear_cdmairq)(struct bm_device_info *bmdi);
    int (*bmdrv_clear_msgirq)(struct bm_device_info *bmdi);
    int (*bmdrv_clear_cpu_msgirq)(struct bm_device_info *bmdi);

    u32 (*bmdrv_pending_msgirq_cnt)(struct bm_device_info *bmdi);
    struct bm_bar_info bar_info;
};

typedef int tpu_kernel_function_t;
typedef struct
{
    tpu_kernel_function_t f_id;
    unsigned char md5[16];
    char func_name[64];

    FAST_MUTEX bm_get_func_mutex;
}bm_get_func_t;

struct bmdrv_exec_func
{
    LIST_ENTRY func_list;
    bm_get_func_t exec_func;
};


typedef struct bm_device_info {
    int dev_index;// sophon0  sophon1
    u64 bm_send_api_seq;

    WDFDEVICE              WdfDevice;
    WDFDRIVER              WdfDriver;
    BUS_INTERFACE_STANDARD BusInterface;
    WDFINTERRUPT           WdfInterrupt;

    WDFQUEUE               IoctlQueue;
    WDFQUEUE               CreateQueue;
    WDFLOOKASIDE           Lookaside;
    void *                 priv;
    unsigned int           device_id;//0x1682 0x1684
    unsigned int           vendor_id;
    unsigned int           bus_number;
    unsigned short         function_number;
    unsigned short         device_number;  //bdf 's d

	int probed;
    int is_surpriseremoved;

    WDFSPINLOCK         device_spinlock;
    struct chip_info    cinfo;
    struct bm_chip_attr c_attr;
    struct bm_card *    bmcd;
    int                 enable_dyn_freq;
    int                 dump_reg_type;
    int                 fixed_fan_speed;
    int                 status; /* active or fault */
    int                 status_pcie;
    int                 status_over_temp;
    int                 status_sync_api;
    u64 dev_refcount;

    struct bmcpu_lib *lib_info;

    struct bmcpu_lib *lib_dyn_info;

    struct bmdrv_exec_func exec_func;

    struct bm_api_info  api_info[BM_MSGFIFO_CHANNEL_NUM];

    struct bm_gmem_info gmem_info;

    LIST_ENTRY handle_list;

    struct bm_memcpy_info memcpy_info;

    FAST_MUTEX clk_reset_mutex;

    struct bm_trace_info trace_info;

    struct bm_misc_info misc_info;

    struct bm_boot_info boot_info;

    struct bm_profile profile;

    struct bm_monitor_thread_info monitor_thread_info;

    ////struct proc_dir_entry *proc_dir;

    vpp_drv_context_t vppdrvctx;
    vpu_drv_context_t vpudrvctx;
    jpu_drv_context_t jpudrvctx;
    FAST_MUTEX        spacc_mutex;
    int               status_bmcpu;
    DEVICE_POWER_STATE DevicePowerState;
} BMDI, *PBMDI;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BMDI, Bm_Get_Bmdi)

//
// The driver context contains global data to the whole driver.
//
typedef struct _DRIVER_CONTEXT {
    //
    // The assumption here is that there is nothing device specific in the
    // lookaside list and hence the same list can be used to do allocations for
    // multiple devices.
    //
    WDFLOOKASIDE RecvLookaside;

} DRIVER_CONTEXT, *PDRIVER_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DRIVER_CONTEXT, GetDriverContext)

struct bin_buffer {
    unsigned char *buf;
    unsigned int   size;
    unsigned int   target_addr;
};

int kfifo_alloc(struct kfifo *fifo, unsigned int size);
void kfifo_free(struct kfifo *fifo);
unsigned int kfifo_len(struct kfifo *fifo);
unsigned int kfifo_avail(struct kfifo *fifo);
unsigned int kfifo_is_empty(struct kfifo *fifo);
unsigned int kfifo_is_full(struct kfifo *fifo);
unsigned int kfifo_in(struct kfifo * fifo, unsigned char *from, unsigned int len);
unsigned int kfifo_out(struct kfifo *fifo, unsigned char *to, unsigned int len);
unsigned int kfifo_out_peek(struct kfifo * fifo,unsigned char *to,unsigned int   len);
void kfifo_reset(struct kfifo *fifo);
/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) CONTAINING_RECORD(ptr, type, member)
/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head)                                               \
    for (pos = (head)->Flink; pos != (head); pos = pos->Flink)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head)                                       \
    for (pos = (head)->Flink, n = pos->Flink; pos != (head);                     \
         pos = n, n = pos->Flink)

/**
 * list_for_each_entry    -    iterate over list of given type
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member, type)                           \
    for (pos = list_entry((head)->Flink, type, member); &pos->member != (head); \
         pos = list_entry(pos->member.Flink, type, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against
 * removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member, type)                   \
    for (pos = list_first_entry(head, type, member),                           \
        n    = list_next_entry(pos, type, member);                             \
         !list_entry_is_head(pos, head, member);                               \
         pos = n, n = list_next_entry(n, type, member))

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member)                                    \
    list_entry((*ptr).Flink, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) list_entry((*ptr).Blink, type, member)

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, type, member)                                     \
    list_entry((pos)->member.Flink, type, member)

/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */

#define list_prev_entry(pos, type, member)                                     \
    list_entry((pos)->member.Blink, type, member)

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) (&pos->member == (head))

#include "device.h"


#endif
