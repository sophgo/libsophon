#ifndef __BM_DRIVER_VNIC_H_
#define __BM_DRIVER_VNIC_H_

#include <linux/io.h>
#include <linux/spinlock.h>
#include "linux/netdevice.h"

#define ETH_MTU 65536
#define VETH_DEVICE_NAME "veth"

#define GP_REG15_SET 0x198

#define HOST_READY_FLAG 0x01234567
#define DEVICE_READY_FLAG 0x89abcde

#define VETH_TX_QUEUE_PHY 0x0
#define VETH_TX_QUEUE_LEN 0x8
#define VETH_TX_QUEUE_HEAD 0x10
#define VETH_TX_QUEUE_TAIL 0x14

#define VETH_RX_QUEUE_PHY 0x0
#define VETH_RX_QUEUE_LEN 0x8
#define VETH_RX_QUEUE_HEAD 0x10
#define VETH_RX_QUEUE_TAIL 0x14

#define VETH_HANDSHAKE_REG 0x40
#define VETH_IPADDRESS_REG 0x44
#define VETH_A53_STATE_REG 0x48
#define VETH_GATE_ADDRESS_REG 0x4c
#define VETH_RESET_REG 0x50

#define VETH_SHM_START_ADDR_1684 0x0201be80
#define VETH_SHM_START_ADDR_1684X 0x101fb400

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long long dma_addr_t;

struct eth_reg_phy {
    u32 eth_shm_phy;
};

struct eth_dev_info {
    u32             index;
    bool            init;
    struct pci_dev *pci_dev;

    struct napi_struct napi;
    struct net_device *ndev;

    struct task_struct *cd_task;
    atomic_t            carrier_on;
    atomic_t            buffer_ready;
    struct pt *         ring_buffer;

    struct eth_reg_phy reg_phy;
};
struct bm_device_info;
int  eth_register_napi(struct eth_dev_info *info);
void eth_unregister_napi(struct eth_dev_info *info);
void bm_eth_request_irq(struct bm_device_info *bmdi);
void bm_eth_free_irq(struct bm_device_info *bmdi);
int bmdrv_veth_init(struct bm_device_info *bmdi, struct pci_dev *pdev);
void bmdrv_veth_deinit(struct bm_device_info *bmdi, struct pci_dev *pdev);
#endif
