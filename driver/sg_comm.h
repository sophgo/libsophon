#ifndef _SOPHGO_COMMUNICATION_H_
#define _SOPHGO_COMMUNICATION_H_
#include <linux/io.h>
#include <linux/spinlock.h>

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
#define COMM_SHM_CARDID_OFFSET 0x60

#define COMM_DMA_TX_BUFFER_SIZE 1*1024*1024
#define COMM_DMA_RX_BUFFER_SIZE 1*1024*1024

#define COMM_SHM_START_ADDR 0x0207F400
#define COMM_RECV_MSG_START_ADDR 0x207F700
#define COMM_SEND_MSG_START_ADDR 0X207F720
#define COMM_MSG_HEAD_OFFSET 0
#define COMM_MSG_TAIL_OFFSET 4
#define COMM_MSG_BUFFER_SIZE 0X400
#define COMM_SEND_MSG_BUFFER_START_ADDR 0X207F800
#define COMM_RECV_MSG_BUFFER_START_ADDR 0X207FC00


#pragma pack(1)
struct sgcpu_comm_data {
	char *data;
	int len;
};

#pragma pack()

struct comm_data {
	struct comm_data *next;
	void *data;
	unsigned int total_len;
	unsigned int processed_len;
};

struct sg_comm_info {
	atomic_t            carrier_on;
	atomic_t            buffer_ready;
	struct pt *         ring_buffer;

	struct task_struct *cd_task;
	struct pci_dev *pdev;
	struct mutex data_mutex;
	struct mutex msg_mutex;
};

int sg_comm_recv(struct bm_device_info *bmdi, void *data, int len);
int sg_comm_send(struct bm_device_info *bmdi, void *data, int len);
int sg_comm_msg_recv(struct bm_device_info *bmdi, void __user *data, int len);
int sg_comm_msg_send(struct bm_device_info *bmdi, void __user *data, int len);
void sg_comm_clear_queue(struct bm_device_info *bmdi);
int sg_comm_init(struct pci_dev *pdev, struct bm_device_info *bmdi);
void sg_comm_deinit(struct bm_device_info *bmdi);
#endif
