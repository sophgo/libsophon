/* SPDX-License-Identifier: GPL-2.0 */
/*
 * 84x6 C2C topology — kernel side.
 *
 * Provides the BMDEV_SETUP_C2C / BMDEV_SETUP_TOPOLOGY / BMDEV_GET_TOPOLOGY
 * ioctls, source/ABI compatible with the tpuv7-runtime tpuRt* C2C APIs.
 *
 * Phase 1 design (host-INI source):
 *   - The host user library parses the board INI ([pcie_N], arch2 format) and
 *     passes a flat array of per-chip C2C port descriptors (struct c2c_pcie_info)
 *     down through BMDEV_SETUP_TOPOLOGY.
 *   - The kernel assembles the N x N c2c_port_info_v2 matrix (see update_topology)
 *     and writes it back, DW by DW via bm_write32(), to every chip's SRAM at
 *     ALL_CHIP_C2C_PORT_INFO_ADDR.
 *   - BMDEV_GET_TOPOLOGY reads that matrix back and returns it to user space.
 *
 * When the boot path later moves to "driver-loaded FSBL + 84x6 AP init" (so the
 * per-chip pcie_info lives in device SRAM), only the *source* of c2c_pcie_info
 * changes: the kernel reads it from each chip via bm_read32() instead of taking
 * it from user space. The matrix layout and the get/set contract stay identical.
 */
#ifndef _BM84X6_TOPOLOGY_H_
#define _BM84X6_TOPOLOGY_H_

#include <linux/types.h>

struct bm_device_info;

/* ---- 84x6 C2C constants ---- */
#define C2C_PORT_NUM			4	/* each 84x6 chip has 4 c2c ports */
/*
 * The topology matrix is indexed by global device id (driver dev_index). A
 * chip's (slot_id, socket_id) is resolved to its dev_index against the driver's
 * per-card grouping (struct bm_card: card_index == slot_id, and socket_id ==
 * dev_index - dev_start_index); see c2c_slot_socket_to_devid() in 84x6_topology.c.
 */
/* Device SRAM address where the assembled N x N matrix is written back (tentative). */
#define ALL_CHIP_C2C_PORT_INFO_ADDR	0x1004000000U

#define PCIE_DATA_LINK_PCIE		0
#define PCIE_DATA_LINK_C2C		1
#define PCIE_LINK_ROLE_RC		0
#define PCIE_LINK_ROLE_EP		1

#define MAX_C2C_LINK_BETWEN2CHIP	2

/*
 * Per-port descriptor (one C2C port of one chip), parsed from the host INI.
 * Layout matches arch2 tpuv7-runtime struct pcie_info so the future
 * FSBL-written-SRAM source is a drop-in.
 */
struct c2c_pcie_info {
	__u64 slot_id;
	__u64 socket_id;
	__u64 pcie_id;
	__u64 send_port;	/* cdma0 / cdma1 on 84x6 */
	__u64 recv_port;
	__u64 enable;
	__u64 data_link_type;	/* PCIE_DATA_LINK_C2C / _PCIE */
	__u64 link_role;
	__u64 link_role_gpio;
	__u64 perst_gpio;
	__u64 phy_role;
	__u64 peer_slotid;
	__u64 peer_socketid;
	__u64 peer_pcie_id;
	__u64 max_link_speed;
	__u64 current_link_width;
	__u64 current_link_speed;
	__u64 send_cdma_pa;
	__u64 recv_cdma_pa;
	__u64 pcie_route;
};

/*
 * One cell of the N x N topology matrix. cell[i * N + j] describes the directed
 * link(s) from chip i to chip j (up to MAX_C2C_LINK_BETWEN2CHIP). send/recv_port
 * == -1 means "no link". Layout is byte-identical to arch2 c2c_port_info_v2 (24B).
 */
struct c2c_port_info_v2 {
	__u32 chip_num;
	__u32 pcie_link_num;
	__u16 src_device_id[MAX_C2C_LINK_BETWEN2CHIP];
	__u16 dst_device_id[MAX_C2C_LINK_BETWEN2CHIP];
	__u8  src_pcie_id[MAX_C2C_LINK_BETWEN2CHIP];
	__u8  dst_pcie_id[MAX_C2C_LINK_BETWEN2CHIP];
	__s8  send_port[MAX_C2C_LINK_BETWEN2CHIP];
	__s8  recv_port[MAX_C2C_LINK_BETWEN2CHIP];
};

/* ioctl argument for BMDEV_SETUP_TOPOLOGY and BMDEV_GET_TOPOLOGY. */
struct bm_c2c_topology_ioctl {
	__u64 buf;		/* setup: c2c_pcie_info[chip_num*port_num]
				 * get:   c2c_port_info_v2[chip_num*chip_num] */
	__u32 chip_num;
	__u32 port_num;		/* = C2C_PORT_NUM (setup only) */
};

int bm84x6_setup_c2c(struct bm_device_info *bmdi, unsigned long arg);
int bm84x6_setup_topology(struct bm_device_info *bmdi, unsigned long arg);
int bm84x6_get_topology(struct bm_device_info *bmdi, unsigned long arg);

#endif /* _BM84X6_TOPOLOGY_H_ */
