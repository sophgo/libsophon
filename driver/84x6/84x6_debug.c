#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/circ_buf.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>

#include "84x6_task.h"
#include "84x6_shmem.h"
#include "84x6_tsh.h"

/* structures (must match cv84x6_process.h) */
#define API_ID_A53LITE_LOAD_LIB   0x90000001
#define API_ID_A53LITE_GET_FUNC   0x90000002
#define API_ID_A53LITE_LAUNCH_FUNC 0x90000003
#define API_ID_A53LITE_UNLOAD_LIB  0x90000004
#define API_ID_A53LITE_GETR_VERSION 0x90000010

#define SHMPKT_MD5SUM_LEN 16
#define SHMPKT_LIB_MAX_NAME_LEN 64
#define SHMPKT_FUNC_MAX_NAME_LEN 64

struct shmpkt_api_header {
    u32 api_id;
    u32 api_size;
    u64 api_handle;
    u32 api_seq;
    u32 duration;
    u32 result;
} __attribute__((packed));

struct shmpkt_load_lib {
    u64 library_path;
    u64 library_addr;
    u32 size;
    u8 library_name[SHMPKT_LIB_MAX_NAME_LEN];
    u8 md5[SHMPKT_MD5SUM_LEN];
    int cur_rec;
} __attribute__((packed));

struct shmpkt_get_func {
    int core_id;
    int f_id;
    u8 md5[SHMPKT_MD5SUM_LEN];
    u8 func_name[SHMPKT_FUNC_MAX_NAME_LEN];
} __attribute__((packed));

struct shmpkt_launch_func {
    int f_id;
    unsigned int size;
    u8 param[4096];
} __attribute__((packed));

#define PROC_DIR_DEFAULT "bmtpu"
#define PROC_WAKEUP_CONTROL "wakeup_control"
#define PROC_KERNEL_INFO "kernel_info"
#define PROC_KERNEL_FIFO "kernel_fifo"
#define PROC_DUMP_TXPSYS "txpsys"
#define PROC_SCALER_STATUS "scaler_info"
#define PROC_SHM_PACKET "shmpacket"

#define SHMPACKET_CMD_BUF_SIZE 64
static int shmpacket_offset = -1;

#define MAX_HWQ_INDEX 8

struct global_thread_info {
    struct task_struct *thread;
    bool running;
    unsigned long wakeup_count;
    struct bm_device_info *bmdi;
};

static struct global_thread_info global_test_thread;
static DEFINE_MUTEX(thread_mutex);

#define KERNEL_FIFO_RECORD_COUNT 64

struct kernel_fifo_record {
    u64 jiffies;
    u32 seq;
    u32 api_id;
    u32 api_seq;
    u8 hwq_idx;
    u8 is_outbox;
    u8 tsh_mode;
    u8 reserved;
    u32 packet_addr;
    u32 group_num;
    u32 block_num;
    u32 group_idx;
    u32 start_core_idx;
    u32 send_core_num;
    u32 phase;
    u32 hwq_packet;
    u64 outbox_packet;
};

static struct kernel_fifo_record kernel_fifo_ring[KERNEL_FIFO_RECORD_COUNT];
static u32 kernel_fifo_head;
static u32 kernel_fifo_total;
static DEFINE_SPINLOCK(kernel_fifo_lock);

extern int test_wakeup_consumer_thread(void *data);
extern struct proc_dir_entry *bmdi_folder;

#define SHM_SIZE 0x400000
#define SHM_BASE_ADDR (0x1080000000UL)  /* after 4x256M scalar parks */
#define SHM_LOG_MEM_SIZE (0x200000)
#define NUM_CORES 4
#define SHM_VERSION_BUFFER_SIZE 128


#define SHM_LOG_BASE_ADDR(core_id) \
    (SHM_BASE_ADDR + SHM_SIZE + (core_id) * SHM_LOG_MEM_SIZE)

struct shared_scaler_info {
    volatile uint32_t log_enable;
    volatile uint32_t write_index;
    volatile uint32_t read_index;
    volatile uint32_t buffer_size;
    volatile uint32_t core_id;
    volatile uint32_t recvmsg_cnt;
    volatile uint32_t procmsg_cnt;
    volatile uint32_t running_flag;
    volatile uint32_t using_flag;
    volatile uint32_t debug_run_cnt;
    volatile uint32_t cur_api_id;
    volatile uint32_t cur_packet_addr;
    volatile uint32_t cur_group_idx;
    volatile uint32_t cur_start_core_idx;
    volatile uint32_t cur_group_num;
    volatile uint32_t cur_block_num;
    volatile uint32_t cur_api_result;
    volatile uint64_t timestamp;
    volatile uint32_t seq_num;
    volatile uint32_t dropped_logs;
    char version_buffer[SHM_VERSION_BUFFER_SIZE];
    char buffer[0];
};

struct core_log_reader {
    struct shared_scaler_info *buffer;
    void __iomem *vaddr;
    struct task_struct *thread;
    bool active;
    uint32_t last_read_idx;
    uint32_t total_logs;
    uint32_t dropped_logs;
    char thread_name[16];
    uint64_t last_timestamp;
};

static struct core_log_reader cores[NUM_CORES];
static bool stop_all_threads = false;
static uint32_t global_log_count = 0;

static const char *tsh_get_proc_dir_name(const struct bm_device_info *bmdi)
{
    static char name_buf[64];
    if (bmdi && bmdi->cinfo.device && bmdi->cinfo.device->of_node)
        return bmdi->cinfo.device->of_node->full_name;
    if (!bmdi)
        return PROC_DIR_DEFAULT;
    snprintf(name_buf, sizeof(name_buf), "%s%d", PROC_DIR_DEFAULT,
             bmdi->dev_index);
    return name_buf;
}

static int read_core_logs(struct core_log_reader *core, int core_id)
{
    uint32_t write_idx, read_idx, buf_size;
    char *log_buf = NULL;
    int logs_read = 0;

    if (!core->buffer || !core->active)
        return 0;

    write_idx = READ_ONCE(core->buffer->write_index);
    read_idx = READ_ONCE(core->buffer->read_index);
    buf_size = READ_ONCE(core->buffer->buffer_size);

    if (write_idx == read_idx || buf_size == 0 || buf_size >= SHM_LOG_MEM_SIZE)
        return 0;

    log_buf = kmalloc(4096, GFP_KERNEL);
    if (!log_buf)
        return 0;

    if (write_idx > read_idx) {
        uint32_t bytes_to_read = write_idx - read_idx;
        bytes_to_read = min(bytes_to_read, 4095U);

        memcpy_fromio(log_buf, &core->buffer->buffer[read_idx], bytes_to_read);
        log_buf[bytes_to_read] = '\0';

        char *line = log_buf;
        char *next_line;

        while ((next_line = strchr(line, '\n')) != NULL) {
            *next_line = '\0';
            if (*line) {
                printk(KERN_INFO "scaler-%d: %s\n", core_id, line);
                logs_read++;
                global_log_count++;
            }
            line = next_line + 1;
        }

        uint32_t processed = line - log_buf;
        if (processed > 0) {
            uint32_t new_read_idx = (read_idx + processed) % buf_size;
            WRITE_ONCE(core->buffer->read_index, new_read_idx);
        }
    } else {
        uint32_t first_part = buf_size - read_idx;
        uint32_t second_part = write_idx;
        uint32_t total_bytes = first_part + second_part;

        total_bytes = min(total_bytes, 4095U);

        if (first_part > 4095)
            first_part = 4095;
        memcpy_fromio(log_buf, &core->buffer->buffer[read_idx], first_part);

        if (second_part > 0 && (first_part < 4095)) {
            second_part = min(second_part, 4095U - first_part);
            memcpy_fromio(log_buf + first_part,
                          core->buffer->buffer, second_part);
        }

        log_buf[first_part + second_part] = '\0';

        char *line = log_buf;
        char *next_line;

        while ((next_line = strchr(line, '\n')) != NULL) {
            *next_line = '\0';
            if (*line) {
                printk(KERN_INFO "scaler-%d: %s\n", core_id, line);
                logs_read++;
                global_log_count++;
            }
            line = next_line + 1;
        }

        uint32_t processed = line - log_buf;
        if (processed > 0) {
            uint32_t new_read_idx = (read_idx + processed) % buf_size;
            WRITE_ONCE(core->buffer->read_index, new_read_idx);
        }
    }

    core->total_logs += logs_read;
    core->dropped_logs = READ_ONCE(core->buffer->dropped_logs);
    core->last_timestamp = READ_ONCE(core->buffer->timestamp);

    kfree(log_buf);
    return logs_read;
}


static int core_log_thread(void *data)
{
    int core_id = (int)(long)data;
    struct core_log_reader *core = &cores[core_id];

    while (!kthread_should_stop() && !stop_all_threads) {
        int logs_read = read_core_logs(core, core_id);

        if (logs_read == 0) {
            msleep_interruptible(20);
        } else {
            msleep_interruptible(2);
        }
    }

    return 0;
}

static int create_log_threads(struct bm_device_info *bmdi)
{
    int i;

    for (i = 0; i < bmdi->cinfo.tpu_core_num; i++) {
        snprintf(cores[i].thread_name, sizeof(cores[i].thread_name),
                 "c906_log_%d", i);

        cores[i].thread = kthread_run(core_log_thread,
                                      (void *)(long)i, cores[i].thread_name);
        if (IS_ERR(cores[i].thread)) {
            printk(KERN_ERR "Failed to create thread for core %d\n", i);
            return PTR_ERR(cores[i].thread);
        }

        cores[i].active = true;
        printk(KERN_INFO "Created log thread for core %d\n", i);
    }

    return 0;
}

static void stop_log_threads(struct bm_device_info *bmdi)
{
    int i;

    stop_all_threads = true;

    for (i = 0; i < bmdi->cinfo.tpu_core_num; i++) {
        if (cores[i].thread && cores[i].active) {
            kthread_stop(cores[i].thread);
            cores[i].active = false;
            printk(KERN_INFO "Stopped log thread for core %d\n", i);
        }
    }
}

static int proc_stats_show(struct seq_file *m, void *v)
{
    int i;
    char version_buf[SHM_VERSION_BUFFER_SIZE];
    struct bm_device_info *bmdi = m->private;

    if (!bmdi) {
        seq_printf(m, "Device info not available\n");
        return 0;
    }

    seq_printf(m, "Scaler Driver Statistics\n");
    seq_printf(m, "================================\n");
    seq_printf(m, "Total logs received: %u\n", global_log_count);
    seq_printf(m, "\n");

    for (i = 0; i < bmdi->cinfo.tpu_core_num; i++) {
        seq_printf(m, "Core %d:\n", i);
        seq_printf(m, "  Status: %s\n",
                   cores[i].active ? "Active" : "Inactive");
        seq_printf(m, "  Logs processed: %u\n", cores[i].total_logs);
        seq_printf(m, "  Logs dropped: %u\n", cores[i].dropped_logs);
        seq_printf(m, "  Last timestamp: %llu ms\n", cores[i].last_timestamp);

        if (cores[i].buffer) {
            memcpy_fromio(version_buf, cores[i].buffer->version_buffer,
                          SHM_VERSION_BUFFER_SIZE - 1);
            version_buf[SHM_VERSION_BUFFER_SIZE - 1] = '\0';
            seq_printf(m, "  Firmware: %s\n", version_buf);
            seq_printf(m, "  recvmsg_cnt: %u\n",
                       READ_ONCE(cores[i].buffer->recvmsg_cnt));
            seq_printf(m, "  procmsg_cnt: %u\n",
                       READ_ONCE(cores[i].buffer->procmsg_cnt));
            seq_printf(m, "  running_flag: %x\n",
                       READ_ONCE(cores[i].buffer->running_flag));
            seq_printf(m, "  using_flag: %x\n",
                       READ_ONCE(cores[i].buffer->using_flag));
            seq_printf(m, "  debug_run_cnt: %u\n",
                       READ_ONCE(cores[i].buffer->debug_run_cnt));
            seq_printf(m, "  cur_api_id: 0x%x\n",
                       READ_ONCE(cores[i].buffer->cur_api_id));
            seq_printf(m, "  cur_packet_addr: %u\n",
                       READ_ONCE(cores[i].buffer->cur_packet_addr));
            seq_printf(m, "  cur_group_idx: %u, cur_start_core_idx: %u\n",
                       READ_ONCE(cores[i].buffer->cur_group_idx),
                       READ_ONCE(cores[i].buffer->cur_start_core_idx));
            seq_printf(m, "  cur_group_num: %u, cur_block_num: %u\n",
                       READ_ONCE(cores[i].buffer->cur_group_num),
                       READ_ONCE(cores[i].buffer->cur_block_num));
            seq_printf(m, "  cur_api_result: %d\n",
                       (int32_t) READ_ONCE(cores[i].buffer->cur_api_result));
            uint32_t log_enable = READ_ONCE(cores[i].buffer->log_enable);
            uint32_t write_idx = READ_ONCE(cores[i].buffer->write_index);
            uint32_t read_idx = READ_ONCE(cores[i].buffer->read_index);
            uint32_t buffer_used = 0;

            if (write_idx >= read_idx) {
                buffer_used = write_idx - read_idx;
            } else {
                buffer_used = cores[i].buffer->buffer_size -
                    (read_idx - write_idx);
            }

            uint32_t buffer_percent = (buffer_used * 100) /
                cores[i].buffer->buffer_size;
            seq_printf(m, "  log enable: %u\n", log_enable);
            seq_printf(m, "  Buffer usage: %u/%u (%u%%)\n",
                       buffer_used, cores[i].buffer->buffer_size,
                       buffer_percent);
        }
        seq_printf(m, "\n");
    }

    return 0;
}

static int proc_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_stats_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS proc_stats_fops = {
    BM_PROC_OPEN = proc_stats_open,
    BM_PROC_READ = seq_read,
    BM_PROC_LLSEEK = seq_lseek,
    BM_PROC_RELEASE = single_release,
};

int bmdev_scaler_log_init(struct bm_device_info *bmdi)
{
    int i, ret = 0;

#ifndef SOC_MODE
    /* PCIe: scaler log lives in device DDR (0x1080000000+), which has no
     * host BAR window; the whole log region is 8MB > bar1 4MB. Skip. */
    pr_info("Multi-Core Log Driver skipped on PCIe mode\n");
    return 0;
#endif

    printk(KERN_INFO "Multi-Core Log Driver Initializing...\n");

    memset(cores, 0, sizeof(cores));

    for (i = 0; i < bmdi->cinfo.tpu_core_num; i++) {
        uint64_t addr = SHM_LOG_BASE_ADDR(i);

        cores[i].vaddr = ioremap(addr, SHM_LOG_MEM_SIZE);
        if (!cores[i].vaddr) {
            printk(KERN_ERR "Failed to ioremap for core %d at 0x%llx\n",
                   i, addr);
            ret = -ENOMEM;
            goto error_cleanup;
        }

        cores[i].buffer = (struct shared_scaler_info *)cores[i].vaddr;
        cores[i].last_read_idx = 0;
        cores[i].total_logs = 0;
        cores[i].dropped_logs = 0;
        cores[i].last_timestamp = 0;

        printk(KERN_INFO "Core %d: mapped at 0x%llx -> %p, buffer_size=%u\n",
               i, addr, cores[i].buffer,
               READ_ONCE(cores[i].buffer->buffer_size));
    }

    ret = create_log_threads(bmdi);
    if (ret) {
        goto error_cleanup;
    }

    if (bmdi->tsh_proc_dir) {
        if (!proc_create_data(PROC_SCALER_STATUS, 0666, bmdi->tsh_proc_dir,
                              &proc_stats_fops, bmdi))
            pr_warn("proc_create %s failed\n", PROC_SCALER_STATUS);
    } else {
        pr_warn("tsh_proc_dir is NULL, skipping proc_create %s\n",
                PROC_SCALER_STATUS);
    }

    printk(KERN_INFO "Multi-Core Log Driver Initialized for %d cores\n",
           bmdi->cinfo.tpu_core_num);
    return 0;

  error_cleanup:
    for (i = 0; i < bmdi->cinfo.tpu_core_num; i++) {
        if (cores[i].vaddr) {
            iounmap(cores[i].vaddr);
            cores[i].vaddr = NULL;
            cores[i].buffer = NULL;
        }
    }
    return ret;
}

void bmdev_scaler_log_deinit(struct bm_device_info *bmdi)
{
    int i;

    printk(KERN_INFO "Multi-Core Log Driver Exiting...\n");

    /* scaler proc file is reclaimed when bmdev_test_proc_exit removes
     * the per-chip proc directory tree — no separate remove_proc_entry here */

    stop_log_threads(bmdi);

    for (i = 0; i < bmdi->cinfo.tpu_core_num; i++) {
        if (cores[i].vaddr) {
            iounmap(cores[i].vaddr);
        }
    }

    printk(KERN_INFO "Multi-Core Log Driver Exited\n");
}

u32 bmdev_get_scaler_using_flag(int core_id)
{
	if (core_id < 0 || core_id >= NUM_CORES)
		return 0;
	if (!cores[core_id].buffer)
		return 0;
	return READ_ONCE(cores[core_id].buffer->using_flag);
}

static int dump_tscalar_pc(struct seq_file *m, int argc, char **argv)
{
    int coreID = 0;
    u64 pc;
    struct bm_device_info *bmdi = m->private;

    for (coreID = 0; coreID < bmdi->cinfo.tpu_core_num; coreID++) {
        pc = ((u64)
              tpusys_read_32(bmdi,
                             TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID +
                             TPU_SYS_SCALAR_RETIRE_PC_H) << 32)
            | tpusys_read_32(bmdi,
                             TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID +
                             TPU_SYS_SCALAR_RETIRE_PC_L);
        seq_printf(m, "tscalar%d_pc=0x%llx\n", coreID, pc);
    }

    return 0;
}

static int dump_txpsys_reg(struct seq_file *m, int argc, char **argv)
{
    int coreID;
    int ret;
    int i;
    struct bm_device_info *bmdi = m->private;

    if (argc < 2) {
        coreID = 0;
    } else {
        ret = kstrtoint(argv[1], 16, &coreID);
        if (ret != 0) {
            seq_printf(m, "Error: Invalid coreID format\n");
            return -EINVAL;
        }
    }
    seq_printf(m, "Dumping TXP SYS registers for core %d:\n", coreID);

    for (i = 0; i <= 0xcc; i += 4) {
        seq_printf(m, "addr=0x%x, val=0x%x\n",
                   TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID + i,
                   tpusys_read_32(bmdi,
                                  TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID + i));
    }

    return 0;
}

static void shmpkt_print_hex_at(struct seq_file *m, const char *prefix,
                                const u8 * data, size_t len, size_t base_off)
{
    size_t i;
    u32 v;

    for (i = 0; i < len; i += 4) {
        if (i + 4 <= len)
            v = *(u32 *) (data + i);
        else {
            v = 0;
            memcpy(&v, data + i, len - i);
        }
        seq_printf(m, "%s[%04zx] 0x%08x\n", prefix, base_off + i, v);
    }
}

static void shmpkt_print_hex(struct seq_file *m, const char *prefix,
                             const u8 * data, size_t len)
{
    shmpkt_print_hex_at(m, prefix, data, len, 0);
}

static int dump_shmpacket(struct seq_file *m, u32 packet_addr)
{
    struct bm_device_info *bmdi = m->private;
    phys_addr_t phys_addr;
    struct shmpkt_api_header hdr;
    int r;

    r = bm84x6_shmem_read_packet(bmdi, packet_addr, &phys_addr, &hdr,
                                     sizeof(hdr));
    if (r < 0) {
        seq_printf(m, "shmpacket: read failed (packet_addr=0x%x)\n",
                   packet_addr);
        return r;
    }
    seq_printf(m, "shmpacket: packet_addr=0x%x -> phys_addr=0x%llx\n",
               packet_addr, (u64) phys_addr);
    seq_printf(m,
               "  API_HEADER: api_id=0x%x api_size=%u api_handle=0x%llx api_seq=%u duration=%u result=%u\n",
               hdr.api_id, hdr.api_size, (u64) hdr.api_handle, hdr.api_seq,
               hdr.duration, hdr.result);

    switch (hdr.api_id) {
    case API_ID_A53LITE_LOAD_LIB:
    case API_ID_A53LITE_UNLOAD_LIB:{
            struct shmpkt_load_lib payload;
            r = bm84x6_shmem_read_packet_at(bmdi, packet_addr, sizeof(hdr),
                                                &payload, sizeof(payload));
            if (r < 0)
                goto hex_dump;
            seq_printf(m,
                       "  [LOAD/UNLOAD_LIB] library_path=0x%llx library_addr=0x%llx size=%u cur_rec=%d\n",
                       (u64) payload.library_path, (u64) payload.library_addr,
                       payload.size, payload.cur_rec);
            seq_printf(m, "    library_name=%.*s\n",
                       (int)sizeof(payload.library_name), payload.library_name);
            seq_printf(m, "    md5=");
            {
                int i;
                for (i = 0; i < SHMPKT_MD5SUM_LEN; i++)
                    seq_printf(m, "%02x", payload.md5[i]);
                seq_printf(m, "\n");
            }
            break;
        }
    case API_ID_A53LITE_GET_FUNC:{
            struct shmpkt_get_func payload;
            r = bm84x6_shmem_read_packet_at(bmdi, packet_addr, sizeof(hdr),
                                                &payload, sizeof(payload));
            if (r < 0)
                goto hex_dump;
            seq_printf(m, "  [GET_FUNC] core_id=%d f_id=%d\n", payload.core_id,
                       payload.f_id);
            seq_printf(m, "    func_name=%.*s\n",
                       (int)sizeof(payload.func_name), payload.func_name);
            seq_printf(m, "    md5=");
            {
                int i;
                for (i = 0; i < SHMPKT_MD5SUM_LEN; i++)
                    seq_printf(m, "%02x", payload.md5[i]);
                seq_printf(m, "\n");
            }
            break;
        }
    case API_ID_A53LITE_LAUNCH_FUNC:{
            int f_id;
            unsigned int sz;
            u8 param_buf[256];
            size_t off, chunk;

            r = bm84x6_shmem_read_packet_at(bmdi, packet_addr, sizeof(hdr),
                                                &f_id, sizeof(f_id));
            if (r < 0)
                goto hex_dump;
            r = bm84x6_shmem_read_packet_at(bmdi, packet_addr,
                                                sizeof(hdr) + 4, &sz,
                                                sizeof(sz));
            if (r < 0)
                goto hex_dump;
            seq_printf(m, "  [LAUNCH_FUNC] f_id=%d size=%u\n", f_id, sz);
            if (sz > 0) {
                seq_printf(m, "    param (%u bytes):\n", sz);
                for (off = 0; off < sz; off += chunk) {
                    chunk =
                        (sz - off) >
                        sizeof(param_buf) ? sizeof(param_buf) : (sz - off);
                    r = bm84x6_shmem_read_packet_at(bmdi, packet_addr,
                                                        sizeof(hdr) + 8 + off,
                                                        param_buf, chunk);
                    if (r < 0)
                        break;
                    shmpkt_print_hex_at(m, "      ", param_buf, chunk, off);
                }
            }
            break;
        }
    case API_ID_A53LITE_GETR_VERSION:
        seq_printf(m, "  [GET_VERSION] (no payload)\n");
        break;
    default:
      hex_dump:{
            size_t dump_len = hdr.api_size > 256 ? 256 : hdr.api_size;
            u8 *buf = NULL;
            if (dump_len > 0) {
                buf = kmalloc(dump_len, GFP_KERNEL);
                if (buf) {
                    r = bm84x6_shmem_read_packet_at(bmdi, packet_addr,
                                                        sizeof(hdr), buf,
                                                        dump_len);
                    if (r == 0) {
                        seq_printf(m,
                                   "  [unknown api_id 0x%x] payload (first %zu bytes):\n",
                                   hdr.api_id, dump_len);
                        shmpkt_print_hex(m, "    ", buf, dump_len);
                    }
                    kfree(buf);
                }
            }
            break;
        }
    }
    return 0;
}

static int dump_tpusys_reg(struct seq_file *m, int argc, char **argv)
{
    struct bm_device_info *bmdi = m->private;
    int i;
    seq_printf(m, "Dumping TPU SYS registers:\n");

    for (i = 0; i <= 0xf0; i += 4) {
        seq_printf(m, "addr=0x%x, val=0x%x\n",
                   TPU_SYS_REG_BASE + i, tpusys_read_32(bmdi,
                                                        TPU_SYS_REG_BASE + i));
    }

    return 0;
}

static int dump_txpsys_show(struct seq_file *m, void *v)
{
    struct bm_device_info *bmdi = m->private;
    const char *name;

    if (!bmdi) {
        seq_printf(m, "Device info not available\n");
        return 0;
    }
    name = tsh_get_proc_dir_name(bmdi);

    seq_printf(m, "=== Data Dump Commands ===\n");
    seq_printf(m, "Usage: echo <command> > /proc/%s/%s\n",
               name, PROC_DUMP_TXPSYS);
    seq_printf(m, "\nAvailable commands:\n");
    seq_printf(m,
               "tscalar <coreID>    - Dump scalar PC for specified core (hex)\n");
    seq_printf(m,
               "txpsys <coreID>     - Dump TXP system registers for core (hex)\n");
    seq_printf(m, "tpusys              - Dump TPU system registers\n");
    seq_printf(m, "\nExamples:\n");
    seq_printf(m, "echo \"tscalar 0\" > /proc/%s/%s\n",
               name, PROC_DUMP_TXPSYS);
    seq_printf(m, "echo \"txpsys 0\" > /proc/%s/%s\n",
               name, PROC_DUMP_TXPSYS);
    seq_printf(m, "echo \"tpusys\" > /proc/%s/%s\n",
               name, PROC_DUMP_TXPSYS);
    return 0;
}

static int dump_txpsys_open(struct inode *inode, struct file *file)
{
    return single_open(file, dump_txpsys_show, PDE_DATA(inode));
}

static ssize_t dump_txpsys_write(struct file *file, const char __user * buffer,
                                 size_t count, loff_t * ppos)
{
    char *cmd_buf, *token;
    char *argv[10] = { 0 };
    int argc = 0;
    int ret = 0;
    char *str_ptr;
    int coreID;
    int i;
    u64 pc;
    struct bm_device_info *bmdi = PDE_DATA(file_inode(file));

    if (*ppos != 0) {
        return -EINVAL;
    }

    cmd_buf = kzalloc(count + 1, GFP_KERNEL);
    if (!cmd_buf) {
        return -ENOMEM;
    }

    if (copy_from_user(cmd_buf, buffer, count)) {
        kfree(cmd_buf);
        return -EFAULT;
    }

    cmd_buf[count] = '\0';

    if (count > 0 && cmd_buf[count - 1] == '\n') {
        cmd_buf[count - 1] = '\0';
    }

    printk(KERN_INFO "Received command: %s\n", cmd_buf);

    str_ptr = cmd_buf;

    while ((token = strsep(&str_ptr, " ")) != NULL && argc < 10) {
        if (strlen(token) > 0) {
            argv[argc++] = token;
        }
    }

    if (argc < 1) {
        printk(KERN_ERR "Error: No command specified\n");
        ret = -EINVAL;
        goto out;
    }

    if (strcmp(argv[0], "tscalar") == 0) {
        coreID = (argc < 2) ? 0 : simple_strtoul(argv[1], NULL, 16);
        pc = ((u64)
              tpusys_read_32(bmdi,
                             TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID +
                             TPU_SYS_SCALAR_RETIRE_PC_H) << 32)
            | tpusys_read_32(bmdi,
                             TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID +
                             TPU_SYS_SCALAR_RETIRE_PC_L);
        printk(KERN_INFO "tscalar%d_pc=0x%llx\n", coreID, pc);
    } else if (strcmp(argv[0], "txpsys") == 0) {
        coreID = (argc < 2) ? 0 : simple_strtoul(argv[1], NULL, 16);
        printk(KERN_INFO "Dumping TXP SYS registers for core %d:\n", coreID);
        for (i = 0; i <= 0xcc; i += 4) {
            u32 val = tpusys_read_32(bmdi,
                                     TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID +
                                     i);
            printk(KERN_INFO "addr=0x%x, val=0x%x\n",
                   (unsigned int)(TXP_SYS0_BASE + TXP_SYS_OFFSET * coreID + i),
                   val);
        }
    } else if (strcmp(argv[0], "tpusys") == 0) {
        printk(KERN_INFO "Dumping TPU SYS registers:\n");
        for (i = 0; i <= 0xf0; i += 4) {
            u32 val = tpusys_read_32(bmdi, TPU_SYS_REG_BASE + i);
            printk(KERN_INFO "addr=0x%x, val=0x%x\n",
                   (unsigned int)(TPU_SYS_REG_BASE + i), val);
        }
    } else {
        printk(KERN_ERR "Unknown dump command: %s\n", argv[0]);
        ret = -EINVAL;
    }

  out:
    kfree(cmd_buf);
    return ret ? ret : count;
}


static int stop_global_test(void)
{
    int ret = 0;

    mutex_lock(&thread_mutex);

    if (!global_test_thread.running || !global_test_thread.thread) {
        mutex_unlock(&thread_mutex);
        pr_info("Global test thread is not running\n");
        return 0;
    }

    global_test_thread.running = false;
    mutex_unlock(&thread_mutex);

    ret = kthread_stop(global_test_thread.thread);

    mutex_lock(&thread_mutex);
    global_test_thread.thread = NULL;
    global_test_thread.bmdi = NULL;
    mutex_unlock(&thread_mutex);

    pr_info("Global test thread stopped\n");
    return ret;
}

static int start_global_test(struct bm_device_info *bmdi)
{
    int ret = 0;

    if (!bmdi) {
        pr_err("Invalid device pointer passed to start_global_test\n");
        return -EINVAL;
    }

    mutex_lock(&thread_mutex);

    if (global_test_thread.running) {
        mutex_unlock(&thread_mutex);
        pr_info("Global test thread is already running\n");
        return 0;
    }

    global_test_thread.thread = kthread_create(test_wakeup_consumer_thread,
                                               bmdi, "bmdev_global_wakeup");

    if (IS_ERR(global_test_thread.thread)) {
        ret = PTR_ERR(global_test_thread.thread);
        global_test_thread.thread = NULL;
        mutex_unlock(&thread_mutex);
        pr_err("Failed to create global test thread: %d\n", ret);
        return ret;
    }

    global_test_thread.running = true;
    global_test_thread.wakeup_count++;
    global_test_thread.bmdi = bmdi;
    mutex_unlock(&thread_mutex);

    wake_up_process(global_test_thread.thread);
    pr_info("Global test thread started for device %p\n", bmdi);
    return 0;
}

static const char *kernel_fifo_api_name(u32 api_id)
{
    switch (api_id) {
    case API_ID_A53LITE_LOAD_LIB:
        return "LOAD_LIB";
    case API_ID_A53LITE_GET_FUNC:
        return "GET_FUNC";
    case API_ID_A53LITE_LAUNCH_FUNC:
        return "LAUNCH_FUNC";
    case API_ID_A53LITE_UNLOAD_LIB:
        return "UNLOAD_LIB";
    case API_ID_A53LITE_GETR_VERSION:
        return "GET_VERSION";
    default:
        return "UNKNOWN";
    }
}

static const char *kernel_fifo_mode_name(u8 mode)
{
    switch (mode) {
    case TSH_SYNC_MODE:
        return "SYNC";
    case TSH_ASYNC_MODE:
        return "ASYNC";
    case TSH_BYPASS_MODE:
        return "BYPASS";
    default:
        return "UNKNOWN";
    }
}

void bmdev_kernel_fifo_record(struct bm_device_info *bmdi, u32 api_id, u32 api_seq,
                              int hwq_idx, u32 hwq_packet, u64 outbox_packet,
                              bool is_outbox)
{
    struct kernel_fifo_record *rec;
    union tsh_hwq_packet hp;
    union tsh_outbox_packet op;
    unsigned long flags;

    if (!bmdi || bmdi->cinfo.chip_id != BM_CHIP_ID_84X6)
        return;

    spin_lock_irqsave(&kernel_fifo_lock, flags);
    rec = &kernel_fifo_ring[kernel_fifo_head % KERNEL_FIFO_RECORD_COUNT];
    kernel_fifo_head++;
    kernel_fifo_total++;

    rec->jiffies = jiffies;
    rec->seq = kernel_fifo_total;
    rec->api_id = api_id;
    rec->api_seq = api_seq;
    rec->hwq_idx = (u8) hwq_idx;
    rec->is_outbox = is_outbox ? 1 : 0;
    rec->tsh_mode = (u8) tsh_getmode(bmdi);
    rec->reserved = 0;

    if (is_outbox) {
        op.packet = outbox_packet;
        rec->packet_addr = (u32) op.fields.packet_addr;
        rec->group_num = (u32) op.fields.group_num;
        rec->block_num = (u32) op.fields.block_num;
        rec->group_idx = (u32) op.fields.group_idx;
        rec->start_core_idx = (u32) op.fields.start_core_idx;
        rec->send_core_num = (u32) op.fields.send_core_num;
        rec->phase = (u32) op.fields.phase;
        rec->hwq_packet = 0;
        rec->outbox_packet = outbox_packet;
    } else {
        hp.packet = hwq_packet;
        rec->packet_addr = hp.packet_addr;
        rec->group_num = hp.group_num;
        rec->block_num = hp.block_num;
        rec->group_idx = 0;
        rec->start_core_idx = 0;
        rec->send_core_num = 0;
        rec->phase = 0;
        rec->hwq_packet = hwq_packet;
        rec->outbox_packet = 0;
    }
    spin_unlock_irqrestore(&kernel_fifo_lock, flags);
}

static int kernel_fifo_show(struct seq_file *m, void *v)
{
    u32 slots[KERNEL_FIFO_RECORD_COUNT];
    u32 total, head, count, i;
    unsigned long flags;

    spin_lock_irqsave(&kernel_fifo_lock, flags);
    total = kernel_fifo_total;
    head = kernel_fifo_head;
    count = min_t(u32, total, KERNEL_FIFO_RECORD_COUNT);
    for (i = 0; i < count; i++)
        slots[i] = (head - count + i) % KERNEL_FIFO_RECORD_COUNT;
    spin_unlock_irqrestore(&kernel_fifo_lock, flags);

    seq_printf(m, "=== kernel fifo history ===\n");
    seq_printf(m, "total=%u showing=%u capacity=%u\n\n",
               total, count, KERNEL_FIFO_RECORD_COUNT);

    for (i = 0; i < count; i++) {
        struct kernel_fifo_record rec;

        spin_lock_irqsave(&kernel_fifo_lock, flags);
        rec = kernel_fifo_ring[slots[i]];
        spin_unlock_irqrestore(&kernel_fifo_lock, flags);

        seq_printf(m, "[%03u] seq=%u age=%ums api=0x%x(%s) api_seq=%u mode=%s hwq=%u\n",
                   i, rec.seq, jiffies_to_msecs(jiffies - rec.jiffies),
                   rec.api_id, kernel_fifo_api_name(rec.api_id),
                   rec.api_seq, kernel_fifo_mode_name(rec.tsh_mode),
                   rec.hwq_idx);
        if (rec.is_outbox) {
            seq_printf(m,
                       "      outbox packet=0x%016llx packet_addr=0x%x group=%u block=%u group_idx=%u\n",
                       rec.outbox_packet, rec.packet_addr, rec.group_num,
                       rec.block_num, rec.group_idx);
            seq_printf(m,
                       "      start_core=%u send_core_num=%u phase=%u\n",
                       rec.start_core_idx, rec.send_core_num, rec.phase);
        } else {
            seq_printf(m,
                       "      hwq packet=0x%08x packet_addr=0x%x group=%u block=%u\n",
                       rec.hwq_packet, rec.packet_addr, rec.group_num,
                       rec.block_num);
        }
        seq_printf(m, "\n");
    }

    return 0;
}

static int kernel_fifo_open(struct inode *inode, struct file *file)
{
    return single_open(file, kernel_fifo_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS kernel_fifo_fops = {
    BM_PROC_OPEN = kernel_fifo_open,
    BM_PROC_READ = seq_read,
    BM_PROC_LLSEEK = seq_lseek,
    BM_PROC_RELEASE = single_release,
};

static int kernel_info_show(struct seq_file *m, void *v)
{
    int i;
    struct bm_device_info *bmdi = m->private;
    bm_kernel_info *pstBmKernelInfo;

    if (!bmdi) {
        seq_printf(m, "Device info not available\n");
        return 0;
    }
    pstBmKernelInfo = bmdi->pstBmKernelInfo;
    if (!pstBmKernelInfo) {
        seq_printf(m, "Kernel information not available\n");
        return 0;
    }

    seq_printf(m, "=== BMDev Kernel Information ===\n");
    seq_printf(m, "  dev_index: %d\n", bmdi->dev_index);
    seq_printf(m, "Global max_depth: %d\n", pstBmKernelInfo->max_depth);
    seq_printf(m, "Number of HWQs (NUM_HWQS): %d\n", NUM_HWQS);
    seq_printf(m, "  irq_tasklet state: %lu\n",
               pstBmKernelInfo->tsh_irq_tasklet.state);
    seq_printf(m, "  irq_handle_cnt   : %u\n",
               pstBmKernelInfo->irq_handle_cnt);
    seq_printf(m, "  irq_fallback_cnt : %u\n",
               pstBmKernelInfo->irq_fallback_cnt);
    seq_printf(m, "\n");

    for (i = 0; i < NUM_HWQS && i < MAX_HWQ_INDEX; i++) {
        bm_hw_q_info *hwq = &pstBmKernelInfo->hw_q_info[i];

        seq_printf(m, "--- HWQ Index: %d ---\n", i);
        seq_printf(m, "  hwq_idx: %d\n", hwq->hwq_idx);
        seq_printf(m, "  hwq_load_counters: %d\n", hwq->hwq_load_counters);
        seq_printf(m, "  free_slots: %d\n", hwq->free_slots);
        seq_printf(m, "  pushed_cnt: %d\n", hwq->pushed_cnt);
        seq_printf(m, "  total done: %lld\n", hwq->last_done_count);
        seq_printf(m, "  filename: %s\n", hwq->filename);
        seq_printf(m, "\n");
    }

    return 0;
}

static int wakeup_control_show(struct seq_file *m, void *v)
{
    int i;
    int free_count;
    bool running;
    unsigned long wakeup_count;
    struct bm_device_info *bmdi = m->private;

    mutex_lock(&thread_mutex);
    running = global_test_thread.running;
    wakeup_count = global_test_thread.wakeup_count;
    mutex_unlock(&thread_mutex);

    seq_printf(m, "=== Global Test Thread Status ===\n");
    seq_printf(m, "Status: %s\n", running ? "RUNNING" : "STOPPED");
    seq_printf(m, "Wakeup Count: %lu\n", wakeup_count);
    seq_printf(m, "Device Pointer: %p\n", global_test_thread.bmdi);
    seq_printf(m, "\n");

    seq_printf(m, "HWQ Index | Free Slots\n");
    seq_printf(m, "---------|------------\n");

    if (bmdi) {
        for (i = 0; i < MAX_HWQ_INDEX; i++) {
            free_count = bmdev_kernel_kfifo_free_count(bmdi, i);
            if (free_count < 0) {
                free_count = 0;
            }
            seq_printf(m, "    %d     | %d\n", i, free_count);
        }
    }

    seq_printf(m, "\nUsage examples:\n");
    seq_printf(m, "echo start > /proc/%s/%s  # Start global test\n",
               tsh_get_proc_dir_name(bmdi), PROC_WAKEUP_CONTROL);
    seq_printf(m, "echo stop  > /proc/%s/%s  # Stop global test\n",
               tsh_get_proc_dir_name(bmdi), PROC_WAKEUP_CONTROL);

    return 0;
}


static const struct BM_PROC_FILE_OPS dump_txpsys_fops = {
    BM_PROC_OPEN = dump_txpsys_open,
    BM_PROC_READ = seq_read,
    BM_PROC_WRITE = dump_txpsys_write,
    BM_PROC_LLSEEK = seq_lseek,
    BM_PROC_RELEASE = single_release,
};

static int shmpacket_show(struct seq_file *m, void *v)
{
    struct bm_device_info *bmdi = m->private;

    if (shmpacket_offset < 0) {
        seq_printf(m, "=== SHM Packet Dump ===\n");
        seq_printf(m, "Usage: echo <packet_addr> > /proc/%s/%s\n",
                   tsh_get_proc_dir_name(bmdi), PROC_SHM_PACKET);
        seq_printf(m, "Then:  cat /proc/%s/%s\n",
                   tsh_get_proc_dir_name(bmdi), PROC_SHM_PACKET);
        seq_printf(m,
                   "\nExample: echo 0 > /proc/%s/%s  (packet_addr can be 0)\n",
                   tsh_get_proc_dir_name(bmdi), PROC_SHM_PACKET);
        return 0;
    }
    return dump_shmpacket(m, (u32) shmpacket_offset);
}

static int shmpacket_open(struct inode *inode, struct file *file)
{
    return single_open(file, shmpacket_show, PDE_DATA(inode));
}

static ssize_t shmpacket_write(struct file *file, const char __user * buffer,
                               size_t count, loff_t * ppos)
{
    char buf[SHMPACKET_CMD_BUF_SIZE];
    char *p;
    u32 addr;
    int ret;

    if (count >= SHMPACKET_CMD_BUF_SIZE)
        return -EINVAL;
    if (copy_from_user(buf, buffer, count))
        return -EFAULT;
    buf[count] = '\0';
    for (p = buf; *p == '\n' || *p == ' ' || *p == '\t'; p++) ;
    if (count > 0 && buf[count - 1] == '\n')
        buf[count - 1] = '\0';
    if (*p == '\0') {
        shmpacket_offset = -1;
        return count;
    }
    ret = kstrtou32(p, 16, &addr);
    if (ret != 0) {
        shmpacket_offset = -1;
        return -EINVAL;
    }
    shmpacket_offset = (int)addr;
    return count;
}

static int shmpacket_release(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_READ)
        shmpacket_offset = -1;
    return single_release(inode, file);
}

static const struct BM_PROC_FILE_OPS shmpacket_fops = {
    BM_PROC_OPEN = shmpacket_open,
    BM_PROC_READ = seq_read,
    BM_PROC_WRITE = shmpacket_write,
    BM_PROC_LLSEEK = seq_lseek,
    BM_PROC_RELEASE = shmpacket_release,
};

static int kernel_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, kernel_info_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS kernel_info_fops = {
    BM_PROC_OPEN = kernel_info_open,
    BM_PROC_READ = seq_read,
    BM_PROC_LLSEEK = seq_lseek,
    BM_PROC_RELEASE = single_release,
};

static int wakeup_control_open(struct inode *inode, struct file *file)
{
    return single_open(file, wakeup_control_show, PDE_DATA(inode));
}

static ssize_t wakeup_control_write(struct file *file,
                                    const char __user * buffer, size_t count,
                                    loff_t * ppos)
{
    char cmd[32];
    int ret;
    struct bm_device_info *bmdi = PDE_DATA(file_inode(file));

    if (!bmdi) {
        pr_err("No valid device pointer available\n");
        return -ENODEV;
    }

    if (count >= sizeof(cmd)) {
        pr_err("Command too long: %zu bytes\n", count);
        return -EINVAL;
    }

    if (copy_from_user(cmd, buffer, count)) {
        pr_err("Failed to copy command from user space\n");
        return -EFAULT;
    }

    cmd[count] = '\0';

    if (count > 0 && cmd[count - 1] == '\n') {
        cmd[count - 1] = '\0';
    }

    if (strcmp(cmd, "start") == 0) {
        ret = start_global_test(bmdi);
        if (ret) {
            pr_err("Failed to start global test: %d\n", ret);
            return ret;
        }
    } else if (strcmp(cmd, "stop") == 0) {
        ret = stop_global_test();
        if (ret) {
            pr_err("Failed to stop global test: %d\n", ret);
            return ret;
        }
    } else {
        pr_err("Unknown command: %s\n", cmd);
        pr_err("Usage: start | stop\n");
        return -EINVAL;
    }

    pr_info("Command '%s' executed successfully\n", cmd);
    return count;
}

static const struct BM_PROC_FILE_OPS wakeup_control_fops = {
    BM_PROC_OPEN = wakeup_control_open,
    BM_PROC_READ = seq_read,
    BM_PROC_WRITE = wakeup_control_write,
    BM_PROC_LLSEEK = seq_lseek,
    BM_PROC_RELEASE = single_release,
};

int bmdev_test_proc_init(struct bm_device_info *bmdi)
{
    char dir_name[64];
    const char *display_name;

    memset(&global_test_thread, 0, sizeof(global_test_thread));
    global_test_thread.running = false;
    global_test_thread.thread = NULL;
    global_test_thread.bmdi = NULL;

    mutex_init(&thread_mutex);

    /* resolve the display name for help messages */
    if (bmdi && bmdi->cinfo.device && bmdi->cinfo.device->of_node)
        display_name = bmdi->cinfo.device->of_node->full_name;
    else
        display_name = PROC_DIR_DEFAULT;

    if (bmdi_folder) {
        bmdi->tsh_proc_dir = bmdi_folder;
        bmdi->tsh_proc_dir_owned = false;
    } else {
        /* PCIe multi-chip: use per-chip name (bmtpu0, bmtpu1) to avoid
         * /proc/bmtpu already-registered clash on second probe */
        if (bmdi && bmdi->cinfo.device && !bmdi->cinfo.device->of_node) {
            snprintf(dir_name, sizeof(dir_name), "%s%d",
                     PROC_DIR_DEFAULT, bmdi->dev_index);
            display_name = dir_name;
        }
        /* remove stale entries from a previous load */
        remove_proc_subtree(display_name, NULL);
        bmdi->tsh_proc_dir = proc_mkdir(display_name, NULL);
        if (!bmdi->tsh_proc_dir) {
            pr_warn("proc_mkdir %s failed, skipping proc init\n",
                    display_name);
            return 0;
        }
        bmdi->tsh_proc_dir_owned = true;
    }

    if (!proc_create_data(PROC_WAKEUP_CONTROL, 0666, bmdi->tsh_proc_dir,
                          &wakeup_control_fops, bmdi)) {
        pr_err("Failed to create proc file %s\n", PROC_WAKEUP_CONTROL);
        if (bmdi->tsh_proc_dir_owned)
            proc_remove(bmdi->tsh_proc_dir);
        bmdi->tsh_proc_dir = NULL;
        return -ENOMEM;
    }

    if (!proc_create_data(PROC_KERNEL_INFO, 0444, bmdi->tsh_proc_dir,
                          &kernel_info_fops, bmdi)) {
        pr_err("Failed to create proc file %s\n", PROC_KERNEL_INFO);
        if (bmdi->tsh_proc_dir_owned)
            proc_remove(bmdi->tsh_proc_dir);
        bmdi->tsh_proc_dir = NULL;
        return -ENOMEM;
    }

    if (!proc_create_data(PROC_KERNEL_FIFO, 0444, bmdi->tsh_proc_dir,
                          &kernel_fifo_fops, bmdi)) {
        pr_err("Failed to create proc file %s\n", PROC_KERNEL_FIFO);
        if (bmdi->tsh_proc_dir_owned)
            proc_remove(bmdi->tsh_proc_dir);
        bmdi->tsh_proc_dir = NULL;
        return -ENOMEM;
    }

    if (!proc_create_data(PROC_DUMP_TXPSYS, 0666, bmdi->tsh_proc_dir,
                          &dump_txpsys_fops, bmdi)) {
        pr_err("Failed to create proc file %s\n", PROC_DUMP_TXPSYS);
        if (bmdi->tsh_proc_dir_owned)
            proc_remove(bmdi->tsh_proc_dir);
        bmdi->tsh_proc_dir = NULL;
        return -ENOMEM;
    }

    if (!proc_create_data(PROC_SHM_PACKET, 0666, bmdi->tsh_proc_dir,
                          &shmpacket_fops, bmdi)) {
        pr_err("Failed to create proc file %s\n", PROC_SHM_PACKET);
        if (bmdi->tsh_proc_dir_owned)
            proc_remove(bmdi->tsh_proc_dir);
        bmdi->tsh_proc_dir = NULL;
        return -ENOMEM;
    }

    pr_info("BMDev proc interface created at /proc/%s/\n",
            display_name);
    pr_info("  - Control interface: /proc/%s/%s\n",
            display_name, PROC_WAKEUP_CONTROL);
    pr_info("  - Kernel info: /proc/%s/%s\n",
            display_name, PROC_KERNEL_INFO);
    pr_info("  - Kernel fifo: /proc/%s/%s\n",
            display_name, PROC_KERNEL_FIFO);
    pr_info("  - txpsys dump: /proc/%s/%s\n",
            display_name, PROC_DUMP_TXPSYS);
    pr_info("  - shmpacket: /proc/%s/%s\n", display_name,
            PROC_SHM_PACKET);

    return 0;
}

void bmdev_test_proc_exit(struct bm_device_info *bmdi)
{
    pr_info("Cleaning up BMDev test proc interface\n");

    if (global_test_thread.running) {
        pr_info("Stopping global test thread during module exit\n");
        stop_global_test();
    }

    /* thread_mutex is global (shared by debug harness on all chips);
     * do not destroy it per-chip — destroying on chip0 then reusing on
     * chip1 would be a use-after-free. */

    if (bmdi->tsh_proc_dir && bmdi->tsh_proc_dir_owned) {
        proc_remove(bmdi->tsh_proc_dir);
    }
    bmdi->tsh_proc_dir = NULL;

    pr_info("BMDev test proc interface removed successfully\n");
}