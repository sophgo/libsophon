/*
 * tpuv7_c2c.h — C2C topology query API for libsophon (84x6).
 *
 * Source/ABI compatible with the tpuv7-runtime tpuRt* C2C APIs and the
 * c2c_port_info / c2c_port_info_v2 data contract, so upper-layer code written
 * against tpuv7-runtime can obtain the chip-to-chip topology on 84x6 unchanged.
 *
 * NOTE: if you also link against the original tpuv7-runtime headers (which
 * define the same names), include only one of them.
 */
#ifndef TPUV7_C2C_H
#define TPUV7_C2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DECL_EXPORT
#ifdef _WIN32
#define DECL_EXPORT __declspec(dllexport)
#else
#define DECL_EXPORT __attribute__((visibility("default")))
#endif
#endif

#ifndef TPURT_STATUS_DEFINED
#define TPURT_STATUS_DEFINED
typedef enum {
	tpuRtSuccess        = 0,
	tpuRtErrFailure     = 1,
	tpuRtErrorNoDevice  = 2,
} tpuRtStatus_t;
#endif

#ifndef MAX_C2C_LINK_BETWEN2CHIP
#define MAX_C2C_LINK_BETWEN2CHIP (2)
#endif

/* v1: one directed link chip src -> dst (single link). 12 bytes. */
struct c2c_port_info {
	uint32_t chip_num;
	uint16_t src_device_id;
	uint16_t dst_device_id;
	uint8_t  src_pcie_id;
	uint8_t  dst_pcie_id;
	int8_t   send_port;
	int8_t   recv_port;
};

/* v2: cell[i*N+j] of the N x N matrix, up to 2 links chip i -> j. 24 bytes. */
struct c2c_port_info_v2 {
	uint32_t chip_num;
	uint32_t pcie_link_num;
	uint16_t src_device_id[MAX_C2C_LINK_BETWEN2CHIP];
	uint16_t dst_device_id[MAX_C2C_LINK_BETWEN2CHIP];
	uint8_t  src_pcie_id[MAX_C2C_LINK_BETWEN2CHIP];
	uint8_t  dst_pcie_id[MAX_C2C_LINK_BETWEN2CHIP];
	int8_t   send_port[MAX_C2C_LINK_BETWEN2CHIP];
	int8_t   recv_port[MAX_C2C_LINK_BETWEN2CHIP];
};

/**
 * tpuRtSetupC2C - report/verify the C2C link of one chip.
 * On 84x6 (Phase 1) the link is brought up at PCIe probe, so this validates
 * and returns success. @device_id is the chip index (0..count-1).
 */
DECL_EXPORT tpuRtStatus_t tpuRtSetupC2C(int device_id);

/**
 * tpuRtSetupTopology - discover the chip-to-chip topology and write the
 * N x N matrix into every chip's SRAM. The per-chip C2C wiring is read from
 * the host INI (arch2 [system_if]/[pcie_N] format), one file per chip.
 */
DECL_EXPORT tpuRtStatus_t tpuRtSetupTopology(void);

/**
 * tpuRtGetTopology - read back the N x N topology as v1 records.
 * @topology points at a caller-allocated c2c_port_info[count*count] buffer.
 */
DECL_EXPORT tpuRtStatus_t tpuRtGetTopology(struct c2c_port_info **topology);

/**
 * tpuRtGetTopologyV2 - read back the N x N topology as v2 records.
 * @topology points at a caller-allocated c2c_port_info_v2[count*count] buffer.
 */
DECL_EXPORT tpuRtStatus_t tpuRtGetTopologyV2(struct c2c_port_info_v2 **topology);

#ifdef __cplusplus
}
#endif

#endif /* TPUV7_C2C_H */
