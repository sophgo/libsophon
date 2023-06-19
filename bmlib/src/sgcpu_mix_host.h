#ifndef SOPHGO_FOR_DAHUA_HOST_COMMUNICATION_H
#define SOPHGO_FOR_DAHUA_HOST_COMMUNICATION_H

#if defined(__cplusplus)
extern "C" {
#endif
#ifndef SOC_MODE
#include "bmlib_runtime.h"

#define SGCPU_RUNTIME_LOG_TAG "sgcpu_runtime"

typedef struct SG_HANDLE_CONTROL {
    int         ref_cnt;
    bm_handle_t handle;
} sg_handle_t;

typedef enum arm9_fw_mode {
    FW_PCIE_MODE,
    FW_SOC_MODE,
    FW_MIX_MODE
} bm_arm9_fw_mode;

bm_status_t bm_setup_veth(bm_handle_t handle);
bm_status_t bm_remove_veth(bm_handle_t handle);
bm_status_t bm_force_reset_bmcpu(bm_handle_t handle);
bm_status_t bmcpu_set_arm9_fw_mode(bm_handle_t handle, bm_arm9_fw_mode mode);
bm_status_t bmcpu_load_boot(bm_handle_t handle, char *boot_file);
bm_status_t bmcpu_load_kernel(bm_handle_t handle, char *kernel_file);
bm_status_t bm_trigger_a53(bm_handle_t handle, int id);
bm_status_t bm_connect_a53(bm_handle_t handle, int timeout);
int bm_write_data(bm_handle_t handle, char *buf, int len);
int bm_read_data(bm_handle_t handle, char *buf, int len);
int bm_write_msg(bm_handle_t handle, char *buf, int len);
int bm_read_msg(bm_handle_t handle, char *buf, int len);

#endif
#if defined(__cplusplus)
}
#endif

#endif /* SGCPU_DAHUA_H */
