#include "bm_common.h"
#include "bm_pt.h"
#include "sg_comm.h"

#define PT_ALIGN 8

extern void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
		void __iomem **bar_vaddr, u32 *offset);

void sg_comm_clear_queue(struct bm_device_info *bmdi)
{
	bm_write32(bmdi, COMM_RECV_MSG_START_ADDR + COMM_MSG_HEAD_OFFSET, 0);
	bm_write32(bmdi, COMM_RECV_MSG_START_ADDR + COMM_MSG_TAIL_OFFSET, 0);
	bm_write32(bmdi, COMM_SEND_MSG_START_ADDR + COMM_MSG_HEAD_OFFSET, 0);
	bm_write32(bmdi, COMM_SEND_MSG_START_ADDR + COMM_MSG_TAIL_OFFSET, 0);
}

static void comm_set_handshake(struct sg_comm_info *comm, u32 value)
{
	struct bm_device_info *bmdi = container_of(comm, struct bm_device_info, comm);

	bm_write32(bmdi, COMM_SHM_START_ADDR + VETH_HANDSHAKE_REG, value);
}

static u32 comm_get_handshake(struct sg_comm_info *comm)
{
	struct bm_device_info *bmdi = container_of(comm, struct bm_device_info, comm);

	return bm_read32(bmdi, COMM_SHM_START_ADDR + VETH_HANDSHAKE_REG);
}

static int comm_handshake(struct sg_comm_info *comm)
{
	int count;
	struct pt *pt;
	struct bm_device_info *bmdi = container_of(comm, struct bm_device_info, comm);

	pt = pt_init(&comm->pdev->dev, COMM_SHM_START_ADDR, COMM_DMA_TX_BUFFER_SIZE, COMM_DMA_RX_BUFFER_SIZE, bmdi);
	if (pt) {
		comm->ring_buffer = pt;
	} else {
		dev_err(&comm->pdev->dev, "comm create ring buffer failed !");
		return -ENOMEM;
	}

	comm_set_handshake(comm, HOST_READY_FLAG);
	count = 0;
	while (comm_get_handshake(comm) != DEVICE_READY_FLAG) {
		count++;
		if (count == 65535)
			return 1;
	}
	mutex_init(&comm->data_mutex);
	mutex_init(&comm->msg_mutex);
	atomic_set(&comm->buffer_ready, 1);
	dev_info(&comm->pdev->dev, "comm carrier on!\n");
	return 0;
}

static int comm_ndo_kthread(void *arg)
{
	struct sg_comm_info *comm = (struct sg_comm_info *)arg;
	int ret;

	comm_set_handshake(comm, 0);
	while (1) {
		if (kthread_should_stop())
			break;
		if (comm_get_handshake(comm) == DEVICE_READY_FLAG) {
			if (atomic_cmpxchg(&comm->carrier_on, 0, 1) == 0) {
				ret = comm_handshake(comm);
				if (ret)
					goto exit;
			}
		} else {
			if (atomic_cmpxchg(&comm->carrier_on, 1, 0) == 1)
				dev_info(&comm->pdev->dev, "comm carrier off!\n");
		}

		if (atomic_read(&comm->buffer_ready))
			msleep(1000);
		else
			msleep(20);
	}
exit:
	dev_info(&comm->pdev->dev, "comm kthread exit!\n");
	return ret;
}

int sg_comm_init(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	int ret;
	struct sg_comm_info *comm;
	struct task_struct *task;

	if (bmdi->cinfo.chip_id == 0x1686)
		return 0;

	comm = &bmdi->comm;
	comm->pdev = pdev;

	atomic_set(&comm->carrier_on, 0);
	atomic_set(&comm->buffer_ready, 0);
	sg_comm_clear_queue(bmdi);
	task = kthread_run(comm_ndo_kthread, comm, "comm%d_kthread", bmdi->dev_index);
	if (IS_ERR(task)) {
		ret = PTR_ERR(task);
		dev_err(&pdev->dev, "create kthread failed %d\n", ret);
		return ret;
	}

	comm->cd_task = task;

	return 0;
}

void sg_comm_deinit(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1686)
		return;

	sg_comm_clear_queue(bmdi);
	comm_set_handshake(&bmdi->comm, 0);
	kthread_stop(bmdi->comm.cd_task);
	pt_uninit(bmdi->comm.ring_buffer);
}

int sg_comm_send(struct bm_device_info *bmdi, void *data, int len)
{
	struct sg_comm_info *comm = &bmdi->comm;

	if (!atomic_read(&comm->buffer_ready)) {
		pr_err("ring buffer is not ready!\n");
		return -EAGAIN;
	}

	pt_load_tx(comm->ring_buffer);
	if (comm_pt_send(comm->ring_buffer, data, len) < 0) {
		pr_err("pt send data error!\n");
		return -ENOMEM;
	}

	pt_store_tx(comm->ring_buffer);

	return len;
}

int sg_comm_recv(struct bm_device_info *bmdi, void *data, int len)
{
	struct sg_comm_info *comm = &bmdi->comm;
	struct pt *pt;
	int ret;
	int recv_len;

	if (!atomic_read(&comm->buffer_ready))
		return -EAGAIN;

	pt = comm->ring_buffer;
	if (pt_load_rx(pt) == true) {
		recv_len = pt_pkg_len(pt);
		if (recv_len < 0) {
			pr_info("data pkg_len is invalid: %d\n", recv_len);
			return -1;
		}
		if (recv_len > len) {
			pr_info("receive pkg len %d over buffer len: %d!", recv_len, len);
			return -2;
		}
		ret = comm_pt_recv(pt, data, recv_len);

		pt_store_rx(pt);
	} else {
		ret = 0;
	}

	return ret;
}

int sg_comm_msg_send(struct bm_device_info *bmdi, void __user *data, int len)
{
	int head, tail, ret;
	unsigned int total_len, pkg_len, tmp, used, free, offset;
	void __iomem *bar_vaddr;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	head = bm_read32(bmdi, COMM_SEND_MSG_START_ADDR + COMM_MSG_HEAD_OFFSET);
	tail = bm_read32(bmdi, COMM_SEND_MSG_START_ADDR + COMM_MSG_TAIL_OFFSET);
	total_len = round_up(len + sizeof(pkg_len), PT_ALIGN);

	if (head >= tail)
		used = head - tail;
	else
		used = COMM_MSG_BUFFER_SIZE - tail + head;

	free = COMM_MSG_BUFFER_SIZE - used - 1;
	if (total_len > free) {
		pr_err("msg fifo not enough! used: %u, head: %d, tail: %d\n", used, head, tail);
		return -ENOMEM;
	}

	bm_get_bar_offset(pbar_info, COMM_SEND_MSG_BUFFER_START_ADDR, &bar_vaddr, &offset);

	pkg_len = cpu_to_le32(len);
	tmp = min_t(unsigned int, sizeof(pkg_len), COMM_MSG_BUFFER_SIZE - head);
	memcpy(bar_vaddr + offset + head, &pkg_len, tmp);
	memcpy(bar_vaddr + offset, (char *)&pkg_len + tmp, sizeof(pkg_len) - tmp);
	head = (head + sizeof(pkg_len)) % COMM_MSG_BUFFER_SIZE;

	tmp = min_t(unsigned int, len, COMM_MSG_BUFFER_SIZE - head);
	ret = copy_from_user(bar_vaddr + offset + head, data, tmp);
	ret |= copy_from_user(bar_vaddr + offset, (char *)data + tmp, len - tmp);
	if (ret != 0) {
		pr_err("copy from user error when msg send\n");
		return head - sizeof(pkg_len);
	}
	head = (head + total_len - sizeof(pkg_len)) % COMM_MSG_BUFFER_SIZE;

	bm_write32(bmdi, COMM_SEND_MSG_START_ADDR + COMM_MSG_HEAD_OFFSET, head);

	return len;
}

int sg_comm_msg_recv(struct bm_device_info *bmdi, void __user *data, int len)
{
	int head, tail, ret;
	unsigned int total_len, pkg_len, tmp, offset;
	void __iomem *bar_vaddr;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	head = bm_read32(bmdi, COMM_RECV_MSG_START_ADDR + COMM_MSG_HEAD_OFFSET);
	tail = bm_read32(bmdi, COMM_RECV_MSG_START_ADDR + COMM_MSG_TAIL_OFFSET);

	if (head == tail)
		return 0;

	bm_get_bar_offset(pbar_info, COMM_RECV_MSG_BUFFER_START_ADDR, &bar_vaddr, &offset);

	pkg_len = le32_to_cpu(bm_read32(bmdi, COMM_RECV_MSG_BUFFER_START_ADDR + tail));
	if (pkg_len < 0) {
		pr_err("msg pkg len is %d\n", pkg_len);
		return -1;
	}
	if (pkg_len > len) {
		pr_info("msg receive pkg len %d over buffer len: %d!", pkg_len, len);
		return -2;
	}

	total_len = round_up(pkg_len + sizeof(u32), PT_ALIGN);
	tail = (tail + sizeof(u32)) % COMM_MSG_BUFFER_SIZE;

	tmp = min_t(unsigned int, pkg_len, COMM_MSG_BUFFER_SIZE - tail);
	ret = copy_to_user(data, bar_vaddr + offset + tail, tmp);
	ret |= copy_to_user(data + tmp, bar_vaddr + offset, pkg_len - tmp);
	if (ret != 0) {
		pr_err("copy to user error when msg recv!\n");
		return tail - sizeof(u32);
	}
	tail = (tail + total_len - sizeof(u32)) % COMM_MSG_BUFFER_SIZE;

	bm_write32(bmdi, COMM_RECV_MSG_START_ADDR + COMM_MSG_TAIL_OFFSET, tail);

	return pkg_len;
}
