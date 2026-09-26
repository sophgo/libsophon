// SPDX-License-Identifier: GPL-2.0
/*
 * 84x6 C2C topology — assemble the inter-chip topology matrix from per-chip
 * C2C port descriptors and write it back to every chip's SRAM.
 *
 * See 84x6_topology.h for the overall Phase-1 design.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/log2.h>
#include "bm_common.h"
#include "bm_io.h"
#include "bm_card.h"
#include "bm_uapi.h"
#include "84x6_topology.h"
#include "84x6_pcie.h"

/*
 * Map a (slot_id, socket_id) pair to a global device index (matrix row/col).
 *   slot_id   = card_index of the owning card (probe order)
 *   socket_id = chip's offset within that card = dev_index - dev_start_index
 * We resolve this against the driver's authoritative per-card grouping rather
 * than trusting the (slot,socket) the host INI/libsophon passed down: walk every
 * chip in the system, and for the one whose card_index and in-card offset match,
 * return its dev_index. Returns -1 if no such chip exists.
 */
static int c2c_slot_socket_to_devid(u64 slot_id, u64 socket_id)
{
	int chip_num = bm_get_chip_num_from_system();
	int i;

	for (i = 0; i < chip_num; i++) {
		struct bm_device_info *bmdi = bmdrv_get_bmdi_by_devid(i);
		struct bm_card *bmcd;

		if (!bmdi)
			continue;
		bmcd = bmdrv_card_get_bm_card(bmdi);
		if (!bmcd)
			continue;
		if ((u64)bmcd->card_index == slot_id &&
		    (u64)(bmdi->dev_index - bmcd->dev_start_index) == socket_id)
			return bmdi->dev_index;
	}
	return -1;
}

static void c2c_show_topology(struct c2c_port_info_v2 *topology, int chip_num)
{
	int i, j;

	for (i = 0; i < chip_num; i++)
		for (j = 0; j < chip_num; j++)
			pr_debug("c2c topo chip %d -> %d: links=%d src_pcie=%d dst_pcie=%d send=%d recv=%d\n",
				 i, j,
				 (int)(topology[i * chip_num + j].pcie_link_num == 0xffffffff ?
					0 : topology[i * chip_num + j].pcie_link_num),
				 topology[i * chip_num + j].src_pcie_id[0],
				 topology[i * chip_num + j].dst_pcie_id[0],
				 topology[i * chip_num + j].send_port[0],
				 topology[i * chip_num + j].recv_port[0]);
}

/*
 * Build the N x N matrix from the flat per-chip port array.
 *
 * The flat array is indexed by global device id: infos[i * C2C_PORT_NUM + k] is
 * port k of chip i, where i is the chip's dev_index. So the matrix row is simply
 * i; we do NOT trust the (slot_id, socket_id) the INI/libsophon put in the
 * descriptor. The column (peer) is resolved from the declared peer_slotid/
 * peer_socketid against the driver's authoritative per-card grouping via
 * c2c_slot_socket_to_devid().
 *
 * Unlike arch2 (which reads [self, peer] pairs exchanged over the C2C link),
 * the 84x6 host has global visibility of the whole board via the INI, so each
 * port only carries its own ("self") descriptor plus the declared peer ids. We
 * fill the forward direction chip i -> peer from chip i's own port; the reverse
 * direction is filled when the peer chip's matching port is processed. For a
 * symmetric INI (each link declared on both ends) this populates both directions.
 */
static void update_topology(struct c2c_pcie_info *infos,
			    struct c2c_port_info_v2 *topology, int chip_num)
{
	int i, k;

	for (i = 0; i < chip_num; i++) {
		for (k = 0; k < C2C_PORT_NUM; k++) {
			struct c2c_pcie_info *p = &infos[i * C2C_PORT_NUM + k];
			struct c2c_port_info_v2 *cell;
			int row, col, link_num;

			if (p->data_link_type != PCIE_DATA_LINK_C2C)
				continue;

			row = i; /* flat array index == global dev_index */
			col = c2c_slot_socket_to_devid(p->peer_slotid,
						       p->peer_socketid);
			if (row < 0 || row >= chip_num || col < 0 || col >= chip_num) {
				pr_err("c2c topo: bad chip id row=%d col=%d (peer slot=%llu socket=%llu chip_num=%d)\n",
				       row, col, p->peer_slotid, p->peer_socketid,
				       chip_num);
				continue;
			}

			cell = &topology[row * chip_num + col];
			if (cell->pcie_link_num == 0xffffffff)
				cell->pcie_link_num = 0;

			link_num = cell->pcie_link_num;
			pr_info("c2c topo: chip%d->chip%d link=%d pcie=%d->%d send=%d recv=%d\n", row, col, link_num, (int)p->pcie_id, (int)p->peer_pcie_id, (int)p->send_port, (int)p->recv_port);
			if (link_num >= MAX_C2C_LINK_BETWEN2CHIP) {
				pr_err("c2c topo: links between chip %d and %d exceed max %d\n",
				       row, col, MAX_C2C_LINK_BETWEN2CHIP);
				continue;
			}

			cell->src_device_id[link_num] = row;
			cell->dst_device_id[link_num] = col;
			cell->src_pcie_id[link_num] = p->pcie_id;
			cell->dst_pcie_id[link_num] = p->peer_pcie_id;
			cell->send_port[link_num] = (__s8)p->send_port;
			cell->recv_port[link_num] = (__s8)p->recv_port;
			cell->pcie_link_num = link_num + 1;
			cell->chip_num = chip_num;
		}
	}
}

/* Write a buffer to one chip's device SRAM, DW by DW, via the BAR1 window. */
static void c2c_writeback_chip(struct bm_device_info *bmdi, u64 dev_addr,
			       const void *buf, size_t size)
{
	const u32 *p32 = buf;
	size_t dwcnt = size / sizeof(u32);
	size_t i;

	pr_info("c2c writeback: chip=%d addr=0x%llx size=%zu dwcnt=%zu dw[0]=0x%x dw[6]=0x%x dw[7]=0x%x\n",
		bmdi->dev_index, dev_addr, size, dwcnt, p32[0], p32[6], p32[7]);
	for (i = 0; i < dwcnt; i++)
		bm_write32(bmdi, dev_addr + (u64)(i * sizeof(u32)), p32[i]);
}

/* Read a buffer from one chip's device SRAM, DW by DW, via the BAR1 window. */
static void c2c_readback_chip(struct bm_device_info *bmdi, u64 dev_addr,
			      void *buf, size_t size)
{
	u32 *p32 = buf;
	size_t dwcnt = size / sizeof(u32);
	size_t i;

	for (i = 0; i < dwcnt; i++)
		p32[i] = bm_read32(bmdi, dev_addr + (u64)(i * sizeof(u32)));
	pr_info("c2c readback: chip=%d addr=0x%llx dwcnt=%zu dw[0]=0x%x dw[6]=0x%x dw[7]=0x%x\n",
		bmdi->dev_index, dev_addr, dwcnt, p32[0], p32[6], p32[7]);
}

/*
 * BMDEV_SETUP_C2C: configure C2C outbound ATU on every card's first chip
 * so that all chips can route C2C traffic to this chip's BAR4 window.
 *
 * For each card in the system, write this chip's BAR4 physical address into
 * the C2C_OB_ATU entry indexed by dev_index on that card's card_bmdi[0].
 * The ATU ctrl enables the entry with chip-to-chip routing (dst != PC).
 *
 * If arg is non-NULL it receives a status int (0 = ok).
 */
int bm84x6_setup_c2c(struct bm_device_info *bmdi, unsigned long arg)
{
	u64 bar4_phy_addr = bmdi->cinfo.bar_info.bar4_start;
	u64 bar4_len = bmdi->cinfo.bar_info.bar4_len;
	u32 atu_id = (u32)bmdi->dev_index;
	u32 ctrl_val;
	int chip_num, i;
	int status = 0;

	if (atu_id >= 8)
		pr_warn("c2c setup_c2c: atu_id %u >= 8, may exceed C2C OB ATU range\n",
			atu_id);

	/* ob_en=1, ob_dst_pc=0, ob_size = log2(bar4_len) in bit[5:0] */
	ctrl_val = 0x80000000 | (ilog2(bar4_len) & 0x3f);

	chip_num = bm_get_chip_num_from_system();

	for (i = 0; i < chip_num; i++) {
		struct bm_device_info *target = bmdrv_get_bmdi_by_devid(i);

		if (!target)
			continue;
		/* Only configure card_bmdi[0] — the first chip on each card */
		if (target != target->bmcd->card_bmdi[0])
			continue;

		bm_write32(target, C2C_TOP_REG_BASE + C2C_OB_ATU_ADDR_UP(atu_id),
			   (u32)(bar4_phy_addr >> 32));
		bm_write32(target, C2C_TOP_REG_BASE + C2C_OB_ATU_ADDR_LOW(atu_id),
			   (u32)(bar4_phy_addr & 0xffffffff));
		bm_write32(target, C2C_TOP_REG_BASE + C2C_OB_ATU_CTRL(atu_id),
			   ctrl_val);
	}

	if (arg && put_user(status, (int __user *)arg))
		return -EFAULT;
	return 0;
}

/*
 * BMDEV_SETUP_TOPOLOGY: take the per-chip port descriptors from user space,
 * assemble the N x N matrix, and write it to every chip's SRAM.
 */
int bm84x6_setup_topology(struct bm_device_info *bmdi, unsigned long arg)
{
	struct bm_c2c_topology_ioctl karg;
	struct c2c_pcie_info *infos = NULL;
	struct c2c_port_info_v2 *topology = NULL;
	size_t infos_sz, topo_sz;
	int chip_num, port_num, i, ret = 0;

	if (copy_from_user(&karg, (void __user *)arg, sizeof(karg)))
		return -EFAULT;

	chip_num = karg.chip_num;
	port_num = karg.port_num ? karg.port_num : C2C_PORT_NUM;
	if (chip_num <= 0 || chip_num > BM_MAX_CHIP_NUM || port_num != C2C_PORT_NUM) {
		pr_err("c2c setup_topology: invalid chip_num=%d port_num=%d\n",
		       chip_num, port_num);
		return -EINVAL;
	}

	infos_sz = sizeof(*infos) * chip_num * port_num;
	topo_sz = sizeof(*topology) * chip_num * chip_num;

	infos = kzalloc(infos_sz, GFP_KERNEL);
	topology = kmalloc(topo_sz, GFP_KERNEL);
	if (!infos || !topology) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(infos, (void __user *)(uintptr_t)karg.buf, infos_sz)) {
		ret = -EFAULT;
		goto out;
	}

	memset(topology, 0xff, topo_sz);
	update_topology(infos, topology, chip_num);
	c2c_show_topology(topology, chip_num);

	/* Write the full matrix into every chip's SRAM so the device side
	 * (AP/TP firmware, CDMA C2C routing) can consume it.
	 */
	for (i = 0; i < chip_num; i++) {
		struct bm_device_info *cbmdi = bmdrv_get_bmdi_by_devid(i);

		if (!cbmdi) {
			pr_err("c2c setup_topology: no device for chip %d\n", i);
			continue;
		}
		c2c_writeback_chip(cbmdi, ALL_CHIP_C2C_PORT_INFO_ADDR,
				   topology, topo_sz);
	}

out:
	kfree(infos);
	kfree(topology);
	return ret;
}

/*
 * BMDEV_GET_TOPOLOGY: read the matrix back from this chip's SRAM and return it.
 * The kernel sizes the matrix from the system chip count; the caller must
 * provide a buffer of chip_num*chip_num*sizeof(c2c_port_info_v2) bytes.
 */
int bm84x6_get_topology(struct bm_device_info *bmdi, unsigned long arg)
{
	struct bm_c2c_topology_ioctl karg;
	struct c2c_port_info_v2 *topology = NULL;
	size_t topo_sz;
	int chip_num, ret = 0;

	if (copy_from_user(&karg, (void __user *)arg, sizeof(karg)))
		return -EFAULT;

	chip_num = bm_get_chip_num_from_system();
	if (chip_num <= 0 || chip_num > BM_MAX_CHIP_NUM)
		return -EINVAL;
	/* Don't overflow the caller's buffer if it sized for fewer chips. */
	if (karg.chip_num && (u32)chip_num > karg.chip_num) {
		pr_err("c2c get_topology: caller buffer too small (have %u, need %d)\n",
		       karg.chip_num, chip_num);
		return -EINVAL;
	}

	topo_sz = sizeof(*topology) * chip_num * chip_num;
	topology = kmalloc(topo_sz, GFP_KERNEL);
	if (!topology)
		return -ENOMEM;

	c2c_readback_chip(bmdi, ALL_CHIP_C2C_PORT_INFO_ADDR, topology, topo_sz);

	if (copy_to_user((void __user *)(uintptr_t)karg.buf, topology, topo_sz))
		ret = -EFAULT;

	kfree(topology);
	return ret;
}
