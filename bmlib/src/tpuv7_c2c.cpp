/*
 * tpuv7_c2c.cpp — C2C topology query for libsophon (84x6).
 *
 * Phase 1: per-chip C2C wiring comes from host INI files (arch2
 * [system_if]/[pcie_N] format), one file per chip. tpuRtSetupTopology parses
 * them, hands the per-chip port descriptors to the 84x6 driver, which assembles
 * the N x N matrix and writes it back into every chip's SRAM. tpuRtGetTopology*
 * read that matrix back.
 *
 * The struct/ioctl layouts here MUST match driver/84x6/84x6_topology.h.
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bmlib_internal.h"
#include "bmlib_runtime.h"
#include "tpuv7_c2c.h"

#if !defined(USING_CMODEL) && !defined(SOC_MODE)

/* ---- constants (keep in sync with driver/84x6/84x6_topology.h) ---- */
#define C2C_PORT_NUM            4
#define PCIE_DATA_LINK_PCIE     0
#define PCIE_DATA_LINK_C2C      1
#define C2C_NO_CDMA_PORT        (-1)

#define C2C_INI_DIR_ENV         "TPU_C2C_INI_DIR"
#define C2C_INI_DIR_DEFAULT     "/etc/sophon/c2c"

/* Per-port descriptor, byte-identical to the kernel struct c2c_pcie_info. */
struct c2c_pcie_info {
	uint64_t slot_id;
	uint64_t socket_id;
	uint64_t pcie_id;
	uint64_t send_port;
	uint64_t recv_port;
	uint64_t enable;
	uint64_t data_link_type;
	uint64_t link_role;
	uint64_t link_role_gpio;
	uint64_t perst_gpio;
	uint64_t phy_role;
	uint64_t peer_slotid;
	uint64_t peer_socketid;
	uint64_t peer_pcie_id;
	uint64_t max_link_speed;
	uint64_t current_link_width;
	uint64_t current_link_speed;
	uint64_t send_cdma_pa;
	uint64_t recv_cdma_pa;
	uint64_t pcie_route;
};

/* ioctl argument, byte-identical to the kernel struct bm_c2c_topology_ioctl. */
struct bm_c2c_topology_ioctl {
	uint64_t buf;
	uint32_t chip_num;
	uint32_t port_num;
};

static char *ini_trim(char *s)
{
	char *end;

	while (*s == ' ' || *s == '\t')
		s++;
	end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
			   end[-1] == '\r' || end[-1] == '\n'))
		*--end = '\0';
	return s;
}

/*
 * Parse one chip's INI (arch2 format) into its C2C_PORT_NUM port descriptors.
 * Returns 0 if the file was read (a missing file leaves all ports disabled and
 * returns -1, which the caller treats as "this chip has no C2C links").
 */
static int parse_chip_ini(int devid, struct c2c_pcie_info *ports)
{
	const char *dir = getenv(C2C_INI_DIR_ENV);
	char path[512];
	char line[256];
	uint64_t slot_id = 0, socket_id = devid; /* legacy self-id; driver ignores these
						  * and re-derives row/col from bm_card */
	int cur_pcie = -1;        /* >=0 while inside a [pcie_N] section */
	int in_system_if = 0;
	FILE *fp;
	int i;

	for (i = 0; i < C2C_PORT_NUM; i++) {
		memset(&ports[i], 0, sizeof(ports[i]));
		ports[i].pcie_id = i;
		ports[i].data_link_type = PCIE_DATA_LINK_PCIE; /* not C2C until INI says so */
		ports[i].send_port = (uint64_t)C2C_NO_CDMA_PORT;
		ports[i].recv_port = (uint64_t)C2C_NO_CDMA_PORT;
	}

	snprintf(path, sizeof(path), "%s/chip%d.ini",
		 dir ? dir : C2C_INI_DIR_DEFAULT, devid);
	fp = fopen(path, "r");
	if (!fp) {
		bmlib_log("tpuv7_c2c", BMLIB_LOG_WARNING,
			  "c2c: no INI for chip %d at %s, treating as no links\n",
			  devid, path);
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		char *s = ini_trim(line);
		char *eq;

		if (*s == '\0' || *s == '#' || *s == ';')
			continue;

		if (*s == '[') {
			char *rb = strchr(s, ']');

			if (rb)
				*rb = '\0';
			s++;
			cur_pcie = -1;
			in_system_if = 0;
			if (strcmp(s, "system_if") == 0)
				in_system_if = 1;
			else if (sscanf(s, "pcie_%d", &cur_pcie) != 1)
				cur_pcie = -1;
			continue;
		}

		eq = strchr(s, '=');
		if (!eq)
			continue;
		*eq = '\0';
		char *key = ini_trim(s);
		char *val = ini_trim(eq + 1);

		if (in_system_if) {
			if (strcmp(key, "slot_id") == 0)
				slot_id = strtoull(val, NULL, 0);
			else if (strcmp(key, "socket_id") == 0)
				socket_id = strtoull(val, NULL, 0);
			continue;
		}

		if (cur_pcie < 0 || cur_pcie >= C2C_PORT_NUM)
			continue;

		struct c2c_pcie_info *p = &ports[cur_pcie];

		if (strcmp(key, "enable") == 0) {
			p->enable = (strcmp(val, "yes") == 0) ? 1 : 0;
		} else if (strcmp(key, "data_link_type") == 0) {
			p->data_link_type = (strcmp(val, "c2c") == 0) ?
				PCIE_DATA_LINK_C2C : PCIE_DATA_LINK_PCIE;
		} else if (strcmp(key, "send_port") == 0) {
			p->send_port = (strcmp(val, "no") == 0) ?
				(uint64_t)C2C_NO_CDMA_PORT : strtoull(val, NULL, 0);
		} else if (strcmp(key, "recv_port") == 0) {
			p->recv_port = (strcmp(val, "no") == 0) ?
				(uint64_t)C2C_NO_CDMA_PORT : strtoull(val, NULL, 0);
		} else if (strcmp(key, "peer_socketid") == 0) {
			p->peer_socketid = strtoull(val, NULL, 0);
		} else if (strcmp(key, "peer_pcieid") == 0) {
			p->peer_pcie_id = strtoull(val, NULL, 0);
		} else if (strcmp(key, "peer_slotid") == 0) {
			p->peer_slotid = strtoull(val, NULL, 0);
			p->link_role = 1; /* mark that peer_slotid was given explicitly */
		}
	}
	fclose(fp);

	/* Apply this chip's identity to every port; default peer to the same slot. */
	for (i = 0; i < C2C_PORT_NUM; i++) {
		ports[i].slot_id = slot_id;
		ports[i].socket_id = socket_id;
		if (ports[i].link_role == 0)  /* no explicit peer_slotid -> same slot */
			ports[i].peer_slotid = slot_id;
		ports[i].link_role = 0;
	}
	return 0;
}

extern "C" {

tpuRtStatus_t tpuRtSetupC2C(int device_id)
{
	bm_handle_t handle = nullptr;
	int status = 0;
	int ret;

	if (bm_dev_request(&handle, device_id) != BM_SUCCESS)
		return tpuRtErrorNoDevice;

	ret = platform_ioctl(handle, BMDEV_SETUP_C2C, &status);
	bm_dev_free(handle);

	if (ret != 0 || status != 0) {
		bmlib_log("tpuv7_c2c", BMLIB_LOG_ERROR,
			  "chip %d setup c2c failed (ret=%d status=%d)\n",
			  device_id, ret, status);
		return tpuRtErrFailure;
	}
	return tpuRtSuccess;
}

tpuRtStatus_t tpuRtSetupTopology(void)
{
	bm_handle_t handle = nullptr;
	struct bm_c2c_topology_ioctl arg;
	int chip_num = 0;
	int i, ret;

	if (bm_dev_getcount(&chip_num) != BM_SUCCESS || chip_num <= 0)
		return tpuRtErrorNoDevice;

	std::vector<c2c_pcie_info> infos((size_t)chip_num * C2C_PORT_NUM);
	for (i = 0; i < chip_num; i++)
		parse_chip_ini(i, &infos[(size_t)i * C2C_PORT_NUM]);

	for (i = 0; i < chip_num; i++) {
		if (tpuRtSetupC2C(i) != tpuRtSuccess)
			return tpuRtErrFailure;
	}

	if (bm_dev_request(&handle, 0) != BM_SUCCESS)
		return tpuRtErrorNoDevice;

	memset(&arg, 0, sizeof(arg));
	arg.buf = (uint64_t)(uintptr_t)infos.data();
	arg.chip_num = (uint32_t)chip_num;
	arg.port_num = C2C_PORT_NUM;

	ret = platform_ioctl(handle, BMDEV_SETUP_TOPOLOGY, &arg);
	bm_dev_free(handle);

	if (ret != 0) {
		bmlib_log("tpuv7_c2c", BMLIB_LOG_ERROR,
			  "setup topology failed in driver (ret=%d)\n", ret);
		return tpuRtErrFailure;
	}
	return tpuRtSuccess;
}

tpuRtStatus_t tpuRtGetTopologyV2(struct c2c_port_info_v2 **topology)
{
	bm_handle_t handle = nullptr;
	struct bm_c2c_topology_ioctl arg;
	int chip_num = 0;
	int ret;

	if (topology == nullptr)
		return tpuRtErrFailure;
	if (bm_dev_getcount(&chip_num) != BM_SUCCESS || chip_num <= 0)
		return tpuRtErrorNoDevice;
	if (bm_dev_request(&handle, 0) != BM_SUCCESS)
		return tpuRtErrorNoDevice;

	memset(&arg, 0, sizeof(arg));
	/* arch2 ABI: the caller passes the matrix base reinterpreted as (v2**),
	 * so the pointer value itself is the buffer address. */
	arg.buf = (uint64_t)(uintptr_t)topology;
	arg.chip_num = (uint32_t)chip_num;
	arg.port_num = 0;

	ret = platform_ioctl(handle, BMDEV_GET_TOPOLOGY, &arg);
	bm_dev_free(handle);

	if (ret != 0) {
		bmlib_log("tpuv7_c2c", BMLIB_LOG_ERROR,
			  "get topology failed in driver (ret=%d)\n", ret);
		return tpuRtErrFailure;
	}
	return tpuRtSuccess;
}

tpuRtStatus_t tpuRtGetTopology(struct c2c_port_info **topology)
{
	int chip_num = 0;
	int i, total;
	tpuRtStatus_t ret;

	if (topology == nullptr)
		return tpuRtErrFailure;
	if (bm_dev_getcount(&chip_num) != BM_SUCCESS || chip_num <= 0)
		return tpuRtErrorNoDevice;

	total = chip_num * chip_num;
	std::vector<c2c_port_info_v2> v2((size_t)total);

	/* Same (v2**)base ABI as above. */
	ret = tpuRtGetTopologyV2((struct c2c_port_info_v2 **)v2.data());
	if (ret != tpuRtSuccess)
		return ret;

	/* Down-convert v2 -> v1, taking link [0] of each cell. */
	struct c2c_port_info *out = (struct c2c_port_info *)topology;
	for (i = 0; i < total; i++) {
		out[i].chip_num = v2[i].chip_num;
		out[i].src_device_id = v2[i].src_device_id[0];
		out[i].dst_device_id = v2[i].dst_device_id[0];
		out[i].src_pcie_id = v2[i].src_pcie_id[0];
		out[i].dst_pcie_id = v2[i].dst_pcie_id[0];
		out[i].send_port = v2[i].send_port[0];
		out[i].recv_port = v2[i].recv_port[0];
	}
	return tpuRtSuccess;
}

} /* extern "C" */

#else /* USING_CMODEL || SOC_MODE: C2C topology not applicable */

extern "C" {
tpuRtStatus_t tpuRtSetupC2C(int device_id) { (void)device_id; return tpuRtSuccess; }
tpuRtStatus_t tpuRtSetupTopology(void) { return tpuRtSuccess; }
tpuRtStatus_t tpuRtGetTopology(struct c2c_port_info **topology) { (void)topology; return tpuRtSuccess; }
tpuRtStatus_t tpuRtGetTopologyV2(struct c2c_port_info_v2 **topology) { (void)topology; return tpuRtSuccess; }
}

#endif
