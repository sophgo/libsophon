/*
 * PT is short for package transmitter
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include <bm_cdma.h>
#include <bm_common.h>
#include <bm_pt.h>
#include <bm_thread.h>
#include <bm_timer.h>

/* define this macro for buffer from dma pool (dma_alloc_noncoherent)
 * speed 1Gbits/s in this mode
 * unless, from system (kmalloc)
 * speed 5Gbits/s in this mode
 */
/* #define PT_BUF_FROM_DMA_POOL */
/* must multi of 4 */
#define PT_ALIGN 8

#define SHM_HOST_PHY_OFFSET 0x00
#define SHM_HOST_LEN_OFFSET 0x08
#define SHM_HOST_HEAD_OFFSET 0x10
#define SHM_HOST_TAIL_OFFSET 0x14
/* reserved 8 bytes */
#define SHM_HOST_RX_OFFSET 0x00
#define SHM_HOST_TX_OFFSET 0x20

#define SHM_SOC_TX_OFFSET SHM_HOST_RX_OFFSET
#define SHM_SOC_RX_OFFSET SHM_HOST_TX_OFFSET

/* CDMA bit definition */
#define BM1684_CDMA_RESYNC_ID_BIT 2
#define BM1684_CDMA_ENABLE_BIT 0
#define BM1684_CDMA_INT_ENABLE_BIT 3

#define BM1684_CDMA_EOD_BIT 2
#define BM1684_CDMA_PLAIN_DATA_BIT 6
#define BM1684_CDMA_OP_CODE_BIT 7

/*cdma register*/

#define CDMA_MAIN_CTRL 0x800
#define CDMA_INT_MASK 0x808
#define CDMA_INT_STATUS 0x80c
#define CDMA_SYNC_STAT 0x814
#define CDMA_CMD_ACCP0 0x878
#define CDMA_CMD_ACCP1 0x87c
#define CDMA_CMD_ACCP2 0x880
#define CDMA_CMD_ACCP3 0x884
#define CDMA_CMD_ACCP4 0x888
#define CDMA_CMD_ACCP5 0x88c
#define CDMA_CMD_ACCP6 0x890
#define CDMA_CMD_ACCP7 0x894
#define CDMA_CMD_ACCP8 0x898
#define CDMA_CMD_ACCP9 0x89c
#define CDMA_CMD_9F8 0x9f8

#define TOP_CDMA_LOCK 0x040

static void host_queue_init(struct host_queue *    q,
                            u32                    shm,
                            struct bm_device_info *bmdi) {
    q->phy  = (shm + SHM_HOST_PHY_OFFSET);
    q->len  = (shm + SHM_HOST_LEN_OFFSET);
    q->head = (shm + SHM_HOST_HEAD_OFFSET);
    q->tail = (shm + SHM_HOST_TAIL_OFFSET);
    q->bmdi = bmdi;
    q->vphy = bm_read64(q->bmdi, q->phy);
}

static inline void host_queue_set_phy(struct host_queue *q, u64 phy) {
    bm_write64(q->bmdi, q->phy, phy);
}

static inline u64 host_queue_get_phy(struct host_queue *q) {
    return bm_read64(q->bmdi, q->phy);
}

static inline void host_queue_set_len(struct host_queue *q, u64 len) {
    bm_write64(q->bmdi, q->len, len);
}

static inline u64 host_queue_get_len(struct host_queue *q) {
    u64 len = bm_read64(q->bmdi, q->len);
    return len;
}

static inline void host_queue_set_head(struct host_queue *q, u64 head) {
    bm_write32(q->bmdi, q->head, head);
}

static inline u32 host_queue_get_head(struct host_queue *q) {
    return bm_read32(q->bmdi, q->head);
}

static inline void host_queue_set_tail(struct host_queue *q, u32 tail) {
    bm_write32(q->bmdi, q->tail, tail);
}

static inline u32 host_queue_get_tail(struct host_queue *q) {
    return bm_read32(q->bmdi, q->tail);
}

/* bytes aligned */
struct pt *pt_init(struct device *        dev,
                   u32                    shm,
                   u32                    tx_len,
                   u32                    rx_len,
                   struct bm_device_info *bmdi) {
    struct pt *         pt;
    struct local_queue *lq;
    void *handle = devm_kzalloc(dev, sizeof(struct pt), GFP_KERNEL);
    if (!handle) {
        dev_err(dev, "allocate pt instance failed\n");
        return NULL;
    }

    pt = handle;

    pt->dev = dev;
    /* setup host buffer */
    pt->shm = shm;
    host_queue_init(&pt->tx.hq, shm + SHM_HOST_TX_OFFSET, bmdi);
    host_queue_init(&pt->rx.hq, shm + SHM_HOST_RX_OFFSET, bmdi);
    pt->tx.lq.len = tx_len;
    pt->rx.lq.len = rx_len;

    lq      = &pt->tx.lq;
    lq->cpu = dma_alloc_coherent(dev, lq->len, &lq->phy, GFP_KERNEL);
    if (!lq->cpu) {
        dev_err(dev, "allocate tx buffer with size %u failed\n", lq->len);
        goto free_handle;
    }

    lq      = &pt->rx.lq;
    lq->cpu = dma_alloc_coherent(dev, lq->len, &lq->phy, GFP_KERNEL);
    if (!lq->cpu) {
        dev_err(dev, "allocate rx buffer with size %u failed\n", lq->len);
        goto free_tx;
    }

    /* setup share memory */
    host_queue_set_phy(&pt->tx.hq, pt->tx.lq.phy);
    host_queue_set_len(&pt->tx.hq, pt->tx.lq.len);
    host_queue_set_head(&pt->tx.hq, 0);
    host_queue_set_tail(&pt->tx.hq, 0);

    host_queue_set_phy(&pt->rx.hq, pt->rx.lq.phy);
    host_queue_set_len(&pt->rx.hq, pt->rx.lq.len);
    host_queue_set_head(&pt->rx.hq, 0);
    host_queue_set_tail(&pt->rx.hq, 0);

    pt_info(pt);

    return handle;

free_tx:
    dma_free_coherent(dev, pt->tx.lq.len, pt->tx.lq.cpu, pt->tx.lq.phy);
free_handle:
    devm_kfree(dev, handle);

    return NULL;
}

void pt_uninit(struct pt *pt) {
    struct device *dev;
    if (pt == NULL)
        return ;

    dev = pt->dev;

    dma_free_coherent(dev, pt->rx.lq.len, pt->rx.lq.cpu, pt->rx.lq.phy);
    dma_free_coherent(dev, pt->tx.lq.len, pt->tx.lq.cpu, pt->tx.lq.phy);
    devm_kfree(dev, pt);
}

static unsigned int local_queue_used(struct local_queue *q) {
    unsigned int used;

    if (q->head >= q->tail)
        used = q->head - q->tail;
    else
        used = q->len - q->tail + q->head;

    return used;
}

static unsigned int local_queue_free(struct local_queue *q) {
    return q->len - local_queue_used(q) - 1;
}

static unsigned int __enqueue(
    void *queue, unsigned int qlen, unsigned int head, void *data, int len) {
    unsigned int tmp;

    tmp = min_t(unsigned int, len, qlen - head);
    memcpy((u8 *)queue + head, data, tmp);
    memcpy((u8 *)queue, (u8 *)data + tmp, len - tmp);

    head = (head + len) % qlen;

    return head;
}

static unsigned int comm__enqueue(void *queue, unsigned int qlen, unsigned int head, void __user *data, int len) {
    unsigned int tmp;
    int ret;

    tmp = min_t(unsigned int, len, qlen - head);
    ret = copy_from_user((u8 *)queue + head, data, tmp);
    ret |= copy_from_user((u8 *)queue, (u8 *)data + tmp, len - tmp);
    if (ret != 0)
    {
        pr_err("enqueue copy frmo user error!\n");
        return head;
    }

    head = (head + len) % qlen;
    return head;
}

static unsigned int __dequeue(
    void *queue, unsigned int qlen, unsigned int tail, void *data, int len) {
    unsigned int tmp;

    tmp = min_t(unsigned int, len, qlen - tail);
    memcpy(data, (u8 *)queue + tail, tmp);
    memcpy((u8 *)data + tmp, queue, len - tmp);

    tail = (tail + len) % qlen;

    return tail;
}

static unsigned int comm__dequeue(
    void *queue, unsigned int qlen, unsigned int tail, void __user *data, int len) {
    unsigned int tmp;
    int ret;

    tmp = min_t(unsigned int, len, qlen - tail);
    ret = copy_to_user(data, (u8 *)queue + tail, tmp);
    ret |= copy_to_user((u8 *)data + tmp, queue, len - tmp);
    if (ret != 0) {
        pr_err("enqueue copy to user error!\n");
        return tail;
    }

    tail = (tail + len) % qlen;

    return tail;
}

int pt_send(struct pt *pt, void *data, int len) {
    unsigned int        head;
    struct local_queue *q;
    u32                 pkg_len;
    unsigned int        total_len = round_up(len + sizeof(pkg_len), PT_ALIGN);

    q = &pt->tx.lq;

    if (local_queue_free(q) < total_len)
        return -ENOMEM;

    pkg_len = cpu_to_le32(len);

    /* load queue head */
    head = q->head;

    head = __enqueue(q->cpu, q->len, head, &pkg_len, sizeof(pkg_len));
    __enqueue(q->cpu, q->len, head, data, len);
    /* store queue head */
    q->head = (q->head + total_len) % q->len;

    return len;
}

int comm_pt_send(struct pt *pt, void __user *data, int len) {
    unsigned int        head;
    struct local_queue *q;
    u32                 pkg_len;
    unsigned int        total_len = round_up(len + sizeof(pkg_len), PT_ALIGN);

    q = &pt->tx.lq;

    if (local_queue_free(q) < total_len) {
        pr_err("total len is %d, empty len is %d\n", total_len, local_queue_free(q));
        return -ENOMEM;
    }

    pkg_len = cpu_to_le32(len);

    /* load queue head */
    head = q->head;

    head = __enqueue(q->cpu, q->len, head, &pkg_len, sizeof(pkg_len));
    comm__enqueue(q->cpu, q->len, head, data, len);
    /* store queue head */
    q->head = (q->head + total_len) % q->len;

    return len;
}

int pt_pkg_len(struct pt *pt) {
    struct local_queue *q = &pt->rx.lq;
    u32 *               pkg_len;

    if (q->head == q->tail)
        return -ENOMEM;

    pkg_len = (u32 *)((u8 *)q->cpu + q->tail);
    WARN((unsigned long)pkg_len & (sizeof(pkg_len) - 1),
         "rx queue not aligned correctly\n");

    return le32_to_cpu(*pkg_len);
}

int pt_recv(struct pt *pt, void *data, int len) {
    unsigned int        tail;
    unsigned int        total_len = round_up(len + sizeof(u32), PT_ALIGN);
    struct local_queue *q         = &pt->rx.lq;

    if (local_queue_used(q) < total_len)
        return -ENOMEM;

    tail = q->tail;
    /* skip 4 bytes header(package length) */
    tail = (tail + sizeof(u32)) % q->len;
    __dequeue(q->cpu, q->len, tail, data, len);

    q->tail = (q->tail + total_len) % q->len;

    return len;
}

int comm_pt_recv(struct pt *pt, void __user *data, int len) {
    unsigned int        tail;
    unsigned int        total_len = round_up(len + sizeof(u32), PT_ALIGN);
    struct local_queue *q         = &pt->rx.lq;

    if (local_queue_used(q) < total_len)
        return -ENOMEM;

    tail = q->tail;
    /* skip 4 bytes header(package length) */
    tail = (tail + sizeof(u32)) % q->len;
    comm__dequeue(q->cpu, q->len, tail, data, len);

    q->tail = (q->tail + total_len) % q->len;

    return len;
}

void pt_load_tx(struct pt *pt) {
    /* sync read pointer */
    pt->tx.lq.tail = host_queue_get_tail(&pt->tx.hq);
}

void pt_store_tx(struct pt *pt) {
    struct local_queue *   lq;
    struct host_queue *    hq;
    lq        = &pt->tx.lq;
    hq        = &pt->tx.hq;

    host_queue_set_head(hq, lq->head);
}

bool pt_load_rx(struct pt *pt) {
    u64                 host_phy;
    u32                 host_head;
    struct local_queue *lq;
    struct host_queue * hq;

    lq = &pt->rx.lq;
    hq = &pt->rx.hq;

    host_phy = hq->vphy;

    host_head = host_queue_get_head(hq);
    lq->head  = host_head;

    if (host_head == lq->tail)
        return false;

    return true;
}

void pt_store_rx(struct pt *pt) {
    host_queue_set_tail(&pt->rx.hq, pt->rx.lq.tail);
}

static void local_queue_info(struct device *dev, struct local_queue *q) {
    dev_info(dev,
             "local address %p, local phy 0x%llx, local length %u, head %u, "
             "tail %u\n",
             q->cpu,
             q->phy,
             q->len,
             q->head,
             q->tail);
}

static void host_queue_info(struct device *dev, struct host_queue *q) {

    dev_info(dev,
             "host phy 0x%016llx, length %llu, head %u, tail %u\n",
             host_queue_get_phy(q),
             host_queue_get_len(q),
             host_queue_get_head(q),
             host_queue_get_tail(q));
}

static void queue_info(struct device *dev, struct pt_queue *q) {
    local_queue_info(dev, &q->lq);
    host_queue_info(dev, &q->hq);
}

void pt_info(struct pt *pt) {
    dev_info(pt->dev, "------------- TX -------------\n");
    queue_info(pt->dev, &pt->tx);
    dev_info(pt->dev, "------------- RX -------------\n");
    queue_info(pt->dev, &pt->rx);
}
