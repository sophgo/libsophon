#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/sockios.h>
#include <linux/tcp.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <net/ipv6.h>

#include "bm1684_reg.h"
#include "bm_cdma.h"
#include "bm_common.h"
#include "bm_io.h"
#include "bm_memcpy.h"
#include "bm_napi.h"
#include "bm_pt.h"
#include "bm_thread.h"
#include "bm_timer.h"
#define HOST_TX_LEN (1 * 1024 * 1024)
#define HOST_RX_LEN (1 * 1024 * 1024)

/*
 * PT is short for package transmitter
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

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

static int ring_buffer_init(struct net_device *ndev) {
    struct eth_dev_info *  info = *((struct eth_dev_info **)netdev_priv(ndev));
    struct pt *            pt;
    struct pci_dev *       pdev = info->pci_dev;
    struct bm_device_info *bmdi =
        container_of(info, struct bm_device_info, vir_eth);

    pt = pt_init(&pdev->dev, info->reg_phy.eth_shm_phy, HOST_TX_LEN, HOST_RX_LEN, bmdi);
    if (pt)
        info->ring_buffer = pt;

    return pt ? 0 : -ENOMEM;
}

static int ring_buffer_send(struct net_device *ndev, struct sk_buff *skb) {
    u32                    value = 0;
    struct eth_dev_info *  info  = *((struct eth_dev_info **)netdev_priv(ndev));
    struct bm_device_info *bmdi =
        container_of(info, struct bm_device_info, vir_eth);

    if (!atomic_read(&info->buffer_ready))
        return -EAGAIN;

    pt_load_tx(info->ring_buffer);
    if (pt_send(info->ring_buffer, skb->data, skb->len) < 0)
        return -ENOMEM;

    pt_store_tx(info->ring_buffer);
    value = top_reg_read(bmdi, TOP_MISC_GP_REG15_STS_OFFSET);

    while ((value & (1<<15)) != 0)
    {
        value = top_reg_read(bmdi, TOP_MISC_GP_REG15_STS_OFFSET);
    }

    value = top_reg_read(bmdi, TOP_MISC_GP_REG15_STS_OFFSET);

    while ((value & 0x1) != 0)
    {
        value = top_reg_read(bmdi, TOP_MISC_GP_REG15_STS_OFFSET);
    }

    value = (1 << 15);
    top_reg_write(bmdi, TOP_MISC_GP_REG15_SET_OFFSET, value);

    return 0;
}

static struct sk_buff *ring_buffer_recv(struct net_device *ndev) {
    struct eth_dev_info *info = *((struct eth_dev_info **)netdev_priv(ndev));
    int                  skb_len, ret;
    struct sk_buff *     skb = NULL;

    if (!atomic_read(&info->buffer_ready))
        return NULL;

    skb_len = pt_pkg_len(info->ring_buffer);

    if (skb_len <= 0)
        goto exit;

    skb = netdev_alloc_skb(ndev, skb_len + NET_IP_ALIGN);
    if (!skb)
        goto exit;

    skb_reserve(skb, NET_IP_ALIGN);  // align IP on 16B boundary
    skb_reset_mac_header(skb);
    ret = pt_recv(info->ring_buffer, skb_put(skb, skb_len), skb_len);

    WARN_ON(ret != skb_len);

    if (ret < 0) {
        kfree_skb(skb);
        skb = NULL;
        goto exit;
    }

exit:
    return skb;
}

int bm1684_clear_ethirq(struct bm_device_info *bmdi) {
    int irq_status = 0;

    irq_status = top_reg_read(bmdi, TOP_MISC_GP_REG14_STS_OFFSET);

    if ((irq_status & 0x4) == 0x4) {
        u32 value = top_reg_read(bmdi, TOP_MISC_GP_REG14_CLR_OFFSET);
        value |= (1 << 2);
        top_reg_write(bmdi, TOP_MISC_GP_REG14_CLR_OFFSET, value);
        return 0;
    } else {
        return -1;
    }
}

void bmdrv_eth_irq_handler(struct bm_device_info *bmdi) {
    struct eth_dev_info *eth = &bmdi->vir_eth;

    bm1684_clear_ethirq(bmdi);
    napi_schedule(&eth->napi);
}
void bm_eth_request_irq(struct bm_device_info *bmdi) {
    bmdrv_submodule_request_irq(bmdi, 50, bmdrv_eth_irq_handler);
}
void bm_eth_free_irq(struct bm_device_info *bmdi) {
    bmdrv_submodule_free_irq(bmdi, 50);
}
static int eth_ndo_open(struct net_device *ndev) {
    struct eth_dev_info *info = *((struct eth_dev_info **)netdev_priv(ndev));
    netif_start_queue(ndev);
    napi_enable(&info->napi);
    napi_schedule(&info->napi);
    return 0;
}

static int eth_ndo_close(struct net_device *ndev) {
    struct eth_dev_info *info = *((struct eth_dev_info **)netdev_priv(ndev));
    netif_stop_queue(ndev);
    napi_disable(&info->napi);
    return 0;
}

static netdev_tx_t eth_ndo_start_xmit(struct sk_buff *   skb,
                                      struct net_device *ndev) {
    int ret;

    ret = ring_buffer_send(ndev, skb);
    if (ret < 0)
        return NETDEV_TX_BUSY;

    ndev->stats.tx_packets++;
    ndev->stats.tx_bytes += skb->len;
    dev_kfree_skb_any(skb);

    return NETDEV_TX_OK;
}
#if  (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0) || (LINUX_VERSION_CODE == KERNEL_VERSION(4,18,0) \
      && CENTOS_KERNEL_FIX >= 240))
void eth_ndo_tx_timeout(struct net_device *ndev, unsigned int txqueue)
#else
static void eth_ndo_tx_timeout(struct net_device *ndev)
#endif
{
    struct eth_dev_info *info = *((struct eth_dev_info **)netdev_priv(ndev));

    dev_info(&info->pci_dev->dev, "Tx timeout\n");
    ndev->stats.tx_errors++;
    netif_wake_queue(ndev);
}

static const struct net_device_ops eth_ndo_netdev_ops = {
    .ndo_open       = eth_ndo_open,
    .ndo_stop       = eth_ndo_close,
    .ndo_start_xmit = eth_ndo_start_xmit,
    .ndo_tx_timeout = eth_ndo_tx_timeout,
};

static int eth_ndo_napi_poll(struct napi_struct *napi, int budget) {
    struct eth_dev_info *info =
        *((struct eth_dev_info **)netdev_priv(napi->dev));
    struct sk_buff *skb;
    int             count = 0;

    if (!atomic_read(&info->buffer_ready)) {
        napi_complete(&info->napi);
        return 0;
    }

    pt_load_rx(info->ring_buffer);

    skb = ring_buffer_recv(info->ndev);
    if (!skb)
        goto done;

    while (skb) {
        skb->protocol  = eth_type_trans(skb, napi->dev);
        skb->dev       = info->ndev;
        skb->ip_summed = CHECKSUM_NONE;
        napi_gro_receive(napi, skb);
        napi->dev->stats.rx_packets++;
        napi->dev->stats.rx_bytes += skb->len;

        count++;
        if (count >= budget) {
            dev_info(&info->pci_dev->dev, "NAPI poll continue %d\n", count);
            goto next;
        }

        skb = ring_buffer_recv(info->ndev);
    };
done:
    napi_complete(&info->napi);
next:
    pt_store_rx(info->ring_buffer);

    return count;
}

static void eth_set_handshake(struct eth_dev_info *info, u32 value) {
    struct bm_device_info *bmdi =
        container_of(info, struct bm_device_info, vir_eth);
    if (bmdi->cinfo.chip_id == 0x1684)
        bm_write32(bmdi, VETH_SHM_START_ADDR_1684 + VETH_HANDSHAKE_REG, value);
    else if (bmdi->cinfo.chip_id == 0x1686)
        bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_HANDSHAKE_REG, value);
}
static u32 eth_get_handshake(struct eth_dev_info *info) {
    struct bm_device_info *bmdi =
        container_of(info, struct bm_device_info, vir_eth);
    if (bmdi->cinfo.chip_id == 0x1684)
        return bm_read32(bmdi, VETH_SHM_START_ADDR_1684 + VETH_HANDSHAKE_REG);
    else if (bmdi->cinfo.chip_id == 0x1686)
        return bm_read32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_HANDSHAKE_REG);
    return -1;
}
static void eth_set_a53ipaddress(struct eth_dev_info *info) {
    struct bm_device_info *bmdi =
        container_of(info, struct bm_device_info, vir_eth);

    pr_info("bmsophon%d dst ip: 192.192.%u.2\n", bmdi->dev_index, bmdi->dev_index);
    if (bmdi->cinfo.chip_id == 0x1684)
        bm_write32(bmdi, VETH_SHM_START_ADDR_1684 + VETH_IPADDRESS_REG, bmdi->dev_index);
    else if (bmdi->cinfo.chip_id == 0x1686) {
        bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_IPADDRESS_REG, 0xc0c00002);
        bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_GATE_ADDRESS_REG, 0);
        bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_RESET_REG, 0);
    }
}

static int eop_handshake(struct eth_dev_info *info) {
    int ret;
    int count;

    ret = ring_buffer_init(info->ndev);
    if (ret < 0) {
        dev_err(&info->pci_dev->dev, "create ring buffer failed %d\n", ret);
        return -ENOMEM;
    }
    eth_set_a53ipaddress(info);
    /* set rc ready flag */
    eth_set_handshake(info, HOST_READY_FLAG);

    count = 0;
    /* wait ep ready again */
    while (eth_get_handshake(info) != DEVICE_READY_FLAG) {
        count++;
        if (count == 65535)
            return 0;
    }

    atomic_set(&info->buffer_ready, 1);
    netif_carrier_on(info->ndev);
    dev_info(&info->pci_dev->dev, "net carrier on\n");

    return 0;
}

static int eth_ndo_kthread(void *arg) {
    struct eth_dev_info *info = (struct eth_dev_info *)arg;
    int                  ret  = 0;

    netif_carrier_off(info->ndev);
    eth_set_handshake(info, 0);
    while (1) {
        if (kthread_should_stop()) {
            break;
        }
        if (eth_get_handshake(info) == DEVICE_READY_FLAG) {
            if (atomic_cmpxchg(&info->carrier_on, 0, 1) == 0) {
                ret = eop_handshake(info);
                if (ret)
                    goto exit;
            }
        } else {
            if (atomic_cmpxchg(&info->carrier_on, 1, 0) == 1) {
                netif_carrier_off(info->ndev);
                dev_info(&info->pci_dev->dev, "net carrier off\n");
            }
        }

        if (atomic_read(&info->buffer_ready))
            msleep(1000);
        else
            msleep(20);
    }
exit:
    dev_info(&info->pci_dev->dev, "kthread exit\n");
    return ret;
}

int napi_handle_irq(struct eth_dev_info *info) {
    napi_schedule(&info->napi);
    return 0;
}

int eth_register_napi(struct eth_dev_info *info) {
    struct eth_dev_info **pinfo;
    struct net_device *   ndev;
    struct task_struct *  task;
    int                   ret = 0;

    ndev = alloc_etherdev(sizeof(struct veth_info *));
    ether_setup(ndev);
    ndev->mtu            = ETH_MTU;
    ndev->watchdog_timeo = 1 * HZ;
    ndev->netdev_ops     = &eth_ndo_netdev_ops;
    random_ether_addr((uint8_t *)ndev->dev_addr);
    sprintf(ndev->name, VETH_DEVICE_NAME "%d", info->index);

    pinfo      = netdev_priv(ndev);
    *pinfo     = info;
    info->ndev = ndev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 19)
    netif_napi_add(ndev, &info->napi, eth_ndo_napi_poll);
#else
    netif_napi_add(ndev, &info->napi, eth_ndo_napi_poll, NAPI_POLL_WEIGHT);
#endif
    register_netdev(ndev);

    task = kthread_run(
        eth_ndo_kthread, info, VETH_DEVICE_NAME "%d_kthread", info->index);
    if (IS_ERR(task)) {
        ret = PTR_ERR(task);
        dev_err(&info->pci_dev->dev, "create kthread failed %d\n", ret);
        unregister_netdev(info->ndev);
        free_netdev(info->ndev);
        goto exit;
    }
    info->cd_task = task;

exit:
    return ret;
}

void eth_unregister_napi(struct eth_dev_info *info) {
    struct bm_device_info *bmdi =
        container_of(info, struct bm_device_info, vir_eth);

    if (bmdi->status_bmcpu != BMCPU_IDLE)
        pt_uninit(info->ring_buffer);

    kthread_stop(info->cd_task);
    unregister_netdev(info->ndev);
    free_netdev(info->ndev);
}

int bmdrv_veth_init(struct bm_device_info *bmdi, struct pci_dev *pdev) {
    int                  ret;
    struct eth_dev_info *veth = &bmdi->vir_eth;

    veth->pci_dev = pdev;
    atomic_set(&veth->carrier_on, 0);
    atomic_set(&veth->buffer_ready, 0);

    if (bmdi->cinfo.chip_id == 0x1684)
        veth->reg_phy.eth_shm_phy = VETH_SHM_START_ADDR_1684;
    else if (bmdi->cinfo.chip_id == 0x1686)
        veth->reg_phy.eth_shm_phy = VETH_SHM_START_ADDR_1684X;
    veth->index               = bmdi->dev_index;

    ret = eth_register_napi(veth);
    bm_eth_request_irq(bmdi);

    return ret;
}

void bmdrv_veth_deinit(struct bm_device_info *bmdi, struct pci_dev *pdev) {
    u32 value;
    struct eth_dev_info *veth = &bmdi->vir_eth;

    netif_carrier_off(veth->ndev);
    atomic_set(&veth->buffer_ready, 0);
    msleep(5000);

    bm_eth_free_irq(bmdi);
    eth_unregister_napi(veth);
    value = top_reg_read(bmdi, TOP_MISC_GP_REG15_STS_OFFSET);
    value &= ~(1 << 15);
    top_reg_write(bmdi, TOP_MISC_GP_REG15_SET_OFFSET, value);
}
