// test_c2c_topology.cpp
//
// Exercises the libsophon C2C topology API (source/ABI compatible with
// tpuv7-runtime):
//   tpuRtSetupTopology()   - parse per-chip INI, assemble the N x N matrix,
//                            and write it back into every chip's SRAM.
//   tpuRtGetTopologyV2()   - read the matrix back (v2: up to 2 links/pair).
//   tpuRtGetTopology()     - same, down-converted to v1 (single link/pair).
//
// Build:  make            (see Makefile; needs libsophon headers + libbmlib)
// Run:    TPU_C2C_INI_DIR=$(pwd) ./test_c2c_topology
//
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "bmlib_runtime.h"   // bm_dev_getcount, BM_SUCCESS
#include "tpuv7_c2c.h"       // tpuRt* C2C topology API

static int link_count(const struct c2c_port_info_v2 &c)
{
	return (c.pcie_link_num == 0xffffffffu) ? 0 : (int)c.pcie_link_num;
}

static void print_matrix(const std::vector<c2c_port_info_v2> &m, int n)
{
	printf("\n        ");
	for (int j = 0; j < n; j++)
		printf(" ->chip%-2d ", j);
	printf("\n");
	for (int i = 0; i < n; i++) {
		printf("chip%-2d  ", i);
		for (int j = 0; j < n; j++) {
			const c2c_port_info_v2 &c = m[(size_t)i * n + j];
			int links = link_count(c);

			if (links == 0)
				printf("    .     ");
			else
				printf(" s%d/r%d L%d ",
				       c.send_port[0], c.recv_port[0], links);
		}
		printf("\n");
	}
	printf("\nlegend: s=send_port r=recv_port (cdma index) L=#links  '.'=no link\n");
}

int main(void)
{
	int n = 0;

	if (bm_dev_getcount(&n) != BM_SUCCESS || n <= 0) {
		fprintf(stderr, "no devices found (is the driver loaded?)\n");
		return 1;
	}
	printf("device count = %d\n", n);

	/* 1) discover topology from INI and write the matrix into each chip. */
	if (tpuRtSetupTopology() != tpuRtSuccess) {
		fprintf(stderr, "tpuRtSetupTopology failed "
				"(check $TPU_C2C_INI_DIR/chip*.ini)\n");
		return 1;
	}
	printf("tpuRtSetupTopology: ok\n");

	/* 2) read the N x N matrix back (v2). NOTE the arch2 ABI: the caller's
	 * matrix base is passed reinterpreted as (c2c_port_info_v2 **). */
	std::vector<c2c_port_info_v2> mat((size_t)n * n);
	if (tpuRtGetTopologyV2((struct c2c_port_info_v2 **)mat.data()) != tpuRtSuccess) {
		fprintf(stderr, "tpuRtGetTopologyV2 failed\n");
		return 1;
	}
	printf("tpuRtGetTopologyV2: ok\n");
	print_matrix(mat, n);

	/* 3) per directed link detail. */
	printf("\ndirected links:\n");
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++) {
			const c2c_port_info_v2 &c = mat[(size_t)i * n + j];
			for (int l = 0; l < link_count(c); l++)
				printf("  chip%d.pcie%d -> chip%d.pcie%d   "
				       "send=cdma%d recv=cdma%d\n",
				       c.src_device_id[l], c.src_pcie_id[l],
				       c.dst_device_id[l], c.dst_pcie_id[l],
				       c.send_port[l], c.recv_port[l]);
		}

	/* 4) v1 readback sanity (single link per pair). */
	std::vector<c2c_port_info> mat1((size_t)n * n);
	c2c_port_info *base1 = mat1.data();
	if (tpuRtGetTopology((struct c2c_port_info **)base1) != tpuRtSuccess) {
		fprintf(stderr, "tpuRtGetTopology (v1) failed\n");
		return 1;
	}
	printf("\ntpuRtGetTopology (v1): ok (chip_num field of [0][1] = %u)\n",
	       mat1[(n > 1) ? 1 : 0].chip_num);

	return 0;
}
