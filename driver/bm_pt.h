#ifndef __BM_PT_H__
#define __BM_PT_H__

#include <linux/kernel.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef u64                dma_addr_t;

#define TOP_MISC_GP_REG14_STS_OFFSET 0x0b8
#define TOP_MISC_GP_REG14_SET_OFFSET 0x190
#define TOP_MISC_GP_REG14_CLR_OFFSET 0x194

#define TOP_MISC_GP_REG15_STS_OFFSET 0x0bc
#define TOP_MISC_GP_REG15_SET_OFFSET 0x198
#define TOP_MISC_GP_REG15_CLR_OFFSET 0x19c

struct host_queue {
    u64                    phy;
    u64                    len;
    u32                    head;
    u32                    tail;
    u64                    vphy;
    struct bm_device_info *bmdi;
};

struct local_queue {
    u32        len;
    u32        head;
    u32        tail;
    dma_addr_t phy;
    u8 *       cpu;
};

// ring queue
struct pt_queue {
    struct local_queue lq;
    struct host_queue  hq;
};

// package transmitter
struct pt {
    struct device * dev;
    struct pt_queue tx, rx;
    u32             shm;
};

/* load configuration from share memory */
struct pt *pt_init(struct device *        dev,
                   u32                    shm,
                   u32                    tx_len,
                   u32                    rx_len,
                   struct bm_device_info *bmdi);
int        pt_send(struct pt *pt, void *data, int len);
int        pt_recv(struct pt *pt, void *data, int len);
void       pt_load_tx(struct pt *pt);
bool       pt_load_rx(struct pt *pt);
void       pt_store_tx(struct pt *pt);
void       pt_store_rx(struct pt *pt);
int        pt_pkg_len(struct pt *pt);
void       pt_uninit(struct pt *pt);
void       pt_info(struct pt *pt);
void       pt_store_rx(struct pt *pt);
int comm_pt_send(struct pt *pt, void __user *data, int len);
int comm_pt_recv(struct pt *pt, void __user *data, int len);

#endif
